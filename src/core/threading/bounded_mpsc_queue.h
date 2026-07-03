#ifndef FEM_CORE_THREADING_BOUNDED_MPSC_QUEUE_H
#define FEM_CORE_THREADING_BOUNDED_MPSC_QUEUE_H

#include <cstddef>
#include <memory>

namespace fem::threading {

class BoundedMPSCQueue final {
public:
    explicit BoundedMPSCQueue(std::size_t capacity_pow2) noexcept;
    ~BoundedMPSCQueue();

    BoundedMPSCQueue(const BoundedMPSCQueue&) = delete;
    BoundedMPSCQueue& operator=(const BoundedMPSCQueue&) = delete;

    [[nodiscard]] bool try_push(void* value) noexcept;
    [[nodiscard]] bool try_pop(void*& value) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fem::threading

#endif // FEM_CORE_THREADING_BOUNDED_MPSC_QUEUE_H