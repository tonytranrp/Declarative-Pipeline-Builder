#include <catch2/catch_test_macros.hpp>
#include <pipeline/pipeline.hpp>
#include <vector>
#include <numeric>
#include <string>

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
