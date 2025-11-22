#include <benchmark/benchmark.h>

#include <pipeline/pipeline.hpp>

#include <vector>
#include <random>
#include <numeric>

using namespace dpb;

// Benchmark: Simple transform throughput
static void BM_SimpleTransform(benchmark::State& state) {
    std::vector<int> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);

    for (auto _ : state) {
        auto result = Pipeline<int,int>::from(data)
            .transform([](int x) { return x * 2; })
            .collect(data);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(int));
}
BENCHMARK(BM_SimpleTransform)->Range(1000, 1<<20); // 1K to 1M items

// Benchmark: Filter + transform pipeline
static void BM_FilterTransform(benchmark::State& state) {
    std::vector<int> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);

    for (auto _ : state) {
        auto result = Pipeline<int,int>::from(data)
            .filter([](int x) { return x % 2 == 0; })
            .transform([](int x) { return x * x; })
            .collect(data);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_FilterTransform)->Range(1000, 1<<20);

// Benchmark: Compare with hand-written loop
static void BM_HandWrittenLoop(benchmark::State& state) {
    std::vector<int> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);

    for (auto _ : state) {
        std::vector<int> result;
        for (int x : data) {
            if (x % 2 == 0) {
                result.push_back(x * x);
            }
        }
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_HandWrittenLoop)->Range(1000, 1<<20);

// Benchmark: Pipeline with stats tracking (overhead measurement)
static void BM_WithStats(benchmark::State& state) {
    std::vector<int> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);

    for (auto _ : state) {
        auto result = Pipeline<int,int>::from(data)
            .with_stats()
            .filter([](int x) { return x % 2 == 0; })
            .transform([](int x) { return x * 2; })
            .collect(data);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_WithStats)->Range(1000, 1<<20);

// Benchmark: Empty pipeline (baseline overhead)
static void BM_EmptyPipeline(benchmark::State& state) {
    std::vector<int> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);

    for (auto _ : state) {
        auto result = Pipeline<int,int>::from(data)
            .collect(data);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_EmptyPipeline)->Range(1000, 1<<20);

// Benchmark: Parallel filter + transform
static void BM_ParallelFilterTransform(benchmark::State& state) {
    std::vector<int> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);

    for (auto _ : state) {
        auto result = Pipeline<int,int>::from(data)
            .filter([](int x) { return x % 2 == 0; })
            .transform([](int x) { return x * x; })
            .parallel(std::thread::hardware_concurrency(), ExecutionPolicy::ParallelPreserveOrder)
            .collect(data);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_ParallelFilterTransform)->Range(10000, 1<<20);

// Benchmark: Parallel unordered vs ordered
static void BM_ParallelUnordered(benchmark::State& state) {
    std::vector<int> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);

    for (auto _ : state) {
        auto result = Pipeline<int,int>::from(data)
            .filter([](int x) { return x % 2 == 0; })
            .transform([](int x) { return x * x; })
            .parallel(std::thread::hardware_concurrency(), ExecutionPolicy::ParallelUnordered)
            .collect(data);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_ParallelUnordered)->Range(10000, 1<<20);

BENCHMARK_MAIN();
