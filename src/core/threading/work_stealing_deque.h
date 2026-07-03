#ifndef FEM_CORE_THREADING_WORK_STEALING_DEQUE_H
#define FEM_CORE_THREADING_WORK_STEALING_DEQUE_H

#include <cstddef>
#include <memory>

namespace fem::threading {

class WorkStealingDeque final {
public:
    explicit WorkStealingDeque(std::size_t capacity_pow2) noexcept;
    ~WorkStealingDeque();

    WorkStealingDeque(const WorkStealingDeque&) = delete;
    WorkStealingDeque& operator=(const WorkStealingDeque&) = delete;

    [[nodiscard]] bool push_bottom(void* value) noexcept;
    [[nodiscard]] void* pop_bottom() noexcept;
    [[nodiscard]] void* steal() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fem::threading

#endif // FEM_CORE_THREADING_WORK_STEALING_DEQUE_H
