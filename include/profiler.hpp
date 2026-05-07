#pragma once

#include <map>
#include <string>
#include <chrono>

#include "dpb/format.hpp"

namespace dpb {

class Profiler {
    struct StageProfile {
        std::chrono::nanoseconds total_time{0};
        size_t call_count{0};

        double avg_time_ns() const {
            return call_count > 0 ? (double)total_time.count() / call_count : 0.0;
        }
    };

    std::map<std::string, StageProfile> profiles_;

public:
    void record(const std::string& stage_name, std::chrono::nanoseconds duration) {
        profiles_[stage_name].total_time += duration;
        profiles_[stage_name].call_count++;
    }

    void print() const {
        dpb::print("\n=== Pipeline Profile ===\n");
        dpb::print("{:>20}{:>15}{:>15}{:>15}\n",
                    "Stage", "Total (ms)", "Avg (ns)", "Calls");
        dpb::print("{}\n", std::string(65, '-'));

        for (const auto& [name, profile] : profiles_) {
            auto total_ms = std::chrono::duration<double, std::milli>(profile.total_time).count();
            dpb::print("{:>20}{:>15.3f}{:>15.0f}{:>15}\n",
                        name, total_ms, profile.avg_time_ns(), profile.call_count);
        }
    }

    void reset() {
        profiles_.clear();
    }

    std::chrono::nanoseconds total_time() const {
        std::chrono::nanoseconds total{0};
        for (const auto& [name, profile] : profiles_) {
            total += profile.total_time;
        }
        return total;
    }
};

} // namespace dpb
