#include "task_pool_allocator.h"

#include "threading_constants.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <new>
#include <stdexcept>
#include <thread>

namespace fem::threading {

namespace {

inline void cpu_relax() noexcept {
#if defined(__clang__) || defined(__GNUC__)
  #if defined(__i386__) || defined(__x86_64__)
    __builtin_ia32_pause();
  #elif defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("yield");
  #else
    std::atomic_signal_fence(std::memory_order_seq_cst);
  #endif
#else
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

inline void backoff_relax(std::size_t spins) noexcept {
    if (spins < constants::kSpinPauseThreshold) {
        cpu_relax();
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
    for (void* slab : slabs_) {
        ::operator delete(slab, std::align_val_t(alignof(std::max_align_t)));
    }
}

void* TaskPoolAllocator::allocate(std::size_t size, std::size_t alignment) {
    if (!can_pool_(size, alignment)) {
        return ::operator new(size, std::align_val_t(alignment));
    }

    FreeNode* node = acquire_batch(1);
    if (node == nullptr) {
        throw std::bad_alloc();
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

TaskPoolAllocator::FreeNode* TaskPoolAllocator::acquire_batch(std::size_t count) {
    if (count == 0) {
        return nullptr;
    }

    lock_();
    while (free_list_ == nullptr) {
        allocate_slab_();
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

void TaskPoolAllocator::allocate_slab_() {
    const std::size_t bytes = block_stride_ * blocks_per_slab_;
    void* slab = ::operator new(bytes, std::align_val_t(alignof(std::max_align_t)));

    try {
        slabs_.push_back(slab);
    } catch (...) {
        ::operator delete(slab, std::align_val_t(alignof(std::max_align_t)));
        throw;
    }

    auto* data = static_cast<std::byte*>(slab);
    for (std::size_t index = 0; index < blocks_per_slab_; ++index) {
        FreeNode* node = reinterpret_cast<FreeNode*>(data + index * block_stride_);
        node->next = free_list_;
        free_list_ = node;
    }
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