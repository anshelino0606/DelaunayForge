#include "thread_pool.h"
#include "threading_constants.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct BenchmarkConfig {
    std::size_t worker_count = 4;
    std::size_t schedule_iterations = 200000;
    std::size_t producer_count = 8;
    std::size_t producer_iterations = 50000;
    std::size_t root_tasks = 256;
    std::size_t branch_tasks = 32;
    std::size_t element_count = 1u << 22;
    std::size_t grain_size = 512;
    std::size_t stride = 64;
    std::size_t large_callable_tasks = 50000;
};

struct BenchmarkResult {
    const char* name = "";
    double seconds = 0.0;
    std::uint64_t operations = 0;
    const char* unit = "ops";
};

constexpr std::size_t kProducerQueueCapacity = fem::threading::constants::kDefaultLocalQueueCapacity;
constexpr std::size_t kRecursiveQueueCapacity = 256;
constexpr std::size_t kLargeCallablePadding = fem::threading::constants::kTaskPoolBlockSize + 64;
constexpr std::uint32_t kDataPatternMultiplier = 2654435761u;
constexpr std::uint32_t kDataPatternXorMask = 0x9e3779b9u;

double elapsed_seconds(Clock::time_point begin, Clock::time_point end) {
    return std::chrono::duration<double>(end - begin).count();
}

void print_result(const BenchmarkResult& result) {
    const double throughput = result.seconds > 0.0
        ? static_cast<double>(result.operations) / result.seconds
        : 0.0;

    std::cout << std::left << std::setw(36) << result.name
              << " time=" << std::fixed << std::setprecision(6) << result.seconds << "s"
              << " throughput=" << std::fixed << std::setprecision(2) << throughput
              << " " << result.unit << "/s"
              << " count=" << result.operations << '\n';
}

BenchmarkResult benchmark_schedule_small_tasks(const BenchmarkConfig& config) {
    fem::threading::ThreadPool pool(config.worker_count);
    std::atomic<std::uint64_t> counter{0};

    const auto begin = Clock::now();
    for (std::size_t index = 0; index < config.schedule_iterations; ++index) {
        if (!pool.schedule([&counter] {
                counter.fetch_add(1, std::memory_order_relaxed);
            })) {
            throw std::runtime_error("schedule_small_tasks failed to enqueue task");
        }
    }
    pool.wait_idle();
    const auto end = Clock::now();

    if (counter.load(std::memory_order_relaxed) != config.schedule_iterations) {
        throw std::runtime_error("schedule_small_tasks counter mismatch");
    }

    return {"schedule_small_tasks", elapsed_seconds(begin, end), config.schedule_iterations, "tasks"};
}

BenchmarkResult benchmark_schedule_large_callables(const BenchmarkConfig& config) {
    struct LargeCallable {
        std::array<std::byte, kLargeCallablePadding> payload{};
        std::atomic<std::uint64_t>* counter = nullptr;

        void operator()() const {
            counter->fetch_add(1, std::memory_order_relaxed);
        }
    };

    fem::threading::ThreadPool pool(config.worker_count);
    std::atomic<std::uint64_t> counter{0};

    const auto begin = Clock::now();
    for (std::size_t index = 0; index < config.large_callable_tasks; ++index) {
        LargeCallable callable;
        callable.counter = &counter;
        if (!pool.schedule(callable)) {
            throw std::runtime_error("schedule_large_callables failed to enqueue task");
        }
    }
    pool.wait_idle();
    const auto end = Clock::now();

    if (counter.load(std::memory_order_relaxed) != config.large_callable_tasks) {
        throw std::runtime_error("schedule_large_callables counter mismatch");
    }

    return {"schedule_large_callables", elapsed_seconds(begin, end), config.large_callable_tasks, "tasks"};
}

BenchmarkResult benchmark_external_producer_contention(const BenchmarkConfig& config) {
    fem::threading::ThreadPool pool(
        config.worker_count,
        kProducerQueueCapacity,
        kProducerQueueCapacity);
    std::atomic<std::uint64_t> counter{0};
    std::vector<std::thread> producers;
    producers.reserve(config.producer_count);

    const auto begin = Clock::now();
    for (std::size_t producer = 0; producer < config.producer_count; ++producer) {
        producers.emplace_back([&pool, &counter, &config] {
            for (std::size_t index = 0; index < config.producer_iterations; ++index) {
                while (!pool.schedule([&counter] {
                    counter.fetch_add(1, std::memory_order_relaxed);
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
    const auto end = Clock::now();

    const std::uint64_t total_tasks = config.producer_count * config.producer_iterations;
    if (counter.load(std::memory_order_relaxed) != total_tasks) {
        throw std::runtime_error("external_producer_contention counter mismatch");
    }

    return {"external_producer_contention", elapsed_seconds(begin, end), total_tasks, "tasks"};
}

BenchmarkResult benchmark_recursive_spawn_pressure(const BenchmarkConfig& config) {
    fem::threading::ThreadPool pool(
        config.worker_count,
        kRecursiveQueueCapacity,
        kRecursiveQueueCapacity);
    std::atomic<std::uint64_t> counter{0};

    const auto begin = Clock::now();
    for (std::size_t root = 0; root < config.root_tasks; ++root) {
        if (!pool.schedule([&pool, &counter, &config] {
                counter.fetch_add(1, std::memory_order_relaxed);
                for (std::size_t branch = 0; branch < config.branch_tasks; ++branch) {
                    pool.schedule([&counter] {
                        counter.fetch_add(1, std::memory_order_relaxed);
                    });
                }
            })) {
            throw std::runtime_error("recursive_spawn_pressure failed to enqueue root task");
        }
    }
    pool.wait_idle();
    const auto end = Clock::now();

    const std::uint64_t total_tasks = config.root_tasks * (config.branch_tasks + 1);
    if (counter.load(std::memory_order_relaxed) != total_tasks) {
        throw std::runtime_error("recursive_spawn_pressure counter mismatch");
    }

    return {"recursive_spawn_pressure", elapsed_seconds(begin, end), total_tasks, "tasks"};
}

BenchmarkResult benchmark_parallel_for_contiguous(const BenchmarkConfig& config) {
    fem::threading::ThreadPool pool(config.worker_count);
    std::vector<std::uint32_t> values(config.element_count, 0);

    const auto begin = Clock::now();
    pool.parallel_for<std::size_t>(0, values.size(), config.grain_size, [&values](std::size_t chunk_begin, std::size_t chunk_end) {
        for (std::size_t index = chunk_begin; index < chunk_end; ++index) {
            values[index] = static_cast<std::uint32_t>((index * kDataPatternMultiplier) ^ kDataPatternXorMask);
        }
    });
    const auto end = Clock::now();

    std::uint64_t checksum = 0;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const std::uint32_t expected = static_cast<std::uint32_t>((index * kDataPatternMultiplier) ^ kDataPatternXorMask);
        if (values[index] != expected) {
            throw std::runtime_error("parallel_for_contiguous value mismatch");
        }
        checksum += values[index];
    }
    if (checksum == 0) {
        throw std::runtime_error("parallel_for_contiguous checksum invalid");
    }

    return {"parallel_for_contiguous", elapsed_seconds(begin, end), values.size(), "elements"};
}

BenchmarkResult benchmark_parallel_for_strided(const BenchmarkConfig& config) {
    fem::threading::ThreadPool pool(config.worker_count);
    std::vector<std::uint32_t> values(config.element_count, 0);
    const std::size_t stride = config.stride == 0 ? 1 : config.stride;

    const auto begin = Clock::now();
    pool.parallel_for<std::size_t>(0, stride, 1, [&values, stride](std::size_t chunk_begin, std::size_t chunk_end) {
        for (std::size_t lane = chunk_begin; lane < chunk_end; ++lane) {
            for (std::size_t index = lane; index < values.size(); index += stride) {
                values[index] = static_cast<std::uint32_t>((index * kDataPatternMultiplier) ^ kDataPatternXorMask);
            }
        }
    });
    const auto end = Clock::now();

    std::uint64_t checksum = 0;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const std::uint32_t expected = static_cast<std::uint32_t>((index * kDataPatternMultiplier) ^ kDataPatternXorMask);
        if (values[index] != expected) {
            throw std::runtime_error("parallel_for_strided value mismatch");
        }
        checksum += values[index];
    }
    if (checksum == 0) {
        throw std::runtime_error("parallel_for_strided checksum invalid");
    }

    return {"parallel_for_strided", elapsed_seconds(begin, end), values.size(), "elements"};
}

using BenchmarkFn = BenchmarkResult (*)(const BenchmarkConfig&);

struct BenchmarkCase {
    const char* name;
    BenchmarkFn run;
};

constexpr BenchmarkCase kBenchmarks[] = {
    {"schedule_small_tasks", &benchmark_schedule_small_tasks},
    {"schedule_large_callables", &benchmark_schedule_large_callables},
    {"external_producer_contention", &benchmark_external_producer_contention},
    {"recursive_spawn_pressure", &benchmark_recursive_spawn_pressure},
    {"parallel_for_contiguous", &benchmark_parallel_for_contiguous},
    {"parallel_for_strided", &benchmark_parallel_for_strided},
};

BenchmarkConfig make_smoke_config() {
    BenchmarkConfig config;
    config.schedule_iterations = 20000;
    config.producer_count = 4;
    config.producer_iterations = 5000;
    config.root_tasks = 64;
    config.branch_tasks = 16;
    config.element_count = 1u << 18;
    config.grain_size = 256;
    config.stride = 32;
    config.large_callable_tasks = 5000;
    return config;
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [--list] [--smoke] [benchmark_name]\n";
}

} // namespace

int main(int argc, char** argv) {
    BenchmarkConfig config;
    const char* selected_name = nullptr;

    for (int index = 1; index < argc; ++index) {
        const std::string_view arg = argv[index];
        if (arg == "--list") {
            for (const auto& benchmark : kBenchmarks) {
                std::cout << benchmark.name << '\n';
            }
            return 0;
        }

        if (arg == "--smoke") {
            config = make_smoke_config();
            continue;
        }

        if (!arg.empty() && arg.front() == '-') {
            print_usage(argv[0]);
            return 2;
        }

        selected_name = argv[index];
    }

    try {
        if (selected_name != nullptr) {
            for (const auto& benchmark : kBenchmarks) {
                if (std::string_view(selected_name) == benchmark.name) {
                    print_result(benchmark.run(config));
                    return 0;
                }
            }

            std::cerr << "Unknown benchmark: " << selected_name << '\n';
            return 2;
        }

        for (const auto& benchmark : kBenchmarks) {
            print_result(benchmark.run(config));
        }
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Benchmark failure: " << exception.what() << '\n';
        return 1;
    }
}