#ifndef MPSC_QUEUE
#define MPSC_QUEUE

#include <atomic>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace logger::mpsc {

constexpr std::size_t kCacheLine = 64;

template <class T, std::size_t CapacityPow2>
class BoundedMPSC {
    static_assert((CapacityPow2 & (CapacityPow2 - 1)) == 0, "power of two only capacity");

    struct alignas(kCacheLine) Cell {
        std::atomic<std::size_t> seq;
        std::aligned_storage_t<sizeof(T), alignof(T)> storage;
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
            const intptr_t dif = (intptr_t)seq - (intptr_t)pos;

            if (dif == 0) {
                if (head_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    ::new (&c.storage) T(std::forward<Args>(args)...);
                    c.seq.store(pos + 1, std::memory_order_release);
                    return true;
                }
                // CAS failed
            } else if (dif < 0) {
                return false;
            } else {
                pos = head_.load(std::memory_order_relaxed);
            }
        }
    }

    bool try_push(const T& v) { return try_emplace(v); }
    bool try_push(T&& v) { return try_emplace(std::move(v)); }

    // Single-consumer only. FOR NOW.
    bool try_pop(T& out) {
        const std::size_t pos = tail_.load(std::memory_order_relaxed);
        Cell& c = cells_[pos & (CapacityPow2 - 1)];

        if (c.seq.load(std::memory_order_acquire) != pos + 1) {
            return false;
        }

        T* ptr = std::launder(reinterpret_cast<T*>(&c.storage));
        out = std::move(*ptr);
        ptr->~T();

        c.seq.store(pos + CapacityPow2, std::memory_order_release); // mark free
        tail_.store(pos + 1, std::memory_order_relaxed);
        return true;
    }

private:
    alignas(kCacheLine) std::atomic<std::size_t> head_;
    alignas(kCacheLine) std::atomic<std::size_t> tail_;
    alignas(kCacheLine) Cell cells_[CapacityPow2];
};

} // namespace logger::mpsc

#endif