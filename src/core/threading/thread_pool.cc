#include "thread_pool.h"

#include "bounded_mpsc_queue.h"
#include "core/macro.h"
#include "task_pool_allocator.h"
#include "threading_log.h"
#include "threading_constants.h"
#include "work_stealing_deque.h"

#include <pthread.h>
#include <thread>

namespace fem::threading {

namespace {

inline void backoff_relax(std::size_t spins) noexcept {
    if (spins < constants::kSpinPauseThreshold) {
        FEM_CPU_RELAX();
        return;
    }

    std::this_thread::yield();
}

} // namespace

struct ThreadPool::Impl {
    struct Worker;

    explicit Impl(
        ThreadPool* owner_in,
        std::size_t worker_count,
        std::size_t local_capacity,
        std::size_t remote_capacity
    )
        : owner(owner_in)
        , local_queue_capacity(local_capacity)
        , remote_queue_capacity(remote_capacity)
        , task_allocator(constants::kTaskPoolBlockSize, constants::kTaskPoolBlocksPerSlab)
        , workers(new (std::nothrow) Worker*[worker_count]{})
        , worker_count(worker_count) {
        if (workers == nullptr) {
            LOGT_ERROR(LogThreading, "ThreadPool::Impl::Impl(): failed to allocate worker table (count=%llu)",
                static_cast<unsigned long long>(worker_count));
            accepting_tasks.store(false, std::memory_order_release);
            this->worker_count = 0;
            return;
        }

        for (std::size_t index = 0; index < worker_count; ++index) {
            workers[index] = new (std::nothrow) Worker(owner, this, index, local_queue_capacity, remote_queue_capacity);
            if (workers[index] == nullptr) {
                LOGT_ERROR(LogThreading, "ThreadPool::Impl::Impl(): failed to allocate worker=%llu",
                    static_cast<unsigned long long>(index));
                this->worker_count = index;
                accepting_tasks.store(false, std::memory_order_release);
                destroy_workers();
                return;
            }
        }

        for (std::size_t index = 0; index < this->worker_count; ++index) {
            Worker& worker = *workers[index];
            const int create_result = ::pthread_create(&worker.thread, nullptr, &Impl::worker_entry_, &worker);
            if (create_result != 0) {
                LOGT_ERROR(LogThreading, "ThreadPool::Impl::Impl(): failed to start worker=%llu error=%d",
                    static_cast<unsigned long long>(index),
                    create_result);
                this->worker_count = index + 1;
                worker.thread_started = false;
                accepting_tasks.store(false, std::memory_order_release);
                signal_epoch.fetch_add(1, std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
                signal_epoch.notify_all();
#endif
                join_workers();
                destroy_workers();
                this->worker_count = 0;
                return;
            }
            worker.thread_started = true;
        }
    }

    ~Impl() {
        join_workers();
        destroy_workers();
    }

    struct Worker final {
        Worker(ThreadPool* owner_in, Impl* impl_in, std::size_t index_in, std::size_t local_capacity, std::size_t remote_capacity) noexcept
            : owner(owner_in)
            , impl(impl_in)
            , index(index_in)
            , local_queue(local_capacity)
            , remote_queue(remote_capacity) {}

        ThreadPool* owner = nullptr;
        Impl* impl = nullptr;
        std::size_t index = 0;
        TaskPoolAllocator::FreeNode* task_cache = nullptr;
        std::size_t task_cache_count = 0;
        WorkStealingDeque local_queue;
        BoundedMPSCQueue remote_queue;
        pthread_t thread{};
        bool thread_started = false;
    };

    static void* worker_entry_(void* argument) noexcept {
        auto* worker = static_cast<Worker*>(argument);
        worker->impl->worker_loop(*worker);
        return nullptr;
    }

    void destroy_workers() noexcept {
        if (workers == nullptr) {
            return;
        }

        for (std::size_t index = 0; index < worker_count; ++index) {
            delete workers[index];
            workers[index] = nullptr;
        }
        workers.reset();
    }

    ThreadPool* owner = nullptr;
    std::size_t local_queue_capacity = 0;
    std::size_t remote_queue_capacity = 0;
    TaskPoolAllocator task_allocator;
    std::unique_ptr<Worker*[]> workers;
    std::size_t worker_count = 0;
    alignas(constants::kCacheLineSize) std::atomic<bool> accepting_tasks{true};
    alignas(constants::kCacheLineSize) std::atomic<std::size_t> pending_tasks{0};
    alignas(constants::kCacheLineSize) std::atomic<std::size_t> next_worker{0};
    alignas(constants::kCacheLineSize) std::atomic<std::uint64_t> signal_epoch{0};
    inline static thread_local Worker* tls_worker = nullptr;

    void refill_task_cache(Worker& worker) {
        if (worker.task_cache != nullptr) {
            return;
        }

        worker.task_cache = task_allocator.acquire_batch(constants::kTaskPoolLocalCacheRefillCount);
        worker.task_cache_count = 0;
        for (auto* node = worker.task_cache; node != nullptr; node = node->next) {
            ++worker.task_cache_count;
        }
    }

    void flush_task_cache(Worker& worker) noexcept {
        if (worker.task_cache == nullptr) {
            return;
        }

        task_allocator.release_batch(worker.task_cache);
        worker.task_cache = nullptr;
        worker.task_cache_count = 0;
    }

    void worker_loop(Worker& self) {
        tls_worker = &self;

        for (;;) {
            if (ThreadPool::TaskBase* task = try_take_task(self)) {
                task->run(*owner);
                task->destroy(*owner);
                finish_task();
                continue;
            }

            if (!accepting_tasks.load(std::memory_order_acquire) &&
                pending_tasks.load(std::memory_order_acquire) == 0) {
                break;
            }

            const std::uint64_t epoch = signal_epoch.load(std::memory_order_acquire);
            if (!accepting_tasks.load(std::memory_order_acquire) &&
                pending_tasks.load(std::memory_order_acquire) == 0) {
                break;
            }

#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
            signal_epoch.wait(epoch, std::memory_order_relaxed);
#else
            backoff_relax(constants::kIdleBackoffSpins);
#endif
        }

        flush_task_cache(self);

        tls_worker = nullptr;
    }

    ThreadPool::TaskBase* try_take_task(Worker& self) {
        if (void* local = self.local_queue.pop_bottom()) {
            return static_cast<ThreadPool::TaskBase*>(local);
        }

        void* remote = nullptr;
        if (self.remote_queue.try_pop(remote)) {
            return static_cast<ThreadPool::TaskBase*>(remote);
        }

        for (std::size_t offset = 1; offset < worker_count; ++offset) {
            Worker& victim = *workers[(self.index + offset) % worker_count];
            if (void* stolen = victim.local_queue.steal()) {
                return static_cast<ThreadPool::TaskBase*>(stolen);
            }
        }

        return nullptr;
    }

    void finish_task() {
        pending_tasks.fetch_sub(1, std::memory_order_acq_rel);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
        pending_tasks.notify_all();
#endif

        if (pending_tasks.load(std::memory_order_acquire) == 0) {
            signal_epoch.fetch_add(1, std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
            signal_epoch.notify_all();
#endif
        }
    }

    void signal_workers() {
        signal_epoch.fetch_add(1, std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
        signal_epoch.notify_one();
#endif
    }

    void join_workers() {
        if (workers == nullptr) {
            return;
        }

        for (std::size_t index = 0; index < worker_count; ++index) {
            Worker* worker = workers[index];
            if (worker != nullptr && worker->thread_started) {
                ::pthread_join(worker->thread, nullptr);
                worker->thread_started = false;
            }
        }
    }
};

ThreadPool::ThreadPool(
    std::size_t worker_count,
    std::size_t local_queue_capacity_pow2,
    std::size_t remote_queue_capacity_pow2) {
    if (worker_count == 0) {
        worker_count = std::thread::hardware_concurrency();
        if (worker_count == 0) {
            worker_count = 1;
        }
    }

    impl_.reset(new (std::nothrow) Impl(
        this,
        worker_count,
        local_queue_capacity_pow2,
        remote_queue_capacity_pow2));
    if (impl_ == nullptr) {
        LOGT_ERROR(LogThreading, "ThreadPool::ThreadPool(): failed to allocate implementation");
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

std::size_t ThreadPool::size() const noexcept {
    return impl_ == nullptr ? 0 : impl_->worker_count;
}

ThreadPool::TaskSubmitStatus ThreadPool::enqueue_owned_task_(TaskBase* task) {
    if (task == nullptr) {
        return TaskSubmitStatus::pool_unavailable;
    }

    const TaskSubmitStatus status = enqueue_task_(task);
    if (is_scheduled(status)) {
        return status;
    }

    task->destroy(*this);
    return status;
}

ThreadPool::TaskSubmitStatus ThreadPool::enqueue_task_(TaskBase* task) {
    if (task == nullptr) {
        return TaskSubmitStatus::pool_unavailable;
    }

    if (impl_ == nullptr) {
        return TaskSubmitStatus::pool_unavailable;
    }

    if (!impl_->accepting_tasks.load(std::memory_order_acquire)) {
        return TaskSubmitStatus::pool_stopped;
    }

    impl_->pending_tasks.fetch_add(1, std::memory_order_release);

    Impl::Worker* local = Impl::tls_worker;
    if (local != nullptr && local->owner == this) {
        if (local->local_queue.push_bottom(task) || local->remote_queue.try_push(task)) {
            impl_->signal_workers();
            return TaskSubmitStatus::scheduled;
        }

        task->run(*this);
        impl_->finish_task();
        task->destroy(*this);
        return TaskSubmitStatus::scheduled;
    }

    std::size_t start = impl_->next_worker.fetch_add(1, std::memory_order_relaxed);
    std::size_t spins = 0;
    while (impl_->accepting_tasks.load(std::memory_order_acquire)) {
        for (std::size_t offset = 0; offset < impl_->worker_count; ++offset) {
            Impl::Worker& worker = *impl_->workers[(start + offset) % impl_->worker_count];
            if (worker.remote_queue.try_push(task)) {
                impl_->signal_workers();
                return TaskSubmitStatus::scheduled;
            }
        }

        backoff_relax(spins++);
        start += 1;
    }

    impl_->finish_task();
    return TaskSubmitStatus::pool_stopped;
}

void ThreadPool::wait_idle() {
    if (impl_ == nullptr) {
        return;
    }

    std::size_t pending = impl_->pending_tasks.load(std::memory_order_acquire);
    while (pending != 0) {
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
        impl_->pending_tasks.wait(pending, std::memory_order_relaxed);
#else
        backoff_relax(constants::kIdleBackoffSpins);
#endif
        pending = impl_->pending_tasks.load(std::memory_order_acquire);
    }
}

void* ThreadPool::allocate_task_memory_(std::size_t size, std::size_t alignment) {
    if (impl_ == nullptr) {
        return nullptr;
    }

    Impl::Worker* local = Impl::tls_worker;
    if (local != nullptr && local->owner == this && impl_->task_allocator.can_pool(size, alignment)) {
        if (local->task_cache == nullptr) {
            impl_->refill_task_cache(*local);
        }

        if (local->task_cache != nullptr) {
            auto* node = local->task_cache;
            local->task_cache = node->next;
            node->next = nullptr;
            if (local->task_cache_count > 0) {
                --local->task_cache_count;
            }
            return node;
        }
    }

    return impl_->task_allocator.allocate(size, alignment);
}

void ThreadPool::deallocate_task_memory_(void* ptr, std::size_t size, std::size_t alignment) noexcept {
    if (impl_ == nullptr) {
        return;
    }

    Impl::Worker* local = Impl::tls_worker;
    if (local != nullptr && local->owner == this && impl_->task_allocator.can_pool(size, alignment)) {
        auto* node = static_cast<TaskPoolAllocator::FreeNode*>(ptr);
        node->next = local->task_cache;
        local->task_cache = node;
        ++local->task_cache_count;

        if (local->task_cache_count >= constants::kTaskPoolLocalCacheHighWatermark) {
            impl_->task_allocator.release_batch(local->task_cache);
            local->task_cache = nullptr;
            local->task_cache_count = 0;
        }
        return;
    }

    impl_->task_allocator.deallocate(ptr, size, alignment);
}

void ThreadPool::log_submit_failure_(TaskSubmitStatus status) noexcept {
    const char* reason = "unknown failure";
    bool expected_rejection = false;
    switch (status) {
    case TaskSubmitStatus::scheduled:
        reason = "unexpected success";
        break;
    case TaskSubmitStatus::task_allocation_failed:
        reason = "task allocation failed";
        break;
    case TaskSubmitStatus::pool_unavailable:
        reason = "pool unavailable";
        break;
    case TaskSubmitStatus::pool_stopped:
        reason = "pool stopped; task rejected";
        expected_rejection = true;
        break;
    }

    if (expected_rejection) {
        LOGT_WARN(LogThreading, "ThreadPool::submit(): %s", reason);
        return;
    }

    LOGT_ERROR(LogThreading, "ThreadPool::submit(): %s", reason);
}

void ThreadPool::shutdown() {
    if (impl_ == nullptr) {
        return;
    }

    bool expected = true;
    if (!impl_->accepting_tasks.compare_exchange_strong(
            expected,
            false,
            std::memory_order_acq_rel,
            std::memory_order_relaxed)) {
        impl_->join_workers();
        return;
    }

    impl_->signal_epoch.fetch_add(1, std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
    impl_->signal_epoch.notify_all();
#endif

    impl_->join_workers();
}

} // namespace fem::threading