#include <catch2/catch_test_macros.hpp>
#include <pipeline/pipeline.hpp>
#include <vector>
#include <numeric>
#include <string>
#include <memory_resource>

using namespace dpb;

TEST_CASE("Enumerate operation", "[new][enumerate]") {
    std::vector<int> input = {10, 20, 30};
    auto result = dpb::from(input)
        .enumerate()
        .collect(input);

    REQUIRE(result.size() == 3);
    REQUIRE(result[0].first == 0);
    REQUIRE(result[0].second == 10);
    REQUIRE(result[1].first == 1);
    REQUIRE(result[1].second == 20);
    REQUIRE(result[2].first == 2);
    REQUIRE(result[2].second == 30);
}

TEST_CASE("Enumerate with filter", "[new][enumerate]") {
    std::vector<int> input = {1, 2, 3, 4, 5};
    auto result = dpb::from(input)
        .where([](int x) { return x % 2 == 0; })
        .enumerate()
        .collect(input);

    REQUIRE(result.size() == 2);
    REQUIRE(result[0].first == 0);
    REQUIRE(result[0].second == 2);
    REQUIRE(result[1].first == 1);
    REQUIRE(result[1].second == 4);
}

TEST_CASE("Unique operation", "[new][unique]") {
    std::vector<int> input = {1, 1, 2, 2, 3, 3, 1};
    auto result = dpb::from(input)
        .unique()
        .collect(input);

    REQUIRE(result.size() == 4);
    REQUIRE(result[0] == 1);
    REQUIRE(result[1] == 2);
    REQUIRE(result[2] == 3);
    REQUIRE(result[3] == 1);
}

TEST_CASE("Dedup is alias for unique", "[new][dedup]") {
    std::vector<int> input = {5, 5, 5, 6, 6, 7};
    auto result = dpb::from(input)
        .dedup()
        .collect(input);

    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == 5);
    REQUIRE(result[1] == 6);
    REQUIRE(result[2] == 7);
}

TEST_CASE("Chunk operation", "[new][chunk]") {
    std::vector<int> input = {1, 2, 3, 4, 5};
    auto result = dpb::from(input)
        .chunk(input, 2);

    REQUIRE(result.size() == 3);
    REQUIRE(result[0].size() == 2);
    REQUIRE(result[0][0] == 1);
    REQUIRE(result[0][1] == 2);
    REQUIRE(result[1][0] == 3);
    REQUIRE(result[1][1] == 4);
    REQUIRE(result[2].size() == 1);
    REQUIRE(result[2][0] == 5);
}

TEST_CASE("Chunk with filter", "[new][chunk]") {
    std::vector<int> input = {1, 2, 3, 4, 5, 6};
    auto result = dpb::from(input)
        .where([](int x) { return x % 2 == 0; })
        .chunk(input, 2);

    // Evens: {2, 4, 6} -> chunks: [{2,4}, {6}] = 2 chunks
    REQUIRE(result.size() == 2);
    REQUIRE(result[0].size() == 2);
    REQUIRE(result[0][0] == 2);
    REQUIRE(result[0][1] == 4);
    REQUIRE(result[1].size() == 1);
    REQUIRE(result[1][0] == 6);
}

TEST_CASE("Window operation", "[new][window]") {
    std::vector<int> input = {1, 2, 3, 4, 5};
    auto result = dpb::from(input)
        .window(input, 3);

    REQUIRE(result.size() == 3);
    REQUIRE(result[0].size() == 3);
    REQUIRE(result[0][0] == 1);
    REQUIRE(result[0][2] == 3);
    REQUIRE(result[1][0] == 2);
    REQUIRE(result[1][2] == 4);
    REQUIRE(result[2][0] == 3);
    REQUIRE(result[2][2] == 5);
}

TEST_CASE("Group by operation", "[new][group_by]") {
    std::vector<int> input = {1, 1, 2, 2, 1, 3};
    auto result = dpb::from(input)
        .group_by(input, [](int x) { return x; });

    REQUIRE(result.size() == 4);
    REQUIRE(result[0].first == 1);
    REQUIRE(result[0].second.size() == 2);
    REQUIRE(result[1].first == 2);
    REQUIRE(result[1].second.size() == 2);
    REQUIRE(result[2].first == 1);
    REQUIRE(result[2].second.size() == 1);
    REQUIRE(result[3].first == 3);
    REQUIRE(result[3].second.size() == 1);
}

TEST_CASE("Reverse operation", "[new][reverse]") {
    std::vector<int> input = {1, 2, 3, 4, 5};
    auto result = dpb::from(input)
        .reverse(input);

    REQUIRE(result.size() == 5);
    REQUIRE(result[0] == 5);
    REQUIRE(result[4] == 1);
}

TEST_CASE("For each operation", "[new][for_each]") {
    std::vector<int> input = {1, 2, 3};
    int sum = 0;
    dpb::from(input)
        .for_each(input, [&sum](int x) { sum += x; });
    REQUIRE(sum == 6);
}

TEST_CASE("For each with filter", "[new][for_each]") {
    std::vector<int> input = {1, 2, 3, 4, 5};
    int sum = 0;
    dpb::from(input)
        .where([](int x) { return x % 2 == 0; })
        .for_each(input, [&sum](int x) { sum += x; });
    REQUIRE(sum == 6); // 2 + 4
}

TEST_CASE("First operation", "[new][first]") {
    std::vector<int> input = {1, 2, 3, 4, 5};
    auto result = dpb::from(input)
        .where([](int x) { return x > 3; })
        .first(input);

    REQUIRE(result.has_value());
    REQUIRE(*result == 4);
}

TEST_CASE("First returns nullopt for no match", "[new][first]") {
    std::vector<int> input = {1, 2, 3};
    auto result = dpb::from(input)
        .where([](int x) { return x > 10; })
        .first(input);

    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("Last operation", "[new][last]") {
    std::vector<int> input = {1, 2, 3, 4, 5};
    auto result = dpb::from(input)
        .where([](int x) { return x > 2; })
        .last(input);

    REQUIRE(result.has_value());
    REQUIRE(*result == 5);
}

TEST_CASE("Last returns nullopt for no match", "[new][last]") {
    std::vector<int> input = {1, 2, 3};
    auto result = dpb::from(input)
        .where([](int x) { return x > 10; })
        .last(input);

    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("Combined new features", "[new][combined]") {
    // enumerate + filter + collect
    std::vector<int> input = {10, 20, 30, 40, 50};
    auto result = dpb::from(input)
        .where([](int x) { return x > 25; })
        .enumerate()
        .collect(input);

    REQUIRE(result.size() == 3);
    REQUIRE(result[0].first == 0);
    REQUIRE(result[0].second == 30);
    REQUIRE(result[2].first == 2);
    REQUIRE(result[2].second == 50);
}

// ============================================================================
// CUSTOM ALLOCATOR TESTS
// ============================================================================

TEST_CASE("Default allocator is transparent", "[allocator][default]") {
    // Pipeline<int,int,DefaultOp,std::allocator<int>> should work identically
    // to Pipeline<int,int> (which defaults to std::allocator<int>)
    std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Default pipeline (no explicit allocator)
    auto result_default = dpb::from(input)
        .where([](int x) { return x % 2 == 0; })
        .transform([](int x) { return x * x; })
        .collect(input);

    // Pipeline with explicit std::allocator<int>
    auto is_even = [](int x) { return x % 2 == 0; };
    auto square = [](int x) { return x * x; };
    dpb::Pipeline<int, int, dpb::DefaultOp, std::allocator<int>> pipe;
    auto pipe2 = std::move(pipe).filter(is_even).transform(square);
    auto result_explicit = std::move(pipe2).collect(input);

    REQUIRE(result_default.size() == result_explicit.size());
    for (size_t i = 0; i < result_default.size(); ++i) {
        REQUIRE(result_default[i] == result_explicit[i]);
    }
}

TEST_CASE("Pipeline with std::allocator compiles and works", "[allocator][std]") {
    std::vector<int> input = {1, 2, 3, 4, 5};

    dpb::Pipeline<int, int, dpb::DefaultOp, std::allocator<int>> pipe;
    auto result = std::move(pipe)
        .filter([](int x) { return x > 2; })
        .collect(input);

    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == 3);
    REQUIRE(result[1] == 4);
    REQUIRE(result[2] == 5);
}

TEST_CASE("Pipeline with PMR allocator compiles and produces correct results", "[allocator][pmr]") {
    // Use pmr::polymorphic_allocator with default resource
    std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    using PmrAlloc = std::pmr::polymorphic_allocator<int>;

    dpb::Pipeline<int, int, dpb::DefaultOp, PmrAlloc> pipe;
    auto result = std::move(pipe)
        .where([](int x) { return x % 2 == 0; })
        .transform([](int x) { return x * x; })
        .collect(input);

    // Verify correctness: even numbers squared: 4, 16, 36, 64, 100
    REQUIRE(result.size() == 5);
    REQUIRE(result[0] == 4);
    REQUIRE(result[1] == 16);
    REQUIRE(result[2] == 36);
    REQUIRE(result[3] == 64);
    REQUIRE(result[4] == 100);
}

TEST_CASE("Pipeline with PMR allocator uses custom memory resource", "[allocator][pmr]") {
    // Use pmr::polymorphic_allocator with a monotonic_buffer_resource
    std::pmr::monotonic_buffer_resource mbr(1024);
    std::pmr::polymorphic_allocator<int> alloc(&mbr);

    using PmrAlloc = std::pmr::polymorphic_allocator<int>;

    // Create pipeline with PMR allocator passed through constructor
    dpb::Pipeline<int, int, dpb::DefaultOp, PmrAlloc> pipe(
        dpb::DefaultOp{}, nullptr, nullptr,
        dpb::ExecutionPolicy::Sequential, 1, alloc);

    std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto result = std::move(pipe)
        .where([](int x) { return x % 2 == 0; })
        .collect(input);

    REQUIRE(result.size() == 5);
    REQUIRE(result[0] == 2);
    REQUIRE(result[1] == 4);
    REQUIRE(result[2] == 6);
    REQUIRE(result[3] == 8);
    REQUIRE(result[4] == 10);

    // Verify the result data uses PMR allocator
    static_assert(std::same_as<decltype(result.data)::allocator_type, PmrAlloc>,
                  "Result vector should use PMR allocator");
}

TEST_CASE("Pipeline allocator propagates through filter chain", "[allocator][propagation]") {
    std::vector<int> input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    using PmrAlloc = std::pmr::polymorphic_allocator<int>;

    dpb::Pipeline<int, int, dpb::DefaultOp, PmrAlloc> pipe;
    auto result = std::move(pipe)
        .where([](int x) { return x > 3; })
        .where([](int x) { return x < 8; })
        .collect(input);

    REQUIRE(result.size() == 4); // 4, 5, 6, 7
    REQUIRE(result[0] == 4);
    REQUIRE(result[1] == 5);
    REQUIRE(result[2] == 6);
    REQUIRE(result[3] == 7);
}

TEST_CASE("Pipeline allocator_type alias is correct", "[allocator][type]") {
    using DefaultPipe = dpb::Pipeline<int, int>;
    using PmrPipe = dpb::Pipeline<int, int, dpb::DefaultOp, std::pmr::polymorphic_allocator<int>>;

    static_assert(std::same_as<DefaultPipe::allocator_type, std::allocator<int>>,
                  "Default allocator_type should be std::allocator<int>");
    static_assert(std::same_as<PmrPipe::allocator_type, std::pmr::polymorphic_allocator<int>>,
                  "PMR allocator_type should be std::pmr::polymorphic_allocator<int>");
}

TEST_CASE("collect_buffer with custom allocator", "[allocator][collect_buffer]") {
    // Test collect_buffer directly with PMR allocator
    std::pmr::monotonic_buffer_resource mbr(256);
    std::pmr::polymorphic_allocator<int> alloc(&mbr);

    dpb::collect_buffer<int, std::pmr::polymorphic_allocator<int>> buf(0, alloc);
    buf.push_back(1);
    buf.push_back(2);
    buf.push_back(3);

    REQUIRE(buf.size() == 3);

    auto vec = std::move(buf).operator std::vector<int, std::pmr::polymorphic_allocator<int>>();
    REQUIRE(vec.size() == 3);
    REQUIRE(vec[0] == 1);
    REQUIRE(vec[1] == 2);
    REQUIRE(vec[2] == 3);
}

TEST_CASE("ResultWithStats with custom allocator", "[allocator][result]") {
    // Test ResultWithStats with PMR allocator
    std::pmr::monotonic_buffer_resource mbr(256);
    std::pmr::polymorphic_allocator<int> alloc(&mbr);

    std::pmr::vector<int> data(alloc);
    data.push_back(10);
    data.push_back(20);
    data.push_back(30);

    dpb::ResultWithStats<int, std::pmr::polymorphic_allocator<int>> result(
        std::move(data), 3, 0, 0, 3, std::chrono::nanoseconds{100}
    );

    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == 10);
    REQUIRE(result[1] == 20);
    REQUIRE(result[2] == 30);
    REQUIRE(result.items_processed == 3);
}
