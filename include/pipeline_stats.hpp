#pragma once

#include <chrono>
#include <atomic>
#include <new>

#include "dpb/format.hpp"

namespace dpb {

struct PipelineStats {
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> items_processed{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> items_filtered{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> errors{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<size_t> total_items{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<int64_t> total_duration_ns{0};

    PipelineStats() = default;

    PipelineStats(size_t processed, size_t filtered, size_t errs, size_t total, int64_t duration_ns)
        : items_processed(processed), items_filtered(filtered), errors(errs),
          total_items(total), total_duration_ns(duration_ns) {}

    size_t processed() const { return items_processed.load(); }
    size_t filtered() const { return items_filtered.load(); }
    size_t error_count() const { return errors.load(); }
    std::chrono::nanoseconds duration() const {
        return std::chrono::nanoseconds{total_duration_ns.load()};
    }

    void print() const {
        dpb::print("=== Pipeline Statistics ===\n");

        size_t proc = items_processed.load();
        size_t filt = items_filtered.load();
        size_t total = total_items.load();
        auto dur_ns = total_duration_ns.load();

        dpb::print("Items processed: {}\n", proc);
        dpb::print("Items filtered: {}\n", filt);
        dpb::print("Errors: {}\n", errors.load());

        if (total > 0) {
            dpb::print("Total input items: {}\n", total);
            dpb::print("Pass rate: {:.2f}%\n", 100.0 * proc / total);
        }

        auto dur_ms = dur_ns / 1'000'000.0;
        dpb::print("Total duration: {:.4f} ms\n", dur_ms);

        if (total > 0 && dur_ns > 0) {
            auto latency_ns = dur_ns / total;
            dpb::print("Average latency: {} ns/item (per input)\n", latency_ns);

            double throughput = (total * 1'000'000'000.0) / dur_ns;
            dpb::print("Throughput: {:.2f} items/sec\n", throughput);
        } else if (total > 0) {
            dpb::print("Average latency: < 1 ns/item (too fast to measure)\n");
            dpb::print("Throughput: > 1 billion items/sec\n");
        }
    }

    void reset() {
        items_processed = 0;
        items_filtered = 0;
        errors = 0;
        total_items = 0;
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
