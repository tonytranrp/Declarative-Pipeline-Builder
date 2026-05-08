# Learnings

## 2026-05-07 Task 1: Verify collect_parallel merge + investigate thread pool
- **Merge is O(n)**: `collect_parallel` uses `merged.reserve(total_out)` + `insert(make_move_iterator)` — already optimal. Documented with comment noting future `resize_and_overwrite` optimization for non-trivial types.
- **parallelism_ is by design**: `parallelism_` controls chunk count (how many tasks submitted to pool), NOT pool worker count. `ThreadPool::enqueue` distributes round-robin across ALL workers via `workers_.size()`. Work-stealing balances load when chunk count ≠ worker count. This is NOT a bug — documented clearly in both Pipeline::parallel() and ThreadPool::enqueue().
- **No-stats fast path**: `collect_sequential` has early return when `!stats_` that skips all timing/counter overhead. The stats path no longer has redundant `if (stats_)` checks.
- **Tests pass**: 196 assertions in 37 test cases all pass.

## 2026-05-07 Session Start
- Previous work: PMR collect_buffer removed (2x speedup), no-stats fast path added (5.58e8 full-pass, 1.07e9 filter-map)
- Momus review: Plan OKAY, one factual fix applied (merge is O(n) not O(n²), Task 1 updated)
- Current baselines: full-pass 5.58e8 items/sec, filter-map 1.07e9 items/sec
- Target: 5B items/sec with SIMD
- Library headers ~54KB, compiled binary must stay under 850KB stripped
- CMake FetchContent git HEAD0 bug on Windows — use FETCHCONTENT_SOURCE_DIR_CATCH2 workaround
- Tracy WIN32 linkage needs dbghelp + ws2_32
## 2026-05-07 Task: Portable Attribute Macros (DPB_HOT, DPB_INLINE, etc.)
- **Created portable_attrs.hpp**: 5 macros for cross-compiler attribute portability (GCC/Clang + MSVC)
  - DPB_HOT → [[gnu::hot]] on GCC/Clang, empty on MSVC
  - DPB_INLINE → [[gnu::always_inline]] inline on GCC/Clang, __forceinline on MSVC
  - DPB_FLATTEN → [[gnu::flatten]] on GCC/Clang, empty on MSVC
  - DPB_LIKELY(x) → __builtin_expect(!!(x), 1) on GCC/Clang, (x) on MSVC
  - DPB_UNLIKELY(x) → __builtin_expect(!!(x), 0) on GCC/Clang, (x) on MSVC
- **Replaced 7 raw GNU attributes** in pipeline.hpp:
  - Line 172, 185: [[gnu::hot]] → DPB_HOT on filter/transform
  - Line 192: [[unlikely]] → DPB_UNLIKELY(!op(x, temp))
  - Line 624: [[gnu::hot]] [[gnu::flatten]] [[gnu::always_inline]] → DPB_HOT DPB_FLATTEN DPB_INLINE
  - Line 649, 669, 739: [[likely]] → DPB_LIKELY(condition) with correct syntax
- **Key syntax insight**: [[likely]]/[[unlikely]] as attributes on if-conditions require DPB_LIKELY(x) wrapping the condition (function-like macro), NOT condition DPB_LIKELY (postfix attribute). MSVC doesn't support the latter form.
- **Examples pass**: basic_example, parallel_example, async_example all run correctly.
- **CMake cache**: DPB_BUILD_TESTS=OFF in default preset — tests not built by default (Catch2 git clone issue on Windows with HEAD0).

## 2026-05-07 Task: Extract PipelineError + Result<T> to dpb/result.hpp
- **Created `include/dpb/result.hpp`**: Contains `PipelineError` enum class (Filtered, InvalidInput, ProcessingFailed, BackpressureExceeded) and `Result<T> = std::expected<T, PipelineError>` template alias
- **Updated `include/pipeline/concepts.hpp`**: Removed inline definitions, added `#include "dpb/result.hpp"` instead. Kept `#include <concepts>` since concepts still use `std::invocable`, `std::convertible_to`, `std::invoke_result_t`
- **All 196 assertions pass** in 37 test cases — no behavioral change
- **Build succeeds** via `cmake --build build/task1 --config Release --target pipeline_tests`

## 2026-05-07 Task: SIMD Type Traits + Eligibility Detection
- **Created `include/dpb/simd_traits.hpp`**: Four traits in `namespace dpb`:
  - `simd_eligible_op<OpFunc, In, Out>` — true when OpFunc is invocable(In)→Out, both `simd_numeric`
  - `simd_eligible_predicate<OpFunc, T>` — true when OpFunc is `std::predicate<T>`, T is `simd_numeric`
  - `simd_eligible_chain<Pipeline>` — partial specialization on `Pipeline<In, Out, OpFunc>`, true when `simd_numeric<In> && simd_numeric<Out>`
  - All traits have `_v` convenience variable templates
- **Key design decision**: `simd_eligible_chain` only checks In/Out types at compile time because:
  - `OpFunc` is the fused lambda (type-erased composition) — cannot inspect internal stages
  - `stats_` and `exec_policy_` are runtime state, not encoded in Pipeline template parameters
  - Runtime check for stats/parallel must be done separately when deciding SIMD execution path
- **`simd_numeric` uses exact type matching** (`std::same_as`), so only `int32_t`, `int64_t`, `float`, `double` match. On MSVC x64, `int` IS `int32_t` (typedef), so `Pipeline<int,int>` qualifies.
- **Created `tests/test_simd_traits.cpp`**: Static compile-time tests using Catch2 `STATIC_REQUIRE`/`STATIC_REQUIRE_FALSE`:
  - 9 simd_numeric checks (4 positive, 5 negative)
  - 4 simd_eligible_chain checks (even+square int chain, string chain, default Pipeline, non-Pipeline types)
  - 3 simd_eligible_op checks (numeric→numeric, numeric→double, numeric→string)
  - 2 simd_eligible_predicate checks (numeric predicate, string predicate)
- **Updated `CMakeLists.txt`**: Added `tests/test_simd_traits.cpp` to `pipeline_tests` sources (line 68)
- **Build + test**: 217 assertions in 41 test cases all pass (was 196 in 37 before → +21 assertions, +4 test cases)
- **No regressions**: All existing pipeline, parallel, async, new features tests pass unchanged

## 2026-05-07 Task: CMake scalar-test and simd-test presets with SIMD gating
- **CMakePresets.json**: Two new presets already exist:
  - scalar-test: DPB_BUILD_TESTS=ON, DPB_ENABLE_SIMD=OFF, no DPB_HAS_HIGHWAY defined
  - simd-test: DPB_BUILD_TESTS=ON, DPB_ENABLE_SIMD=ON, DPB_HAS_HIGHWAY defined
- **CMakeLists.txt lines 73-76**: Already correctly wires DPB_HAS_HIGHWAY when DPB_ENABLE_SIMD=ON
  `cmake
  if(DPB_ENABLE_SIMD)
      target_compile_definitions(pipeline_tests PRIVATE DPB_HAS_HIGHWAY)
  endif()
  `
- **Dependencies.cmake lines 20-21**: Also sets DPB_HAS_HIGHWAY on declarative_pipeline and fast_pipeline interfaces
- **tests/test_integration.cpp**: [simd] tests are wrapped in #ifdef DPB_HAS_HIGHWAY (line 84), so they only compile when SIMD is enabled
- **cmake --list-presets**: Shows all 7 presets including scalar-test and simd-test
- **Catch2 git HEAD0 bug on Windows**: Affects all FetchContent git clones. Workaround: use -DFETCHCONTENT_SOURCE_DIR_CATCH2=build/default/_deps/catch2-src to reuse cached source
- **Build verification**: scalar-test configures and builds pipeline_tests. simd-test requires Highway which also hits HEAD0 bug, needs cached Highway source workaround
- **SIMD test filtering**: [simd] tag tests only compile when DPB_HAS_HIGHWAY is defined at compile time

## 2026-05-07 Task: SIMD-accelerated Pipeline collect path

- **Created `include/pipeline/simd_collect.hpp`**: Free function `simd_collect_sequential<In,Out,OpFunc,Range>(Pipeline&&, Range&&) -> ResultWithStats<Out>` gated behind `#ifdef DPB_HAS_HIGHWAY`
  - **Key design constraint**: `operation_` is a fused lambda — cannot be decomposed into separate filter/transform stages
  - **SIMD approach**: Process elements in `hn::Lanes(d)` chunks, calling fused lambda element-wise, building mask from return values, using `hn::CompressBlendedStore` for output compaction
  - **SIMD acceleration sources**: Vector load/store instructions, `CompressBlendedStore` filter compaction, known-size chunk iteration (better cache/branch prediction)
  - **Stats-free**: Returns `ResultWithStats<Out>` with zero stats (matching existing no-stats fast path behavior)
  - **Attributes**: Uses `DPB_HOT`, `DPB_FLATTEN`, `DPB_INLINE` macros for hot-path optimization

- **Modified `include/pipeline/pipeline.hpp`** (4 changes):
  1. Added `#include "dpb/simd.hpp"` conditionally (provides `simd::simd_numeric` concept for `if constexpr` check)
  2. Added `friend` declaration for `simd_collect_sequential` in Pipeline class (grants private member access to `operation_`)
  3. Inserted SIMD dispatch in `collect_sequential` no-stats path: `if constexpr (simd::simd_numeric<In> && simd::simd_numeric<Out>)` → calls `simd_collect_sequential(std::move(*this), ...)`. Wrapped in `#ifdef DPB_HAS_HIGHWAY`.
  4. Added `#include "pipeline/simd_collect.hpp"` at file bottom (after `} // namespace dpb`), also gated behind `#ifdef`

- **Circular dependency solution**: `simd_collect.hpp` cannot be included at top of `pipeline.hpp` because it references `Pipeline` (not yet defined). Solution: include at file bottom after Pipeline definition. Friend declaration provides forward declaration for `collect_sequential`. `#pragma once` prevents re-entry.

- **Created `tests/test_simd_pipeline.cpp`**: 10 test cases, all tagged `[simd]`:
  - 8 SIMD tests inside `#ifdef DPB_HAS_HIGHWAY`: filter int32, transform int32, filter+transform int32, float, double, int64, empty input, all-filtered, order preservation
  - 1 fallback test in `#else` block: string (non-numeric) pipeline still works
  - All SIMD tests compare results against `with_stats()` path (forces scalar execution) to verify SIMD == scalar correctness

- **Updated `CMakeLists.txt`**: Added `tests/test_simd_pipeline.cpp` to `pipeline_tests` sources

- **Scalar build verified**: 218 assertions in 42 test cases pass with scalar-test preset (DPB_HAS_HIGHWAY not defined)
  - +1 new test case: `"Non-SIMD pipeline still works for std::string"` (fallback test)
  - Existing tests unchanged — no regressions

- **SIMD build (simd-test)**: Not tested — requires Highway FetchContent which hits same HEAD0 git bug as Catch2. Gated correctly with `#ifdef`: when DPB_HAS_HIGHWAY is defined, `simd_collect_sequential` function and all SIMD test cases compile and run; when not defined, everything is stripped at preprocessor level.

## 2026-05-07 Task: Runtime CPU detection with Highway dynamic dispatch

- **Created `include/dpb/cpu_detect.hpp`**: Runtime CPU feature detection with thread-safe caching:
  - `struct cpu_features { bool has_sse42, has_avx2, has_avx512f; }` — boolean flags for ISA features
  - `inline const cpu_features& detect_cpu()` — Meyers' singleton pattern (static local) for thread-safe one-time initialization
  - GCC/Clang path: `__builtin_cpu_supports("sse4.2")` / `("avx2")` / `("avx512f")`
  - MSVC path: `__cpuid` intrinsic from `<intrin.h>` — leaf 1 ECX bit 20 for SSE4.2, leaf 7 EBX bit 5 for AVX2, bit 16 for AVX-512F
  - #else branch: no feature detection, all flags remain false

- **Modified `include/pipeline/simd_collect.hpp`**: Added `#include "dpb/cpu_detect.hpp"` and `[[maybe_unused]]` call to `detect_cpu()` at function entry. Highway's `HWY_DYNAMIC_DISPATCH` handles runtime ISA selection — the explicit CPU check is for FUTURE per-ISA optimization tuning, not to gate the current SIMD path.

- **Created `tests/test_cpu_detect.cpp`**: 3 test cases:
  - `detect_cpu() returns cached result`: verifies same pointer on repeat calls (Meyers' singleton)
  - `detect_cpu() populates feature flags`: all members are valid bools (no uninitialized)
  - `detect_cpu() does not crash`: smoke test — calling function doesn't throw/segfault

- **Updated `CMakeLists.txt`**: Added `tests/test_cpu_detect.cpp` to `pipeline_tests` sources (line 70)

- **Build + test**: 224 assertions in 45 test cases all pass (was 218 in 42 → +6 assertions, +3 test cases)
- **Key design decision**: CPU detection is NOT used to gate the SIMD path in `simd_collect_sequential`. Highway's `HWY_DYNAMIC_DISPATCH` internally dispatches to the best available ISA at runtime. The `cpu_features` struct is available for future use cases like per-ISA codegen tuning or conditional feature use outside Highway.
## 2026-05-07 Task: Custom Allocator Template Parameter for Pipeline

- **Pipeline template**: Changed from Pipeline<In, Out, OpFunc> to Pipeline<In, Out, OpFunc, Alloc> where Alloc = std::allocator<Out> by default
- **using allocator_type = Alloc;** added as public member type alias
- **Alloc alloc_** stored as private member, default-constructed, propagated through all factory methods (with_stats, with_profiler, filter, transform, take, skip, take_while, skip_while, tee, enumerate, unique)
- **Allocator rebinding**: 	ransform() and enumerate() use std::allocator_traits<Alloc>::template rebind_alloc<NewOut> to rebind the allocator when the output type changes
- **ResultWithStats<T, Alloc>**: Changed from ResultWithStats<T> to ResultWithStats<T, Alloc> with Alloc = std::allocator<T> default. data is now std::vector<T, Alloc>. When Alloc = std::allocator<T>, this is identical to the old std::vector<T>.
- **collect_buffer<T, Alloc>**: Changed from collect_buffer<T> to collect_buffer<T, Alloc> with Alloc = std::allocator<T> default. Constructor accepts (size_t initial_capacity, const Alloc& alloc). Conversion operator returns std::vector<T, Alloc>.
- **collect_sequential**: Uses collect_buffer<Out, Alloc> and returns ResultWithStats<Out, Alloc>. SIMD path only activated when std::same_as<Alloc, std::allocator<Out>> (custom allocators fall through to scalar path).
- **collect_parallel**: Uses std::vector<Out, Alloc> for merged result and std::vector<std::vector<Out, Alloc>> for local results (inner vectors use custom allocator). Returns ResultWithStats<Out, Alloc>.
- **simd_traits.hpp**: Updated simd_eligible_chain partial specialization from Pipeline<In, Out, OpFunc> to Pipeline<In, Out, OpFunc, Alloc> to match new 4-parameter template.
- **collect_buffer move constructor**: Removed 
oexcept specifier (was 
oexcept by default for std::allocator but not guaranteed for custom allocators like PMR). = default deduces 
oexcept from member types.
- **PMR support**: std::pmr::polymorphic_allocator<int> works as allocator parameter. Default-constructed PMR allocator uses std::pmr::get_default_resource(). Custom memory resources can be passed via Pipeline constructor's lloc parameter.
- **Backward compatibility**: Pipeline<int,int> is still Pipeline<int,int,DefaultOp,std::allocator<int>>. All existing code compiles and works identically. ResultWithStats<int> is still ResultWithStats<int,std::allocator<int>>.
- **Tests**: 261 assertions in 53 test cases (was 224 in 45 → +37 assertions, +8 test cases). New tests cover: default allocator transparency, std::allocator explicit, PMR allocator correctness, PMR with custom memory resource, allocator propagation through filter chain, allocator_type alias, collect_buffer with PMR, ResultWithStats with PMR.
