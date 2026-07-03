#include "thread_pool.h"
#include "spin_rw_lock.h"
#include "threading_constants.h"
#include "threading_log.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using fem::threading::LogThreading;
using TaskSubmitStatus = fem::threading::ThreadPool::TaskSubmitStatus;

bool is_scheduled(TaskSubmitStatus status) {
    return fem::threading::ThreadPool::is_scheduled(status);
}

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
constexpr std::size_t kLargeCallablePadding = fem::threading::constants::kTaskPoolBlockSize + 64;

bool test_schedule_and_wait_idle() {
    fem::threading::ThreadPool pool(kDefaultTestWorkerCount);
    std::atomic<int> counter{0};

    for (int index = 0; index < kScheduleTaskCount; ++index) {
        if (!is_scheduled(pool.schedule([&counter]() noexcept {
                counter.fetch_add(1, std::memory_order_relaxed);
            }))) {
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
        auto submit_result = pool.submit([index]() noexcept {
            return index * index;
        });
        std::future<int> future;
        if (!std::move(submit_result).try_take_future(future)) {
            return false;
        }
        futures.push_back(std::move(future));
    }

    int sum = 0;
    for (int index = 0; index < kFutureTaskCount; ++index) {
        sum += futures[index].get();
    }

    return sum == 85344;
}

bool test_submit_result_preserves_status_after_take() {
    fem::threading::ThreadPool pool(kDefaultTestWorkerCount);

    auto submit_result = pool.submit([]() noexcept {
        return 42;
    });
    if (!submit_result.scheduled() || submit_result.status() != TaskSubmitStatus::scheduled ||
        !submit_result.future_available()) {
        return false;
    }

    std::future<int> future;
    if (!std::move(submit_result).try_take_future(future)) {
        return false;
    }

    if (!submit_result.scheduled() || submit_result.status() != TaskSubmitStatus::scheduled ||
        submit_result.future_available()) {
        return false;
    }

    if (future.get() != 42) {
        return false;
    }

    std::future<int> second_take;
    return !std::move(submit_result).try_take_future(second_take);
}

bool test_schedule_reports_shutdown_failure() {
    fem::threading::ThreadPool pool(kDefaultTestWorkerCount);
    pool.shutdown();

    return pool.schedule([]() noexcept {}) == TaskSubmitStatus::pool_stopped;
}

bool test_submit_reports_shutdown_failure() {
    fem::threading::ThreadPool pool(kDefaultTestWorkerCount);
    pool.shutdown();

    auto submit_result = pool.submit([]() noexcept {
        return 7;
    });
    if (submit_result.scheduled()) {
        return false;
    }

    if (submit_result.status() != TaskSubmitStatus::pool_stopped) {
        return false;
    }

    std::future<int> future;
    return !std::move(submit_result).try_take_future(future);
}

bool test_parallel_for_covers_all_ranges() {
    fem::threading::ThreadPool pool(kDefaultTestWorkerCount);
    std::vector<std::atomic<int>> counts(kParallelForCountSize);
    for (auto& count : counts) {
        count.store(0, std::memory_order_relaxed);
    }

    pool.parallel_for<int>(0, static_cast<int>(counts.size()), kParallelForGrainSize, [&counts](int begin, int end) noexcept {
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

bool test_parallel_for_move_only_body() {
    fem::threading::ThreadPool pool(kDefaultTestWorkerCount);
    std::vector<int> values(kParallelForCountSize, 0);
    auto bias = std::make_unique<int>(7);

    pool.parallel_for<int>(0, static_cast<int>(values.size()), kParallelForGrainSize,
        [&values, bias = std::move(bias)](int begin, int end) noexcept {
            for (int index = begin; index < end; ++index) {
                values[static_cast<std::size_t>(index)] = index + *bias;
            }
        });

    for (std::size_t index = 0; index < values.size(); ++index) {
        if (values[index] != static_cast<int>(index) + 7) {
            return false;
        }
    }

    return true;
}

bool test_parallel_for_keeps_pool_reusable() {
    fem::threading::ThreadPool pool(kDefaultTestWorkerCount);
    std::atomic<int> touched{0};
    std::atomic<int> scheduled_after_exception{0};

    pool.parallel_for<int>(0, static_cast<int>(kParallelForCountSize), kParallelForGrainSize,
        [&touched](int begin, int end) noexcept {
            for (int index = begin; index < end; ++index) {
                touched.fetch_add(1, std::memory_order_relaxed);
            }
        });

    if (!is_scheduled(pool.schedule([&scheduled_after_exception]() noexcept {
            scheduled_after_exception.fetch_add(1, std::memory_order_relaxed);
        }))) {
        return false;
    }

    pool.wait_idle();
    return touched.load(std::memory_order_relaxed) == static_cast<int>(kParallelForCountSize) &&
           scheduled_after_exception.load(std::memory_order_relaxed) == 1;
}

bool test_nested_scheduling() {
    fem::threading::ThreadPool pool(kDefaultTestWorkerCount);
    std::atomic<int> counter{0};
    std::atomic<bool> child_schedule_failed{false};

    for (int index = 0; index < kNestedRootTaskCount; ++index) {
        if (!is_scheduled(pool.schedule([&pool, &counter, &child_schedule_failed]() noexcept {
                counter.fetch_add(1, std::memory_order_relaxed);
                for (int child = 0; child < kNestedChildTaskCount; ++child) {
                    if (!is_scheduled(pool.schedule([&counter]() noexcept {
                        counter.fetch_add(1, std::memory_order_relaxed);
                    }))) {
                        child_schedule_failed.store(true, std::memory_order_release);
                        return;
                    }
                }
            }))) {
            return false;
        }
    }

    pool.wait_idle();
    return !child_schedule_failed.load(std::memory_order_acquire) &&
           counter.load(std::memory_order_relaxed) == kNestedRootTaskCount * (kNestedChildTaskCount + 1);
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
    std::atomic<bool> submit_failed{false};

    for (int producer = 0; producer < kProducerCount; ++producer) {
        producers.emplace_back([producer, &pool, &sum, &submit_failed] {
            for (int task = 0; task < kTasksPerProducer; ++task) {
                if (submit_failed.load(std::memory_order_acquire)) {
                    return;
                }
                const std::uint64_t value = static_cast<std::uint64_t>(producer) * kTasksPerProducer + task + 1;
                if (!is_scheduled(pool.schedule([&sum, value]() noexcept {
                    sum.fetch_add(value, std::memory_order_relaxed);
                }))) {
                    submit_failed.store(true, std::memory_order_release);
                    return;
                }
            }
        });
    }

    for (auto& producer : producers) {
        producer.join();
    }

    pool.wait_idle();

    if (submit_failed.load(std::memory_order_acquire)) {
        return false;
    }

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

    pool.parallel_for<std::size_t>(0, values.size(), kStressLargeRangeGrainSize, [&values](std::size_t begin, std::size_t end) noexcept {
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
    std::atomic<bool> child_schedule_failed{false};

    for (int root = 0; root < kRecursiveRootTaskCount; ++root) {
        if (!is_scheduled(pool.schedule([&pool, &counter, &child_schedule_failed]() noexcept {
                counter.fetch_add(1, std::memory_order_relaxed);
                for (int branch = 0; branch < kRecursiveBranchTaskCount; ++branch) {
                    if (!is_scheduled(pool.schedule([&counter]() noexcept {
                        counter.fetch_add(1, std::memory_order_relaxed);
                    }))) {
                        child_schedule_failed.store(true, std::memory_order_release);
                        return;
                    }
                }
            }))) {
            return false;
        }
    }

    pool.wait_idle();
    return !child_schedule_failed.load(std::memory_order_acquire) &&
           counter.load(std::memory_order_relaxed) == kRecursiveRootTaskCount * (kRecursiveBranchTaskCount + 1);
}

bool test_small_pool_processes_detached_tasks() {
    fem::threading::ThreadPool pool(kSmallTestWorkerCount);
    std::atomic<int> counter{0};

    if (!is_scheduled(pool.schedule([&counter]() noexcept {
            counter.fetch_add(1, std::memory_order_relaxed);
        }))) {
        return false;
    }

    pool.wait_idle();
    return counter.load(std::memory_order_relaxed) == 1;
}

bool test_large_callable_fallback_allocation() {
    struct LargeCallable {
        std::array<std::byte, kLargeCallablePadding> payload{};
        std::atomic<int>* counter = nullptr;

        void operator()() const noexcept {
            counter->fetch_add(1, std::memory_order_relaxed);
        }
    };

    fem::threading::ThreadPool pool(kSmallTestWorkerCount);
    std::atomic<int> counter{0};
    LargeCallable callable;
    callable.counter = &counter;

    if (!is_scheduled(pool.schedule(callable))) {
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
        {"submit_result_preserves_status_after_take", &test_submit_result_preserves_status_after_take},
        {"schedule_reports_shutdown_failure", &test_schedule_reports_shutdown_failure},
        {"submit_reports_shutdown_failure", &test_submit_reports_shutdown_failure},
        {"parallel_for_covers_all_ranges", &test_parallel_for_covers_all_ranges},
        {"parallel_for_move_only_body", &test_parallel_for_move_only_body},
        {"parallel_for_keeps_pool_reusable", &test_parallel_for_keeps_pool_reusable},
        {"nested_scheduling", &test_nested_scheduling},
        {"spin_rw_lock_parallel_readers", &test_spin_rw_lock_allows_parallel_readers},
        {"small_pool_processes_detached_tasks", &test_small_pool_processes_detached_tasks},
        {"large_callable_fallback_allocation", &test_large_callable_fallback_allocation},
        {"stress_many_external_producers", &test_stress_many_external_producers},
        {"stress_parallel_for_large_range", &test_stress_parallel_for_large_range},
        {"stress_recursive_fan_out", &test_stress_recursive_fan_out},
    };

    if (argc == 2 && std::strcmp(argv[1], "--list") == 0) {
        for (const auto& test : tests) {
            LOGT_INFO(LogThreading, "%s", test.name);
        }
        logger::shutdown();
        return 0;
    }

    if (argc == 2) {
        const std::string_view selected = argv[1];
        for (const auto& test : tests) {
            if (selected == test.name) {
                const bool passed = test.run();
                if (passed) {
                    LOGT_INFO(LogThreading, "PASS %s", test.name);
                } else {
                    LOGT_ERROR(LogThreading, "FAIL %s", test.name);
                }
                logger::shutdown();
                return passed ? 0 : 1;
            }
        }

        LOGT_ERROR(LogThreading, "Unknown test: %s", argv[1]);
        logger::shutdown();
        return 2;
    }

    bool all_passed = true;
    for (const auto& test : tests) {
        const bool passed = test.run();
        if (passed) {
            LOGT_INFO(LogThreading, "PASS %s", test.name);
        } else {
            LOGT_ERROR(LogThreading, "FAIL %s", test.name);
        }
        all_passed = all_passed && passed;
    }

    logger::shutdown();
    return all_passed ? 0 : 1;
}