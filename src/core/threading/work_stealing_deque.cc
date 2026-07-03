#include "work_stealing_deque.h"

#include "threading_log.h"
#include "threading_constants.h"

#include <bit>
#include <atomic>
#include <cstdint>

namespace fem::threading {

namespace {

bool is_power_of_two(std::size_t value) noexcept {
    return value != 0 && (value & (value - 1)) == 0;
}

std::size_t sanitize_capacity_pow2(std::size_t requested_capacity) noexcept {
    if (requested_capacity == 0) {
        LOGT_WARN(LogThreading,
            "WorkStealingDeque::WorkStealingDeque(): capacity=0, using default=%llu",
            static_cast<unsigned long long>(constants::kDefaultLocalQueueCapacity));
        return constants::kDefaultLocalQueueCapacity;
    }

    if (is_power_of_two(requested_capacity)) {
        return requested_capacity;
    }

    const std::size_t adjusted_capacity = std::bit_ceil(requested_capacity);
    LOGT_WARN(LogThreading,
        "WorkStealingDeque::WorkStealingDeque(): capacity=%llu is not a power of two, using %llu",
        static_cast<unsigned long long>(requested_capacity),
        static_cast<unsigned long long>(adjusted_capacity));
    return adjusted_capacity;
}

} // namespace

struct WorkStealingDeque::Impl {
    explicit Impl(std::size_t capacity_pow2_in) noexcept
        : capacity_pow2(capacity_pow2_in)
        , mask(capacity_pow2_in - 1)
        , slots(new (std::nothrow) std::atomic<void*>[capacity_pow2_in]) {
        if (slots == nullptr) {
            return;
        }

        for (std::size_t index = 0; index < capacity_pow2; ++index) {
            slots[index].store(nullptr, std::memory_order_relaxed);
        }
    }

    [[nodiscard]] bool ok() const noexcept {
        return slots != nullptr;
    }

    const std::size_t capacity_pow2;
    const std::size_t mask;
    alignas(constants::kCacheLineSize) std::atomic<std::size_t> top{0};
    alignas(constants::kCacheLineSize) std::atomic<std::size_t> bottom{0};
    std::unique_ptr<std::atomic<void*>[]> slots;
};

WorkStealingDeque::WorkStealingDeque(std::size_t capacity_pow2) noexcept {
    const std::size_t adjusted_capacity = sanitize_capacity_pow2(capacity_pow2);
    impl_.reset(new (std::nothrow) Impl(adjusted_capacity));
    if (impl_ == nullptr || !impl_->ok()) {
        impl_.reset();
        LOGT_ERROR(LogThreading,
            "WorkStealingDeque::WorkStealingDeque(): failed to allocate deque state (capacity=%llu)",
            static_cast<unsigned long long>(adjusted_capacity));
    }
}

WorkStealingDeque::~WorkStealingDeque() = default;

bool WorkStealingDeque::push_bottom(void* value) noexcept {
    if (impl_ == nullptr) {
        return false;
    }

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
    if (impl_ == nullptr) {
        return nullptr;
    }

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
    if (impl_ == nullptr) {
        return nullptr;
    }

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
