#pragma once

#include <chrono>
#include <atomic>
#include <iostream>
#include <iomanip>

namespace dpb {

struct PipelineStats {
    std::atomic<size_t> items_processed{0};
    std::atomic<size_t> items_filtered{0};
    std::atomic<size_t> errors{0};
    std::chrono::nanoseconds total_duration{0};

    PipelineStats() = default;

    // Constructor for creating result stats
    PipelineStats(size_t processed, size_t filtered, size_t errs, std::chrono::nanoseconds duration)
        : items_processed(processed), items_filtered(filtered), errors(errs), total_duration(duration) {}

    // Extract values for copying
    size_t processed() const { return items_processed.load(); }
    size_t filtered() const { return items_filtered.load(); }
    size_t error_count() const { return errors.load(); }

    void print() const {
        std::cout << "=== Pipeline Statistics ===\n";
        std::cout << "Items processed: " << processed() << "\n";
        std::cout << "Items filtered: " << filtered() << "\n";
        std::cout << "Errors: " << error_count() << "\n";
        std::cout << "Total duration: "
                  << std::chrono::duration<double, std::milli>(total_duration).count()
                  << " ms\n";

        if (processed() > 0) {
            auto latency_ns = total_duration.count() / processed();
            std::cout << "Average latency: " << latency_ns << " ns/item\n";

            double throughput = processed() /
                (std::chrono::duration<double>(total_duration).count());
            std::cout << "Throughput: " << throughput << " items/sec\n";
        }
    }

    // Reset stats for reuse
    void reset() {
        items_processed = 0;
        items_filtered = 0;
        errors = 0;
        total_duration = std::chrono::nanoseconds{0};
    }
};

// RAII timer for automatic duration tracking
class ScopedTimer {
    std::chrono::high_resolution_clock::time_point start_;
    std::chrono::nanoseconds& duration_;

public:
    ScopedTimer(std::chrono::nanoseconds& duration)
        : start_(std::chrono::high_resolution_clock::now())
        , duration_(duration) {}

    ~ScopedTimer() {
        duration_ = std::chrono::high_resolution_clock::now() - start_;
    }
};

} // namespace dpb
