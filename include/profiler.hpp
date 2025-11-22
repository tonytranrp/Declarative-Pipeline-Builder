#pragma once

#include <map>
#include <string>
#include <chrono>
#include <iostream>
#include <iomanip>

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
        std::cout << "\n=== Pipeline Profile ===\n";
        std::cout << std::setw(20) << "Stage"
                  << std::setw(15) << "Total (ms)"
                  << std::setw(15) << "Avg (ns)"
                  << std::setw(15) << "Calls\n";
        std::cout << std::string(65, '-') << "\n";

        for (const auto& [name, profile] : profiles_) {
            std::cout << std::setw(20) << name
                      << std::setw(15) << std::fixed << std::setprecision(3)
                      << std::chrono::duration<double, std::milli>(profile.total_time).count()
                      << std::setw(15) << std::fixed << std::setprecision(0)
                      << profile.avg_time_ns()
                      << std::setw(15) << profile.call_count << "\n";
        }
    }

    // Reset profiler for reuse
    void reset() {
        profiles_.clear();
    }

    // Get total time across all stages
    std::chrono::nanoseconds total_time() const {
        std::chrono::nanoseconds total{0};
        for (const auto& [name, profile] : profiles_) {
            total += profile.total_time;
        }
        return total;
    }
};

} // namespace dpb
