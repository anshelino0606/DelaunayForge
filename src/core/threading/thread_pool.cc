#include "thread_pool.h"

#include "bounded_mpsc_queue.h"
#include "work_stealing_deque.h"

#include <thread>
#include <vector>

namespace fem::threading {

namespace {

constexpr std::size_t kThreadingCacheLine = 64;

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
    if (spins < 32) {
        cpu_relax();
        return;
    }

    std::this_thread::yield();
}

} // namespace

struct ThreadPool::Impl {
    struct Worker;

    explicit Impl(ThreadPool* owner_in, std::size_t worker_count, std::size_t local_capacity, std::size_t remote_capacity)
        : owner(owner_in)
        , local_queue_capacity(local_capacity)
        , remote_queue_capacity(remote_capacity) {
        workers.reserve(worker_count);
        for (std::size_t index = 0; index < worker_count; ++index) {
            workers.push_back(std::make_unique<Worker>(owner, this, index, local_queue_capacity, remote_queue_capacity));
        }

        for (auto& worker : workers) {
            worker->thread = std::thread([this, worker = worker.get()] {
                worker_loop(*worker);
            });
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
        WorkStealingDeque local_queue;
        BoundedMPSCQueue remote_queue;
        std::thread thread;
    };

    ThreadPool* owner = nullptr;
    std::size_t local_queue_capacity = 0;
    std::size_t remote_queue_capacity = 0;
    std::vector<std::unique_ptr<Worker>> workers;
    alignas(kThreadingCacheLine) std::atomic<bool> accepting_tasks{true};
    alignas(kThreadingCacheLine) std::atomic<std::size_t> pending_tasks{0};
    alignas(kThreadingCacheLine) std::atomic<std::size_t> next_worker{0};
    alignas(kThreadingCacheLine) std::atomic<std::uint64_t> signal_epoch{0};
    inline static thread_local Worker* tls_worker = nullptr;

    void worker_loop(Worker& self) {
        tls_worker = &self;

        for (;;) {
            if (ThreadPool::TaskBase* task = try_take_task(self)) {
                task->run();
                delete task;
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
            backoff_relax(64);
#endif
        }

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

ThreadPool::ThreadPool(std::size_t worker_count, std::size_t local_queue_capacity_pow2, std::size_t remote_queue_capacity_pow2) {
    if (worker_count == 0) {
        worker_count = std::thread::hardware_concurrency();
        if (worker_count == 0) {
            worker_count = 1;
        }
    }

    impl_ = std::make_unique<Impl>(this, worker_count, local_queue_capacity_pow2, remote_queue_capacity_pow2);
}

ThreadPool::ThreadPool(std::size_t worker_count)
    : ThreadPool(worker_count, 1024, 1024) {}

ThreadPool::~ThreadPool() {
    shutdown();
}

std::size_t ThreadPool::size() const noexcept {
    return impl_ == nullptr ? 0 : impl_->workers.size();
}

bool ThreadPool::enqueue_owned_task_(std::unique_ptr<TaskBase> task) {
    TaskBase* raw = task.release();
    if (enqueue_task_(raw)) {
        return true;
    }

    delete raw;
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

        task->run();
        impl_->finish_task();
        delete task;
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
        backoff_relax(64);
#endif
        pending = impl_->pending_tasks.load(std::memory_order_acquire);
    }
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