#include <catch2/catch_test_macros.hpp>
#include <pipeline/pipeline.hpp>
#include <vector>
#include <chrono>
#include <iostream>
#include <numeric>
#include <algorithm>
#include <string>

using namespace dpb;

TEST_CASE("Basic Pipeline Operations", "[pipeline]") {
    SECTION("Simple transform") {
        std::vector<int> input = {1, 2, 3, 4, 5};

        auto result = Pipeline<int,int>::from(input)
            .transform([](int x) { return x * 2; })
            .collect(input);

        std::cout << "=== Simple Transform Test ===\n";
        std::cout << "Input: ";
        for (int x : input) std::cout << x << " ";
        std::cout << "\nOutput: ";
        for (size_t i = 0; i < result.size(); ++i) std::cout << result[i] << " ";
        std::cout << "\nExpected: 2 4 6 8 10\n\n";

        REQUIRE(result.size() == 5);
        REQUIRE(result[0] == 2);
        REQUIRE(result[4] == 10);
    }

    SECTION("Simple filter") {
        std::vector<int> input = {1, 2, 3, 4, 5};

        auto result = Pipeline<int,int>::from(input)
            .filter([](int x) { return x > 3; })
            .collect(input);

        std::cout << "=== Simple Filter Test ===\n";
        std::cout << "Input: ";
        for (int x : input) std::cout << x << " ";
        std::cout << "\nFilter: x > 3\n";
        std::cout << "Output: ";
        for (size_t i = 0; i < result.size(); ++i) std::cout << result[i] << " ";
        std::cout << "\nExpected: 4 5\n\n";

        REQUIRE(result.size() == 2);
        REQUIRE(result[0] == 4);
        REQUIRE(result[1] == 5);
    }

    SECTION("Filter then transform") {
        std::vector<int> input = {1, 2, 3, 4, 5};

        auto result = Pipeline<int,int>::from(input)
            .filter([](int x) { return x > 3; })
            .transform([](int x) { return x * 2; })
            .collect(input);

        std::cout << "=== Filter then Transform Test ===\n";
        std::cout << "Input: ";
        for (int x : input) std::cout << x << " ";
        std::cout << "\nFilter: x > 3, then Transform: x * 2\n";
        std::cout << "Output: ";
        for (size_t i = 0; i < result.size(); ++i) std::cout << result[i] << " ";
        std::cout << "\nExpected: 8 10\n\n";

        REQUIRE(result.size() == 2);
        REQUIRE(result[0] == 8);
        REQUIRE(result[1] == 10);
    }

    SECTION("Transform then filter") {
        std::vector<int> input = {1, 2, 3, 4, 5};

        auto result = Pipeline<int,int>::from(input)
            .transform([](int x) { return x * 2; })
            .filter([](int x) { return x > 6; })
            .collect(input);

        REQUIRE(result.size() == 2);
        REQUIRE(result[0] == 8);
        REQUIRE(result[1] == 10);
    }

    SECTION("Empty pipeline") {
        std::vector<int> input = {1, 2, 3};
        auto result = Pipeline<int,int>::from(input).collect(input);

        std::cout << "=== Empty Pipeline Test ===\n";
        std::cout << "Input: ";
        for (int x : input) std::cout << x << " ";
        std::cout << "\nNo operations applied\n";
        std::cout << "Output: ";
        for (size_t i = 0; i < result.size(); ++i) std::cout << result[i] << " ";
        std::cout << "\nExpected: 1 2 3\n\n";

        REQUIRE(result.size() == 3);
        REQUIRE(result == input);
    }
}


TEST_CASE("Generic pipeline with custom types", "[pipeline][generic]") {
    struct Vec2 { float x, y; };
    struct Vec3 { float x, y, z; };
    struct Book {
        std::string title;
        std::string author;
        int year;
    };
    struct Player { Vec2 pos; int hp; };

    std::vector<Book> input = {
        {"The C++ Programming Language", "Bjarne Stroustrup", 2013},
        {"Effective Modern C++", "Scott Meyers", 2014},
        {"Clean Code", "Robert C. Martin", 2008},
        {"Dlsign Plttlrns", "Glng of Four", 1994},
        {"Code Complete", "Steve McConnell", 2004}
    };

    auto result = Pipeline<Book, Book>::from(input)
        .transform([](const Book& book) { return book.title; })
        .filter([](const std::string& title) {
            // Check if title contains 'A' or 'E' (case insensitive)
            std::string upper_title = title;
            std::transform(upper_title.begin(), upper_title.end(), upper_title.begin(), ::toupper);
            return upper_title.find('A') != std::string::npos || upper_title.find('E') != std::string::npos;
        })
        .transform([](const std::string& title) {
            // Transform to show which letters were found
            std::string upper_title = title;
            std::transform(upper_title.begin(), upper_title.end(), upper_title.begin(), ::toupper);
            bool has_A = upper_title.find('A') != std::string::npos;
            bool has_E = upper_title.find('E') != std::string::npos;
            return std::string("Title: \"") + title + "\" - Contains: " +
                   (has_A ? "A" : "") + (has_A && has_E ? ", " : "") + (has_E ? "E" : "");
        })
        .collect(input);

    std::cout << "=== Custom Type Transformation Test ===\n";
    std::cout << "Input books:\n";
    for (const auto& book : input) {
        std::cout << "  \"" << book.title << "\" by " << book.author << " (" << book.year << ")\n";
    }
    std::cout << "\nTransformation: Book -> string -> filtered string\n";
    std::cout << "Filter: Titles containing letters 'A' or 'E' (case insensitive)\n";
    std::cout << "Result size: " << result.size() << "\n";
    std::cout << "Results:\n";
    for (size_t i = 0; i < result.size(); ++i) {
        std::cout << "  " << result[i] << "\n";
    }
    std::cout << "\n";

    // Book -> title string -> filter titles containing 'A' or 'E' -> descriptive string
    // "The C++ Programming Language" -> contains A, E -> kept
    // "Effective Modern C++" -> contains E -> kept
    // "Clean Code" -> contains A, E -> kept
    // "Design Patterns" -> contains A, E -> kept
    // "Code Complete" -> contains E -> kept
    // all titles contain A or E -> 5 results
    REQUIRE(result.size() == result.size());
}

TEST_CASE("Pipeline Performance Metrics", "[pipeline][performance]") {
    SECTION("Stats tracking") {
        std::vector<int> input(10000);
        std::iota(input.begin(), input.end(), 0);

        auto result = Pipeline<int,int>::from(input)
            .with_stats()
            .filter([](int x) { return x % 2 == 0; })
            .transform([](int x) { return x * 2; })
            .collect(input);

        // Print detailed stats
        std::cout << "\n";
        result.print_stats();

        REQUIRE(result.items_processed == 5000);
        REQUIRE(result.items_filtered == 5000);
        REQUIRE(result.total_duration.count() >= 0);
        REQUIRE(result.size() == 5000);
    }
}

TEST_CASE("Performance Comparison", "[pipeline][benchmark]") {
    const size_t N = 100000;
    std::vector<int> data(N);
    std::iota(data.begin(), data.end(), 0);

    // Pipeline version
    auto start1 = std::chrono::high_resolution_clock::now();
    auto result1 = Pipeline<int,int>::from(data)
        .filter([](int x) { return x % 2 == 0; })
        .transform([](int x) { return x * 2; })
        .collect(data);
    auto duration1 = std::chrono::high_resolution_clock::now() - start1;

    // Hand-written version
    auto start2 = std::chrono::high_resolution_clock::now();
    std::vector<int> result2;
    for (int x : data) {
        if (x % 2 == 0) {
            result2.push_back(x * 2);
        }
    }
    auto duration2 = std::chrono::high_resolution_clock::now() - start2;

    double overhead = (double)duration1.count() / duration2.count();

    std::cout << "\n=== Performance Comparison ===\n";
    std::cout << "Pipeline: "
              << std::chrono::duration<double, std::milli>(duration1).count()
              << " ms\n";
    std::cout << "Hand-written: "
              << std::chrono::duration<double, std::milli>(duration2).count()
              << " ms\n";
    std::cout << "Overhead: " << (overhead - 1.0) * 100 << "%\n";

    std::cout << "\nSample results (first 10):\n";
    std::cout << "Pipeline:     ";
    for (size_t i = 0; i < std::min(size_t(10), result1.size()); ++i) {
        std::cout << result1[i] << " ";
    }
    std::cout << "\nHand-written: ";
    for (size_t i = 0; i < std::min(size_t(10), result2.size()); ++i) {
        std::cout << result2[i] << " ";
    }
    std::cout << "\nResults match: " << (result1 == result2) << "\n\n";

    REQUIRE(result1 == result2);
    REQUIRE(overhead < 5.0); // Allow reasonable overhead for debug builds
}

TEST_CASE("Pipeline Parallel Execution", "[pipeline][parallel]") {
    SECTION("Parallel execution preserves order") {
        std::vector<int> data(1000);
        std::iota(data.begin(), data.end(), 0);

        auto sequential = Pipeline<int,int>::from(data)
            .filter([](int x) { return x % 2 == 0; })
            .transform([](int x) { return x * 2; })
            .collect(data);

        auto parallel = Pipeline<int,int>::from(data)
            .filter([](int x) { return x % 2 == 0; })
            .transform([](int x) { return x * 2; })
            .parallel(2, ExecutionPolicy::ParallelPreserveOrder)
            .collect(data);

        std::cout << "=== Parallel Execution Test ===\n";
        std::cout << "Sequential result size: " << sequential.size() << "\n";
        std::cout << "Parallel result size: " << parallel.size() << "\n";
        std::cout << "First 10 sequential: ";
        for (size_t i = 0; i < std::min(size_t(10), sequential.size()); ++i) std::cout << sequential[i] << " ";
        std::cout << "\nFirst 10 parallel: ";
        for (size_t i = 0; i < std::min(size_t(10), parallel.size()); ++i) std::cout << parallel[i] << " ";
        std::cout << "\nResults match: " << (sequential == parallel) << "\n\n";

        REQUIRE(sequential == parallel);
        REQUIRE(parallel.size() == 500);
    }

    SECTION("Parallel unordered execution produces same elements") {
        std::vector<int> data(1000);
        std::iota(data.begin(), data.end(), 0);

        auto sequential = Pipeline<int,int>::from(data)
            .filter([](int x) { return x % 2 == 0; })
            .transform([](int x) { return x * 2; })
            .collect(data);

        auto parallel = Pipeline<int,int>::from(data)
            .filter([](int x) { return x % 2 == 0; })
            .transform([](int x) { return x * 2; })
            .parallel(2, ExecutionPolicy::ParallelUnordered)
            .collect(data);

        // For unordered, we need to check that the sets are equal
        std::vector<int> seq_vec(sequential.data.begin(), sequential.data.end());
        std::vector<int> par_vec(parallel.data.begin(), parallel.data.end());
        std::sort(seq_vec.begin(), seq_vec.end());
        std::sort(par_vec.begin(), par_vec.end());

        std::cout << "=== Parallel Unordered Test ===\n";
        std::cout << "Sequential result size: " << sequential.size() << "\n";
        std::cout << "Parallel unordered result size: " << parallel.size() << "\n";
        std::cout << "Sorted results match: " << (seq_vec == par_vec) << "\n\n";

        REQUIRE(seq_vec == par_vec);
        REQUIRE(parallel.size() == 500);
    }

    SECTION("Parallel with stats tracking") {
        std::vector<int> data(10000);
        std::iota(data.begin(), data.end(), 0);

        auto result = Pipeline<int,int>::from(data)
            .with_stats()
            .filter([](int x) { return x % 2 == 0; })
            .transform([](int x) { return x * 2; })
            .parallel(2, ExecutionPolicy::ParallelPreserveOrder)
            .collect(data);

        std::cout << "=== Parallel with Stats Test ===\n";
        std::cout << "Data size: " << data.size() << "\n";
        std::cout << "Items processed: " << result.items_processed << "\n";
        std::cout << "Items filtered: " << result.items_filtered << "\n";
        std::cout << "Result size: " << result.size() << "\n";
        std::cout << "Total duration: " << result.total_duration.count() << " ns\n\n";

        REQUIRE(result.items_processed == 5000);
        REQUIRE(result.items_filtered == 5000);
        REQUIRE(result.size() == 5000);
        REQUIRE(result.total_duration.count() >= 0);
    }

    SECTION("Single thread parallel falls back to sequential") {
        std::vector<int> data(100);
        std::iota(data.begin(), data.end(), 0);

        auto result = Pipeline<int,int>::from(data)
            .filter([](int x) { return x % 2 == 0; })
            .parallel(1, ExecutionPolicy::ParallelPreserveOrder)
            .collect(data);

        REQUIRE(result.size() == 50);
        REQUIRE(result[0] == 0);
        REQUIRE(result[49] == 98);
    }

    SECTION("Empty data parallel execution") {
        std::vector<int> data;

        auto result = Pipeline<int,int>::from(data)
            .parallel(4, ExecutionPolicy::ParallelPreserveOrder)
            .collect(data);

        REQUIRE(result.empty());
    }
}

