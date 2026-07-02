#include "thread_pool.h"

#include "bounded_mpsc_queue.h"
#include "task_pool_allocator.h"
#include "threading_constants.h"
#include "work_stealing_deque.h"

#include <thread>
#include <vector>

namespace fem::threading {

namespace {

inline void cpu_relax() noexcept {
#if defined(__clang__) || defined(__GNUC__)
  #if defined(__i386__) || defined(__x86_64__)
    __builtin_ia32_pause();
  #elif defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("yield");
  #else
    std::atomic_signal_fence(std::memory_order_seq_cst);
  #endif
#else
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

inline void backoff_relax(std::size_t spins) noexcept {
    if (spins < constants::kSpinPauseThreshold) {
        cpu_relax();
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
        std::size_t remote_capacity,
        UnhandledExceptionHandler unhandled_exception_handler_in
    )
        : owner(owner_in)
        , local_queue_capacity(local_capacity)
        , remote_queue_capacity(remote_capacity)
        , task_allocator(constants::kTaskPoolBlockSize, constants::kTaskPoolBlocksPerSlab)
        , unhandled_exception_handler(
            unhandled_exception_handler_in != nullptr
                ? unhandled_exception_handler_in
                : &ThreadPool::terminate_on_unhandled_exception_) {
        workers.reserve(worker_count);
        for (std::size_t index = 0; index < worker_count; ++index) {
            workers.push_back(std::make_unique<Worker>(owner, this, index, local_queue_capacity, remote_queue_capacity));
        }

        try {
            for (auto& worker : workers) {
                worker->thread = std::thread([this, worker = worker.get()] {
                    worker_loop(*worker);
                });
            }
        } catch (...) {
            accepting_tasks.store(false, std::memory_order_release);
            signal_epoch.fetch_add(1, std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
            signal_epoch.notify_all();
#endif
            join_workers();
            throw;
        }
    }

    struct Worker final {
        Worker(ThreadPool* owner_in, Impl* impl_in, std::size_t index_in, std::size_t local_capacity, std::size_t remote_capacity)
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
        std::thread thread;
    };

    ThreadPool* owner = nullptr;
    std::size_t local_queue_capacity = 0;
    std::size_t remote_queue_capacity = 0;
    TaskPoolAllocator task_allocator;
    std::vector<std::unique_ptr<Worker>> workers;
    UnhandledExceptionHandler unhandled_exception_handler = nullptr;
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

        const std::size_t worker_count = workers.size();
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
        for (auto& worker : workers) {
            if (worker->thread.joinable()) {
                worker->thread.join();
            }
        }
    }
};

ThreadPool::ThreadPool(
    std::size_t worker_count,
    std::size_t local_queue_capacity_pow2,
    std::size_t remote_queue_capacity_pow2,
    UnhandledExceptionHandler unhandled_exception_handler) {
    if (worker_count == 0) {
        worker_count = std::thread::hardware_concurrency();
        if (worker_count == 0) {
            worker_count = 1;
        }
    }

    impl_ = std::make_unique<Impl>(
        this,
        worker_count,
        local_queue_capacity_pow2,
        remote_queue_capacity_pow2,
        unhandled_exception_handler);
}

ThreadPool::~ThreadPool() {
    shutdown();
}

std::size_t ThreadPool::size() const noexcept {
    return impl_ == nullptr ? 0 : impl_->workers.size();
}

bool ThreadPool::enqueue_owned_task_(TaskBase* task) {
    if (enqueue_task_(task)) {
        return true;
    }

    task->destroy(*this);
    return false;
}

bool ThreadPool::enqueue_task_(TaskBase* task) {
    if (!impl_->accepting_tasks.load(std::memory_order_acquire)) {
        return false;
    }

    impl_->pending_tasks.fetch_add(1, std::memory_order_release);

    Impl::Worker* local = Impl::tls_worker;
    if (local != nullptr && local->owner == this) {
        if (local->local_queue.push_bottom(task) || local->remote_queue.try_push(task)) {
            impl_->signal_workers();
            return true;
        }

        task->run(*this);
        impl_->finish_task();
        task->destroy(*this);
        return true;
    }

    std::size_t start = impl_->next_worker.fetch_add(1, std::memory_order_relaxed);
    std::size_t spins = 0;
    while (impl_->accepting_tasks.load(std::memory_order_acquire)) {
        for (std::size_t offset = 0; offset < impl_->workers.size(); ++offset) {
            Impl::Worker& worker = *impl_->workers[(start + offset) % impl_->workers.size()];
            if (worker.remote_queue.try_push(task)) {
                impl_->signal_workers();
                return true;
            }
        }

        backoff_relax(spins++);
        start += 1;
    }

    impl_->finish_task();
    return false;
}

void ThreadPool::wait_idle() {
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

void ThreadPool::handle_detached_task_exception_(std::exception_ptr exception) noexcept {
    if (impl_ == nullptr || impl_->unhandled_exception_handler == nullptr) {
        terminate_on_unhandled_exception_(exception);
        return;
    }

    impl_->unhandled_exception_handler(exception);
}

void ThreadPool::terminate_on_unhandled_exception_(std::exception_ptr exception) noexcept {
    (void)exception;
    std::terminate();
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