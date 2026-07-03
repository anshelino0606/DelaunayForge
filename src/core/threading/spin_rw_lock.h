#ifndef FEM_CORE_THREADING_SPIN_RW_LOCK_H
#define FEM_CORE_THREADING_SPIN_RW_LOCK_H

#include <atomic>
#include <cstdint>

namespace fem::threading {

class SpinRWLock final {
public:
    SpinRWLock() = default;

    void lock_shared() noexcept;
    [[nodiscard]] bool try_lock_shared() noexcept;
    void unlock_shared() noexcept;

    void lock() noexcept;
    [[nodiscard]] bool try_lock() noexcept;
    void unlock() noexcept;

private:
    static constexpr uint32_t kWriterHeld = 1u << 31;
    static constexpr uint32_t kWriterPending = 1u << 30;
    static constexpr uint32_t kWriterMask = kWriterHeld | kWriterPending;
    static constexpr uint32_t kReaderMask = ~(kWriterMask);

    std::atomic<uint32_t> state_{0};
};

class SharedSpinLockGuard final {
public:
    explicit SharedSpinLockGuard(SpinRWLock& lock) noexcept;
    ~SharedSpinLockGuard();

    SharedSpinLockGuard(const SharedSpinLockGuard&) = delete;
    SharedSpinLockGuard& operator=(const SharedSpinLockGuard&) = delete;

private:
    SpinRWLock& lock_;
};

class UniqueSpinLockGuard final {
public:
    explicit UniqueSpinLockGuard(SpinRWLock& lock) noexcept;
    ~UniqueSpinLockGuard();

    UniqueSpinLockGuard(const UniqueSpinLockGuard&) = delete;
    UniqueSpinLockGuard& operator=(const UniqueSpinLockGuard&) = delete;

private:
    SpinRWLock& lock_;
};

} // namespace fem::threading

#endif // FEM_CORE_THREADING_SPIN_RW_LOCK_H