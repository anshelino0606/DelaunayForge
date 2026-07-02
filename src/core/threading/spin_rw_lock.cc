#include "spin_rw_lock.h"

#include <thread>

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
    if (spins < 32) {
        cpu_relax();
        return;
    }

    std::this_thread::yield();
}

template <class T>
inline void atomic_notify_all(std::atomic<T>& value) noexcept {
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
    value.notify_all();
#else
    (void)value;
#endif
}

} // namespace

void SpinRWLock::lock_shared() noexcept {
    std::uint32_t state = state_.load(std::memory_order_relaxed);
    std::size_t spins = 0;

    for (;;) {
        if ((state & kWriterMask) == 0) {
            if (state_.compare_exchange_weak(
                    state,
                    state + 1,
                    std::memory_order_acquire,
                    std::memory_order_relaxed)) {
                return;
            }
        } else {
            backoff_relax(spins++);
            state = state_.load(std::memory_order_relaxed);
        }
    }
}

bool SpinRWLock::try_lock_shared() noexcept {
    std::uint32_t state = state_.load(std::memory_order_relaxed);
    while ((state & kWriterMask) == 0) {
        if (state_.compare_exchange_weak(
                state,
                state + 1,
                std::memory_order_acquire,
                std::memory_order_relaxed)) {
            return true;
        }
    }

    return false;
}

void SpinRWLock::unlock_shared() noexcept {
    const std::uint32_t previous = state_.fetch_sub(1, std::memory_order_release);
    if ((previous & kWriterPending) != 0) {
        atomic_notify_all(state_);
    }
}

void SpinRWLock::lock() noexcept {
    std::size_t spins = 0;

    for (;;) {
        std::uint32_t state = state_.load(std::memory_order_relaxed);

        if ((state & kWriterMask) != 0) {
            backoff_relax(spins++);
            continue;
        }

        if (!state_.compare_exchange_weak(
                state,
                state | kWriterPending,
                std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
            backoff_relax(spins++);
            continue;
        }

        spins = 0;
        for (;;) {
            state = state_.load(std::memory_order_acquire);
            if ((state & kReaderMask) == 0) {
                std::uint32_t expected = kWriterPending;
                if (state_.compare_exchange_weak(
                        expected,
                        kWriterHeld,
                        std::memory_order_acq_rel,
                        std::memory_order_relaxed)) {
                    return;
                }
            }

            backoff_relax(spins++);
        }
    }
}

bool SpinRWLock::try_lock() noexcept {
    std::uint32_t expected = 0;
    return state_.compare_exchange_strong(
        expected,
        kWriterHeld,
        std::memory_order_acq_rel,
        std::memory_order_relaxed);
}

void SpinRWLock::unlock() noexcept {
    state_.store(0, std::memory_order_release);
    atomic_notify_all(state_);
}

SharedSpinLockGuard::SharedSpinLockGuard(SpinRWLock& lock) noexcept : lock_(lock) {
    lock_.lock_shared();
}

SharedSpinLockGuard::~SharedSpinLockGuard() {
    lock_.unlock_shared();
}

UniqueSpinLockGuard::UniqueSpinLockGuard(SpinRWLock& lock) noexcept : lock_(lock) {
    lock_.lock();
}

UniqueSpinLockGuard::~UniqueSpinLockGuard() {
    lock_.unlock();
}

} // namespace fem::threading