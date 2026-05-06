#include <catch2/catch_test_macros.hpp>
#include <declarative_pipeline.hpp>

#include <vector>
#include <numeric>
#include <string>
#include <future>

using namespace dpb;

TEST_CASE("Pipeline with std::future", "[async]") {
    SECTION("Pipeline processes future results") {
        std::vector<int> input(1000);
        std::iota(input.begin(), input.end(), 0);

        auto fut = std::async(std::launch::async, [&input]() {
            return Pipeline<int, int>::from(input)
                .filter([](int x) { return x % 2 == 0; })
                .transform([](int x) { return x * x; })
                .collect(input);
        });

        auto result = fut.get();
        REQUIRE(result.size() == 500);
        REQUIRE(result[0] == 0);
        REQUIRE(result[1] == 4);
        REQUIRE(result[2] == 16);
    }

    SECTION("Multiple async pipelines run concurrently") {
        std::vector<int> data1(5000);
        std::vector<int> data2(5000);
        std::iota(data1.begin(), data1.end(), 0);
        std::iota(data2.begin(), data2.end(), 10000);

        auto fut1 = std::async(std::launch::async, [&data1]() {
            return Pipeline<int, int>::from(data1)
                .filter([](int x) { return x % 3 == 0; })
                .transform([](int x) { return x * 2; })
                .collect(data1);
        });

        auto fut2 = std::async(std::launch::async, [&data2]() {
            return Pipeline<int, int>::from(data2)
                .filter([](int x) { return x % 5 == 0; })
                .transform([](int x) { return x * 3; })
                .collect(data2);
        });

        auto r1 = fut1.get();
        auto r2 = fut2.get();

        REQUIRE(r1.size() > 0);
        REQUIRE(r2.size() > 0);
        // Both should have produced valid results concurrently
        REQUIRE(r1[0] % 2 == 0);
        REQUIRE(r2[0] % 3 == 0);
    }

    SECTION("Pipeline with string transformation in async context") {
        struct Person {
            std::string name;
            int age;
        };

        std::vector<Person> people = {
            {"Alice", 30}, {"Bob", 17}, {"Charlie", 25}, {"Diana", 42}
        };

        auto fut = std::async(std::launch::async, [&people]() {
            return Pipeline<Person, Person>::from(people)
                .filter([](const Person& p) { return p.age >= 18; })
                .transform([](const Person& p) { return p.name + " (" + std::to_string(p.age) + ")"; })
                .collect(people);
        });

        auto result = fut.get();
        REQUIRE(result.size() == 3);  // Alice, Charlie, Diana
    }
}
