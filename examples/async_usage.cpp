#include <declarative_pipeline.hpp>
#include <vector>
#include <iostream>
#include <numeric>
#include <future>
#include <chrono>

int main() {
    using namespace dpb;

    std::cout << "=== Async Pipeline Example ===\n\n";

    std::vector<int> data(50000);
    std::iota(data.begin(), data.end(), 0);

    // Launch multiple pipelines asynchronously
    auto fut1 = std::async(std::launch::async, [&data]() {
        return from(data)
            .where([](int x) { return x % 2 == 0; })
            .map([](int x) { return x * x; })
            .with_stats()
            .collect(data);
    });

    auto fut2 = std::async(std::launch::async, [&data]() {
        return from(data)
            .where([](int x) { return x % 3 == 0; })
            .map([](int x) { return x * 10; })
            .collect(data);
    });

    auto r1 = fut1.get();
    auto r2 = fut2.get();

    std::cout << "Pipeline 1 (even squares): " << r1.size() << " results\n";
    r1.print_stats();

    std::cout << "\nPipeline 2 (multiples of 3 * 10): " << r2.size() << " results\n";

    return 0;
}
