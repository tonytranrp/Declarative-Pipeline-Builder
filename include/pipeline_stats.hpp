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
    std::atomic<size_t> total_items{0};  // ✅ NEW: Total input items (processed + filtered)
    std::atomic<int64_t> total_duration_ns{0};  // CRITICAL: atomic duration in nanoseconds

    PipelineStats() = default;

    // Constructor for creating result stats (from atomic values)
    PipelineStats(size_t processed, size_t filtered, size_t errs, size_t total, int64_t duration_ns)
        : items_processed(processed), items_filtered(filtered), errors(errs),
          total_items(total), total_duration_ns(duration_ns) {}

    // Extract values for copying
    size_t processed() const { return items_processed.load(); }
    size_t filtered() const { return items_filtered.load(); }
    size_t error_count() const { return errors.load(); }
    std::chrono::nanoseconds duration() const { 
        return std::chrono::nanoseconds{total_duration_ns.load()}; 
    }

    void print() const {

        std::cout << "=== Pipeline Statistics ===\n";



        size_t proc = items_processed.load();

        size_t filt = items_filtered.load();

        size_t total = total_items.load();

        auto dur_ns = total_duration_ns.load();



        std::cout << "Items processed: " << proc << "\n";

        std::cout << "Items filtered: " << filt << "\n";

        std::cout << "Errors: " << errors.load() << "\n";



        // Show total input items for clarity

        if (total > 0) {

            std::cout << "Total input items: " << total << "\n";

            std::cout << "Pass rate: " << std::fixed << std::setprecision(2)

                      << (100.0 * proc / total) << "%\n";

        }



        auto dur_ms = dur_ns / 1'000'000.0;

        std::cout << "Total duration: " << std::fixed << std::setprecision(4)

                  << dur_ms << " ms\n";



        // ✅ FIXED: Calculate latency based on TOTAL items, not just processed

        if (total > 0 && dur_ns > 0) {

            auto latency_ns = dur_ns / total;  // Per input item, not per passed item

            std::cout << "Average latency: " << latency_ns << " ns/item (per input)\n";



            double throughput = (total * 1'000'000'000.0) / dur_ns;

            std::cout << "Throughput: " << std::fixed << std::setprecision(2)

                      << throughput << " items/sec\n";

        } else if (total > 0) {

            std::cout << "Average latency: < 1 ns/item (too fast to measure)\n";

            std::cout << "Throughput: > 1 billion items/sec\n";

        }

    }

    // Reset stats for reuse
    void reset() {
        items_processed = 0;
        items_filtered = 0;
        errors = 0;
        total_items = 0;  // ✅ NEW
        total_duration_ns = 0;
    }
};

// RAII timer for automatic duration tracking (THREAD-SAFE)
class ScopedTimer {
    std::chrono::high_resolution_clock::time_point start_;
    std::atomic<int64_t>& duration_ns_;

public:
    ScopedTimer(std::atomic<int64_t>& duration_ns)
        : start_(std::chrono::high_resolution_clock::now())
        , duration_ns_(duration_ns) {}

    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count();
        // ATOMIC ADD: thread-safe accumulation
        duration_ns_.fetch_add(elapsed, std::memory_order_relaxed);
    }
};

} // namespace dpb
