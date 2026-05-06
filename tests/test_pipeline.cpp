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
    // "Design Patterns" and "Code Complete" contain 'E', others contain 'A' or 'E'
    // 5 books total, all should pass the filter
    REQUIRE(result.size() == 5);
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

TEST_CASE("DSL Operations - Aliases", "[dsl]") {
    SECTION("map is alias for transform") {
        std::vector<int> input = {1, 2, 3};
        auto result = dpb::from(input)
            .map([](int x) { return x * 10; })
            .collect(input);
        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == 10);
        REQUIRE(result[2] == 30);
    }

    SECTION("where is alias for filter") {
        std::vector<int> input = {1, 2, 3, 4, 5};
        auto result = dpb::from(input)
            .where([](int x) { return x > 3; })
            .collect(input);
        REQUIRE(result.size() == 2);
        REQUIRE(result[0] == 4);
        REQUIRE(result[1] == 5);
    }
}

TEST_CASE("DSL Operations - Stream Control", "[dsl]") {
    SECTION("take limits output count") {
        std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto result = dpb::from(input)
            .take(3)
            .collect(input);
        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == 1);
        REQUIRE(result[2] == 3);
    }

    SECTION("skip discards first N items") {
        std::vector<int> input = {1, 2, 3, 4, 5};
        auto result = dpb::from(input)
            .skip(2)
            .collect(input);
        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == 3);
        REQUIRE(result[2] == 5);
    }

    SECTION("take_while stops when predicate fails") {
        std::vector<int> input = {1, 2, 3, 10, 4, 5};
        auto result = dpb::from(input)
            .take_while([](int x) { return x < 10; })
            .collect(input);
        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == 1);
        REQUIRE(result[2] == 3);
    }

    SECTION("skip_while skips until predicate fails") {
        std::vector<int> input = {1, 3, 5, 8, 7, 9};
        auto result = dpb::from(input)
            .skip_while([](int x) { return x % 2 == 1; })
            .collect(input);
        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == 8);
    }
}

TEST_CASE("DSL Operations - Side Effects", "[dsl]") {
    SECTION("tee observes items without modifying") {
        std::vector<int> input = {1, 2, 3};
        int sum = 0;
        auto result = dpb::from(input)
            .tee([&sum](int x) { sum += x; })
            .collect(input);
        REQUIRE(sum == 6);
        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == 1);
    }

    SECTION("inspect is alias for tee") {
        std::vector<int> input = {1, 2, 3};
        int count = 0;
        auto result = dpb::from(input)
            .inspect([&count](int) { count++; })
            .collect(input);
        REQUIRE(count == 3);
    }
}

TEST_CASE("DSL Operations - Reductions", "[dsl]") {
    SECTION("fold accumulates values") {
        std::vector<int> input = {1, 2, 3, 4, 5};
        auto result = dpb::from(input)
            .fold(input, 0, [](int acc, int x) { return acc + x; });
        REQUIRE(result == 15);
    }

    SECTION("sum adds all values") {
        std::vector<int> input = {1, 2, 3, 4, 5};
        auto result = dpb::from(input).sum(input);
        REQUIRE(result == 15);
    }

    SECTION("count returns number of items") {
        std::vector<int> input(100);
        std::iota(input.begin(), input.end(), 0);
        auto result = dpb::from(input)
            .filter([](int x) { return x % 2 == 0; })
            .count(input);
        REQUIRE(result == 50);
    }

    SECTION("min returns minimum value") {
        std::vector<int> input = {5, 3, 8, 1, 4};
        auto result = dpb::from(input).min(input);
        REQUIRE(result.has_value());
        REQUIRE(*result == 1);
    }

    SECTION("max returns maximum value") {
        std::vector<int> input = {5, 3, 8, 1, 4};
        auto result = dpb::from(input).max(input);
        REQUIRE(result.has_value());
        REQUIRE(*result == 8);
    }

    SECTION("all_of checks if all items satisfy predicate") {
        std::vector<int> input = {2, 4, 6, 8};
        REQUIRE(dpb::from(input).all_of(input, [](int x) { return x % 2 == 0; }));
        REQUIRE_FALSE(dpb::from(input).all_of(input, [](int x) { return x > 5; }));
    }

    SECTION("any_of checks if any item satisfies predicate") {
        std::vector<int> input = {1, 3, 5, 6};
        REQUIRE(dpb::from(input).any_of(input, [](int x) { return x % 2 == 0; }));
        REQUIRE_FALSE(dpb::from(input).any_of(input, [](int x) { return x > 10; }));
    }

    SECTION("none_of checks if no items satisfy predicate") {
        std::vector<int> input = {1, 3, 5, 7};
        REQUIRE(dpb::from(input).none_of(input, [](int x) { return x % 2 == 0; }));
        REQUIRE_FALSE(dpb::from(input).none_of(input, [](int x) { return x < 5; }));
    }
}

TEST_CASE("DSL Operations - Convenience Collectors", "[dsl]") {
    SECTION("to_vector returns plain vector") {
        std::vector<int> input = {1, 2, 3};
        auto result = dpb::from(input)
            .map([](int x) { return x * 2; })
            .to_vector(input);
        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == 2);
        REQUIRE(result[2] == 6);
    }

    SECTION("collect_sorted returns sorted result") {
        std::vector<int> input = {3, 1, 4, 1, 5, 9};
        auto result = dpb::from(input)
            .collect_sorted(input);
        REQUIRE(result.size() == 6);
        REQUIRE(result[0] == 1);
        REQUIRE(result[5] == 9);
    }

    SECTION("collect_distinct returns unique elements") {
        std::vector<int> input = {3, 1, 4, 1, 5, 9, 2, 6, 5};
        auto result = dpb::from(input)
            .collect_distinct(input);
        REQUIRE(result.size() == 8);
    }

    SECTION("collect_into writes to output iterator") {
        std::vector<int> input = {1, 2, 3, 4, 5};
        std::vector<int> output;
        dpb::from(input)
            .filter([](int x) { return x % 2 == 0; })
            .collect_into(input, std::back_inserter(output));
        REQUIRE(output.size() == 2);
        REQUIRE(output[0] == 2);
        REQUIRE(output[1] == 4);
    }
}

TEST_CASE("DSL Operations - flat_map and scan", "[dsl]") {
    SECTION("flat_map expands each item") {
        std::vector<int> input = {1, 2, 3};
        auto result = dpb::from(input)
            .flat_map(input, [](int x) {
                return std::vector<int>{x, x * 10};
            });
        REQUIRE(result.size() == 6);
        REQUIRE(result[0] == 1);
        REQUIRE(result[1] == 10);
        REQUIRE(result[2] == 2);
        REQUIRE(result[3] == 20);
    }

    SECTION("scan produces running accumulation") {
        std::vector<int> input = {1, 2, 3, 4, 5};
        auto result = dpb::from(input)
            .scan(input, 0, [](int acc, int x) { return acc + x; });
        REQUIRE(result.size() == 5);
        REQUIRE(result[0] == 1);
        REQUIRE(result[1] == 3);
        REQUIRE(result[2] == 6);
        REQUIRE(result[3] == 10);
        REQUIRE(result[4] == 15);
    }
}

TEST_CASE("Free function dpb::from", "[dsl]") {
    SECTION("deduces types from range") {
        std::vector<int> input = {1, 2, 3};
        auto result = dpb::from(input)
            .map([](int x) { return x * 100; })
            .where([](int x) { return x > 100; })
            .collect(input);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == 200);
    }

    SECTION("works with custom types") {
        struct Item { std::string name; };
        std::vector<Item> items = {{"a"}, {"bb"}, {"ccc"}};
        auto result = dpb::from(items)
            .transform([](const Item& i) { return i.name; })
            .where([](const std::string& s) { return s.size() > 1; })
            .collect(items);
        REQUIRE(result.size() == 2);
    }
}

