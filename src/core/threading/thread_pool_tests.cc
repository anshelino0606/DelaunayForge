#include "thread_pool.h"
#include "threading_constants.h"
#include "spin_rw_lock.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace {

constexpr std::size_t kDefaultTestWorkerCount = 4;
constexpr std::size_t kSmallTestWorkerCount = 2;
constexpr std::size_t kLargeRangeWorkerCount = 6;
constexpr std::size_t kSharedReaderCount = 4;
constexpr int kScheduleTaskCount = 1000;
constexpr int kFutureTaskCount = 64;
constexpr int kNestedRootTaskCount = 64;
constexpr int kNestedChildTaskCount = 8;
constexpr std::size_t kParallelForCountSize = 1024;
constexpr int kParallelForGrainSize = 17;
constexpr int kProducerCount = 8;
constexpr int kTasksPerProducer = 20000;
constexpr std::size_t kStressProducerQueueCapacity = fem::threading::constants::kDefaultLocalQueueCapacity;
constexpr std::size_t kStressLargeRangeItemCount = 1u << 20;
constexpr std::size_t kStressLargeRangeLocalCapacity = 512;
constexpr std::size_t kStressLargeRangeRemoteCapacity = 512;
constexpr std::size_t kStressLargeRangeGrainSize = 257;
constexpr std::uint32_t kStressLargeRangeMultiplier = 2654435761u;
constexpr std::uint32_t kStressLargeRangeXorMask = 0x9e3779b9u;
constexpr int kRecursiveRootTaskCount = 128;
constexpr int kRecursiveBranchTaskCount = 32;
constexpr std::size_t kRecursiveQueueCapacity = 256;
constexpr std::size_t kExceptionTestQueueCapacity = 128;
constexpr std::size_t kLargeCallablePadding = fem::threading::constants::kTaskPoolBlockSize + 64;

std::atomic<int> g_unhandled_exception_count{0};

void count_unhandled_exception(std::exception_ptr exception) noexcept {
    if (exception == nullptr) {
        return;
    }

    g_unhandled_exception_count.fetch_add(1, std::memory_order_relaxed);
}

bool test_schedule_and_wait_idle() {
    fem::threading::ThreadPool pool(kDefaultTestWorkerCount);
    std::atomic<int> counter{0};

    for (int index = 0; index < kScheduleTaskCount; ++index) {
        if (!pool.schedule([&counter] {
                counter.fetch_add(1, std::memory_order_relaxed);
            })) {
            return false;
        }
    }

    pool.wait_idle();
    return counter.load(std::memory_order_relaxed) == kScheduleTaskCount;
}

bool test_submit_returns_values() {
    fem::threading::ThreadPool pool(kDefaultTestWorkerCount);
    std::vector<std::future<int>> futures;
    futures.reserve(kFutureTaskCount);

    for (int index = 0; index < kFutureTaskCount; ++index) {
        futures.push_back(pool.submit([index] {
            return index * index;
        }));
    }

    int sum = 0;
    for (int index = 0; index < kFutureTaskCount; ++index) {
        sum += futures[index].get();
    }

    return sum == 85344;
}

bool test_parallel_for_covers_all_ranges() {
    fem::threading::ThreadPool pool(kDefaultTestWorkerCount);
    std::vector<std::atomic<int>> counts(kParallelForCountSize);
    for (auto& count : counts) {
        count.store(0, std::memory_order_relaxed);
    }

    pool.parallel_for<int>(0, static_cast<int>(counts.size()), kParallelForGrainSize, [&counts](int begin, int end) {
        for (int index = begin; index < end; ++index) {
            counts[static_cast<std::size_t>(index)].fetch_add(1, std::memory_order_relaxed);
        }
    });

    for (const auto& count : counts) {
        if (count.load(std::memory_order_relaxed) != 1) {
            return false;
        }
    }

    return true;
}

bool test_nested_scheduling() {
    fem::threading::ThreadPool pool(kDefaultTestWorkerCount);
    std::atomic<int> counter{0};

    for (int index = 0; index < kNestedRootTaskCount; ++index) {
        if (!pool.schedule([&pool, &counter] {
                counter.fetch_add(1, std::memory_order_relaxed);
                for (int child = 0; child < kNestedChildTaskCount; ++child) {
                    pool.schedule([&counter] {
                        counter.fetch_add(1, std::memory_order_relaxed);
                    });
                }
            })) {
            return false;
        }
    }

    pool.wait_idle();
    return counter.load(std::memory_order_relaxed) == kNestedRootTaskCount * (kNestedChildTaskCount + 1);
}

bool test_spin_rw_lock_allows_parallel_readers() {
    fem::threading::SpinRWLock lock;
    std::atomic<bool> start{false};
    std::atomic<bool> release_readers{false};
    std::atomic<int> readers_inside{0};
    std::atomic<int> max_readers_inside{0};
    std::atomic<int> readers_ready{0};
    std::atomic<int> shared_value{0};
    std::vector<std::thread> readers;

    for (int index = 0; index < kSharedReaderCount; ++index) {
        readers.emplace_back([&] {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            fem::threading::SharedSpinLockGuard guard(lock);
            const int current = readers_inside.fetch_add(1, std::memory_order_acq_rel) + 1;
            readers_ready.fetch_add(1, std::memory_order_acq_rel);

            int observed = max_readers_inside.load(std::memory_order_relaxed);
            while (observed < current &&
                   !max_readers_inside.compare_exchange_weak(observed, current, std::memory_order_relaxed)) {
            }

            while (!release_readers.load(std::memory_order_acquire)) {
                if (shared_value.load(std::memory_order_relaxed) != 0) {
                    readers_inside.fetch_sub(1, std::memory_order_acq_rel);
                    return;
                }
                std::this_thread::yield();
            }

            readers_inside.fetch_sub(1, std::memory_order_acq_rel);
        });
    }

    std::thread writer([&] {
        start.store(true, std::memory_order_release);
        while (readers_ready.load(std::memory_order_acquire) != kSharedReaderCount) {
            std::this_thread::yield();
        }

        fem::threading::UniqueSpinLockGuard guard(lock);
        shared_value.store(1, std::memory_order_relaxed);
    });

    while (readers_ready.load(std::memory_order_acquire) != kSharedReaderCount) {
        std::this_thread::yield();
    }

    release_readers.store(true, std::memory_order_release);

    for (auto& reader : readers) {
        reader.join();
    }
    writer.join();

    return max_readers_inside.load(std::memory_order_relaxed) > 1 &&
           shared_value.load(std::memory_order_relaxed) == 1;
}

bool test_stress_many_external_producers() {
    fem::threading::ThreadPool pool(
        kDefaultTestWorkerCount,
        kStressProducerQueueCapacity,
        kStressProducerQueueCapacity);
    std::atomic<std::uint64_t> sum{0};
    std::vector<std::thread> producers;
    producers.reserve(kProducerCount);

    for (int producer = 0; producer < kProducerCount; ++producer) {
        producers.emplace_back([producer, &pool, &sum] {
            for (int task = 0; task < kTasksPerProducer; ++task) {
                const std::uint64_t value = static_cast<std::uint64_t>(producer) * kTasksPerProducer + task + 1;
                while (!pool.schedule([&sum, value] {
                    sum.fetch_add(value, std::memory_order_relaxed);
                })) {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& producer : producers) {
        producer.join();
    }

    pool.wait_idle();

    const std::uint64_t total_tasks = static_cast<std::uint64_t>(kProducerCount) * kTasksPerProducer;
    const std::uint64_t expected_sum = total_tasks * (total_tasks + 1) / 2;
    return sum.load(std::memory_order_relaxed) == expected_sum;
}

bool test_stress_parallel_for_large_range() {
    std::vector<std::uint32_t> values(kStressLargeRangeItemCount, 0);
    fem::threading::ThreadPool pool(
        kLargeRangeWorkerCount,
        kStressLargeRangeLocalCapacity,
        kStressLargeRangeRemoteCapacity);

    pool.parallel_for<std::size_t>(0, values.size(), kStressLargeRangeGrainSize, [&values](std::size_t begin, std::size_t end) {
        for (std::size_t index = begin; index < end; ++index) {
            values[index] = static_cast<std::uint32_t>((index * kStressLargeRangeMultiplier) ^ kStressLargeRangeXorMask);
        }
    });

    std::uint64_t checksum = 0;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const std::uint32_t expected = static_cast<std::uint32_t>((index * kStressLargeRangeMultiplier) ^ kStressLargeRangeXorMask);
        if (values[index] != expected) {
            return false;
        }
        checksum += values[index];
    }

    return checksum != 0;
}

bool test_stress_recursive_fan_out() {
    fem::threading::ThreadPool pool(
        kDefaultTestWorkerCount,
        kRecursiveQueueCapacity,
        kRecursiveQueueCapacity);
    std::atomic<int> counter{0};

    for (int root = 0; root < kRecursiveRootTaskCount; ++root) {
        if (!pool.schedule([&pool, &counter] {
                counter.fetch_add(1, std::memory_order_relaxed);
                for (int branch = 0; branch < kRecursiveBranchTaskCount; ++branch) {
                    pool.schedule([&counter] {
                        counter.fetch_add(1, std::memory_order_relaxed);
                    });
                }
            })) {
            return false;
        }
    }

    pool.wait_idle();
    return counter.load(std::memory_order_relaxed) == kRecursiveRootTaskCount * (kRecursiveBranchTaskCount + 1);
}

bool test_detached_exception_handler_invoked() {
    g_unhandled_exception_count.store(0, std::memory_order_relaxed);
    fem::threading::ThreadPool pool(
        kSmallTestWorkerCount,
        kExceptionTestQueueCapacity,
        kExceptionTestQueueCapacity,
        &count_unhandled_exception);

    if (!pool.schedule([] {
            throw std::runtime_error("detached task failure");
        })) {
        return false;
    }

    pool.wait_idle();
    return g_unhandled_exception_count.load(std::memory_order_relaxed) == 1;
}

bool test_large_callable_fallback_allocation() {
    struct LargeCallable {
        std::array<std::byte, kLargeCallablePadding> payload{};
        std::atomic<int>* counter = nullptr;

        void operator()() const {
            counter->fetch_add(1, std::memory_order_relaxed);
        }
    };

    fem::threading::ThreadPool pool(kSmallTestWorkerCount);
    std::atomic<int> counter{0};
    LargeCallable callable;
    callable.counter = &counter;

    if (!pool.schedule(callable)) {
        return false;
    }

    pool.wait_idle();
    return counter.load(std::memory_order_relaxed) == 1;
}

} // namespace

int main(int argc, char** argv) {
    struct TestCase {
        const char* name;
        bool (*run)();
    };

    const TestCase tests[] = {
        {"schedule_and_wait_idle", &test_schedule_and_wait_idle},
        {"submit_returns_values", &test_submit_returns_values},
        {"parallel_for_covers_all_ranges", &test_parallel_for_covers_all_ranges},
        {"nested_scheduling", &test_nested_scheduling},
        {"spin_rw_lock_parallel_readers", &test_spin_rw_lock_allows_parallel_readers},
        {"detached_exception_handler_invoked", &test_detached_exception_handler_invoked},
        {"large_callable_fallback_allocation", &test_large_callable_fallback_allocation},
        {"stress_many_external_producers", &test_stress_many_external_producers},
        {"stress_parallel_for_large_range", &test_stress_parallel_for_large_range},
        {"stress_recursive_fan_out", &test_stress_recursive_fan_out},
    };

    if (argc == 2 && std::strcmp(argv[1], "--list") == 0) {
        for (const auto& test : tests) {
            std::cout << test.name << '\n';
        }
        return 0;
    }

    if (argc == 2) {
        const std::string_view selected = argv[1];
        for (const auto& test : tests) {
            if (selected == test.name) {
                const bool passed = test.run();
                std::cout << (passed ? "PASS" : "FAIL") << " " << test.name << '\n';
                return passed ? 0 : 1;
            }
        }

        std::cerr << "Unknown test: " << argv[1] << '\n';
        return 2;
    }

    bool all_passed = true;
    for (const auto& test : tests) {
        const bool passed = test.run();
        std::cout << (passed ? "PASS" : "FAIL") << " " << test.name << '\n';
        all_passed = all_passed && passed;
    }

    return all_passed ? 0 : 1;
}