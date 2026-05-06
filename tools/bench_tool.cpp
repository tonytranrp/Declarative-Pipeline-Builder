#include <iostream>
#include <iomanip>
#include <vector>
#include <numeric>
#include <declarative_pipeline.hpp>

void run_benchmark(size_t data_size) {
    std::vector<int> data(data_size);
    std::iota(data.begin(), data.end(), 0);

    std::cout << "\n=== Benchmarking with " << data_size << " items ===\n";

    auto result = dpb::from(data)
        .with_stats()
        .where([](int x) { return x % 2 == 0; })
        .map([](int x) { return x * x; })
        .where([](int x) { return x < 100000; })
        .collect(data);

    result.print_stats();
    std::cout << "Result size: " << result.size() << "\n";
}

int main(int argc, char* argv[]) {
    std::cout << "=== Pipeline Benchmark Tool ===\n";

    if (argc > 1) {
        size_t size = std::stoull(argv[1]);
        run_benchmark(size);
    } else {
        for (size_t size : {1000, 10000, 100000, 1000000}) {
            run_benchmark(size);
        }
    }

    return 0;
}
