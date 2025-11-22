#include <declarative_pipeline.hpp>
#include <vector>
#include <iostream>
#include <numeric>
#include <chrono>

int main() {
    using namespace dpb;

    std::cout << "=== Declarative Pipeline Builder Examples ===\n\n";

    // Test 1: Simple transform
    std::cout << "Test 1: Simple transform\n";
    std::vector<int> input1 = {1, 2, 3};
    auto result1 = Pipeline<int,int>::from(input1)
        .transform([](int x) { return x * 2; })
        .collect(input1);

    std::cout << "Input: 1, 2, 3\n";
    std::cout << "Result: ";
    for (int x : result1) std::cout << x << " ";
    std::cout << "\n\n";

    // Test 2: Simple filter
    std::cout << "Test 2: Simple filter\n";
    std::vector<int> input2 = {1, 2, 3, 4, 5};
    auto result2 = Pipeline<int,int>::from(input2)
        .filter([](int x) { return x > 3; })
        .collect(input2);

    std::cout << "Input: 1, 2, 3, 4, 5\n";
    std::cout << "Filtered (> 3): ";
    for (int x : result2) std::cout << x << " ";
    std::cout << "\n\n";

    // Test 3: Filter then transform
    std::cout << "Test 3: Filter then transform\n";
    std::vector<int> input3 = {1, 2, 3, 4, 5};
    auto result3 = Pipeline<int,int>::from(input3)
        .filter([](int x) { return x > 3; })
        .transform([](int x) { return x * 2; })
        .collect(input3);

    std::cout << "Filter (> 3) then transform (* 2): ";
    for (int x : result3) std::cout << x << " ";
    std::cout << "\n\n";

    // Test 4: Transform then filter
    std::cout << "Test 4: Transform then filter\n";
    std::vector<int> input4 = {1, 2, 3, 4, 5, 6};
    auto result4 = Pipeline<int,int>::from(input4)
        .transform([](int x) { return x * 2; })
        .filter([](int x) { return x > 6; })
        .collect(input4);

    std::cout << "Transform (* 2) then filter (> 6): ";
    for (int x : result4) std::cout << x << " ";
    std::cout << "\n\n";

    // Test 5: Parallel execution
    std::cout << "Test 5: Parallel execution\n";
    std::vector<int> input5(10000);
    std::iota(input5.begin(), input5.end(), 0);

    auto start = std::chrono::high_resolution_clock::now();
    auto result5_seq = Pipeline<int,int>::from(input5)
        .filter([](int x) { return x % 2 == 0; })
        .transform([](int x) { return x * 2; })
        .collect(input5);
    auto end_seq = std::chrono::high_resolution_clock::now();

    auto start_par = std::chrono::high_resolution_clock::now();
    auto result5_par = Pipeline<int,int>::from(input5)
        .filter([](int x) { return x % 2 == 0; })
        .transform([](int x) { return x * 2; })
        .parallel(4, ExecutionPolicy::ParallelPreserveOrder)
        .collect(input5);
    auto end_par = std::chrono::high_resolution_clock::now();

    auto seq_time = std::chrono::duration<double, std::milli>(end_seq - start).count();
    auto par_time = std::chrono::duration<double, std::milli>(end_par - start_par).count();

    std::cout << "Sequential time: " << seq_time << " ms\n";
    std::cout << "Parallel time: " << par_time << " ms\n";
    std::cout << "Speedup: " << (seq_time / par_time) << "x\n";
    std::cout << "Results match: " << (result5_seq == result5_par) << "\n\n";

    return 0;
}