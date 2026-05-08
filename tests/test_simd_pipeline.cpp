#include <catch2/catch_test_macros.hpp>
#include <declarative_pipeline.hpp>
#include <vector>
#include <string>
#include <cstdint>
#include <numeric>
#include <algorithm>

using namespace dpb;

// ============================================================================
// Helper: generate increasing values [0, n)
// ============================================================================

inline std::vector<int> iota_data(size_t n) {
    std::vector<int> data(n);
    std::iota(data.begin(), data.end(), 0);
    return data;
}

// ============================================================================
// SIMD vs scalar comparison tests
// ============================================================================

#ifdef DPB_HAS_HIGHWAY

TEST_CASE("SIMD filter matches scalar filter for int32_t", "[simd]") {
    const size_t N = 10000;
    auto data = iota_data(N);

    // SIMD path: no stats → SIMD-eligible pipeline uses simd_collect_sequential
    auto simd_result = from(data)
        .where([](int x) { return x % 2 == 0; })
        .collect(data);

    // Scalar path: with_stats → forces scalar collect_sequential path
    auto scalar_result = from(data)
        .with_stats()
        .where([](int x) { return x % 2 == 0; })
        .collect(data);

    REQUIRE(simd_result.size() == scalar_result.size());
    REQUIRE(simd_result.size() == N / 2);
    for (size_t i = 0; i < simd_result.size(); ++i) {
        REQUIRE(simd_result[i] == scalar_result[i]);
        REQUIRE(simd_result[i] % 2 == 0);
    }
}

TEST_CASE("SIMD transform matches scalar transform for int32_t", "[simd]") {
    const size_t N = 5000;
    auto data = iota_data(N);

    auto simd_result = from(data)
        .map([](int x) { return x * 3 + 7; })
        .collect(data);

    auto scalar_result = from(data)
        .with_stats()
        .map([](int x) { return x * 3 + 7; })
        .collect(data);

    REQUIRE(simd_result.size() == scalar_result.size());
    REQUIRE(simd_result.size() == N);
    for (size_t i = 0; i < simd_result.size(); ++i) {
        REQUIRE(simd_result[i] == scalar_result[i]);
        int expected = static_cast<int>(i) * 3 + 7;
        REQUIRE(simd_result[i] == expected);
    }
}

TEST_CASE("SIMD filter+transform matches scalar for int32_t", "[simd]") {
    const size_t N = 10000;
    auto data = iota_data(N);

    auto simd_result = from(data)
        .where([](int x) { return x % 3 == 0; })
        .map([](int x) { return x * x + 1; })
        .collect(data);

    auto scalar_result = from(data)
        .with_stats()
        .where([](int x) { return x % 3 == 0; })
        .map([](int x) { return x * x + 1; })
        .collect(data);

    REQUIRE(simd_result.size() == scalar_result.size());
    // expected: roughly N/3 elements
    REQUIRE(simd_result.size() > 0);
    for (size_t i = 0; i < simd_result.size(); ++i) {
        REQUIRE(simd_result[i] == scalar_result[i]);
    }
    // verify correctness: each result should be x*x+1 for some x % 3 == 0
    for (auto val : simd_result) {
        REQUIRE((val - 1) % 9 == 0); // (3k)^2 + 1 = 9k^2 + 1 → (val-1) % 9 == 0
        int x = static_cast<int>(std::sqrt(static_cast<double>(val - 1)));
        REQUIRE(x % 3 == 0);
    }
}

TEST_CASE("SIMD works for float type", "[simd]") {
    const size_t N = 5000;
    std::vector<float> data(N);
    for (size_t i = 0; i < N; ++i) data[i] = static_cast<float>(i) * 0.5f;

    auto simd_result = from(data)
        .where([](float x) { return x > 10.0f; })
        .map([](float x) { return x * 2.0f + 0.5f; })
        .collect(data);

    auto scalar_result = from(data)
        .with_stats()
        .where([](float x) { return x > 10.0f; })
        .map([](float x) { return x * 2.0f + 0.5f; })
        .collect(data);

    REQUIRE(simd_result.size() == scalar_result.size());
    REQUIRE(simd_result.size() > 0);
    for (size_t i = 0; i < simd_result.size(); ++i) {
        REQUIRE(simd_result[i] == Catch::Approx(scalar_result[i]));
    }
}

TEST_CASE("SIMD works for double type", "[simd]") {
    const size_t N = 5000;
    std::vector<double> data(N);
    for (size_t i = 0; i < N; ++i) data[i] = static_cast<double>(i) * 0.25;

    auto simd_result = from(data)
        .where([](double x) { return x > 5.0; })
        .map([](double x) { return x * x + 1.0; })
        .collect(data);

    auto scalar_result = from(data)
        .with_stats()
        .where([](double x) { return x > 5.0; })
        .map([](double x) { return x * x + 1.0; })
        .collect(data);

    REQUIRE(simd_result.size() == scalar_result.size());
    REQUIRE(simd_result.size() > 0);
    for (size_t i = 0; i < simd_result.size(); ++i) {
        REQUIRE(simd_result[i] == Catch::Approx(scalar_result[i]));
    }
}

TEST_CASE("SIMD works for int64_t type", "[simd]") {
    const size_t N = 5000;
    std::vector<int64_t> data(N);
    for (size_t i = 0; i < N; ++i) data[i] = static_cast<int64_t>(i);

    auto simd_result = from(data)
        .where([](int64_t x) { return x % 7 == 0; })
        .map([](int64_t x) { return x * 10 + 3; })
        .collect(data);

    auto scalar_result = from(data)
        .with_stats()
        .where([](int64_t x) { return x % 7 == 0; })
        .map([](int64_t x) { return x * 10 + 3; })
        .collect(data);

    REQUIRE(simd_result.size() == scalar_result.size());
    REQUIRE(simd_result.size() > 0);
    for (size_t i = 0; i < simd_result.size(); ++i) {
        REQUIRE(simd_result[i] == scalar_result[i]);
    }
}

TEST_CASE("SIMD falls back to scalar for std::string (non-numeric)", "[simd]") {
    std::vector<std::string> data = {"hello", "world", "a", "bb", "ccc", "dddd"};

    // Even without Highway, non-numeric pipelines must work correctly
    auto result = from(data)
        .where([](const std::string& s) { return s.size() > 2; })
        .map([](const std::string& s) { return s + "!"; })
        .collect(data);

    REQUIRE(result.size() == 3);
    REQUIRE(result[0] == "hello!");
    REQUIRE(result[1] == "world!");
    REQUIRE(result[2] == "ddd!");
}

TEST_CASE("SIMD path handles empty input", "[simd]") {
    std::vector<int> data;

    auto result = from(data)
        .where([](int x) { return x % 2 == 0; })
        .map([](int x) { return x * x; })
        .collect(data);

    REQUIRE(result.size() == 0);
}

TEST_CASE("SIMD path handles all-filtered input", "[simd]") {
    const size_t N = 1000;
    auto data = iota_data(N);

    // Predicate that rejects everything
    auto result = from(data)
        .where([](int) { return false; })
        .collect(data);

    REQUIRE(result.size() == 0);
}

TEST_CASE("SIMD path preserves element order", "[simd]") {
    const size_t N = 5000;
    auto data = iota_data(N);

    auto result = from(data)
        .where([](int x) { return x % 10 == 0; })
        .map([](int x) { return x + 1; })
        .collect(data);

    REQUIRE(result.size() == N / 10);
    // Elements should be in order: 1, 11, 21, 31, ...
    for (size_t i = 0; i < result.size(); ++i) {
        REQUIRE(result[i] == static_cast<int>(i * 10 + 1));
    }
}

#else // !DPB_HAS_HIGHWAY

TEST_CASE("Non-SIMD pipeline still works for std::string", "[simd]") {
    std::vector<std::string> data = {"alpha", "beta", "gamma", "delta"};

    auto result = from(data)
        .where([](const std::string& s) { return s.size() > 4; })
        .collect(data);

    REQUIRE(result.size() == 3);
}

#endif // DPB_HAS_HIGHWAY
