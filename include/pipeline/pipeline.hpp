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
    std::chrono::nanoseconds total_duration{0};

    ResultWithStats(std::vector<T> d, size_t processed, size_t filtered, size_t errs, std::chrono::nanoseconds duration)
        : data(std::move(d)), items_processed(processed), items_filtered(filtered), errors(errs), total_duration(duration) {}

    void print_stats() const {
        std::cout << "=== Pipeline Statistics ===\n";
        std::cout << "Items processed: " << items_processed << "\n";
        std::cout << "Items filtered: " << items_filtered << "\n";
        std::cout << "Errors: " << errors << "\n";
        std::cout << "Total duration: "
                  << std::chrono::duration<double, std::milli>(total_duration).count()
                  << " ms\n";

        if (items_processed > 0) {
            auto latency_ns = total_duration.count() / items_processed;
            std::cout << "Average latency: " << latency_ns << " ns/item\n";

            double throughput = items_processed /
                (std::chrono::duration<double>(total_duration).count());
            std::cout << "Throughput: " << throughput << " items/sec\n";
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

template<typename In, typename Out = In>
class Pipeline {
    std::function<std::optional<Out>(const In&)> operation_;
    std::shared_ptr<PipelineStats> stats_;
    std::shared_ptr<Profiler>      profiler_;
    ExecutionPolicy                exec_policy_ = ExecutionPolicy::Sequential;
    std::size_t                    parallelism_ = std::thread::hardware_concurrency();

public:
    // Internal constructor for chaining operations (not for public use)
    Pipeline(std::function<std::optional<Out>(const In&)> op,
             std::shared_ptr<PipelineStats> stats,
             std::shared_ptr<Profiler> profiler,
             ExecutionPolicy exec_policy,
             std::size_t parallelism)
        : operation_(std::move(op)),
          stats_(stats),
          profiler_(profiler),
          exec_policy_(exec_policy),
          parallelism_(parallelism) {}
    Pipeline() : operation_([](const In& x) -> std::optional<Out> {
        if constexpr (std::convertible_to<const In&, Out>) {
            return static_cast<Out>(x);
        } else {
            return std::nullopt;
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
        stats_ = std::make_shared<PipelineStats>();
        return std::move(*this);
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

    // Add filter stage (on current Out)
    template<typename Fn>
    [[nodiscard]] auto filter(Fn&& fn) && {
        auto prev_op = std::move(operation_);
        auto new_op = [prev = std::move(prev_op),
                       f    = std::forward<Fn>(fn)](const In& x) -> std::optional<Out> {
            auto res = prev(x);
            if (res && f(*res)) {
                return res;
            } else {
                return std::nullopt;
            }
        };

        return Pipeline<In, Out>(std::move(new_op), stats_, profiler_, exec_policy_, parallelism_);
    }

    // Add transform stage (type change)
    template<typename Fn>
    [[nodiscard]] auto transform(Fn&& fn) && {
        using NewOut = decltype(fn(std::declval<const Out&>()));

        auto prev_op = std::move(operation_);
        auto new_op = [prev = std::move(prev_op),
                       f    = std::forward<Fn>(fn)](const In& x) -> std::optional<NewOut> {
            auto res = prev(x);
            if (res) {
                return std::optional<NewOut>{f(*res)};
            } else {
                return std::nullopt;
            }
        };

        return Pipeline<In, NewOut>(std::move(new_op), stats_, profiler_, exec_policy_, parallelism_);
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
    [[nodiscard]] auto collect_sequential(Range&& input) {
        std::vector<Out> result;
        if constexpr (std::ranges::sized_range<Range>) {
            result.reserve(std::ranges::size(input));
        }

        std::unique_ptr<ScopedTimer> timer;
        if (stats_) {
            timer = std::make_unique<ScopedTimer>(stats_->total_duration);
        }

        for (auto&& in_val : input) {
            auto res = operation_(in_val);
            if (res) {
                result.emplace_back(std::move(*res));
                if (stats_) stats_->items_processed++;
            } else if (stats_) {
                stats_->items_filtered++;
            }
        }

        if (stats_) {
            return ResultWithStats<Out>{
                std::move(result),
                stats_->items_processed.load(),
                stats_->items_filtered.load(),
                stats_->errors.load(),
                stats_->total_duration
            };
        }
        return ResultWithStats<Out>{
            std::move(result),
            result.size(), 0, 0,
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

                for (auto* p = chunk_begin; p != chunk_end; ++p) {
                    auto res = operation_(*p);
                    if (res) {
                        local_results[t].emplace_back(std::move(*res));
                        if (stats_) stats_->items_processed++;
                    } else {
                        if (stats_) stats_->items_filtered++;
                    }
                }
            });
        }

        // Wait for all threads to complete
        for (auto& th : workers) {
            if (th.joinable()) th.join();
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        if (stats_) {
            stats_->total_duration +=
                std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
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
                stats_->total_duration
            };
        } else {
            return ResultWithStats<Out>{std::move(merged), merged.size(), 0, 0, std::chrono::nanoseconds{0}};
        }
    }
};

} // namespace dpb