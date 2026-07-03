#include "thread_pool.h"
#include "threading_log.h"
#include "threading_constants.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using fem::threading::LogThreading;
using TaskSubmitStatus = fem::threading::ThreadPool::TaskSubmitStatus;

using Clock = std::chrono::steady_clock;

bool is_scheduled(TaskSubmitStatus status) {
    return fem::threading::ThreadPool::is_scheduled(status);
}

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
    const char* error = nullptr;
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

    LOGT_INFO(LogThreading,
        "%-36s time=%.6fs throughput=%.2f %s/s count=%llu",
        result.name,
        result.seconds,
        throughput,
        result.unit,
        static_cast<unsigned long long>(result.operations));
}

bool benchmark_failed(const BenchmarkResult& result) {
    if (result.error == nullptr) {
        return false;
    }

    LOGT_ERROR(LogThreading, "%s: %s", result.name, result.error);
    return true;
}

int finish_benchmark_program(int exit_code) {
    logger::shutdown();
    return exit_code;
}

BenchmarkResult benchmark_schedule_small_tasks(const BenchmarkConfig& config) {
    fem::threading::ThreadPool pool(config.worker_count);
    std::atomic<std::uint64_t> counter{0};

    const auto begin = Clock::now();
    for (std::size_t index = 0; index < config.schedule_iterations; ++index) {
        if (!is_scheduled(pool.schedule([&counter]() noexcept {
                counter.fetch_add(1, std::memory_order_relaxed);
            }))) {
            return {"schedule_small_tasks", 0.0, config.schedule_iterations, "tasks", "failed to enqueue task"};
        }
    }
    pool.wait_idle();
    const auto end = Clock::now();

    if (counter.load(std::memory_order_relaxed) != config.schedule_iterations) {
        return {"schedule_small_tasks", elapsed_seconds(begin, end), config.schedule_iterations, "tasks", "counter mismatch"};
    }

    return {"schedule_small_tasks", elapsed_seconds(begin, end), config.schedule_iterations, "tasks", nullptr};
}

BenchmarkResult benchmark_schedule_large_callables(const BenchmarkConfig& config) {
    struct LargeCallable {
        std::array<std::byte, kLargeCallablePadding> payload{};
        std::atomic<std::uint64_t>* counter = nullptr;

        void operator()() const noexcept {
            counter->fetch_add(1, std::memory_order_relaxed);
        }
    };

    fem::threading::ThreadPool pool(config.worker_count);
    std::atomic<std::uint64_t> counter{0};

    const auto begin = Clock::now();
    for (std::size_t index = 0; index < config.large_callable_tasks; ++index) {
        LargeCallable callable;
        callable.counter = &counter;
        if (!is_scheduled(pool.schedule(callable))) {
            return {"schedule_large_callables", 0.0, config.large_callable_tasks, "tasks", "failed to enqueue task"};
        }
    }
    pool.wait_idle();
    const auto end = Clock::now();

    if (counter.load(std::memory_order_relaxed) != config.large_callable_tasks) {
        return {"schedule_large_callables", elapsed_seconds(begin, end), config.large_callable_tasks, "tasks", "counter mismatch"};
    }

    return {"schedule_large_callables", elapsed_seconds(begin, end), config.large_callable_tasks, "tasks", nullptr};
}

BenchmarkResult benchmark_external_producer_contention(const BenchmarkConfig& config) {
    fem::threading::ThreadPool pool(
        config.worker_count,
        kProducerQueueCapacity,
        kProducerQueueCapacity);
    std::atomic<std::uint64_t> counter{0};
    std::vector<std::thread> producers;
    producers.reserve(config.producer_count);
    std::atomic<bool> submit_failed{false};

    const auto begin = Clock::now();
    for (std::size_t producer = 0; producer < config.producer_count; ++producer) {
        producers.emplace_back([&pool, &counter, &config, &submit_failed] {
            for (std::size_t index = 0; index < config.producer_iterations; ++index) {
                if (submit_failed.load(std::memory_order_acquire)) {
                    return;
                }

                if (!is_scheduled(pool.schedule([&counter]() noexcept {
                    counter.fetch_add(1, std::memory_order_relaxed);
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
    const auto end = Clock::now();

    const std::uint64_t total_tasks = config.producer_count * config.producer_iterations;
    if (submit_failed.load(std::memory_order_acquire)) {
        return {"external_producer_contention", elapsed_seconds(begin, end), total_tasks, "tasks", "failed to enqueue task"};
    }

    if (counter.load(std::memory_order_relaxed) != total_tasks) {
        return {"external_producer_contention", elapsed_seconds(begin, end), total_tasks, "tasks", "counter mismatch"};
    }

    return {"external_producer_contention", elapsed_seconds(begin, end), total_tasks, "tasks", nullptr};
}

BenchmarkResult benchmark_recursive_spawn_pressure(const BenchmarkConfig& config) {
    fem::threading::ThreadPool pool(
        config.worker_count,
        kRecursiveQueueCapacity,
        kRecursiveQueueCapacity);
    std::atomic<std::uint64_t> counter{0};
    std::atomic<bool> child_schedule_failed{false};

    const auto begin = Clock::now();
    for (std::size_t root = 0; root < config.root_tasks; ++root) {
        if (!is_scheduled(pool.schedule([&pool, &counter, &config, &child_schedule_failed]() noexcept {
                counter.fetch_add(1, std::memory_order_relaxed);
                for (std::size_t branch = 0; branch < config.branch_tasks; ++branch) {
                    if (!is_scheduled(pool.schedule([&counter]() noexcept {
                        counter.fetch_add(1, std::memory_order_relaxed);
                    }))) {
                        child_schedule_failed.store(true, std::memory_order_release);
                        return;
                    }
                }
            }))) {
            return {"recursive_spawn_pressure", 0.0, config.root_tasks * (config.branch_tasks + 1), "tasks", "failed to enqueue root task"};
        }
    }
    pool.wait_idle();
    const auto end = Clock::now();

    const std::uint64_t total_tasks = config.root_tasks * (config.branch_tasks + 1);
    if (child_schedule_failed.load(std::memory_order_acquire)) {
        return {"recursive_spawn_pressure", elapsed_seconds(begin, end), total_tasks, "tasks", "failed to enqueue child task"};
    }

    if (counter.load(std::memory_order_relaxed) != total_tasks) {
        return {"recursive_spawn_pressure", elapsed_seconds(begin, end), total_tasks, "tasks", "counter mismatch"};
    }

    return {"recursive_spawn_pressure", elapsed_seconds(begin, end), total_tasks, "tasks", nullptr};
}

BenchmarkResult benchmark_parallel_for_contiguous(const BenchmarkConfig& config) {
    fem::threading::ThreadPool pool(config.worker_count);
    std::vector<std::uint32_t> values(config.element_count, 0);

    const auto begin = Clock::now();
    pool.parallel_for<std::size_t>(0, values.size(), config.grain_size, [&values](std::size_t chunk_begin, std::size_t chunk_end) noexcept {
        for (std::size_t index = chunk_begin; index < chunk_end; ++index) {
            values[index] = static_cast<std::uint32_t>((index * kDataPatternMultiplier) ^ kDataPatternXorMask);
        }
    });
    const auto end = Clock::now();

    std::uint64_t checksum = 0;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const std::uint32_t expected = static_cast<std::uint32_t>((index * kDataPatternMultiplier) ^ kDataPatternXorMask);
        if (values[index] != expected) {
            return {"parallel_for_contiguous", elapsed_seconds(begin, end), values.size(), "elements", "value mismatch"};
        }
        checksum += values[index];
    }
    if (checksum == 0) {
        return {"parallel_for_contiguous", elapsed_seconds(begin, end), values.size(), "elements", "checksum invalid"};
    }

    return {"parallel_for_contiguous", elapsed_seconds(begin, end), values.size(), "elements", nullptr};
}

BenchmarkResult benchmark_parallel_for_strided(const BenchmarkConfig& config) {
    fem::threading::ThreadPool pool(config.worker_count);
    std::vector<std::uint32_t> values(config.element_count, 0);
    const std::size_t stride = config.stride == 0 ? 1 : config.stride;

    const auto begin = Clock::now();
    pool.parallel_for<std::size_t>(0, stride, 1, [&values, stride](std::size_t chunk_begin, std::size_t chunk_end) noexcept {
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
            return {"parallel_for_strided", elapsed_seconds(begin, end), values.size(), "elements", "value mismatch"};
        }
        checksum += values[index];
    }
    if (checksum == 0) {
        return {"parallel_for_strided", elapsed_seconds(begin, end), values.size(), "elements", "checksum invalid"};
    }

    return {"parallel_for_strided", elapsed_seconds(begin, end), values.size(), "elements", nullptr};
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
    LOGT_INFO(LogThreading, "Usage: %s [--list] [--smoke] [benchmark_name]", program_name);
}

} // namespace

int main(int argc, char** argv) {
    BenchmarkConfig config;
    const char* selected_name = nullptr;

    for (int index = 1; index < argc; ++index) {
        const std::string_view arg = argv[index];
        if (arg == "--list") {
            for (const auto& benchmark : kBenchmarks) {
                LOGT_INFO(LogThreading, "%s", benchmark.name);
            }
            return finish_benchmark_program(0);
        }

        if (arg == "--smoke") {
            config = make_smoke_config();
            continue;
        }

        if (!arg.empty() && arg.front() == '-') {
            print_usage(argv[0]);
            return finish_benchmark_program(2);
        }

        selected_name = argv[index];
    }

    if (selected_name != nullptr) {
        for (const auto& benchmark : kBenchmarks) {
            if (std::string_view(selected_name) == benchmark.name) {
                const BenchmarkResult result = benchmark.run(config);
                if (benchmark_failed(result)) {
                    return finish_benchmark_program(1);
                }
                print_result(result);
                return finish_benchmark_program(0);
            }
        }

        LOGT_ERROR(LogThreading, "Unknown benchmark: %s", selected_name);
        return finish_benchmark_program(2);
    }

    for (const auto& benchmark : kBenchmarks) {
        const BenchmarkResult result = benchmark.run(config);
        if (benchmark_failed(result)) {
            return finish_benchmark_program(1);
        }
        print_result(result);
    }

    return finish_benchmark_program(0);
}