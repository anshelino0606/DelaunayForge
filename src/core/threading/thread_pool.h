#ifndef FEM_CORE_THREADING_THREAD_POOL_H
#define FEM_CORE_THREADING_THREAD_POOL_H

#include "threading_constants.h"

#include <atomic>
#include <cstddef>
#include <exception>
#include <future>
#include <memory>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

namespace fem::threading {

class ThreadPool final {
public:
    using UnhandledExceptionHandler = void (*)(std::exception_ptr) noexcept;

    explicit ThreadPool(
        std::size_t worker_count = 0,
        std::size_t local_queue_capacity_pow2 = constants::kDefaultLocalQueueCapacity,
        std::size_t remote_queue_capacity_pow2 = constants::kDefaultRemoteQueueCapacity,
        UnhandledExceptionHandler unhandled_exception_handler = nullptr
    );
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    [[nodiscard]] std::size_t size() const noexcept;

    template <class Fn>
    bool schedule(Fn&& fn) {
        using TaskType = DetachedTask<std::decay_t<Fn>>;
        return enqueue_owned_task_(create_task_<TaskType>(std::forward<Fn>(fn)));
    }

    template <class Fn>
    auto submit(Fn&& fn) -> std::future<std::invoke_result_t<std::decay_t<Fn>&>> {
        using Result = std::invoke_result_t<std::decay_t<Fn>&>;
        using TaskType = PromiseTask<std::decay_t<Fn>, Result>;

        std::promise<Result> promise;
        auto future = promise.get_future();

        if (!enqueue_owned_task_(create_task_<TaskType>(std::forward<Fn>(fn), std::move(promise)))) {
            throw std::runtime_error("ThreadPool is shutting down");
        }

        return future;
    }

    template <class Index, class Fn>
    void parallel_for(Index begin, Index end, Index grain_size, Fn&& fn) {
        static_assert(std::is_integral_v<Index>, "parallel_for requires an integral index type");

        if (end <= begin) {
            return;
        }

        if (grain_size <= 0) {
            grain_size = 1;
        }

        const std::size_t worker_count = size() == 0 ? 1 : size();
        const std::size_t chunk_count = static_cast<std::size_t>((end - begin + grain_size - 1) / grain_size);
        const std::size_t task_count = worker_count < chunk_count ? worker_count : chunk_count;
        auto next = std::make_shared<std::atomic<Index>>(begin);
        auto body = std::make_shared<std::decay_t<Fn>>(std::forward<Fn>(fn));

        auto consume = [next, body, begin, end, grain_size]() mutable {
            (void)begin;
            for (;;) {
                const Index chunk_begin = next->fetch_add(grain_size, std::memory_order_relaxed);
                if (chunk_begin >= end) {
                    return;
                }

                const Index chunk_end = chunk_begin + grain_size < end ? chunk_begin + grain_size : end;
                (*body)(chunk_begin, chunk_end);
            }
        };

        if (task_count <= 1) {
            consume();
            return;
        }

        auto pending = std::make_shared<std::atomic<std::size_t>>(task_count - 1);
        auto wake = std::make_shared<std::atomic<std::size_t>>(0);

        for (std::size_t index = 1; index < task_count; ++index) {
            if (!schedule([consume, pending, wake]() mutable {
                    consume();
                    pending->fetch_sub(1, std::memory_order_acq_rel);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
                    wake->fetch_add(1, std::memory_order_release);
                    wake->notify_one();
#endif
                })) {
                pending->fetch_sub(1, std::memory_order_acq_rel);
            }
        }

        consume();

        while (pending->load(std::memory_order_acquire) != 0) {
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
            const std::size_t ticket = wake->load(std::memory_order_acquire);
            if (pending->load(std::memory_order_acquire) == 0) {
                break;
            }
            wake->wait(ticket, std::memory_order_relaxed);
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
        explicit DetachedTask(F&& fn_in)
            : TaskBase(&DetachedTask::destroy_)
            , fn(std::forward<F>(fn_in)) {}

        void run(ThreadPool& pool) noexcept override {
            try {
                fn();
            } catch (...) {
                pool.handle_detached_task_exception_(std::current_exception());
            }
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
        PromiseTask(F&& fn_in, std::promise<Result>&& promise_in)
            : TaskBase(&PromiseTask::destroy_)
            , fn(std::forward<F>(fn_in))
            , promise(std::move(promise_in)) {}

        void run(ThreadPool& pool) noexcept override {
            (void)pool;
            try {
                if constexpr (std::is_void_v<Result>) {
                    fn();
                    promise.set_value();
                } else {
                    promise.set_value(fn());
                }
            } catch (...) {
                promise.set_exception(std::current_exception());
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
    TaskType* create_task_(Args&&... args) {
        void* memory = allocate_task_memory_(sizeof(TaskType), alignof(TaskType));
        try {
            return ::new (memory) TaskType(std::forward<Args>(args)...);
        } catch (...) {
            deallocate_task_memory_(memory, sizeof(TaskType), alignof(TaskType));
            throw;
        }
    }

    bool enqueue_owned_task_(TaskBase* task);
    bool enqueue_task_(TaskBase* task);
    void* allocate_task_memory_(std::size_t size, std::size_t alignment);
    void deallocate_task_memory_(void* ptr, std::size_t size, std::size_t alignment) noexcept;
    void handle_detached_task_exception_(std::exception_ptr exception) noexcept;
    static void terminate_on_unhandled_exception_(std::exception_ptr exception) noexcept;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

using DefaultThreadPool = ThreadPool;

} // namespace fem::threading

#endif // FEM_CORE_THREADING_THREAD_POOL_H