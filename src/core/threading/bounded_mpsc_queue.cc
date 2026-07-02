#include "bounded_mpsc_queue.h"

#include "threading_constants.h"

#include <atomic>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <vector>

namespace fem::threading {

namespace {

bool is_power_of_two(std::size_t value) noexcept {
    return value != 0 && (value & (value - 1)) == 0;
}

} // namespace

struct BoundedMPSCQueue::Impl {
    struct alignas(constants::kCacheLineSize) Cell {
        std::atomic<std::size_t> seq{0};
        void* value = nullptr;
    };

    explicit Impl(std::size_t capacity_pow2_in)
        : capacity_pow2(capacity_pow2_in)
        , mask(capacity_pow2_in - 1)
        , cells(capacity_pow2_in) {
        for (std::size_t index = 0; index < capacity_pow2; ++index) {
            cells[index].seq.store(index, std::memory_order_relaxed);
        }
    }

    const std::size_t capacity_pow2;
    const std::size_t mask;
    alignas(constants::kCacheLineSize) std::atomic<std::size_t> head{0};
    alignas(constants::kCacheLineSize) std::atomic<std::size_t> tail{0};
    std::vector<Cell> cells;
};

BoundedMPSCQueue::BoundedMPSCQueue(std::size_t capacity_pow2) {
    if (!is_power_of_two(capacity_pow2)) {
        throw std::invalid_argument("BoundedMPSCQueue capacity must be a power of two");
    }

    impl_ = std::make_unique<Impl>(capacity_pow2);
}

BoundedMPSCQueue::~BoundedMPSCQueue() = default;

bool BoundedMPSCQueue::try_push(void* value) noexcept {
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