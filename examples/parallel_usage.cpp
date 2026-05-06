#include <declarative_pipeline.hpp>
#include <vector>
#include <iostream>
#include <numeric>
#include <chrono>
#include <algorithm>

int main() {
    using namespace dpb;

    std::cout << "=== Parallel Pipeline Example ===\n\n";

    const size_t N = 1'000'000;
    std::vector<int> data(N);
    std::iota(data.begin(), data.end(), 0);

    // Sequential baseline
    auto t0 = std::chrono::high_resolution_clock::now();
    auto seq = from(data)
        .where([](int x) { return x % 2 == 0; })
        .map([](int x) { return x * x; })
        .filter([](int x) { return x > 0; })
        .collect(data);
    auto seq_ms = std::chrono::duration<double, std::milli>(
        std::chrono::high_resolution_clock::now() - t0).count();

    // Parallel with order preservation
    auto t1 = std::chrono::high_resolution_clock::now();
    auto par_ordered = from(data)
        .where([](int x) { return x % 2 == 0; })
        .map([](int x) { return x * x; })
        .filter([](int x) { return x > 0; })
        .parallel(8, ExecutionPolicy::ParallelPreserveOrder)
        .collect(data);
    auto par_ms = std::chrono::duration<double, std::milli>(
        std::chrono::high_resolution_clock::now() - t1).count();

    // Parallel unordered
    auto t2 = std::chrono::high_resolution_clock::now();
    auto par_unordered = from(data)
        .where([](int x) { return x % 2 == 0; })
        .map([](int x) { return x * x; })
        .filter([](int x) { return x > 0; })
        .parallel(8, ExecutionPolicy::ParallelUnordered)
        .collect(data);
    auto par_unord_ms = std::chrono::duration<double, std::milli>(
        std::chrono::high_resolution_clock::now() - t2).count();

    std::cout << "Dataset: " << N << " integers\n";
    std::cout << "Sequential:            " << seq_ms << " ms\n";
    std::cout << "Parallel (ordered):    " << par_ms << " ms  ("
              << seq_ms / par_ms << "x)\n";
    std::cout << "Parallel (unordered):  " << par_unord_ms << " ms  ("
              << seq_ms / par_unord_ms << "x)\n";

    std::cout << "\nResults match (ordered):  " << (seq == par_ordered ? "yes" : "no") << "\n";
    std::cout << "Result size: " << seq.size() << "\n";

    return 0;
}
