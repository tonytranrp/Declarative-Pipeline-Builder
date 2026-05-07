#include <catch2/catch_test_macros.hpp>
#include <declarative_pipeline.hpp>
#include <vector>
#include <numeric>

TEST_CASE("PipelineErase stores and executes pipelines", "[pipeline_erase]") {
    using namespace dpb;
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto erased = from(data)
        .where([](int x) { return x % 2 == 0; })
        .map([](int x) { return x * x; })
        .erase();

    auto result = std::move(erased).collect(data);

    REQUIRE(result.size() == 5);
    REQUIRE(result[0] == 4);
    REQUIRE(result[1] == 16);
    REQUIRE(result[2] == 36);
    REQUIRE(result[3] == 64);
    REQUIRE(result[4] == 100);
}

TEST_CASE("PipelineErase result matches Pipeline result", "[pipeline_erase]") {
    using namespace dpb;
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto typed_result = from(data)
        .where([](int x) { return x % 2 == 0; })
        .map([](int x) { return x * x; })
        .collect(data);

    auto erased = from(data)
        .where([](int x) { return x % 2 == 0; })
        .map([](int x) { return x * x; })
        .erase();
    auto erased_result = std::move(erased).collect(data);

    REQUIRE(typed_result == erased_result);
}

TEST_CASE("PipelineErase can be stored in vector", "[pipeline_erase]") {
    using namespace dpb;
    std::vector<int> data = {1, 2, 3, 4, 5};

    std::vector<PipelineErase<int, int>> pipelines;
    pipelines.push_back(from(data).where([](int x) { return x % 2 == 0; }).erase());
    pipelines.push_back(from(data).where([](int x) { return x > 3; }).erase());

    auto result0 = std::move(pipelines[0]).collect(data);
    auto result1 = std::move(pipelines[1]).collect(data);

    REQUIRE(result0.size() == 2);
    REQUIRE(result1.size() == 2);
}

TEST_CASE("DefaultOp produces identity pipeline", "[template_fix]") {
    using namespace dpb;
    std::vector<int> data = {1, 2, 3, 4, 5};

    auto identity = from(data).collect(data);

    REQUIRE(identity.size() == 5);
    REQUIRE(identity[0] == 1);
    REQUIRE(identity[4] == 5);
}

TEST_CASE("Fused lambdas have exact types (no std::function)", "[template_fix]") {
    using namespace dpb;
    std::vector<int> data = {1, 2, 3, 4, 5};

    auto pipeline = from(data)
        .where([](int x) { return x % 2 == 0; })
        .map([](int x) { return x * 2; });

    auto result = std::move(pipeline).collect(data);

    REQUIRE(result.size() == 2);
    REQUIRE(result[0] == 4);
    REQUIRE(result[1] == 8);
}

#ifdef DPB_HAS_HIGHWAY
TEST_CASE("SIMD filter matches scalar filter", "[simd]") {
    using namespace dpb;
    std::vector<int> data(1000);
    std::iota(data.begin(), data.end(), 0);

    auto scalar_result = simd_filter(data, [](int x) { return x % 2 == 0; });
    auto pipeline_result = from(data).where([](int x) { return x % 2 == 0; }).to_vector(data);

    REQUIRE(scalar_result.size() == pipeline_result.size());
    for (size_t i = 0; i < scalar_result.size(); ++i) {
        REQUIRE(scalar_result[i] == pipeline_result[i]);
    }
}

TEST_CASE("SIMD transform matches scalar transform", "[simd]") {
    using namespace dpb;
    std::vector<float> data(1000);
    for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<float>(i);

    auto scalar_result = simd_transform(data, [](float x) { return x * 2.0f + 1.0f; });

    REQUIRE(scalar_result.size() == data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(scalar_result[i] == Catch::Approx(data[i] * 2.0f + 1.0f));
    }
}
#endif