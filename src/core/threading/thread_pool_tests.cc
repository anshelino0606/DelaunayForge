#include "thread_pool.h"
#include "spin_rw_lock.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

namespace {

bool test_schedule_and_wait_idle() {
    fem::threading::ThreadPool pool(4, 256, 256);
    std::atomic<int> counter{0};

    for (int index = 0; index < 1000; ++index) {
        if (!pool.schedule([&counter] {
                counter.fetch_add(1, std::memory_order_relaxed);
            })) {
            return false;
        }
    }

    pool.wait_idle();
    return counter.load(std::memory_order_relaxed) == 1000;
}

bool test_submit_returns_values() {
    fem::threading::ThreadPool pool(4, 256, 256);
    std::vector<std::future<int>> futures;
    futures.reserve(64);

    for (int index = 0; index < 64; ++index) {
        futures.push_back(pool.submit([index] {
            return index * index;
        }));
    }

    int sum = 0;
    for (int index = 0; index < 64; ++index) {
        sum += futures[index].get();
    }

    return sum == 85344;
}

bool test_parallel_for_covers_all_ranges() {
    fem::threading::ThreadPool pool(4, 256, 256);
    std::vector<std::atomic<int>> counts(1024);
    for (auto& count : counts) {
        count.store(0, std::memory_order_relaxed);
    }

    pool.parallel_for<int>(0, static_cast<int>(counts.size()), 17, [&counts](int begin, int end) {
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
    fem::threading::ThreadPool pool(4, 256, 256);
    std::atomic<int> counter{0};

    for (int index = 0; index < 64; ++index) {
        if (!pool.schedule([&pool, &counter] {
                counter.fetch_add(1, std::memory_order_relaxed);
                for (int child = 0; child < 8; ++child) {
                    pool.schedule([&counter] {
                        counter.fetch_add(1, std::memory_order_relaxed);
                    });
                }
            })) {
            return false;
        }
    }

    pool.wait_idle();
    return counter.load(std::memory_order_relaxed) == 64 * 9;
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

    for (int index = 0; index < 4; ++index) {
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
        while (readers_ready.load(std::memory_order_acquire) != 4) {
            std::this_thread::yield();
        }

        fem::threading::UniqueSpinLockGuard guard(lock);
        shared_value.store(1, std::memory_order_relaxed);
    });

    while (readers_ready.load(std::memory_order_acquire) != 4) {
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
    constexpr int producer_count = 8;
    constexpr int tasks_per_producer = 20000;

    fem::threading::ThreadPool pool(4, 1024, 1024);
    std::atomic<std::uint64_t> sum{0};
    std::vector<std::thread> producers;
    producers.reserve(producer_count);

    for (int producer = 0; producer < producer_count; ++producer) {
        producers.emplace_back([producer, &pool, &sum] {
            for (int task = 0; task < tasks_per_producer; ++task) {
                const std::uint64_t value = static_cast<std::uint64_t>(producer) * tasks_per_producer + task + 1;
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

    const std::uint64_t total_tasks = static_cast<std::uint64_t>(producer_count) * tasks_per_producer;
    const std::uint64_t expected_sum = total_tasks * (total_tasks + 1) / 2;
    return sum.load(std::memory_order_relaxed) == expected_sum;
}

bool test_stress_parallel_for_large_range() {
    constexpr std::size_t item_count = 1u << 20;
    std::vector<std::uint32_t> values(item_count, 0);
    fem::threading::ThreadPool pool(6, 512, 512);

    pool.parallel_for<std::size_t>(0, values.size(), 257, [&values](std::size_t begin, std::size_t end) {
        for (std::size_t index = begin; index < end; ++index) {
            values[index] = static_cast<std::uint32_t>((index * 2654435761u) ^ 0x9e3779b9u);
        }
    });

    std::uint64_t checksum = 0;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const std::uint32_t expected = static_cast<std::uint32_t>((index * 2654435761u) ^ 0x9e3779b9u);
        if (values[index] != expected) {
            return false;
        }
        checksum += values[index];
    }

    return checksum != 0;
}

bool test_stress_recursive_fan_out() {
    constexpr int root_tasks = 128;
    constexpr int branch_tasks = 32;

    fem::threading::ThreadPool pool(4, 256, 256);
    std::atomic<int> counter{0};

    for (int root = 0; root < root_tasks; ++root) {
        if (!pool.schedule([&pool, &counter] {
                counter.fetch_add(1, std::memory_order_relaxed);
                for (int branch = 0; branch < branch_tasks; ++branch) {
                    pool.schedule([&counter] {
                        counter.fetch_add(1, std::memory_order_relaxed);
                    });
                }
            })) {
            return false;
        }
    }

    pool.wait_idle();
    return counter.load(std::memory_order_relaxed) == root_tasks * (branch_tasks + 1);
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