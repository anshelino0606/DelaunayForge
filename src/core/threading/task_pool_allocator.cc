#include "task_pool_allocator.h"

#include "core/macro.h"
#include "threading_log.h"
#include "threading_constants.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <new>
#include <thread>

namespace fem::threading {

namespace {

inline void backoff_relax(std::size_t spins) noexcept {
    if (spins < constants::kSpinPauseThreshold) {
        FEM_CPU_RELAX();
        return;
    }

    std::this_thread::yield();
}

[[nodiscard]] std::size_t align_up(std::size_t value, std::size_t alignment) noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}

} // namespace

TaskPoolAllocator::TaskPoolAllocator(std::size_t block_size, std::size_t blocks_per_slab)
    : block_size_(std::max(block_size, constants::kTaskPoolBlockSize))
    , block_stride_(align_up(std::max(block_size_, sizeof(FreeNode)), alignof(std::max_align_t)))
    , blocks_per_slab_(std::max(blocks_per_slab, constants::kTaskPoolBlocksPerSlab)) {}

TaskPoolAllocator::~TaskPoolAllocator() {
    while (slabs_ != nullptr) {
        SlabNode* node = slabs_;
        slabs_ = node->next;
        ::operator delete(node->slab, std::align_val_t(alignof(std::max_align_t)));
        delete node;
    }
}

void* TaskPoolAllocator::allocate(std::size_t size, std::size_t alignment) noexcept {
    if (!can_pool_(size, alignment)) {
        void* memory = ::operator new(size, std::align_val_t(alignment), std::nothrow);
        if (memory == nullptr) {
            LOGT_ERROR(LogThreading, "TaskPoolAllocator::allocate(): fallback allocation failed (size=%llu alignment=%llu)",
                static_cast<unsigned long long>(size),
                static_cast<unsigned long long>(alignment));
        }
        return memory;
    }

    FreeNode* node = acquire_batch(1);
    if (node == nullptr) {
        LOGT_ERROR(LogThreading, "TaskPoolAllocator::allocate(): pooled allocation failed (size=%llu alignment=%llu)",
            static_cast<unsigned long long>(size),
            static_cast<unsigned long long>(alignment));
    }

    return node;
}

void TaskPoolAllocator::deallocate(void* ptr, std::size_t size, std::size_t alignment) noexcept {
    if (ptr == nullptr) {
        return;
    }

    if (!can_pool_(size, alignment)) {
        ::operator delete(ptr, std::align_val_t(alignment));
        return;
    }

    lock_();
    FreeNode* node = static_cast<FreeNode*>(ptr);
    node->next = free_list_;
    free_list_ = node;
    unlock_();
}

bool TaskPoolAllocator::can_pool(std::size_t size, std::size_t alignment) const noexcept {
    return can_pool_(size, alignment);
}

TaskPoolAllocator::FreeNode* TaskPoolAllocator::acquire_batch(std::size_t count) noexcept {
    if (count == 0) {
        return nullptr;
    }

    lock_();
    while (free_list_ == nullptr) {
        if (!allocate_slab_()) {
            unlock_();
            return nullptr;
        }
    }

    FreeNode* head = free_list_;
    FreeNode* tail = head;
    std::size_t acquired = 1;
    while (acquired < count && tail->next != nullptr) {
        tail = tail->next;
        ++acquired;
    }

    free_list_ = tail->next;
    tail->next = nullptr;
    unlock_();
    return head;
}

void TaskPoolAllocator::release_batch(FreeNode* head) noexcept {
    if (head == nullptr) {
        return;
    }

    lock_();
    FreeNode* tail = head;
    while (tail->next != nullptr) {
        tail = tail->next;
    }
    tail->next = free_list_;
    free_list_ = head;
    unlock_();
}

bool TaskPoolAllocator::can_pool_(std::size_t size, std::size_t alignment) const noexcept {
    return size <= block_size_ && alignment <= alignof(std::max_align_t);
}

bool TaskPoolAllocator::allocate_slab_() noexcept {
    const std::size_t bytes = block_stride_ * blocks_per_slab_;
    void* slab = ::operator new(bytes, std::align_val_t(alignof(std::max_align_t)), std::nothrow);
    if (slab == nullptr) {
        LOGT_ERROR(LogThreading, "TaskPoolAllocator::allocate_slab_(): slab allocation failed (bytes=%llu blocks=%llu)",
            static_cast<unsigned long long>(bytes),
            static_cast<unsigned long long>(blocks_per_slab_));
        return false;
    }

    SlabNode* node = new (std::nothrow) SlabNode{slab, slabs_};
    if (node == nullptr) {
        ::operator delete(slab, std::align_val_t(alignof(std::max_align_t)));
        LOGT_ERROR(LogThreading, "TaskPoolAllocator::allocate_slab_(): slab tracking allocation failed");
        return false;
    }

    slabs_ = node;

    auto* data = static_cast<std::byte*>(slab);
    for (std::size_t index = 0; index < blocks_per_slab_; ++index) {
        FreeNode* node = reinterpret_cast<FreeNode*>(data + index * block_stride_);
        node->next = free_list_;
        free_list_ = node;
    }

    return true;
}

void TaskPoolAllocator::lock_() noexcept {
    std::size_t spins = 0;
    while (lock_flag_.test_and_set(std::memory_order_acquire)) {
        backoff_relax(spins++);
    }
}

void TaskPoolAllocator::unlock_() noexcept {
    lock_flag_.clear(std::memory_order_release);
}

} // namespace fem::threading
