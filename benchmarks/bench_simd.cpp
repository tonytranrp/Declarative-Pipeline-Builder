#include <benchmark/benchmark.h>
#include <declarative_pipeline.hpp>

#ifdef DPB_HAS_HIGHWAY
#include <dpb/simd.hpp>
#include <dpb/simd_filter.hpp>
#include <dpb/simd_transform.hpp>
#endif

#include <vector>
#include <numeric>

static void BM_Pipeline_Filter_Transform(benchmark::State& state) {
    std::vector<int> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);

    for (auto _ : state) {
        auto result = dpb::from(data)
            .where([](int x) { return x % 2 == 0; })
            .map([](int x) { return x * x; })
            .to_vector(data);
        benchmark::DoNotOptimize(result.data());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

#ifdef DPB_HAS_HIGHWAY
static void BM_SIMD_Filter(benchmark::State& state) {
    std::vector<int> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);

    for (auto _ : state) {
        auto result = dpb::simd_filter(data, [](int x) { return x % 2 == 0; });
        benchmark::DoNotOptimize(result.data());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

static void BM_SIMD_Transform(benchmark::State& state) {
    std::vector<float> data(state.range(0));
    for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<float>(i);

    for (auto _ : state) {
        auto result = dpb::simd_transform(data, [](float x) { return x * 2.0f + 1.0f; });
        benchmark::DoNotOptimize(result.data());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

BENCHMARK(BM_SIMD_Filter)->Arg(10000)->Arg(100000)->Arg(1000000);
BENCHMARK(BM_SIMD_Transform)->Arg(10000)->Arg(100000)->Arg(1000000);
#endif

BENCHMARK(BM_Pipeline_Filter_Transform)->Arg(10000)->Arg(100000)->Arg(1000000);

BENCHMARK_MAIN();