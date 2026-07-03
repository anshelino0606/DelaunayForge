#ifndef FEM_CORE_THREADING_TASK_POOL_ALLOCATOR_H
#define FEM_CORE_THREADING_TASK_POOL_ALLOCATOR_H

#include <atomic>
#include <cstddef>

namespace fem::threading {

class TaskPoolAllocator final {
public:
    struct FreeNode {
        FreeNode* next;
    };

    TaskPoolAllocator(
        std::size_t block_size = 0,
        std::size_t blocks_per_slab = 0
    );
    ~TaskPoolAllocator();

    TaskPoolAllocator(const TaskPoolAllocator&) = delete;
    TaskPoolAllocator& operator=(const TaskPoolAllocator&) = delete;

    [[nodiscard]] void* allocate(std::size_t size, std::size_t alignment) noexcept;
    void deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept;
    [[nodiscard]] bool can_pool(std::size_t size, std::size_t alignment) const noexcept;
    [[nodiscard]] FreeNode* acquire_batch(std::size_t count) noexcept;
    void release_batch(FreeNode* head) noexcept;

private:
    [[nodiscard]] bool can_pool_(std::size_t size, std::size_t alignment) const noexcept;
    [[nodiscard]] bool allocate_slab_() noexcept;
    void lock_() noexcept;
    void unlock_() noexcept;

    struct SlabNode {
        void* slab = nullptr;
        SlabNode* next = nullptr;
    };

    std::size_t block_size_ = 0;
    std::size_t block_stride_ = 0;
    std::size_t blocks_per_slab_ = 0;
    FreeNode* free_list_ = nullptr;
    SlabNode* slabs_ = nullptr;
    std::atomic_flag lock_flag_ = ATOMIC_FLAG_INIT;
};

} // namespace fem::threading

#endif // FEM_CORE_THREADING_TASK_POOL_ALLOCATOR_H
