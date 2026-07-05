#ifndef FEM_CORE_THREADING_MPSC_QUEUE_H
#define FEM_CORE_THREADING_MPSC_QUEUE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>

namespace fem::threading {

inline constexpr std::size_t kCacheLine = 64;

template <class T, std::size_t CapacityPow2>
class BoundedMPSC {
    static_assert((CapacityPow2 & (CapacityPow2 - 1)) == 0, "power of two only capacity");

    struct alignas(kCacheLine) Cell {
        std::atomic<std::size_t> seq;
        alignas(T) std::byte storage[sizeof(T)];
    };

public:
    BoundedMPSC() {
        for (std::size_t i = 0; i < CapacityPow2; ++i) {
            cells_[i].seq.store(i, std::memory_order_relaxed);
        }
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

    BoundedMPSC(const BoundedMPSC&) = delete;
    BoundedMPSC& operator=(const BoundedMPSC&) = delete;

    ~BoundedMPSC() {
        T tmp;
        while (try_pop(tmp)) {}
    }

    template <class... Args>
    bool try_emplace(Args&&... args) {
        std::size_t pos = head_.load(std::memory_order_relaxed);
        for (;;) {
            Cell& c = cells_[pos & (CapacityPow2 - 1)];
            const std::size_t seq = c.seq.load(std::memory_order_acquire);
            const intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

            if (dif == 0) {
                if (head_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    ::new (static_cast<void*>(c.storage)) T(std::forward<Args>(args)...);
                    c.seq.store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (dif < 0) {
                return false;
            } else {
                pos = head_.load(std::memory_order_relaxed);
            }
        }
    }

    bool try_push(const T& value) { return try_emplace(value); }
    bool try_push(T&& value) { return try_emplace(std::move(value)); }

    bool try_pop(T& out) {
        const std::size_t pos = tail_.load(std::memory_order_relaxed);
        Cell& c = cells_[pos & (CapacityPow2 - 1)];

        if (c.seq.load(std::memory_order_acquire) != pos + 1) {
            return false;
        }

        T* ptr = std::launder(reinterpret_cast<T*>(c.storage));
        out = std::move(*ptr);
        ptr->~T();

        c.seq.store(pos + CapacityPow2, std::memory_order_release);
        tail_.store(pos + 1, std::memory_order_relaxed);
        return true;
    }

private:
    alignas(kCacheLine) std::atomic<std::size_t> head_;
    alignas(kCacheLine) std::atomic<std::size_t> tail_;
    alignas(kCacheLine) Cell cells_[CapacityPow2];
};

} // namespace fem::threading

#endif // FEM_CORE_THREADING_MPSC_QUEUE_H