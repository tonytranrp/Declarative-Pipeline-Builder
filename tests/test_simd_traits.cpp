#include <catch2/catch_test_macros.hpp>
#include <dpb/simd_traits.hpp>
#include <dpb/simd.hpp>
#include <pipeline/pipeline.hpp>
#include <vector>
#include <string>
#include <cstdint>

using namespace dpb;

// ============================================================================
// simd_numeric concept verification
// ============================================================================

TEST_CASE("simd_numeric concept is correct", "[simd_traits]") {
    STATIC_REQUIRE(simd::simd_numeric<int32_t>);
    STATIC_REQUIRE(simd::simd_numeric<int64_t>);
    STATIC_REQUIRE(simd::simd_numeric<float>);
    STATIC_REQUIRE(simd::simd_numeric<double>);

    STATIC_REQUIRE_FALSE(simd::simd_numeric<std::string>);
    STATIC_REQUIRE_FALSE(simd::simd_numeric<bool>);
    STATIC_REQUIRE_FALSE(simd::simd_numeric<char>);
    STATIC_REQUIRE_FALSE(simd::simd_numeric<int16_t>);
    STATIC_REQUIRE_FALSE(simd::simd_numeric<uint32_t>);
}

// ============================================================================
// simd_eligible_chain — Pipeline chain eligibility
// ============================================================================

TEST_CASE("simd_eligible_chain for Pipeline types", "[simd_traits]") {
    SECTION("even+square int chain is SIMD-eligible") {
        std::vector<int> data = {1, 2, 3, 4, 5};
        auto pipe = dpb::from(data)
            .where([](int x) { return x % 2 == 0; })
            .map([](int x) { return x * x; });
        using EvenSquarePipe = decltype(pipe);
        STATIC_REQUIRE(simd_eligible_chain_v<EvenSquarePipe>);
        // Verify the pipeline still works (non-regression)
        auto result = std::move(pipe).collect(data);
        REQUIRE(result.size() == 2);
    }

    SECTION("string chain is NOT SIMD-eligible") {
        std::vector<std::string> data = {"a", "bb", "ccc"};
        auto pipe = dpb::from(data)
            .where([](const std::string& s) { return s.size() > 1; })
            .map([](const std::string& s) { return s + s; });
        using StringPipe = decltype(pipe);
        STATIC_REQUIRE_FALSE(simd_eligible_chain_v<StringPipe>);
    }

    SECTION("default-constructed int Pipeline is SIMD-eligible") {
        STATIC_REQUIRE(simd_eligible_chain_v<Pipeline<int, int>>);
    }

    SECTION("non-Pipeline types are NOT SIMD-eligible") {
        STATIC_REQUIRE_FALSE(simd_eligible_chain_v<int>);
        STATIC_REQUIRE_FALSE(simd_eligible_chain_v<std::string>);
        STATIC_REQUIRE_FALSE(simd_eligible_chain_v<std::vector<int>>);
    }
}

// ============================================================================
// simd_eligible_op — individual operation eligibility
// ============================================================================

TEST_CASE("simd_eligible_op for individual operations", "[simd_traits]") {
    SECTION("numeric-to-numeric transform is eligible") {
        auto square = [](int x) { return x * x; };
        STATIC_REQUIRE(simd_eligible_op_v<decltype(square), int, int>);

        auto to_double = [](int x) { return static_cast<double>(x); };
        STATIC_REQUIRE(simd_eligible_op_v<decltype(to_double), int, double>);
    }

    SECTION("numeric-to-string transform is NOT eligible") {
        auto to_string = [](int x) { return std::to_string(x); };
        STATIC_REQUIRE_FALSE(simd_eligible_op_v<decltype(to_string), int, std::string>);
    }
}

// ============================================================================
// simd_eligible_predicate — individual predicate eligibility
// ============================================================================

TEST_CASE("simd_eligible_predicate for individual predicates", "[simd_traits]") {
    SECTION("numeric predicate is eligible") {
        auto is_even = [](int x) { return x % 2 == 0; };
        STATIC_REQUIRE(simd_eligible_predicate_v<decltype(is_even), int>);
    }

    SECTION("non-numeric predicate is NOT eligible") {
        auto non_empty = [](const std::string& s) { return !s.empty(); };
        STATIC_REQUIRE_FALSE(simd_eligible_predicate_v<decltype(non_empty), std::string>);
    }
}
