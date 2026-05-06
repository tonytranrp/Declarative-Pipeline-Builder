#include <declarative_pipeline.hpp>
#include <vector>
#include <iostream>
#include <numeric>
#include <chrono>
#include <string>

int main() {
    using namespace dpb;

    std::cout << "=== Declarative Pipeline Builder ===\n\n";

    // ── Aliases: map and where ──────────────────────────────────────────
    std::cout << "--- Aliases: map / where ---\n";
    auto v1 = from(std::vector{1, 2, 3, 4, 5})
        .where([](int x) { return x > 2; })
        .map([](int x) { return x * 10; })
        .to_vector(std::vector{1, 2, 3, 4, 5});
    for (int x : v1) std::cout << x << " ";
    std::cout << "\n\n";

    // ── Stream control: take / skip ─────────────────────────────────────
    std::cout << "--- Stream control: take / skip ---\n";
    std::vector<int> numbers(20);
    std::iota(numbers.begin(), numbers.end(), 1);

    auto taken = from(numbers).take(5).to_vector(numbers);
    std::cout << "take(5): ";
    for (int x : taken) std::cout << x << " ";
    std::cout << "\n";

    auto skipped = from(numbers).skip(15).to_vector(numbers);
    std::cout << "skip(15): ";
    for (int x : skipped) std::cout << x << " ";
    std::cout << "\n\n";

    // ── Side effects: tee ───────────────────────────────────────────────
    std::cout << "--- Side effects: tee ---\n";
    int seen = 0;
    auto result = from(numbers)
        .take(6)
        .tee([&seen](int x) { seen++; })
        .collect(numbers);
    std::cout << "Processed " << result.size() << " items, tee observed " << seen << "\n\n";

    // ── Reductions ───────────────────────────────────────────────────────
    std::cout << "--- Reductions ---\n";
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto total = from(data).sum(data);
    std::cout << "sum: " << total << "\n";

    auto evens = from(data)
        .where([](int x) { return x % 2 == 0; })
        .count(data);
    std::cout << "even count: " << evens << "\n";

    auto min_val = from(data).min(data);
    std::cout << "min: " << (min_val.has_value() ? std::to_string(*min_val) : "none") << "\n";

    bool all_positive = from(data).all_of(data, [](int x) { return x > 0; });
    std::cout << "all positive: " << (all_positive ? "true" : "false") << "\n\n";

    // ── flat_map ─────────────────────────────────────────────────────────
    std::cout << "--- flat_map ---\n";
    auto expanded = from(data)
        .take(3)
        .flat_map(data, [](int x) {
            return std::vector<int>{x, x * 100};
        });
    std::cout << "flat_map [1,2,3] -> ";
    for (int x : expanded) std::cout << x << " ";
    std::cout << "\n\n";

    // ── scan ─────────────────────────────────────────────────────────────
    std::cout << "--- scan ---\n";
    auto running = from(data)
        .take(5)
        .scan(data, 0, [](int acc, int x) { return acc + x; });
    std::cout << "running sum: ";
    for (int x : running) std::cout << x << " ";
    std::cout << "\n\n";

    // ── Performance comparison ──────────────────────────────────────────
    std::cout << "--- Performance ---\n";
    std::vector<int> big(100000);
    std::iota(big.begin(), big.end(), 0);

    auto start_seq = std::chrono::high_resolution_clock::now();
    auto seq = from(big)
        .where([](int x) { return x % 2 == 0; })
        .map([](int x) { return x * x; })
        .collect(big);
    auto seq_time = std::chrono::duration<double, std::milli>(
        std::chrono::high_resolution_clock::now() - start_seq).count();

    auto start_par = std::chrono::high_resolution_clock::now();
    auto par = from(big)
        .where([](int x) { return x % 2 == 0; })
        .map([](int x) { return x * x; })
        .parallel(8)
        .collect(big);
    auto par_time = std::chrono::duration<double, std::milli>(
        std::chrono::high_resolution_clock::now() - start_par).count();

    std::cout << "Sequential: " << seq_time << " ms\n";
    std::cout << "Parallel:   " << par_time << " ms\n";
    std::cout << "Speedup:    " << (seq_time / par_time) << "x\n";
    std::cout << "Match:      " << (seq == par ? "yes" : "no") << "\n";

    return 0;
}
