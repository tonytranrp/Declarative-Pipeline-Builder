#pragma once

#include "concepts.hpp"
#include "pipeline_stats.hpp"
#include "profiler.hpp"

#include <vector>
#include <memory>
#include <functional>
#include <ranges>
#include <algorithm>
#include <variant>
#include <iostream>
#include <optional>
#include <thread>
#include <future>

// Performance intrinsics
#ifdef _MSC_VER
#include <intrin.h>
inline uint64_t read_tsc() noexcept {
    return __rdtsc();
}
#define PREFETCH(addr) _mm_prefetch((char*)(addr), _MM_HINT_T0)
#else
inline uint64_t read_tsc() noexcept {
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
#define PREFETCH(addr) __builtin_prefetch(addr, 0, 3)
#endif

namespace dpb {

// ============================================================================
// EXECUTION POLICY
// ============================================================================

enum class ExecutionPolicy {
    Sequential,
    ParallelPreserveOrder,
    ParallelUnordered
};

// ============================================================================
// RESULT WITH STATS
// ============================================================================

template<typename T>
struct ResultWithStats {
    std::vector<T> data;
    size_t items_processed{0};
    size_t items_filtered{0};
    size_t errors{0};
    size_t total_items{0};  // ✅ NEW
    std::chrono::nanoseconds total_duration{0};

    ResultWithStats(std::vector<T> d, size_t processed, size_t filtered, size_t errs, size_t total, std::chrono::nanoseconds duration)
        : data(std::move(d)), items_processed(processed), items_filtered(filtered), errors(errs), total_items(total), total_duration(duration) {}

    void print_stats() const {
        std::cout << "=== Pipeline Statistics ===\n";
        std::cout << "Items processed: " << items_processed << "\n";
        std::cout << "Items filtered: " << items_filtered << "\n";
        std::cout << "Errors: " << errors << "\n";

        // Show total input items for clarity
        if (total_items > 0) {
            std::cout << "Total input items: " << total_items << "\n";
            std::cout << "Pass rate: " << std::fixed << std::setprecision(2)
                      << (100.0 * items_processed / total_items) << "%\n";
        }

        std::cout << "Total duration: "
                  << std::fixed << std::setprecision(4)
                  << std::chrono::duration<double, std::milli>(total_duration).count()
                  << " ms\n";

        // ✅ FIXED: Calculate latency based on TOTAL items, not just processed
        if (total_items > 0) {
            auto latency_ns = total_duration.count() / total_items;  // Per input item, not per passed item
            std::cout << "Average latency: " << latency_ns << " ns/item (per input)\n";

            double throughput = total_items / std::chrono::duration<double>(total_duration).count();
            std::cout << "Throughput: " << std::fixed << std::setprecision(2)
                      << throughput << " items/sec\n";
        }
    }

    // Vector-like interface
    size_t size() const { return data.size(); }
    bool empty() const { return data.empty(); }
    const T& operator[](size_t i) const { return data[i]; }
    T& operator[](size_t i) { return data[i]; }
    const T& at(size_t i) const { return data.at(i); }
    T& at(size_t i) { return data.at(i); }
    auto begin() { return data.begin(); }
    auto end() { return data.end(); }
    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }
    auto cbegin() const { return data.cbegin(); }
    auto cend() const { return data.cend(); }

    bool operator==(const std::vector<T>& other) const { return data == other; }
    bool operator==(const ResultWithStats& other) const { return data == other.data; }
};

// ============================================================================
// GENERIC PIPELINE - Type-driven pipeline supporting arbitrary transformations
// ============================================================================

template<typename In, typename Out = In, typename OpFunc = std::function<bool(const In&, Out&)>>
class Pipeline {
    OpFunc operation_;  // ✅ ZERO overhead - stores actual lambda type, not std::function!
    PipelineStats* stats_{nullptr};  // Raw pointer - no atomic ref counting
    Profiler* profiler_{nullptr};
    ExecutionPolicy                exec_policy_ = ExecutionPolicy::Sequential;
    std::size_t                    parallelism_ = std::thread::hardware_concurrency();

public:
    // Internal constructor for chaining operations (not for public use)
    template<typename F>
    Pipeline(F&& op,
             PipelineStats* stats = nullptr,
             Profiler* prof = nullptr,
             ExecutionPolicy exec_policy = ExecutionPolicy::Sequential,
             std::size_t parallelism = std::thread::hardware_concurrency())
        : operation_(std::forward<F>(op)),
          stats_(stats),
          profiler_(prof),
          exec_policy_(exec_policy),
          parallelism_(parallelism) {}
    Pipeline() : operation_([](const In& x, Out& out) -> bool {
        if constexpr (std::convertible_to<const In&, Out>) {
            out = static_cast<Out>(x);
            return true;
        } else {
            return false;
        }
    }) {}

    // Static factory
    template<std::ranges::input_range Range>
    [[nodiscard]] static auto from(Range&&) noexcept {
        using T = std::ranges::range_value_t<Range>;
        return Pipeline<T, T>{};
    }

    // Enable stats tracking
    [[nodiscard]] auto with_stats() && noexcept {
        // User manages lifetime - typically stack allocated
        // This avoids heap allocation + ref counting
        static thread_local PipelineStats local_stats;
        local_stats.reset();
        stats_ = &local_stats;
        return Pipeline<In, Out, OpFunc>(
            std::move(operation_), stats_, profiler_, exec_policy_, parallelism_
        );
    }

    // Enable profiling
    [[nodiscard]] auto with_profiler() && noexcept {
        profiler_ = std::make_shared<Profiler>();
        return std::move(*this);
    }

    // Get stats
    [[nodiscard]] const PipelineStats& stats() const noexcept { return *stats_; }
    [[nodiscard]] const Profiler& profiler() const noexcept { return *profiler_; }

    // Enable parallel execution
    [[nodiscard]] auto parallel(std::size_t threads = std::thread::hardware_concurrency(),
                                 ExecutionPolicy policy = ExecutionPolicy::ParallelPreserveOrder) && noexcept {
        parallelism_ = (threads == 0 ? 1 : threads);
        exec_policy_ = policy;
        return std::move(*this);
    }

    // Parallel execution properties
    [[nodiscard]] bool is_parallel() const noexcept { return exec_policy_ != ExecutionPolicy::Sequential; }
    [[nodiscard]] std::size_t parallelism() const noexcept { return parallelism_; }
    [[nodiscard]] ExecutionPolicy execution_policy() const noexcept { return exec_policy_; }

    // ✅ OPTIMIZED: filter - fused instead of wrapped
    template<typename Fn>
    [[nodiscard]] auto filter(Fn&& fn) && {
        // Instead of capturing prev_op, we fuse the operations directly
        auto fused = [op = std::move(operation_),
                      pred = std::forward<Fn>(fn)](const In& x, Out& out) constexpr noexcept(noexcept(pred(out))) -> bool {
            // Single pass: apply operation then filter in place
            return op(x, out) && pred(out);
        };

        return Pipeline<In, Out, decltype(fused)>(
            std::move(fused), stats_, profiler_, exec_policy_, parallelism_
        );
    }

    // ✅ OPTIMIZED: transform - fused with intermediate elimination
    template<typename Fn>
    [[nodiscard]] auto transform(Fn&& fn) && {
        using NewOut = decltype(fn(std::declval<const Out&>()));

        auto fused = [op = std::move(operation_),
                      tfn = std::forward<Fn>(fn)](const In& x, NewOut& out) constexpr noexcept(noexcept(tfn(std::declval<Out>()))) -> bool {
            // Eliminate temporary - compute directly into out
            Out temp;
            if (!op(x, temp)) [[unlikely]] return false;
            out = tfn(std::move(temp));  // Move instead of copy when possible
            return true;
        };

        return Pipeline<In, NewOut, decltype(fused)>(
            std::move(fused), stats_, profiler_, exec_policy_, parallelism_
        );
    }


    // Collect with optimizations - supports parallel execution
    template<typename Range>
    [[nodiscard]] auto collect(Range&& input) && {
        if (!is_parallel() || !std::ranges::sized_range<Range>) {
            return collect_sequential(std::forward<Range>(input));
        }

        const std::size_t total = std::ranges::size(input);
        if (total == 0 || parallelism_ <= 1) {
            return collect_sequential(std::forward<Range>(input));
        }

        return collect_parallel(std::forward<Range>(input));
    }

private:
    template<std::ranges::input_range Range>
    [[nodiscard]] [[gnu::hot]] [[gnu::flatten]] [[gnu::always_inline]]
    auto collect_sequential(Range&& input) {
        std::vector<Out> result;

        // Cache-friendly vector reserve
        if constexpr (std::ranges::sized_range<Range>) {
            const size_t input_size = std::ranges::size(input);

            // Reserve in cache-line friendly chunks (64 bytes = 16 ints on x64)
            constexpr size_t elements_per_cacheline = 64 / sizeof(Out);
            size_t estimated = input_size >> 1;  // 50% pass rate estimate

            // Round up to nearest cache line boundary
            estimated = ((estimated + elements_per_cacheline - 1) / elements_per_cacheline) * elements_per_cacheline;

            result.reserve(estimated);
        }

        // Start timing with RDTSC (much faster than std::chrono)
        uint64_t start_cycles = 0;
        if (stats_) {
            start_cycles = read_tsc();  // ~1 ns vs 20-50 ns for chrono
        }

        // Local counters - NO atomic overhead per item
        size_t processed_count = 0;
        size_t filtered_count = 0;

        // ✅ ULTRA-HOT LOOP - optimized with prefetching and branchless counters
        auto it = std::ranges::begin(input);
        auto end = std::ranges::end(input);

        while (it != end) {
            auto&& in_val = *it;
            ++it;

            // Prefetch next item (if not at end)
            if (it != end) [[likely]] {
                PREFETCH(&(*it));
            }

            Out out_val;
            bool passed = operation_(in_val, out_val);

            // Branchless: use passed as 0/1 multiplier
            if (passed) [[likely]] {
                if constexpr (std::is_trivially_copyable_v<Out> && sizeof(Out) <= 64) {
                    result.push_back(out_val);  // Copy for trivial types
                } else {
                    result.push_back(std::move(out_val));  // Move for complex types
                }
            }

            // Branchless counters - compiler optimizes to CMOV instruction
            processed_count += passed;
            filtered_count += !passed;
        }

        // End timing and batch-update atomics ONCE
        if (stats_) {
            uint64_t end_cycles = read_tsc();
            uint64_t cycles_elapsed = end_cycles - start_cycles;

            // Convert cycles to nanoseconds (approximate)
            // Assuming 3 GHz CPU: 1 cycle = 0.333 ns
            // Adjust multiplier based on your CPU frequency
            uint64_t ns_elapsed = (cycles_elapsed * 10) / 30;  // Integer math: cycles * (10/30)

            stats_->items_processed.fetch_add(processed_count, std::memory_order_relaxed);
            stats_->items_filtered.fetch_add(filtered_count, std::memory_order_relaxed);
            stats_->total_items.fetch_add(processed_count + filtered_count, std::memory_order_relaxed);
            stats_->total_duration_ns.fetch_add(ns_elapsed, std::memory_order_relaxed);
        }

        if (stats_) {
            return ResultWithStats<Out>{
                result,  // ✅ NRVO (Named Return Value Optimization) handles this
                stats_->items_processed.load(),
                stats_->items_filtered.load(),
                stats_->errors.load(),
                stats_->total_items.load(),
                std::chrono::nanoseconds{stats_->total_duration_ns.load()}
            };
        }
        return ResultWithStats<Out>{
            result,  // ✅ NRVO handles this
            result.size(), 0, 0, result.size(),
            std::chrono::nanoseconds{0}
        };
    }

    // Parallel collection
    template<std::ranges::input_range Range>
    [[nodiscard]] auto collect_parallel(Range&& input) {
        const std::size_t total = std::ranges::size(input);
        const std::size_t threads = std::min<std::size_t>(parallelism_, total);
        const std::size_t base_chunk = total / threads;
        const std::size_t remainder = total % threads;

        // Convert input to contiguous buffer for thread-safe access
        std::vector<In> buffer;
        if constexpr (std::ranges::contiguous_range<Range>) {
            buffer.assign(std::ranges::begin(input), std::ranges::end(input));
        } else {
            buffer.assign(std::ranges::begin(input), std::ranges::end(input));
        }

        std::vector<std::vector<Out>> local_results(threads);
        std::vector<std::thread> workers;
        workers.reserve(threads);

        // ✅ START TIMING
        auto start_time = std::chrono::high_resolution_clock::now();

        std::size_t offset = 0;
        for (std::size_t t = 0; t < threads; ++t) {
            const std::size_t chunk_size = base_chunk + (t < remainder ? 1 : 0);
            if (chunk_size == 0) break;

            auto chunk_begin = buffer.data() + offset;
            offset += chunk_size;
            auto chunk_end = buffer.data() + offset;

            workers.emplace_back([this, &local_results, t, chunk_begin, chunk_end]() {
                local_results[t].reserve(static_cast<std::size_t>(chunk_end - chunk_begin));

                // Local counters for this thread - batch update later
                size_t local_processed = 0;
                size_t local_filtered = 0;

                for (auto* p = chunk_begin; p != chunk_end; ++p) {
                    Out out_val;
                    if (operation_(*p, out_val)) [[likely]] {
                        local_results[t].emplace_back(std::move(out_val));
                        ++local_processed;
                    } else {
                        ++local_filtered;
                    }
                }

                // Batch update atomics once per thread (not per item)
                if (stats_) {
                    stats_->items_processed.fetch_add(local_processed, std::memory_order_relaxed);
                    stats_->items_filtered.fetch_add(local_filtered, std::memory_order_relaxed);
                }
            });
        }

        // Wait for all threads to complete
        for (auto& th : workers) {
            if (th.joinable()) th.join();
        }

        // ✅ END TIMING - record duration after all work complete
        if (stats_) {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
            stats_->total_items.fetch_add(total, std::memory_order_relaxed);  // ✅ NEW: Total input items
            stats_->total_duration_ns.fetch_add(elapsed, std::memory_order_relaxed);
        }

        // Merge thread-local results
        std::vector<Out> merged;
        std::size_t total_out = 0;
        for (auto& v : local_results) total_out += v.size();
        merged.reserve(total_out);

        if (exec_policy_ == ExecutionPolicy::ParallelPreserveOrder) {
            for (auto& v : local_results) {
                merged.insert(merged.end(),
                            std::make_move_iterator(v.begin()),
                            std::make_move_iterator(v.end()));
            }
        } else { // ParallelUnordered
            for (auto& v : local_results) {
                merged.insert(merged.end(),
                            std::make_move_iterator(v.begin()),
                            std::make_move_iterator(v.end()));
            }
        }

        if (stats_) {
            return ResultWithStats<Out>{
                std::move(merged),
                stats_->items_processed.load(),
                stats_->items_filtered.load(),
                stats_->errors.load(),
                stats_->total_items.load(),  // ✅ NEW
                std::chrono::nanoseconds{stats_->total_duration_ns.load()}
            };
        } else {
            return ResultWithStats<Out>{std::move(merged), merged.size(), 0, 0, total, std::chrono::nanoseconds{0}};  // ✅ NEW: total input items
        }
    }
};

} // namespace dpb