#pragma once

#include "concepts.hpp"
#include "pipeline_stats.hpp"
#include "profiler.hpp"
#include "../thread_pool.hpp"
#include "dpb/tracy_config.hpp"
#include "dpb/memory.hpp"
#include "dpb/pipeline_erase.hpp"
#include "dpb/portable_attrs.hpp"
#include "pipeline/operations/zip.hpp"
#include "pipeline/operations/join.hpp"

#ifdef DPB_HAS_HIGHWAY
#include "dpb/simd.hpp"
#endif

#include <vector>
#include <memory>
#include <ranges>
#include <algorithm>
#include <thread>
#include <optional>
#include <unordered_map>

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

template<typename T, typename Alloc = std::allocator<T>>
struct ResultWithStats {
    std::vector<T, Alloc> data;
    size_t items_processed{0};
    size_t items_filtered{0};
    size_t errors{0};
    size_t total_items{0};
    std::chrono::nanoseconds total_duration{0};

    ResultWithStats(std::vector<T, Alloc> d, size_t processed, size_t filtered, size_t errs, size_t total, std::chrono::nanoseconds duration)
        : data(std::move(d)), items_processed(processed), items_filtered(filtered), errors(errs), total_items(total), total_duration(duration) {}

    void print_stats() const {
        dpb::print("=== Pipeline Statistics ===\n");
        dpb::print("Items processed: {}\n", items_processed);
        dpb::print("Items filtered: {}\n", items_filtered);
        dpb::print("Errors: {}\n", errors);

        if (total_items > 0) {
            dpb::print("Total input items: {}\n", total_items);
            dpb::print("Pass rate: {:.2f}%\n", 100.0 * items_processed / total_items);
        }

        dpb::print("Total duration: {:.4f} ms\n", std::chrono::duration<double, std::milli>(total_duration).count());

        if (total_items > 0) {
            auto latency_ns = total_duration.count() / total_items;
            dpb::print("Average latency: {} ns/item (per input)\n", latency_ns);

            double throughput = total_items / std::chrono::duration<double>(total_duration).count();
            dpb::print("Throughput: {:.2f} items/sec\n", throughput);
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

    bool operator==(const std::vector<T, Alloc>& other) const { return data == other; }
    bool operator==(const ResultWithStats& other) const { return data == other.data; }
};

// ============================================================================
// DEFAULT OPERATION - Identity-like pass-through, default-constructible
// ============================================================================

struct DefaultOp {
    template<typename In, typename Out>
    bool operator()(const In& x, Out& out) const {
        if constexpr (std::is_convertible_v<const In&, Out>) {
            out = static_cast<Out>(x);
            return true;
        }
        return false;
    }
};

// ============================================================================
// GENERIC PIPELINE - Type-driven pipeline supporting arbitrary transformations
// ============================================================================

template<typename In, typename Out = In, typename OpFunc = DefaultOp, typename Alloc = std::allocator<Out>>
class Pipeline {
    OpFunc operation_;
    std::shared_ptr<PipelineStats> stats_;
    std::shared_ptr<Profiler> profiler_;
    ExecutionPolicy exec_policy_ = ExecutionPolicy::Sequential;
    std::size_t parallelism_ = std::thread::hardware_concurrency();  // chunk count, not pool worker count
    Alloc alloc_;

public:
    using allocator_type = Alloc;

#ifdef DPB_HAS_HIGHWAY
    template<typename I, typename O, typename F, std::ranges::input_range R>
    friend auto simd_collect_sequential(Pipeline<I, O, F>&& pipe, R&& input) -> ResultWithStats<O>;
#endif

public:
    template<typename F>
    Pipeline(F&& op,
             std::shared_ptr<PipelineStats> stats = nullptr,
             std::shared_ptr<Profiler> prof = nullptr,
             ExecutionPolicy exec_policy = ExecutionPolicy::Sequential,
             std::size_t parallelism = std::thread::hardware_concurrency(),
             Alloc alloc = Alloc())
        : operation_(std::forward<F>(op)),
          stats_(std::move(stats)),
          profiler_(std::move(prof)),
          exec_policy_(exec_policy),
          parallelism_(parallelism),
          alloc_(std::move(alloc)) {}

    Pipeline() : operation_(DefaultOp{}), alloc_() {}

    // Static factory
    template<std::ranges::input_range Range>
    [[nodiscard]] static auto from(Range&&) noexcept {
        using T = std::ranges::range_value_t<Range>;
        return Pipeline<T, T>{};
    }

    [[nodiscard]] auto with_stats() && {
        auto new_stats = std::make_shared<PipelineStats>();
        return Pipeline<In, Out, OpFunc, Alloc>(
            std::move(operation_), std::move(new_stats), profiler_, exec_policy_, parallelism_, alloc_
        );
    }

    [[nodiscard]] auto with_profiler() && {
        auto new_prof = std::make_shared<Profiler>();
        return Pipeline<In, Out, OpFunc, Alloc>(
            std::move(operation_), stats_, std::move(new_prof), exec_policy_, parallelism_, alloc_
        );
    }

    [[nodiscard]] const PipelineStats* stats() const noexcept { return stats_.get(); }
    [[nodiscard]] const Profiler* profiler() const noexcept { return profiler_.get(); }

    // Enable parallel execution.
    // Note: parallelism_ controls the number of data chunks (and thus effective
    // parallelism), not the thread pool size. The global thread pool has its own
    // fixed worker count (default: hardware_concurrency). Tasks are distributed
    // round-robin across all pool workers — if chunk count < worker count, some
    // workers idle; if chunk count > worker count, work-stealing balances load.
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

    // Fused filter: operation + predicate compose into single lambda
    template<typename Fn>
    [[nodiscard]] DPB_HOT auto filter(Fn&& fn) && {
        auto fused = [op = std::move(operation_),
                      pred = std::forward<Fn>(fn)](const In& x, Out& out) noexcept(noexcept(pred(out))) -> bool {
            return op(x, out) && pred(out);
        };

        return Pipeline<In, Out, decltype(fused), Alloc>(
            std::move(fused), stats_, profiler_, exec_policy_, parallelism_, alloc_
        );
    }

    // Fused transform: operation + transform compose into single lambda, no intermediate allocation
    template<typename Fn>
    [[nodiscard]] DPB_HOT auto transform(Fn&& fn) && {
        using NewOut = decltype(fn(std::declval<const Out&>()));
        using NewAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<NewOut>;

        auto fused = [op = std::move(operation_),
                      tfn = std::forward<Fn>(fn)](const In& x, NewOut& out) noexcept(noexcept(tfn(std::declval<Out>()))) -> bool {
            Out temp;
            if (DPB_UNLIKELY(!op(x, temp))) return false;
            out = tfn(std::move(temp));
            return true;
        };

        return Pipeline<In, NewOut, decltype(fused), NewAlloc>(
            std::move(fused), stats_, profiler_, exec_policy_, parallelism_,
            typename std::allocator_traits<Alloc>::template rebind_alloc<NewOut>(alloc_)
        );
    }

    // ── Aliases ──────────────────────────────────────────────────────────

    template<typename Fn>
    [[nodiscard]] auto map(Fn&& fn) && {
        return std::move(*this).transform(std::forward<Fn>(fn));
    }

    template<typename Fn>
    [[nodiscard]] auto where(Fn&& fn) && {
        return std::move(*this).filter(std::forward<Fn>(fn));
    }

    // ── Stream control ───────────────────────────────────────────────────

    [[nodiscard]] auto take(std::size_t n) && {
        auto fused = [op = std::move(operation_),
                      remaining = n](const In& x, Out& out) mutable -> bool {
            if (remaining == 0) return false;
            --remaining;
            return op(x, out);
        };
        return Pipeline<In, Out, decltype(fused), Alloc>(
            std::move(fused), stats_, profiler_, exec_policy_, parallelism_, alloc_
        );
    }

    [[nodiscard]] auto skip(std::size_t n) && {
        auto fused = [op = std::move(operation_),
                      skip_count = n](const In& x, Out& out) mutable -> bool {
            if (skip_count > 0) {
                --skip_count;
                return false;
            }
            return op(x, out);
        };
        return Pipeline<In, Out, decltype(fused), Alloc>(
            std::move(fused), stats_, profiler_, exec_policy_, parallelism_, alloc_
        );
    }

    template<typename Fn>
    [[nodiscard]] auto take_while(Fn&& pred) && {
        auto fused = [op = std::move(operation_),
                      pred = std::forward<Fn>(pred),
                      active = true](const In& x, Out& out) mutable -> bool {
            if (!active) return false;
            if (!op(x, out)) return false;
            if (!pred(out)) { active = false; return false; }
            return true;
        };
        return Pipeline<In, Out, decltype(fused), Alloc>(
            std::move(fused), stats_, profiler_, exec_policy_, parallelism_, alloc_
        );
    }

    template<typename Fn>
    [[nodiscard]] auto skip_while(Fn&& pred) && {
        auto fused = [op = std::move(operation_),
                      pred = std::forward<Fn>(pred),
                      skipping = true](const In& x, Out& out) mutable -> bool {
            if (!op(x, out)) return false;
            if (skipping && pred(out)) return false;
            skipping = false;
            return true;
        };
        return Pipeline<In, Out, decltype(fused), Alloc>(
            std::move(fused), stats_, profiler_, exec_policy_, parallelism_, alloc_
        );
    }

    // ── Side-effect tap ──────────────────────────────────────────────────

    template<typename Fn>
    [[nodiscard]] auto tee(Fn&& fn) && {
        auto fused = [op = std::move(operation_),
                      side_effect = std::forward<Fn>(fn)](const In& x, Out& out) mutable -> bool {
            if (!op(x, out)) return false;
            side_effect(out);
            return true;
        };
        return Pipeline<In, Out, decltype(fused), Alloc>(
            std::move(fused), stats_, profiler_, exec_policy_, parallelism_, alloc_
        );
    }

    template<typename Fn>
    [[nodiscard]] auto inspect(Fn&& fn) && {
        return std::move(*this).tee(std::forward<Fn>(fn));
    }

    // ── Terminal reductions ──────────────────────────────────────────────

    template<std::ranges::input_range Range, typename Acc, typename Fn>
    [[nodiscard]] auto fold(Range&& input, Acc initial, Fn&& fn) && {
        Acc acc = std::move(initial);
        auto it = std::ranges::begin(input);
        auto end = std::ranges::end(input);
        while (it != end) {
            Out out_val;
            if (operation_(*it++, out_val)) {
                acc = fn(std::move(acc), std::move(out_val));
            }
        }
        return acc;
    }

    template<std::ranges::input_range Range>
    [[nodiscard]] auto count(Range&& input) && {
        size_t c = 0;
        auto it = std::ranges::begin(input);
        auto end = std::ranges::end(input);
        while (it != end) {
            Out out_val;
            if (operation_(*it++, out_val)) ++c;
        }
        return c;
    }

    template<std::ranges::input_range Range>
    [[nodiscard]] auto sum(Range&& input) && {
        return std::move(*this).fold(std::forward<Range>(input), Out{}, std::plus<>{});
    }

    template<std::ranges::input_range Range, typename Comp = std::less<Out>>
    [[nodiscard]] auto min(Range&& input, Comp comp = {}) && {
        return std::move(*this).fold(std::forward<Range>(input),
            std::optional<Out>{},
            [&comp](std::optional<Out> acc, Out val) {
                return (!acc || comp(val, *acc)) ? std::optional<Out>(std::move(val)) : acc;
            });
    }

    template<std::ranges::input_range Range, typename Comp = std::less<Out>>
    [[nodiscard]] auto max(Range&& input, Comp comp = {}) && {
        return std::move(*this).fold(std::forward<Range>(input),
            std::optional<Out>{},
            [&comp](std::optional<Out> acc, Out val) {
                return (!acc || comp(*acc, val)) ? std::optional<Out>(std::move(val)) : acc;
            });
    }

    template<std::ranges::input_range Range, typename Fn>
    [[nodiscard]] bool all_of(Range&& input, Fn&& pred) && {
        auto it = std::ranges::begin(input);
        auto end = std::ranges::end(input);
        while (it != end) {
            Out out_val;
            if (operation_(*it++, out_val)) {
                if (!pred(out_val)) return false;
            }
        }
        return true;
    }

    template<std::ranges::input_range Range, typename Fn>
    [[nodiscard]] bool any_of(Range&& input, Fn&& pred) && {
        auto it = std::ranges::begin(input);
        auto end = std::ranges::end(input);
        while (it != end) {
            Out out_val;
            if (operation_(*it++, out_val)) {
                if (pred(out_val)) return true;
            }
        }
        return false;
    }

    template<std::ranges::input_range Range, typename Fn>
    [[nodiscard]] bool none_of(Range&& input, Fn&& pred) && {
        return !std::move(*this).any_of(std::forward<Range>(input), std::forward<Fn>(pred));
    }

    // ── Collect to vector (no stats wrapper) ─────────────────────────────

    template<typename Range>
    [[nodiscard]] auto to_vector(Range&& input) && {
        return std::move(*this).collect(std::forward<Range>(input)).data;
    }

    // Collect with post-processing: sort
    template<std::ranges::input_range Range, typename Comp = std::less<Out>>
    [[nodiscard]] auto collect_sorted(Range&& input, Comp comp = {}) && {
        auto result = std::move(*this).collect(std::forward<Range>(input));
        std::sort(result.data.begin(), result.data.end(), comp);
        return result;
    }

    // Collect with post-processing: distinct
    template<std::ranges::input_range Range>
    [[nodiscard]] auto collect_distinct(Range&& input) && {
        auto result = std::move(*this).collect(std::forward<Range>(input));
        std::sort(result.data.begin(), result.data.end());
        auto new_last = std::unique(result.data.begin(), result.data.end());
        result.data.erase(new_last, result.data.end());
        return result;
    }

    // ── Collect with output iterator (zero-copy sink) ────────────────────
    template<std::ranges::input_range Range, std::output_iterator<Out> OutIter>
    [[nodiscard]] OutIter collect_into(Range&& input, OutIter out) && {
        auto it = std::ranges::begin(input);
        auto end = std::ranges::end(input);
        while (it != end) {
            Out out_val;
            if (operation_(*it++, out_val)) {
                *out++ = std::move(out_val);
            }
        }
        return out;
    }

    // ── Flat map (one-to-many transform, terminal) ────────────────────────

    template<std::ranges::input_range Range, typename Fn>
    [[nodiscard]] auto flat_map(Range&& input, Fn&& fn) && {
        using MappedRange = decltype(fn(std::declval<Out>()));
        using MappedType = std::ranges::range_value_t<MappedRange>;
        std::vector<MappedType> result;
        auto it = std::ranges::begin(input);
        auto end = std::ranges::end(input);
        while (it != end) {
            Out out_val;
            if (operation_(*it++, out_val)) {
                auto mapped = fn(std::move(out_val));
                result.insert(result.end(),
                              std::ranges::begin(mapped),
                              std::ranges::end(mapped));
            }
        }
        return result;
    }

    // ── Scan (prefix accumulation) ────────────────────────────────────────

    template<std::ranges::input_range Range, typename Acc, typename Fn>
    [[nodiscard]] auto scan(Range&& input, Acc initial, Fn&& fn) && {
        std::vector<Acc> result;
        Acc acc = std::move(initial);
        auto it = std::ranges::begin(input);
        auto end = std::ranges::end(input);
        while (it != end) {
            Out out_val;
            if (operation_(*it++, out_val)) {
                acc = fn(acc, std::move(out_val));
            }
            result.push_back(acc);
        }
        return result;
    }

    // ── Enumerate: pairs each item with its index ──────────────────────

    [[nodiscard]] auto enumerate() && {
        using EnumOut = std::pair<size_t, Out>;
        using EnumAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<EnumOut>;

        auto fused = [op = std::move(operation_), idx = size_t(0)](const In& x, EnumOut& out) mutable -> bool {
            if (!op(x, out.second)) return false;
            out.first = idx++;
            return true;
        };
        return Pipeline<In, EnumOut, decltype(fused), EnumAlloc>(
            std::move(fused), stats_, profiler_, exec_policy_, parallelism_,
            typename std::allocator_traits<Alloc>::template rebind_alloc<EnumOut>(alloc_)
        );
    }

    // ── Unique: removes consecutive duplicates (fused) ─────────────────

    [[nodiscard]] auto unique() && {
        auto fused = [op = std::move(operation_), last = std::optional<Out>{}](const In& x, Out& out) mutable -> bool {
            if (!op(x, out)) return false;
            if (last && *last == out) return false;
            last = out;
            return true;
        };
        return Pipeline<In, Out, decltype(fused), Alloc>(
            std::move(fused), stats_, profiler_, exec_policy_, parallelism_, alloc_
        );
    }

    // ── Dedup: alias for unique() ──────────────────────────────────────

    [[nodiscard]] auto dedup() && {
        return std::move(*this).unique();
    }

    // ── Chunk: groups items into chunks of size n (terminal) ───────────

    template<std::ranges::input_range Range>
    [[nodiscard]] auto chunk(Range&& input, size_t chunk_size) && {
        std::vector<std::vector<Out>> result;
        std::vector<Out> current_chunk;
        auto it = std::ranges::begin(input);
        auto end = std::ranges::end(input);
        while (it != end) {
            Out out_val;
            if (operation_(*it++, out_val)) {
                current_chunk.push_back(std::move(out_val));
                if (current_chunk.size() == chunk_size) {
                    result.push_back(std::move(current_chunk));
                    current_chunk = std::vector<Out>();
                }
            }
        }
        if (!current_chunk.empty()) result.push_back(std::move(current_chunk));
        return result;
    }

    // ── Window: sliding window of size n (terminal) ────────────────────

    template<std::ranges::input_range Range>
    [[nodiscard]] auto window(Range&& input, size_t window_size) && {
        std::vector<std::vector<Out>> result;
        std::vector<Out> buffer;
        auto it = std::ranges::begin(input);
        auto end = std::ranges::end(input);
        while (it != end) {
            Out out_val;
            if (operation_(*it++, out_val)) {
                buffer.push_back(std::move(out_val));
                if (buffer.size() >= window_size) {
                    std::vector<Out> win(buffer.end() - window_size, buffer.end());
                    result.push_back(std::move(win));
                }
            }
        }
        return result;
    }

    // ── Group by: groups consecutive items by key (terminal) ───────────

    template<std::ranges::input_range Range, typename Fn>
    [[nodiscard]] auto group_by(Range&& input, Fn&& key_fn) && {
        using Key = decltype(key_fn(std::declval<const Out&>()));
        std::vector<std::pair<Key, std::vector<Out>>> result;
        auto it = std::ranges::begin(input);
        auto end = std::ranges::end(input);
        while (it != end) {
            Out out_val;
            if (operation_(*it++, out_val)) {
                Key key = key_fn(out_val);
                if (result.empty() || result.back().first != key) {
                    result.emplace_back(std::move(key), std::vector<Out>{});
                }
                result.back().second.push_back(std::move(out_val));
            }
        }
        return result;
    }

    // ── Reverse: reverses the collected output (terminal) ──────────────

    template<std::ranges::input_range Range>
    [[nodiscard]] auto reverse(Range&& input) && {
        auto collected = std::move(*this).collect(std::forward<Range>(input));
        std::reverse(collected.data.begin(), collected.data.end());
        return collected;
    }

    // ── For each: applies fn to each item, returns void (terminal) ─────

    template<std::ranges::input_range Range, typename Fn>
    void for_each(Range&& input, Fn&& fn) && {
        auto it = std::ranges::begin(input);
        auto end = std::ranges::end(input);
        while (it != end) {
            Out out_val;
            if (operation_(*it++, out_val)) {
                fn(std::move(out_val));
            }
        }
    }

    // ── Zip: pairs pipeline output with another range (terminal) ────────

    template<std::ranges::input_range Range, std::ranges::input_range OtherRange>
    [[nodiscard]] auto zip(Range&& input, OtherRange&& other) && {
        using OtherT = std::ranges::range_value_t<OtherRange>;
        using ResultType = std::pair<Out, OtherT>;

        std::vector<ResultType> result;

        auto it = std::ranges::begin(input);
        auto end = std::ranges::end(input);
        auto other_it = std::ranges::begin(other);
        auto other_end = std::ranges::end(other);

        while (it != end && other_it != other_end) {
            Out out_val;
            if (operation_(*it++, out_val)) {
                result.emplace_back(std::move(out_val), *other_it++);
            }
        }

        return result;
    }

    // ── Join: inner join with another range on matching key (terminal) ──

    template<std::ranges::input_range Range, std::ranges::input_range OtherRange, typename KeyFn>
    [[nodiscard]] auto join(Range&& input, OtherRange&& other, KeyFn&& key_fn) && {
        using OtherT = std::ranges::range_value_t<OtherRange>;
        using Key = decltype(key_fn(std::declval<const Out&>()));
        using ResultType = std::pair<Out, OtherT>;

        // Build hash map from other range (hash join build phase)
        std::unordered_multimap<Key, OtherT> other_map;
        for (auto&& elem : other) {
            other_map.emplace(key_fn(elem), elem);
        }

        std::vector<ResultType> result;

        auto it = std::ranges::begin(input);
        auto end = std::ranges::end(input);

        while (it != end) {
            Out out_val;
            if (operation_(*it++, out_val)) {
                auto key = key_fn(out_val);
                auto range = other_map.equal_range(key);
                for (auto map_it = range.first; map_it != range.second; ++map_it) {
                    result.emplace_back(out_val, map_it->second);
                }
            }
        }

        return result;
    }

    // ── First: returns optional<Out> - first matching item (terminal) ──

    template<std::ranges::input_range Range>
    [[nodiscard]] auto first(Range&& input) && -> std::optional<Out> {
        auto it = std::ranges::begin(input);
        auto end = std::ranges::end(input);
        while (it != end) {
            Out out_val;
            if (operation_(*it++, out_val)) {
                return out_val;
            }
        }
        return std::nullopt;
    }

    // ── Last: returns optional<Out> - last matching item (terminal) ────

    template<std::ranges::input_range Range>
    [[nodiscard]] auto last(Range&& input) && -> std::optional<Out> {
        std::optional<Out> result;
        auto it = std::ranges::begin(input);
        auto end = std::ranges::end(input);
        while (it != end) {
            Out out_val;
            if (operation_(*it++, out_val)) {
                result = std::move(out_val);
            }
        }
        return result;
    }

    // ── Type-erased conversion ──────────────────────────────────────────
    [[nodiscard]] auto erase() && {
        return PipelineErase<In, Out>(std::move(operation_));
    }

    // ── Collect ──────────────────────────────────────────────────────────
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
    [[nodiscard]] DPB_HOT DPB_FLATTEN DPB_INLINE
    auto collect_sequential(Range&& input) {
        DPB_ZONE_SCOPED;
        size_t estimated = 0;
        
        if constexpr (std::ranges::sized_range<Range>) {
            estimated = std::ranges::size(input);
        }
        
        dpb::collect_buffer<Out, Alloc> result(estimated, alloc_);

        auto start_time = std::chrono::high_resolution_clock::now();

        size_t processed_count = 0;
        size_t filtered_count = 0;
        auto it = std::ranges::begin(input);
        auto end = std::ranges::end(input);

        if (!stats_) {
#ifdef DPB_HAS_HIGHWAY
            if constexpr (simd::simd_numeric<In> && simd::simd_numeric<Out> && std::same_as<Alloc, std::allocator<Out>>) {
                return simd_collect_sequential(std::move(*this), std::forward<Range>(input));
            }
#endif

            while (it != end) {
                auto&& in_val = *it;
                ++it;

                Out out_val;
                if (DPB_LIKELY(operation_(in_val, out_val))) {
                    result.emplace_back(std::move(out_val));
                }
            }

            const std::size_t result_size = result.size();
            return ResultWithStats<Out, Alloc>{
                std::move(result),
                result_size, 0, 0, result_size,
                std::chrono::nanoseconds{0}
            };
        }

        while (it != end) {
            auto&& in_val = *it;
            ++it;

            Out out_val;
            bool passed = operation_(in_val, out_val);

            if (DPB_LIKELY(passed)) {
                result.emplace_back(std::move(out_val));
            }

            processed_count += passed;
            filtered_count += !passed;
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();

        stats_->items_processed.fetch_add(processed_count, std::memory_order_relaxed);
        stats_->items_filtered.fetch_add(filtered_count, std::memory_order_relaxed);
        stats_->total_items.fetch_add(processed_count + filtered_count, std::memory_order_relaxed);
        stats_->total_duration_ns.fetch_add(elapsed, std::memory_order_relaxed);

        DPB_FRAME_MARK;
        return ResultWithStats<Out, Alloc>{
            std::move(result),
            stats_->items_processed.load(),
            stats_->items_filtered.load(),
            stats_->errors.load(),
            stats_->total_items.load(),
            std::chrono::nanoseconds{stats_->total_duration_ns.load()}
        };
    }

    // Parallel collection using thread pool
    template<std::ranges::input_range Range>
    [[nodiscard]] auto collect_parallel(Range&& input) {
        DPB_ZONE_SCOPED;
        const std::size_t total = std::ranges::size(input);
        const std::size_t num_threads = std::min<std::size_t>(parallelism_, total);
        const std::size_t base_chunk = total / num_threads;
        const std::size_t remainder = total % num_threads;

        // Thread-safe access: for contiguous ranges use data pointer to avoid copy
        const In* data_ptr;
        std::vector<In> noncontiguous_buffer;
        if constexpr (std::ranges::contiguous_range<Range>) {
            data_ptr = std::ranges::data(input);
        } else {
            noncontiguous_buffer.assign(std::ranges::begin(input), std::ranges::end(input));
            data_ptr = noncontiguous_buffer.data();
        }

        std::vector<std::vector<Out, Alloc>> local_results;
        local_results.reserve(num_threads);
        for (std::size_t i = 0; i < num_threads; ++i) {
            local_results.emplace_back(alloc_);
        }
        auto& pool = global_thread_pool();
        std::vector<std::future<void>> futures;
        futures.reserve(num_threads);

        auto start_time = std::chrono::high_resolution_clock::now();

        std::size_t offset = 0;
        for (std::size_t t = 0; t < num_threads; ++t) {
            const std::size_t chunk_size = base_chunk + (t < remainder ? 1 : 0);
            if (chunk_size == 0) continue;

            auto chunk_begin = data_ptr + offset;
            offset += chunk_size;
            auto chunk_end = data_ptr + offset;

            futures.push_back(pool.enqueue([this, &local_results, t, chunk_begin, chunk_end, chunk_size]() {
                local_results[t].reserve(chunk_size);

                size_t local_processed = 0;
                size_t local_filtered = 0;

                for (auto* p = chunk_begin; p != chunk_end; ++p) {
                    Out out_val;
                    if (DPB_LIKELY(operation_(*p, out_val))) {
                        local_results[t].emplace_back(std::move(out_val));
                        ++local_processed;
                    } else {
                        ++local_filtered;
                    }
                }

                if (stats_) {
                    stats_->items_processed.fetch_add(local_processed, std::memory_order_relaxed);
                    stats_->items_filtered.fetch_add(local_filtered, std::memory_order_relaxed);
                }
            }));
        }

        for (auto& f : futures) f.get();

        if (stats_) {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
            stats_->total_items.fetch_add(total, std::memory_order_relaxed);
            stats_->total_duration_ns.fetch_add(elapsed, std::memory_order_relaxed);
        }

        // Merge thread-local results — O(n), single pass over all sub-results.
        // reserve(total_out) prevents reallocation; insert(make_move_iterator)
        // uses move semantics for optimal bulk transfer. For trivially copyable
        // types the compiler reduces this to memmove. Future optimization:
        // resize_and_overwrite (C++23) + std::uninitialized_move would avoid
        // move-assign for non-trivial types, but requires broad compiler support.
        std::vector<Out, Alloc> merged(alloc_);
        std::size_t total_out = 0;
        for (auto& v : local_results) total_out += v.size();
        merged.reserve(total_out);

        for (auto& v : local_results) {
            merged.insert(merged.end(),
                          std::make_move_iterator(v.begin()),
                          std::make_move_iterator(v.end()));
        }

        DPB_FRAME_MARK;
        if (stats_) {
            return ResultWithStats<Out, Alloc>{
                std::move(merged),
                stats_->items_processed.load(),
                stats_->items_filtered.load(),
                stats_->errors.load(),
                stats_->total_items.load(),
                std::chrono::nanoseconds{stats_->total_duration_ns.load()}
            };
        }
        const std::size_t merged_size = merged.size();
        return ResultWithStats<Out, Alloc>{
            std::move(merged),
            merged_size, 0, 0, total,
            std::chrono::nanoseconds{0}
        };
    }
};

// ── Free function: convenience entry point ───────────────────────────────────

template<std::ranges::input_range Range>
[[nodiscard]] auto from(Range&&) noexcept {
    using T = std::ranges::range_value_t<Range>;
    return Pipeline<T, T>{};
}

} // namespace dpb

#ifdef DPB_HAS_HIGHWAY
#include "pipeline/simd_collect.hpp"
#endif
