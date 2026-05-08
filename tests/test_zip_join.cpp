#include <catch2/catch_test_macros.hpp>
#include <pipeline/pipeline.hpp>
#include <vector>
#include <string>
#include <utility>

using namespace dpb;

// ============================================================================
// ZIP TESTS
// ============================================================================

TEST_CASE("Zip pairs elements from two ranges", "[zip]") {
    std::vector<int> data = {1, 2, 3};
    std::vector<std::string> other = {"a", "b", "c"};

    auto result = dpb::from(data)
        .zip(data, other);

    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == std::make_pair(1, std::string("a")));
    REQUIRE(result[1] == std::make_pair(2, std::string("b")));
    REQUIRE(result[2] == std::make_pair(3, std::string("c")));
}

TEST_CASE("Zip with filter", "[zip]") {
    std::vector<int> data = {1, 2, 3, 4, 5};
    std::vector<std::string> other = {"a", "b", "c"};

    auto result = dpb::from(data)
        .where([](int x) { return x % 2 == 0; })
        .zip(data, other);

    // Even numbers: 2, 4 → zip with "a", "b"
    REQUIRE(result.size() == 2);
    REQUIRE(result[0] == std::make_pair(2, std::string("a")));
    REQUIRE(result[1] == std::make_pair(4, std::string("b")));
}

TEST_CASE("Zip with different lengths truncates to shorter", "[zip]") {
    std::vector<int> data = {1, 2, 3, 4, 5};
    std::vector<std::string> other = {"a", "b"};

    auto result = dpb::from(data)
        .zip(data, other);

    REQUIRE(result.size() == 2);
    REQUIRE(result[0] == std::make_pair(1, std::string("a")));
    REQUIRE(result[1] == std::make_pair(2, std::string("b")));
}

TEST_CASE("Zip with empty other range returns empty", "[zip]") {
    std::vector<int> data = {1, 2, 3};
    std::vector<std::string> other;

    auto result = dpb::from(data)
        .zip(data, other);

    REQUIRE(result.empty());
}

TEST_CASE("Zip with empty input returns empty", "[zip]") {
    std::vector<int> data;
    std::vector<std::string> other = {"a", "b", "c"};

    auto result = dpb::from(data)
        .zip(data, other);

    REQUIRE(result.empty());
}

TEST_CASE("Zip with transform", "[zip]") {
    std::vector<int> data = {1, 2, 3};
    std::vector<std::string> other = {"a", "b", "c"};

    auto result = dpb::from(data)
        .transform([](int x) { return x * 10; })
        .zip(data, other);

    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == std::make_pair(10, std::string("a")));
    REQUIRE(result[1] == std::make_pair(20, std::string("b")));
    REQUIRE(result[2] == std::make_pair(30, std::string("c")));
}

// ============================================================================
// JOIN TESTS
// ============================================================================

TEST_CASE("Join inner join on int key", "[join]") {
    std::vector<int> data = {1, 2, 3};
    std::vector<int> other = {2, 3, 4};

    auto result = dpb::from(data)
        .join(data, other, [](int x) { return x; });

    // 1 has no match, 2 matches 2, 3 matches 3
    REQUIRE(result.size() == 2);
    REQUIRE(result[0] == std::make_pair(2, 2));
    REQUIRE(result[1] == std::make_pair(3, 3));
}

TEST_CASE("Join with no matching keys returns empty", "[join]") {
    std::vector<int> data = {1, 2, 3};
    std::vector<int> other = {4, 5, 6};

    auto result = dpb::from(data)
        .join(data, other, [](int x) { return x; });

    REQUIRE(result.empty());
}

TEST_CASE("Join with multiple matches produces all pairs", "[join]") {
    struct Item {
        int category;
        std::string name;
    };

    std::vector<Item> items = {{1, "A"}, {1, "B"}, {2, "C"}};
    std::vector<Item> tags = {{1, "X"}, {1, "Y"}, {2, "Z"}};

    auto result = dpb::from(items)
        .join(items, tags, [](const Item& x) { return x.category; });

    // Item A (cat 1) joins with X (cat 1) and Y (cat 1) = 2 pairs
    // Item B (cat 1) joins with X (cat 1) and Y (cat 1) = 2 pairs
    // Item C (cat 2) joins with Z (cat 2) = 1 pair
    REQUIRE(result.size() == 5);
}

TEST_CASE("Join with filter", "[join]") {
    std::vector<int> data = {1, 2, 3, 4, 5};
    std::vector<int> other = {2, 4, 6};

    auto result = dpb::from(data)
        .where([](int x) { return x % 2 == 0; })
        .join(data, other, [](int x) { return x; });

    // Even numbers from data: 2, 4
    // 2 matches 2, 4 matches 4
    REQUIRE(result.size() == 2);
    REQUIRE(result[0] == std::make_pair(2, 2));
    REQUIRE(result[1] == std::make_pair(4, 4));
}

TEST_CASE("Join with empty input returns empty", "[join]") {
    std::vector<int> data;
    std::vector<int> other = {1, 2, 3};

    auto result = dpb::from(data)
        .join(data, other, [](int x) { return x; });

    REQUIRE(result.empty());
}

TEST_CASE("Join with empty other returns empty", "[join]") {
    std::vector<int> data = {1, 2, 3};
    std::vector<int> other;

    auto result = dpb::from(data)
        .join(data, other, [](int x) { return x; });

    REQUIRE(result.empty());
}

TEST_CASE("Join with string keys", "[join]") {
    struct Person {
        std::string name;
        int age;
    };

    struct Score {
        std::string name;
        int score;
    };

    std::vector<Person> people = {{"Alice", 25}, {"Bob", 30}};
    std::vector<Score> scores = {{"Alice", 95}, {"Bob", 87}};

    auto result = dpb::from(people)
        .join(people, scores, [](const auto& x) { return x.name; });

    REQUIRE(result.size() == 2);
    REQUIRE(result[0].first.name == "Alice");
    REQUIRE(result[0].second.score == 95);
    REQUIRE(result[1].first.name == "Bob");
    REQUIRE(result[1].second.score == 87);
}