#ifndef FEM_CORE_THREADING_TASK_POOL_ALLOCATOR_H
#define FEM_CORE_THREADING_TASK_POOL_ALLOCATOR_H

#include <atomic>
#include <cstddef>
#include <vector>

namespace fem::threading {

class TaskPoolAllocator final {
public:
    TaskPoolAllocator(
        std::size_t block_size = 0,
        std::size_t blocks_per_slab = 0
    );
    ~TaskPoolAllocator();

    TaskPoolAllocator(const TaskPoolAllocator&) = delete;
    TaskPoolAllocator& operator=(const TaskPoolAllocator&) = delete;

    [[nodiscard]] void* allocate(std::size_t size, std::size_t alignment);
    void deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept;

private:
    struct FreeNode {
        FreeNode* next;
    };

    [[nodiscard]] bool can_pool_(std::size_t size, std::size_t alignment) const noexcept;
    void allocate_slab_();
    void lock_() noexcept;
    void unlock_() noexcept;

    std::size_t block_size_ = 0;
    std::size_t block_stride_ = 0;
    std::size_t blocks_per_slab_ = 0;
    FreeNode* free_list_ = nullptr;
    std::vector<void*> slabs_;
    std::atomic_flag lock_flag_ = ATOMIC_FLAG_INIT;
};

} // namespace fem::threading

#endif // FEM_CORE_THREADING_TASK_POOL_ALLOCATOR_H