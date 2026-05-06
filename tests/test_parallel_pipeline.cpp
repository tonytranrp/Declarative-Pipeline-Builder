#include <catch2/catch_test_macros.hpp>
#include <declarative_pipeline.hpp>

#include <vector>
#include <numeric>
#include <algorithm>

using namespace dpb;

TEST_CASE("Parallel Pipeline - Correctness", "[parallel]") {
    SECTION("Parallel produces same results as sequential") {
        std::vector<int> data(10000);
        std::iota(data.begin(), data.end(), 0);

        auto seq = Pipeline<int, int>::from(data)
            .filter([](int x) { return x % 2 == 0; })
            .transform([](int x) { return x * x; })
            .collect(data);

        auto par = Pipeline<int, int>::from(data)
            .filter([](int x) { return x % 2 == 0; })
            .transform([](int x) { return x * x; })
            .parallel(4, ExecutionPolicy::ParallelPreserveOrder)
            .collect(data);

        REQUIRE(seq == par);
        REQUIRE(seq.size() == 5000);
        REQUIRE(par.size() == 5000);
    }

    SECTION("Parallel unordered produces same elements") {
        std::vector<int> data(10000);
        std::iota(data.begin(), data.end(), 0);

        auto seq = Pipeline<int, int>::from(data)
            .filter([](int x) { return x % 2 == 0; })
            .transform([](int x) { return x * 2; })
            .collect(data);

        auto par_unordered = Pipeline<int, int>::from(data)
            .filter([](int x) { return x % 2 == 0; })
            .transform([](int x) { return x * 2; })
            .parallel(4, ExecutionPolicy::ParallelUnordered)
            .collect(data);

        std::vector<int> seq_sorted(seq.data.begin(), seq.data.end());
        std::vector<int> par_sorted(par_unordered.data.begin(), par_unordered.data.end());
        std::sort(seq_sorted.begin(), seq_sorted.end());
        std::sort(par_sorted.begin(), par_sorted.end());

        REQUIRE(seq_sorted == par_sorted);
        REQUIRE(par_unordered.size() == 5000);
    }

    SECTION("Single thread parallel falls back") {
        std::vector<int> data(1000);
        std::iota(data.begin(), data.end(), 0);

        auto result = Pipeline<int, int>::from(data)
            .filter([](int x) { return x % 2 == 0; })
            .parallel(1)
            .collect(data);

        REQUIRE(result.size() == 500);
        REQUIRE(result[0] == 0);
    }
}

TEST_CASE("Parallel Pipeline - Edge Cases", "[parallel]") {
    SECTION("Empty input") {
        std::vector<int> empty;
        auto result = Pipeline<int, int>::from(empty)
            .parallel(4)
            .collect(empty);
        REQUIRE(result.empty());
    }

    SECTION("Single element input") {
        std::vector<int> single = {42};
        auto result = Pipeline<int, int>::from(single)
            .parallel(8)
            .collect(single);
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == 42);
    }

    SECTION("All items filtered out") {
        std::vector<int> data(1000);
        std::iota(data.begin(), data.end(), 0);
        auto result = Pipeline<int, int>::from(data)
            .filter([](int) { return false; })
            .parallel(4)
            .collect(data);
        REQUIRE(result.empty());
    }
}

TEST_CASE("Parallel Pipeline - Custom Types", "[parallel]") {
    struct Vec3 { float x, y, z; };

    std::vector<Vec3> input(500);
    for (size_t i = 0; i < 500; ++i) {
        input[i] = {float(i), float(i * 2), float(i * 3)};
    }

    auto result = Pipeline<Vec3, Vec3>::from(input)
        .transform([](const Vec3& v) { return v.y; })
        .filter([](float y) { return y > 100.0f; })
        .transform([](float y) { return y * 2.0f; })
        .parallel(4, ExecutionPolicy::ParallelPreserveOrder)
        .collect(input);

    REQUIRE(result.size() == 449);
}
