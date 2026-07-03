#include "bounded_mpsc_queue.h"

#include "threading_log.h"
#include "threading_constants.h"

#include <bit>
#include <atomic>
#include <cstdint>
#include <new>

namespace fem::threading {

namespace {

bool is_power_of_two(std::size_t value) noexcept {
    return value != 0 && (value & (value - 1)) == 0;
}

std::size_t sanitize_capacity_pow2(std::size_t requested_capacity) noexcept {
    if (requested_capacity == 0) {
        LOGT_WARN(LogThreading,
            "BoundedMPSCQueue::BoundedMPSCQueue(): capacity=0, using default=%llu",
            static_cast<unsigned long long>(constants::kDefaultRemoteQueueCapacity));
        return constants::kDefaultRemoteQueueCapacity;
    }

    if (is_power_of_two(requested_capacity)) {
        return requested_capacity;
    }

    const std::size_t adjusted_capacity = std::bit_ceil(requested_capacity);
    LOGT_WARN(LogThreading,
        "BoundedMPSCQueue::BoundedMPSCQueue(): capacity=%llu is not a power of two, using %llu",
        static_cast<unsigned long long>(requested_capacity),
        static_cast<unsigned long long>(adjusted_capacity));
    return adjusted_capacity;
}

} // namespace

struct BoundedMPSCQueue::Impl {
    struct alignas(constants::kCacheLineSize) Cell {
        std::atomic<std::size_t> seq{0};
        void* value = nullptr;
    };

    explicit Impl(std::size_t capacity_pow2_in) noexcept
        : capacity_pow2(capacity_pow2_in)
        , mask(capacity_pow2_in - 1)
        , cells(new (std::nothrow) Cell[capacity_pow2_in]) {
        if (cells == nullptr) {
            return;
        }

        for (std::size_t index = 0; index < capacity_pow2; ++index) {
            cells[index].seq.store(index, std::memory_order_relaxed);
        }
    }

    [[nodiscard]] bool ok() const noexcept {
        return cells != nullptr;
    }

    const std::size_t capacity_pow2;
    const std::size_t mask;
    alignas(constants::kCacheLineSize) std::atomic<std::size_t> head{0};
    alignas(constants::kCacheLineSize) std::atomic<std::size_t> tail{0};
    std::unique_ptr<Cell[]> cells;
};

BoundedMPSCQueue::BoundedMPSCQueue(std::size_t capacity_pow2) noexcept {
    const std::size_t adjusted_capacity = sanitize_capacity_pow2(capacity_pow2);
    impl_.reset(new (std::nothrow) Impl(adjusted_capacity));
    if (impl_ == nullptr || !impl_->ok()) {
        impl_.reset();
        LOGT_ERROR(LogThreading,
            "BoundedMPSCQueue::BoundedMPSCQueue(): failed to allocate queue state (capacity=%llu)",
            static_cast<unsigned long long>(adjusted_capacity));
    }
}

BoundedMPSCQueue::~BoundedMPSCQueue() = default;

bool BoundedMPSCQueue::try_push(void* value) noexcept {
    if (impl_ == nullptr) {
        return false;
    }

    std::size_t pos = impl_->head.load(std::memory_order_relaxed);

    for (;;) {
        Impl::Cell& cell = impl_->cells[pos & impl_->mask];
        const std::size_t seq = cell.seq.load(std::memory_order_acquire);
        const auto diff = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos);

        if (diff == 0) {
            if (impl_->head.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                cell.value = value;
                cell.seq.store(pos + 1, std::memory_order_release);
                return true;
            }
        } else if (diff < 0) {
            return false;
        } else {
            pos = impl_->head.load(std::memory_order_relaxed);
        }
    }
}

bool BoundedMPSCQueue::try_pop(void*& value) noexcept {
    if (impl_ == nullptr) {
        return false;
    }

    const std::size_t pos = impl_->tail.load(std::memory_order_relaxed);
    Impl::Cell& cell = impl_->cells[pos & impl_->mask];

    if (cell.seq.load(std::memory_order_acquire) != pos + 1) {
        return false;
    }

    value = cell.value;
    cell.value = nullptr;
    cell.seq.store(pos + impl_->capacity_pow2, std::memory_order_release);
    impl_->tail.store(pos + 1, std::memory_order_relaxed);
    return true;
}

} // namespace fem::threading