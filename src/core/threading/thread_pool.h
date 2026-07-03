#ifndef FEM_CORE_THREADING_THREAD_POOL_H
#define FEM_CORE_THREADING_THREAD_POOL_H

#include "threading_constants.h"

#include <atomic>
#include <cstddef>
#include <future>
#include <memory>
#include <new>
#include <thread>
#include <type_traits>
#include <utility>

namespace fem::threading {

class ThreadPool final {
public:
    enum class TaskSubmitStatus : std::uint8_t {
        scheduled,
        task_allocation_failed,
        pool_unavailable,
        pool_stopped,
    };

    [[nodiscard]] static bool is_scheduled(TaskSubmitStatus status) noexcept {
        return status == TaskSubmitStatus::scheduled;
    }

    template <class Result>
    class [[nodiscard]] SubmitResult final {
    public:
        SubmitResult() noexcept = default;

        SubmitResult(TaskSubmitStatus status, std::future<Result>&& future) noexcept
            : status_(status)
            , future_(std::move(future)) {}

        [[nodiscard]] bool scheduled() const noexcept {
            return is_scheduled(status_);
        }

        [[nodiscard]] TaskSubmitStatus status() const noexcept {
            return status_;
        }

        [[nodiscard]] bool future_available() const noexcept {
            return scheduled() && !future_taken_;
        }

        explicit operator bool() const noexcept {
            return scheduled();
        }

        [[nodiscard]] bool try_take_future(std::future<Result>& out_future) && noexcept {
            if (!scheduled() || future_taken_) {
                return false;
            }

            out_future = std::move(future_);
            future_taken_ = true;
            return true;
        }

    private:
        TaskSubmitStatus status_ = TaskSubmitStatus::pool_unavailable;
        bool future_taken_ = false;
        std::future<Result> future_{};
    };

    explicit ThreadPool(
        std::size_t worker_count = 0,
        std::size_t local_queue_capacity_pow2 = constants::kDefaultLocalQueueCapacity,
        std::size_t remote_queue_capacity_pow2 = constants::kDefaultRemoteQueueCapacity
    );
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    [[nodiscard]] std::size_t size() const noexcept;

    template <class Fn>
    [[nodiscard]]
    TaskSubmitStatus schedule(Fn&& fn) {
        static_assert(
            std::is_nothrow_invocable_v<std::decay_t<Fn>&>,
            "ThreadPool::schedule requires a noexcept callable");
        using TaskType = DetachedTask<std::decay_t<Fn>>;
        TaskSubmitStatus status = TaskSubmitStatus::scheduled;
        TaskType* task = create_task_<TaskType>(status, std::forward<Fn>(fn));
        if (task == nullptr) {
            return status;
        }

        return enqueue_owned_task_(task);
    }

    template <class Fn>
    [[nodiscard]]
    auto submit(Fn&& fn) -> SubmitResult<std::invoke_result_t<std::decay_t<Fn>&>> {
        using Result = std::invoke_result_t<std::decay_t<Fn>&>;
        static_assert(
            std::is_nothrow_invocable_r_v<Result, std::decay_t<Fn>&>,
            "ThreadPool::submit requires a noexcept callable");
        using TaskType = PromiseTask<std::decay_t<Fn>, Result>;

        std::promise<Result> promise;
        auto future = promise.get_future();

        TaskSubmitStatus status = TaskSubmitStatus::scheduled;
        TaskType* task = create_task_<TaskType>(status, std::forward<Fn>(fn), std::move(promise));
        if (task == nullptr) {
            log_submit_failure_(status);
            return SubmitResult<Result>{status, {}};
        }

        status = enqueue_task_(task);
        if (!is_scheduled(status)) {
            log_submit_failure_(status);
            task->destroy(*this);
            return SubmitResult<Result>{status, {}};
        }

        return SubmitResult<Result>{status, std::move(future)};
    }

    template <class Index, class Fn>
    void parallel_for(Index begin, Index end, Index grain_size, Fn&& fn) {
        static_assert(std::is_integral_v<Index>, "parallel_for requires an integral index type");
        static_assert(
            std::is_nothrow_invocable_v<std::decay_t<Fn>&, Index, Index>,
            "ThreadPool::parallel_for requires a noexcept callable");

        if (end <= begin) {
            return;
        }

        if (grain_size <= 0) {
            grain_size = 1;
        }

        const std::size_t worker_count = size() == 0 ? 1 : size();
        const std::size_t chunk_count = static_cast<std::size_t>((end - begin + grain_size - 1) / grain_size);
        const std::size_t task_count = worker_count < chunk_count ? worker_count : chunk_count;
        std::decay_t<Fn> body(std::forward<Fn>(fn));
        std::atomic<Index> next(begin);
        std::atomic<std::size_t> pending(task_count - 1);
        std::atomic<std::size_t> wake(0);

        auto consume = [&body, &next, end, grain_size]() noexcept {
            for (;;) {
                const Index chunk_begin = next.fetch_add(grain_size, std::memory_order_relaxed);
                if (chunk_begin >= end) {
                    return;
                }

                const Index chunk_end = chunk_begin + grain_size < end ? chunk_begin + grain_size : end;
                body(chunk_begin, chunk_end);
            }
        };

        if (task_count <= 1) {
            consume();
            return;
        }

        for (std::size_t index = 1; index < task_count; ++index) {
            if (!is_scheduled(schedule([&consume, &pending, &wake]() noexcept {
                    consume();
                    pending.fetch_sub(1, std::memory_order_acq_rel);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
                    wake.fetch_add(1, std::memory_order_release);
                    wake.notify_one();
#endif
                }))) {
                pending.fetch_sub(1, std::memory_order_acq_rel);
            }
        }

        consume();

        while (pending.load(std::memory_order_acquire) != 0) {
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
            const std::size_t ticket = wake.load(std::memory_order_acquire);
            if (pending.load(std::memory_order_acquire) == 0) {
                break;
            }
            wake.wait(ticket, std::memory_order_relaxed);
#else
            std::this_thread::yield();
#endif
        }
    }

    void wait_idle();
    void shutdown();

private:
    struct TaskBase {
        using DestroyFn = void (*)(TaskBase*, ThreadPool&) noexcept;

        explicit TaskBase(DestroyFn destroy_fn) noexcept : destroy_fn_(destroy_fn) {}
        virtual ~TaskBase() = default;
        virtual void run(ThreadPool& pool) noexcept = 0;

        void destroy(ThreadPool& pool) noexcept {
            destroy_fn_(this, pool);
        }

    private:
        DestroyFn destroy_fn_;
    };

    template <class Fn>
    struct DetachedTask final : TaskBase {
        template <class F>
        explicit DetachedTask(F&& fn_in) noexcept(std::is_nothrow_constructible_v<Fn, F&&>)
            : TaskBase(&DetachedTask::destroy_)
            , fn(std::forward<F>(fn_in)) {}

        void run(ThreadPool& pool) noexcept override {
            (void)pool;
            fn();
        }

        Fn fn;

        static void destroy_(TaskBase* base, ThreadPool& pool) noexcept {
            auto* task = static_cast<DetachedTask*>(base);
            task->~DetachedTask();
            pool.deallocate_task_memory_(task, sizeof(DetachedTask), alignof(DetachedTask));
        }
    };

    template <class Fn, class Result>
    struct PromiseTask final : TaskBase {
        template <class F>
        PromiseTask(F&& fn_in, std::promise<Result>&& promise_in) noexcept(
            std::is_nothrow_constructible_v<Fn, F&&> &&
            std::is_nothrow_move_constructible_v<std::promise<Result>>)
            : TaskBase(&PromiseTask::destroy_)
            , fn(std::forward<F>(fn_in))
            , promise(std::move(promise_in)) {}

        void run(ThreadPool& pool) noexcept override {
            (void)pool;
            if constexpr (std::is_void_v<Result>) {
                fn();
                promise.set_value();
            } else {
                promise.set_value(fn());
            }
        }

        Fn fn;
        std::promise<Result> promise;

        static void destroy_(TaskBase* base, ThreadPool& pool) noexcept {
            auto* task = static_cast<PromiseTask*>(base);
            task->~PromiseTask();
            pool.deallocate_task_memory_(task, sizeof(PromiseTask), alignof(PromiseTask));
        }
    };

    template <class TaskType, class... Args>
    TaskType* create_task_(TaskSubmitStatus& status, Args&&... args) {
        static_assert(
            std::is_nothrow_constructible_v<TaskType, Args...>,
            "ThreadPool tasks must be nothrow constructible");
        void* memory = allocate_task_memory_(sizeof(TaskType), alignof(TaskType));
        if (memory == nullptr) {
            status = TaskSubmitStatus::task_allocation_failed;
            return nullptr;
        }

        status = TaskSubmitStatus::scheduled;
        return ::new (memory) TaskType(std::forward<Args>(args)...);
    }

    TaskSubmitStatus enqueue_owned_task_(TaskBase* task);
    TaskSubmitStatus enqueue_task_(TaskBase* task);
    void* allocate_task_memory_(std::size_t size, std::size_t alignment);
    void deallocate_task_memory_(void* ptr, std::size_t size, std::size_t alignment) noexcept;
    static void log_submit_failure_(TaskSubmitStatus status) noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

using DefaultThreadPool = ThreadPool;

} // namespace fem::threading

#endif // FEM_CORE_THREADING_THREAD_POOL_H