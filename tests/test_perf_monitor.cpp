#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <catch2/catch_test_case_info.hpp>

#include <dpb/tracy_config.hpp>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct TestTiming {
    std::string name;
    double milliseconds{};
};

class DpbPerformanceListener final : public Catch::EventListenerBase {
    using Clock = std::chrono::steady_clock;

    Clock::time_point test_start_{};
    std::vector<TestTiming> timings_;

public:
    using EventListenerBase::EventListenerBase;

    void testRunStarting(Catch::TestRunInfo const&) override {
        timings_.clear();
        DPB_FRAME_MARK;
    }

    void testCaseStarting(Catch::TestCaseInfo const&) override {
        DPB_ZONE_NAMED(test_case_start_zone, "Catch2 test case start");
        test_start_ = Clock::now();
        DPB_FRAME_MARK;
    }

    void testCaseEnded(Catch::TestCaseStats const& stats) override {
        DPB_ZONE_NAMED(test_case_end_zone, "Catch2 test case end");
        const auto elapsed = Clock::now() - test_start_;
        const double ms = std::chrono::duration<double, std::milli>(elapsed).count();
        timings_.push_back({std::string(stats.testInfo->name), ms});
        DPB_FRAME_MARK;
    }

    void testRunEnded(Catch::TestRunStats const&) override {
        if (timings_.empty()) return;

        std::vector<double> sorted;
        sorted.reserve(timings_.size());
        for (const auto& timing : timings_) {
            sorted.push_back(timing.milliseconds);
        }
        std::sort(sorted.begin(), sorted.end());
        const double median = sorted[sorted.size() / 2];

        std::cout << "\n=== DPB Test Speed Comparison ===\n";
        std::cout << "Each test is timed and framed for Tracy when DPB_ENABLE_TRACY is ON.\n";
        std::cout << std::left << std::setw(58) << "Test" << std::right
                  << std::setw(14) << "ms"
                  << std::setw(18) << "vs median" << '\n';

        for (const auto& timing : timings_) {
            const double speed_ratio = timing.milliseconds > 0.0 ? median / timing.milliseconds : 0.0;
            std::string_view name = timing.name;
            if (name.size() > 56) {
                name = name.substr(0, 56);
            }

            std::cout << std::left << std::setw(58) << name << std::right
                      << std::setw(14) << std::fixed << std::setprecision(4) << timing.milliseconds
                      << std::setw(17) << std::setprecision(2) << speed_ratio << "x\n";
        }
    }
};

}  // namespace

CATCH_REGISTER_LISTENER(DpbPerformanceListener)
