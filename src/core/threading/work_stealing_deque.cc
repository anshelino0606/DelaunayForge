#include "work_stealing_deque.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace fem::threading {

namespace {

constexpr std::size_t kThreadingCacheLine = 64;

bool is_power_of_two(std::size_t value) noexcept {
    return value != 0 && (value & (value - 1)) == 0;
}

} // namespace

struct WorkStealingDeque::Impl {
    explicit Impl(std::size_t capacity_pow2_in)
        : capacity_pow2(capacity_pow2_in)
        , mask(capacity_pow2_in - 1)
        , slots(capacity_pow2_in) {
        for (auto& slot : slots) {
            slot.store(nullptr, std::memory_order_relaxed);
        }
    }

    const std::size_t capacity_pow2;
    const std::size_t mask;
    alignas(kThreadingCacheLine) std::atomic<std::size_t> top{0};
    alignas(kThreadingCacheLine) std::atomic<std::size_t> bottom{0};
    std::vector<std::atomic<void*>> slots;
};

WorkStealingDeque::WorkStealingDeque(std::size_t capacity_pow2) {
    if (!is_power_of_two(capacity_pow2)) {
        throw std::invalid_argument("WorkStealingDeque capacity must be a power of two");
    }

    impl_ = std::make_unique<Impl>(capacity_pow2);
}

WorkStealingDeque::~WorkStealingDeque() = default;

bool WorkStealingDeque::push_bottom(void* value) noexcept {
    const std::size_t bottom = impl_->bottom.load(std::memory_order_relaxed);
    const std::size_t top = impl_->top.load(std::memory_order_acquire);

    if (bottom - top >= impl_->capacity_pow2) {
        return false;
    }

    impl_->slots[bottom & impl_->mask].store(value, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    impl_->bottom.store(bottom + 1, std::memory_order_relaxed);
    return true;
}

void* WorkStealingDeque::pop_bottom() noexcept {
    std::size_t bottom = impl_->bottom.load(std::memory_order_relaxed);
    if (bottom == 0) {
        return nullptr;
    }

    bottom -= 1;
    impl_->bottom.store(bottom, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);

    std::size_t top = impl_->top.load(std::memory_order_relaxed);
    if (top > bottom) {
        impl_->bottom.store(bottom + 1, std::memory_order_relaxed);
        return nullptr;
    }

    auto& slot = impl_->slots[bottom & impl_->mask];
    void* value = slot.load(std::memory_order_relaxed);

    if (top == bottom) {
        if (!impl_->top.compare_exchange_strong(
                top,
                top + 1,
                std::memory_order_seq_cst,
                std::memory_order_relaxed)) {
            impl_->bottom.store(bottom + 1, std::memory_order_relaxed);
            return nullptr;
        }

        impl_->bottom.store(bottom + 1, std::memory_order_relaxed);
    }

    slot.store(nullptr, std::memory_order_relaxed);
    return value;
}

void* WorkStealingDeque::steal() noexcept {
    std::size_t top = impl_->top.load(std::memory_order_acquire);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    const std::size_t bottom = impl_->bottom.load(std::memory_order_acquire);

    if (top >= bottom) {
        return nullptr;
    }

    auto& slot = impl_->slots[top & impl_->mask];
    void* value = slot.load(std::memory_order_relaxed);
    if (value == nullptr) {
        return nullptr;
    }

    if (!impl_->top.compare_exchange_strong(
            top,
            top + 1,
            std::memory_order_seq_cst,
            std::memory_order_relaxed)) {
        return nullptr;
    }

    slot.store(nullptr, std::memory_order_relaxed);
    return value;
}

} // namespace fem::threading