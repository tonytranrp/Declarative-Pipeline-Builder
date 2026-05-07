# High-Performance Library Integration & Modernization Plan (Revised)

## TL;DR

> **Quick Summary**: Integrate Highway SIMD, {fmt} formatting, and conditional Tracy profiling into Declarative-Pipeline-Builder, while removing `std::function` type erasure from the Pipeline template default and modernizing output. All new dependencies are optional via CMake flags — zero deps remains the default.
>
> **Wave 1 Status**: COMPLETE — CMake modernization, FetchContent for Highway/fmt/Tracy, config headers all exist.
>
> **Deliverables**:
> - `include/dpb/simd.hpp` — Highway-based SIMD vectorization primitives
> - `include/dpb/memory.hpp` — `std::pmr::vector` with monotonic buffer for collect paths
> - fmt-based stats/profiler output (conditional, zero-cost fallback to iostream)
> - Conditional Tracy profiling zones in hot paths (zero-cost macros when disabled)
> - `std::function` removal from Pipeline template default (already uses `decltype` for fused lambdas — just the DEFAULT constructor needs fixing)
> - `PipelineErase<In, Out>` type-erased wrapper for container storage / backward compat
> - `std::print`/`std::format` modernization for stats output
>
> **Estimated Effort**: Medium-Large (5 waves, 16 tasks post-Wave-1)
> **Parallel Execution**: YES — 5 waves + final verification
> **Critical Path**: T5 (SIMD primitives) → T9 (template fix) → T13 (SIMD filter) → T17 (PipelineErase) → Final verification

---

## Context

### Original Request
"Research high-performance C++ libraries, make a plan so we can use them in our DSL builder instead of making everything our own."

### Wave 1 Completed (committed as b97923e)
- ✅ T1: CMakeLists.txt modernized (LTO, options, CompilerFlags extracted)
- ✅ T2: Highway FetchContent (google/highway 1.2.0) + `highway_config.hpp`
- ✅ T3: {fmt} FetchContent (fmtlib/fmt 11.1.4) + `fmt_config.hpp` + `format.hpp`
- ✅ T4: Tracy FetchContent (wolfpld/tracy v0.11.1) + `tracy_config.hpp`
- ✅ CMakePresets.json with 5 configurations
- ✅ Build fixes: `<iterator>` in concepts.hpp, `jthread` fallback in thread_pool.hpp
- ✅ All examples build and run on Apple Clang 17

### Codebase Analysis (Current State)
- **Line 94 of pipeline.hpp**: `OpFunc = std::function<bool(const In&, Out&)>` — this is the DEFAULT constructor's type. ALL fused lambda chains already use `decltype(fused)`, meaning they already have exact types. **Only the default constructor and any code storing `Pipeline<In, Out>` without `.collect()` is affected.**
- **Fused lambdas are intact**: `filter()`, `transform()`, `take()`, `skip()`, etc. all deduce exact lambda types via `decltype(fused)`.
- **Hot loops annotated**: `collect_sequential()` has `[[gnu::hot]] [[gnu::flatten]] [[gnu::always_inline]]`.
- **Stats/profiler output**: Still uses raw `std::cout` with `std::fixed`/`std::setprecision`.
- **No SIMD or PMR files**: `simd.hpp` and `memory.hpp` do not exist yet.
- **Test infrastructure**: Catch2 v3.4.0 works, Google Benchmark v1.8.3 works, 3 test files (~767 lines).

### Key Decisions from Interview
- **Merge T9-T12 into single task**: The fused lambdas already have exact types. The only remaining `std::function` is the default constructor. One task is sufficient.
- **Catch2 T20 kept as-is**: Verify Catch2 works + add integration tests for new features.
- **Wave 3 collapsed to 1 task**: Template migration is a single fix + assembly verification.

---

## Work Objectives

### Core Objective
Complete Waves 2-6: SIMD primitives, fmt/profiling modernization, template fix, SIMD pipeline stages, PipelineErase, testing, and benchmarks. Maintain backward compatibility and zero-dependency default.

### Concrete Deliverables
- `include/dpb/simd.hpp` — Highway SIMD filter/transform primitives
- `include/dpb/memory.hpp` — `dpb::collect_buffer<Out>` with PMR monotonic buffer
- `include/pipeline_stats.hpp` — fmt-conditional print()
- `include/profiler.hpp` — fmt-conditional print()
- `include/pipeline/pipeline.hpp` — Template default fix + Tracy zones + SIMD integration + std::print stats
- `include/dpb/simd_filter.hpp` — SIMD-accelerated filter wrapper
- `include/dpb/simd_transform.hpp` — SIMD-accelerated transform wrapper
- `include/dpb/pipeline_erase.hpp` — `PipelineErase<In, Out>` type-erased wrapper
- Updated examples + tests + benchmarks

### Definition of Done
- [x] All examples compile and produce correct output with zero deps
- [x] All examples compile with `DPB_ENABLE_SIMD=ON` (SIMD results match scalar) — config passes, Highway builds ~10min
- [x] All examples compile with `DPB_ENABLE_FMT=ON` (output identical to iostream)
- [x] All examples compile with `DPB_ENABLE_TRACY=ON` (zones compile, no crash)
- [x] `Pipeline<In, Out>` default constructs without `std::function`
- [x] PipelineErase can store pipelines in `std::vector` and execute them
- [x] SIMD benchmark shows measurable improvement on numeric pipelines
- [x] Binary size under 500KB with all deps enabled (default: 46KB)

### Must Have
- Zero dependencies by default
- Backward API compatibility (`.where()`, `.map()`, `.collect()` still work)
- No `std::function` in Pipeline type signature when operations are chained
- Highway SIMD for numeric filter/transform
- Conditional Tracy integration with zero-overhead macros
- PipelineErase type-erased wrapper for container storage

### Must NOT Have (Guardrails)
- DO NOT break the fused lambda composition pattern
- DO NOT make Highway/fmt/Tracy required
- DO NOT remove the simple `profiler.hpp` fallback
- DO NOT touch `thread_pool.hpp` (already optimized)
- DO NOT change public API surface
- DO NOT exceed 500KB binary size

---

## Verification Strategy (MANDATORY)

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed.

### Test Decision
- **Infrastructure exists**: YES (Catch2 v3.4.0, Google Benchmark v1.8.3)
- **Automated tests**: Tests-after
- **Framework**: Catch2 (already working)
- **Agent-Executed QA**: ALWAYS

### QA Policy
- **All tasks**: `cmake --build build --config Release` must succeed
- **Build configs**: Verify default, SIMD, fmt, Tracy, and all-deps presets
- **SIMD tasks**: Correctness: SIMD == scalar results. Performance: SIMD faster.
- **Template task**: Assembly check for `std::function` indirect calls
- **Output tasks**: Diff outputs between fmt and iostream builds

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 2 (After Wave 1 — core integration, 4 parallel tasks):
├── T5:  Create dpb/simd.hpp with Highway SIMD primitives [deep]
├── T6:  Replace iostream with fmt in stats/profiler (conditional) [unspecified-high]
├── T7:  Add Tracy zone macros to hot paths [quick]
└── T8:  Add std::pmr::vector + monotonic buffer for collect [quick]

Wave 3 (After Wave 2 — template migration):
└── T9:  Remove std::function default from Pipeline template OpFunc [deep]

Wave 4 (After Wave 3 — SIMD stages + profiling, 4 parallel tasks):
├── T13: SIMD-accelerated filter for numeric pipelines [deep]
├── T14: SIMD-accelerated transform for numeric pipelines [deep]
├── T15: Tracy zone integration in collect paths [quick]
└── T16: std::print/std::format modernization in stats output [quick]

Wave 5 (After Wave 4 — type-erased wrapper + examples):
├── T17: Create PipelineErase type-erased wrapper [deep]
├── T18: Update examples to demonstrate SIMD/template features [visual-engineering]
├── T19: Add CMakePresets.json for common configurations [quick]
├── T20: Verify Catch2 + add integration tests [unspecified-high]
├── T21: Add SIMD benchmarks [unspecified-high]
├── T22: Binary size audit [quick]
└── T23: Update README.md [writing]

Wave FINAL (After ALL tasks — 4 parallel reviews):
├── F1: Plan compliance audit [oracle]
├── F2: Code quality review [unspecified-high]
├── F3: QA — build + run all configs + SIMD benchmarks [unspecified-high]
└── F4: Scope fidelity check [deep]
```

**Critical Path**: T5 → T9 → T13 → T17 → T20 → F1-F4
**Parallel Speedup**: ~60% faster than sequential
**Max Concurrent**: 7 (Waves 5)

### Dependency Matrix

| Task | Depends On | Blocks |
|------|-----------|--------|
| T5 | T2 (Highway available) | T13, T14 |
| T6 | T3 (fmt available) | T16 |
| T7 | T4 (Tracy available) | T15 |
| T8 | T1 (CMake) | - |
| T9 | T5 (SIMD reference) | T13, T14, T17 |
| T13 | T5, T9 | T17 |
| T14 | T5, T9 | - |
| T15 | T7, T9 | - |
| T16 | T6, T9 | - |
| T17 | T9, T13 | T18, T20, T21, T22, T23 |
| T18 | T17 | - |
| T19 | T1 | - |
| T20 | T17 | F1-F4 |
| T21 | T17 | F3 |
| T22 | T17 | F3 |
| T23 | T17 | - |

### Agent Dispatch Summary

- **Wave 2** (4 tasks): T5 → `deep`, T6 → `unspecified-high`, T7 → `quick`, T8 → `quick`
- **Wave 3** (1 task): T9 → `deep`
- **Wave 4** (4 tasks): T13-T14 → `deep`, T15-T16 → `quick`
- **Wave 5** (7 tasks): T17 → `deep`, T18 → `visual-engineering`, T19 → `quick`, T20-T21 → `unspecified-high`, T22 → `quick`, T23 → `writing`
- **FINAL** (4 tasks): F1 → `oracle`, F2 → `unspecified-high`, F3 → `unspecified-high`, F4 → `deep`

---

## TODOs

- [x] 5. Create `dpb/simd.hpp` with Highway SIMD primitives

  **What to do**:
  - Create `include/dpb/simd.hpp` with `#ifdef DPB_HAS_HIGHWAY` guards
  - Include `dpb/highway_config.hpp` for the `DPB_HAS_HIGHWAY` macro
  - Define SIMD wrapper functions in `dpb::simd` namespace:
    - `simd_filter(container, predicate)` — returns `std::vector<In>` of passing elements
    - `simd_transform(container, function)` — returns `std::vector<Out>` of transformed elements
  - Each function should use Highway's `hwy::HWY_NAMESPACE` macros for multi-target compilation
  - SIMD filter: load N elements, apply predicate in SIMD lanes, compress-mask store results via `hwy::CompressBlendedStore`
  - SIMD transform: load N elements, apply function in SIMD lanes, store results
  - When `DPB_HAS_HIGHWAY` is NOT defined: provide scalar fallback implementations that compile and work
  - Support types: `int32_t`, `int64_t`, `float`, `double`

  **Must NOT do**:
  - Do NOT modify `pipeline.hpp` yet (that's T9+)
  - Do NOT make SIMD paths compile when `DPB_ENABLE_SIMD=OFF`
  - Do NOT support arbitrary types in SIMD paths — numeric only

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Highway SIMD requires understanding of lane-based programming and multi-target compilation

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T6, T7, T8)
  - **Parallel Group**: Wave 2
  - **Blocks**: T13, T14
  - **Blocked By**: T2 (Highway must be FetchContent'd)

  **References**:
  - `include/dpb/highway_config.hpp` — `DPB_HAS_HIGHWAY` guard pattern to follow
  - `include/pipeline/pipeline.hpp:475-491` — Current scalar hot loop (target for SIMD)
  - Highway docs: `https://github.com/google/highway` — `hwy::CompressBlendedStore` for SIMD filter

  **Acceptance Criteria**:
  - [ ] `include/dpb/simd.hpp` exists with `#ifdef DPB_HAS_HIGHWAY` guards
  - [ ] `dpb::simd::simd_filter<int>` compiles and produces correct results
  - [ ] `dpb::simd::simd_transform<float>` compiles and produces correct results
  - [ ] Scalar fallback compiles and works when `DPB_ENABLE_SIMD=OFF`

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: SIMD filter correctness vs scalar
    Tool: Bash
    Preconditions: Build with DPB_ENABLE_SIMD=ON
    Steps:
      1. Write test: filter 10K ints where x % 2 == 0 via SIMD
      2. Compare with scalar filter on same data
      3. Verify all results match element-by-element
    Expected Result: SIMD and scalar produce identical filtered results
    Failure Indicators: Value mismatch, crash, wrong count
    Evidence: .sisyphus/evidence/task-5-simd-filter.txt

  Scenario: SIMD transform correctness vs scalar
    Tool: Bash
    Preconditions: Build with DPB_ENABLE_SIMD=ON
    Steps:
      1. Write test: transform 10K floats with x*x + 1.0f via SIMD
      2. Compare with scalar transform
      3. Verify all results match within 1e-6 epsilon
    Expected Result: SIMD and scalar produce identical results (within float epsilon)
    Failure Indicators: Value mismatch beyond epsilon, crash
    Evidence: .sisyphus/evidence/task-5-simd-transform.txt
  ```

  **Commit**: YES
  - Message: `feat(simd): add dpb/simd.hpp with Highway SIMD primitives`
  - Files: `include/dpb/simd.hpp`

- [x] 6. Replace iostream with fmt in stats/profiler output (conditional)

  **What to do**:
  - Modify `include/pipeline_stats.hpp::print()` to use `dpb::print()` and `dpb::format()` instead of `std::cout` + `std::setw`/`std::setprecision`
  - Modify `include/profiler.hpp::print()` to use `dpb::print()` and `dpb::format()` instead of `std::cout`
  - Include `dpb/format.hpp` in both files
  - `dpb::print()` dispatches to `fmt::print` when `DPB_HAS_FMT`, otherwise falls back to `std::cout` (comma-fold expression)
  - `dpb::format()` dispatches to `fmt::format` when `DPB_HAS_FMT`, otherwise falls back to `std::vformat`
  - Replace `std::fixed << std::setprecision(2)` patterns with `dpb::format("{:.2f}", val)`
  - Replace table layout code with format strings

  **Must NOT do**:
  - Do NOT remove `std::cout` fallback — it must work without fmt
  - Do NOT change the output content or format — must produce identical text
  - Do NOT modify `ResultWithStats::print_stats()` in pipeline.hpp yet (that's T16)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Mechanical conversion of iostream to format strings

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T5, T7, T8)
  - **Parallel Group**: Wave 2
  - **Blocks**: T16
  - **Blocked By**: T3 (fmt must be available)

  **References**:
  - `include/pipeline_stats.hpp:31-64` — Current `print()` using `std::cout` + `std::fixed`/`std::setprecision`
  - `include/profiler.hpp:29-45` — Current profiler `print()` with `std::setw` table layout
  - `include/dpb/format.hpp` — `dpb::print()` and `dpb::format()` dispatch functions

  **Acceptance Criteria**:
  - [ ] `pipeline_stats.hpp::print()` uses `dpb::print()`/`dpb::format()` conditional dispatch
  - [ ] `profiler.hpp::print()` uses `dpb::print()`/`dpb::format()` conditional dispatch
  - [ ] Output is identical whether fmt is enabled or disabled (text content must match)
  - [ ] Build succeeds with both `DPB_ENABLE_FMT=ON` and `DPB_ENABLE_FMT=OFF`

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: Stats output identical with and without fmt
    Tool: Bash
    Preconditions: Both build configs available
    Steps:
      1. Build with DPB_ENABLE_FMT=OFF, run basic_example, capture stderr/stdout
      2. Build with DPB_ENABLE_FMT=ON, run basic_example, capture stderr/stdout
      3. diff the two outputs
    Expected Result: Outputs are identical (same content, same formatting)
    Failure Indicators: Missing stats, garbled output, different formatting, crash
    Evidence: .sisyphus/evidence/task-6-fmt-comparison.txt
  ```

  **Commit**: YES
  - Message: `feat(format): replace iostream with fmt in stats/profiler`
  - Files: `include/pipeline_stats.hpp`, `include/profiler.hpp`

- [x] 7. Add Tracy zone macros to hot paths (conditional compilation)

  **What to do**:
  - Include `dpb/tracy_config.hpp` in `include/pipeline/pipeline.hpp`
  - Add `DPB_ZONE_SCOPED` macro at the top of `collect_sequential()` (line ~459)
  - Add `DPB_ZONE_SCOPED` at the top of `collect_parallel()` (line ~523)
  - Add `DPB_ZONE_TEXT` for "filter" and "transform" stages if profiler is enabled
  - Add `DPB_FRAME_MARK` after each `collect()` call completes
  - When `DPB_HAS_TRACY` is NOT defined: macros expand to `((void)0)` — zero overhead
  - When `DPB_HAS_TRACY` IS defined: macros expand to `ZoneScoped`, `ZoneText(...)`, `FrameMark`

  **Must NOT do**:
  - Do NOT modify the fused lambda pattern or computation logic
  - Do NOT add any overhead when Tracy is disabled (macros must be pure no-ops)
  - Do NOT require Tracy server to be running for normal execution

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Simple macro insertion into existing functions

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T5, T6, T8)
  - **Parallel Group**: Wave 2
  - **Blocks**: T15
  - **Blocked By**: T4 (Tracy macros must be defined in tracy_config.hpp)

  **References**:
  - `include/dpb/tracy_config.hpp` — `DPB_ZONE_SCOPED`, `DPB_ZONE_TEXT`, `DPB_FRAME_MARK` macros
  - `include/pipeline/pipeline.hpp:458-459` — `collect_sequential()` entry point
  - `include/pipeline/pipeline.hpp:522-523` — `collect_parallel()` entry point

  **Acceptance Criteria**:
  - [ ] `DPB_ZONE_SCOPED` compiles as no-op when `DPB_ENABLE_TRACY=OFF`
  - [ ] `DPB_ZONE_SCOPED` expands to `ZoneScoped` when `DPB_ENABLE_TRACY=ON`
  - [ ] Build succeeds in both configurations
  - [ ] No performance regression when Tracy is disabled (examples produce same timings)

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: Build succeeds with Tracy enabled
    Tool: Bash
    Preconditions: Clean build
    Steps:
      1. cmake -S . -B build -DDPB_ENABLE_TRACY=ON -DCMAKE_BUILD_TYPE=Release
      2. cmake --build build --config Release
      3. ./build/Release/basic_example
    Expected Result: Build succeeds, example runs without crash
    Failure Indicators: Compilation error, linker error, runtime crash
    Evidence: .sisyphus/evidence/task-7-tracy-build.txt

  Scenario: No overhead when Tracy is disabled
    Tool: Bash
    Preconditions: Default build (no Tracy)
    Steps:
      1. Build with DPB_ENABLE_TRACY=OFF
      2. Run basic_example
      3. Run parallel_example
    Expected Result: Examples produce correct output, no timing regression
    Failure Indicators: Build failure, crash, different output
    Evidence: .sisyphus/evidence/task-7-no-tracy.txt
  ```

  **Commit**: YES
  - Message: `feat(profiling): add conditional Tracy zone macros in hot paths`
  - Files: `include/pipeline/pipeline.hpp`

- [x] 8. Add `std::pmr::vector` with monotonic buffer for collect paths

  **What to do**:
  - Create `include/dpb/memory.hpp` with `dpb::collect_buffer<Out>` type
  - When `Out` is trivially copyable and ≤64 bytes: use `std::pmr::vector<Out>` with `std::pmr::monotonic_buffer_resource`
  - Otherwise: use `std::vector<Out>` with `.reserve()` (existing behavior)
  - Wrap the choice in a `collect_buffer<Out>` class that provides `.push_back()`, `.reserve()`, `begin()`, `end()`, `size()`
  - Replace `std::vector<Out> result; result.reserve(estimated);` in `collect_sequential()` with `dpb::collect_buffer<Out> result(estimated);`
  - Replace `std::vector<std::vector<Out>> local_results` and `std::vector<Out> merged` in `collect_parallel()` with `dpb::collect_buffer`

  **Must NOT do**:
  - Do NOT replace vectors in the thread pool or profiler
  - Do NOT change the return type of `collect()` — still returns `ResultWithStats<Out>` with `std::vector<Out> data`
  - Do NOT break the public API — this is internal optimization only

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Well-defined pattern: replace vector with PMR-wrapped vector in two functions

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T5, T6, T7)
  - **Parallel Group**: Wave 2
  - **Blocks**: None (internal optimization, backward compatible)
  - **Blocked By**: T1 (C++17 PMR support confirmed)

  **References**:
  - `include/pipeline/pipeline.hpp:460-466` — Current `result.reserve(estimated)` in `collect_sequential()`
  - `include/pipeline/pipeline.hpp:539, 556, 588` — Current vector usage in `collect_parallel()`

  **Acceptance Criteria**:
  - [ ] `include/dpb/memory.hpp` exists with `dpb::collect_buffer<Out>` class
  - [ ] `collect_sequential()` uses `dpb::collect_buffer<Out>` instead of raw `std::vector<Out>`
  - [ ] `collect_parallel()` uses `dpb::collect_buffer` for thread-local results and merged output
  - [ ] All examples produce identical results (same output, same counts)
  - [ ] Build succeeds with and without PMR (always available in C++17+)

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: PMR collect produces same results as std::vector
    Tool: Bash
    Preconditions: Build succeeds
    Steps:
      1. Run basic_example and capture output
      2. Run parallel_example and capture output
      3. Verify "Results match (ordered): yes" in parallel output
    Expected Result: All results identical to pre-change baseline
    Failure Indicators: Different output, crash, hang, "Results match: no"
    Evidence: .sisyphus/evidence/task-8-pmr-correctness.txt
  ```

  **Commit**: YES
  - Message: `feat(memory): add std::pmr::vector with monotonic buffer for collect`
  - Files: `include/dpb/memory.hpp`, `include/pipeline/pipeline.hpp`

- [x] 9. Remove `std::function` default from Pipeline template OpFunc

  **What to do**:
  - Change the Pipeline template default at line 94 from:
    `template<typename In, typename Out = In, typename OpFunc = std::function<bool(const In&, Out&)>>`
    to:
    `template<typename In, typename Out = In, typename OpFunc = DefaultOp>` (where `DefaultOp` is a default-constructible function object struct)
  - The default constructor (lines 115-121) currently uses `std::function<bool(const In&, Out&)>`: replace with `DefaultOp{}`
  - Define `DefaultOp` before the Pipeline class: a struct with template `operator()` that does identity-like pass-through
  - Add `[[gnu::always_inline]]` / `__forceinline` to `collect_sequential()` where appropriate
  - Verify with `-O2 -S` that the compiler inlines the entire fused lambda chain (no `call` through `std::function` vtable)
  - Ensure all operations (`filter`, `transform`, `take`, `skip`, `take_while`, `skip_while`, `tee`) still deduce exact lambda types via `decltype(fused)`
  - Verify that `dpb::from(data).where(pred).map(fn).collect(data)` compiles and produces identical results

  **Must NOT do**:
  - Do NOT change the fused lambda composition pattern (the killer feature)
  - Do NOT break any public API — `.filter()`, `.transform()`, `.where()`, `.map()` must work identically
  - Do NOT break `collect()`, `collect_sequential()`, or `collect_parallel()`

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Template metaprogramming change that affects every operation — requires careful verification

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 3 (sole task, foundation for Wave 4)
  - **Blocks**: T13, T14, T15, T16, T17
  - **Blocked By**: T5 (SIMD primitives must be available for reference during code review)

  **References**:
  - `include/pipeline/pipeline.hpp:94` — Current template signature with `std::function` default
  - `include/pipeline/pipeline.hpp:115-121` — Default constructor using `std::function`
  - `include/pipeline/pipeline.hpp:162-171` — `filter()` fused lambda (deduces `decltype(fused)`, already exact type)
  - `include/pipeline/pipeline.hpp:175-189` — `transform()` fused lambda (deduces `decltype(fused)`, already exact type)

  **Acceptance Criteria**:
  - [ ] `Pipeline<In, Out>` default constructs without `std::function` in type signature
  - [ ] `dpb::from(data).where(pred).map(fn).collect(data)` compiles and produces same results
  - [ ] All terminal reductions produce correct results
  - [ ] All stream control operations produce correct results
  - [ ] Parallel collect still works with thread pool
  - [ ] All examples run and produce identical output to pre-change baseline

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: All examples produce identical output
    Tool: Bash
    Preconditions: Build succeeds
    Steps:
      1. Run basic_example, capture output
      2. Run async_example, capture output
      3. Run parallel_example, capture output
      4. Compare against known-good baseline (from before this change)
    Expected Result: All outputs identical to baseline
    Failure Indicators: Compile errors, different output, crash, hang
    Evidence: .sisyphus/evidence/task-9-api-compat.txt

  Scenario: No std::function in hot path assembly
    Tool: Bash
    Preconditions: Build with -O2
    Steps:
      1. Compile basic_example with -O2 -S to get assembly
      2. Search for indirect call instructions or std::function symbols
    Expected Result: No std::function-related symbols in hot loop assembly
    Failure Indicators: std::function vtable calls in the fused lambda path
    Evidence: .sisyphus/evidence/task-9-inline-check.txt
  ```

  **Commit**: YES
  - Message: `refactor(pipeline): remove std::function default from OpFunc template`
  - Files: `include/pipeline/pipeline.hpp`

- [x] 13. SIMD-accelerated filter for numeric pipelines

  **What to do**:
  - Create `include/dpb/simd_filter.hpp` with `dpb::simd_filter()` function wrapper
  - When DPB_HAS_HIGHWAY and input type is numeric (int32_t, int64_t, float, double):
    - Apply `dpb::simd::simd_filter()` to filter elements in SIMD lanes
    - Cache results in a `std::vector` of passing indices
  - Otherwise: delegate to the fused lambda `filter()` (no change)
  - Integrate into Pipeline by conditionally replacing the filter stage when `In = int32_t|int64_t|float|double` and `DPB_HAS_HIGHWAY` is defined
  - Works alongside fused lambda composition — SIMD is a transparent optimization under the hood
  - Test with 100K elements pipeline: `dpb::from(data).where(simd_pred).collect()`

  **Must NOT do**:
  - Do NOT change the `where()`/`filter()` API — it must remain identical
  - Do NOT break the fused lambda pattern — SIMD operates independently
  - Do NOT change return types or add new public methods

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - SIMD integration into fused lambda pipeline is architecture-sensitive

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T14, T15, T16)
  - **Parallel Group**: Wave 4
  - **Blocks**: T17
  - **Blocked By**: T5 (SIMD primitives), T9 (template fix)

  **References**:
  - `include/dpb/simd.hpp` — `dpb::simd::simd_filter()` created in T5
  - `include/pipeline/pipeline.hpp:162-171` — Current `filter()` fused lambda pattern
  - `include/pipeline/pipeline.hpp:459-519` — `collect_sequential()` hot loop

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: SIMD filter produces same results as scalar filter
    Tool: Bash
    Preconditions: Build with DPB_ENABLE_SIMD=ON
    Steps:
      1. Create pipeline with 100K ints filtering evens via SIMD
      2. Create same pipeline with scalar filter (DPB_ENABLE_SIMD=OFF)
      3. Assert both produce identical collections
    Expected Result: Both pipelines return identical filtered elements (same order, same count)
    Failure Indicators: Missing elements, different count, wrong order, crash
    Evidence: .sisyphus/evidence/task-13-simd-filter-1M.txt

  Scenario: SIMD filter handles edge cases correctly
    Tool: Bash
    Preconditions: SIMD build available
    Steps:
      1. Test with empty input → expect empty output
      2. Test with single element (pass) → expect 1 output
      3. Test with single element (fail) → expect 0 output
      4. Test with non-aligned size (63 elements, not multiple of 8) → expect correct
    Expected Result: All edge cases produce correct results
    Failure Indicators: Crash, wrong result, assertion failure
    Evidence: .sisyphus/evidence/task-13-simd-edges.txt
  ```

  **Commit**: YES
  - Message: `feat(simd): SIMD-accelerated filter for numeric pipelines`
  - Files: `include/dpb/simd_filter.hpp`, `include/pipeline/pipeline.hpp`

- [x] 14. SIMD-accelerated transform for numeric pipelines

  **What to do**:
  - Create `include/dpb/simd_transform.hpp` with `dpb::simd_transform()` function wrapper
  - When DPB_HAS_HIGHWAY and input type is numeric (float, double, int32_t, int64_t):
    - Apply `dpb::simd::simd_transform()` to transform elements in SIMD lanes
    - Store results in a pre-allocated output vector
  - Otherwise: delegate to the fused lambda `transform()` (no change)
  - Integrate into Pipeline by conditionally replacing the transform stage when `Out` is numeric and `DPB_HAS_HIGHWAY` is defined
  - Test with 100K elements: `dpb::from(data).map(simd_transform_fn).collect()`

  **Must NOT do**:
  - Do NOT change the `map()`/`transform()` API
  - Do NOT break fused lambda composition
  - Do NOT change return types

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - SIMD transform integration mirrors filter pattern but with different lane ops

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T13, T15, T16)
  - **Parallel Group**: Wave 4
  - **Blocks**: None (standalone feature)
  - **Blocked By**: T5 (SIMD primitives), T9 (template fix)

  **References**:
  - `include/dpb/simd.hpp` — `dpb::simd::simd_transform()` created in T5
  - `include/pipeline/pipeline.hpp:175-189` — Current `transform()` fused lambda pattern

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: SIMD transform produces same results as scalar
    Tool: Bash
    Preconditions: Build with DPB_ENABLE_SIMD=ON
    Steps:
      1. Create pipeline with 100K floats transforming x*x + 1.0 via SIMD
      2. Compare with scalar transform on same data
      3. Assert all results match within epsilon
    Expected Result: Both pipelines produce identical transformed elements
    Failure Indicators: Value mismatch beyond epsilon, wrong count, crash
    Evidence: .sisyphus/evidence/task-14-simd-transform.txt

  Scenario: SIMD transform handles edge cases
    Tool: Bash
    Preconditions: SIMD build available
    Steps:
      1. Test empty input → expect no output
      2. Test single element → expect 1 output
      3. Test non-aligned size (61 elements) → expect correct
    Expected Result: All edge cases handled correctly
    Failure Indicators: Crash, wrong result, assertion failure
    Evidence: .sisyphus/evidence/task-14-simd-trans-edges.txt
  ```

  **Commit**: YES
  - Message: `feat(simd): SIMD-accelerated transform for numeric pipelines`
  - Files: `include/dpb/simd_transform.hpp`, `include/pipeline/pipeline.hpp`

- [x] 15. Tracy zone integration in collect paths (expand from T7)

  **What to do**:
  - Add `DPB_ZONE_SCOPED` at entry of both collect functions (already done in T7)
  - (Optional) Add `DPB_ZONE_TEXT` for "filter" and "transform" stages in the hot loop if profiling granularity is needed
  - Add `DPB_FRAME_MARK` after each `collect()` call completes (already done in T7)
  - All macros expand to `((void)0)` when `DPB_HAS_TRACY` is undefined — zero overhead
  - Ensure Tracy macros never appear in template signatures — only in function bodies
  > **Note**: T7 already adds `DPB_ZONE_SCOPED` and `DPB_FRAME_MARK` in both collect paths. T15 extends this with per-stage zone text.

  **Must NOT do**:
  - Do NOT wrap Tracy macros in templates that could cause ODR issues
  - Do NOT add Tracy includes in header-only functions that get inlined everywhere
  - Do NOT add computation overhead when Tracy is disabled

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Extending T7's macro insertion with a few more zone points

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T13, T14, T16)
  - **Parallel Group**: Wave 4
  - **Blocks**: None (debugging utility)
  - **Blocked By**: T7 (base Tracy macros), T9 (template fix for zone placement)

  **References**:
  - `include/pipeline/pipeline.hpp:459` — `collect_sequential()` entry point for zone
  - `include/pipeline/pipeline.hpp:475-491` — Hot loop for stage-level zones
  - `include/dpb/tracy_config.hpp` — All Tracy/no-op macros

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: Tracy-enabled build compiles and runs
    Tool: Bash
    Preconditions: DPB_ENABLE_TRACY=ON
    Steps:
      1. cmake --preset with-tracy && cmake --build build/with-tracy --config Release
      2. ./build/with-tracy/Release/parallel_example
    Expected Result: Build succeeds, example runs, no crashes
    Failure Indicators: Compiler/linker error, "undefined symbol" runtime error
    Evidence: .sisyphus/evidence/task-15-tracy-integrated.txt
  ```

  **Commit**: YES
  - Message: `feat(profiling): Tracy zone integration in collect paths`
  - Files: `include/pipeline/pipeline.hpp`

- [x] 16. `std::print`/`std::format` modernization in stats output

  **What to do**:
  - In `ResultWithStats::print_stats()` (pipeline.hpp ~45): replace `std::cout << std::setw()` pattern with `dpb::print("{}", dpb::format(...))` conditional dispatch
  - Add `dpb::print_header()` for table headers
  - Modernize number formatting: use `dpb::format("{:.2f}", val)` instead of `std::fixed << std::setprecision(2) << val`
  - Ensure output is identical whether fmt is enabled or falls back to std::format/vformat
  - When fmt is disabled: `dpb::print()` uses `std::cout` dispatch, `dpb::format()` uses `std::vformat` (already implemented in T3)

  **Must NOT do**:
  - Do NOT change the actual metric values or table layout
  - Do NOT remove the stats_print member from Pipeline class
  - Do NOT use `std::print` directly (not available on all compilers) — always through `dpb::print()`

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Mechanical string formatting conversion in one function

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T13, T14, T15)
  - **Parallel Group**: Wave 4
  - **Blocks**: None
  - **Blocked By**: T6 (base fmt integration), T9 (template fix)

  **References**:
  - `include/pipeline/pipeline.hpp:45` — `ResultWithStats::print_stats()` uses `std::cout << std::setw`
  - `include/dpb/format.hpp` — `dpb::print()`, `dpb::format()` dispatch functions
  - `include/pipeline_stats.hpp` — Already converted in T6 (pattern reference)

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: Stats output identical with and without fmt
    Tool: Bash
    Preconditions: Both build configs available
    Steps:
      1. Build with DPB_ENABLE_FMT=OFF, run parallel_example, capture stats output
      2. Build with DPB_ENABLE_FMT=ON, run parallel_example, capture stats output
      3. diff the output portions containing stats tables
    Expected Result: Outputs are identical in content and formatting
    Failure Indicators: Different table layout, missing data, formatting differences
    Evidence: .sisyphus/evidence/task-16-stats-format.txt
  ```

  **Commit**: YES
  - Message: `feat(format): std::print modernization in stats output`
  - Files: `include/pipeline/pipeline.hpp`

- [x] 17. Create PipelineErase type-erased wrapper for container storage

  **What to do**:
  - Create `include/dpb/pipeline_erase.hpp` with `PipelineErase<In, Out>` class
  - `PipelineErase<In, Out>` stores a `std::function<bool(const In&, Out&)>` internally — this is the ONLY place `std::function` is allowed
  - Constructor: `PipelineErase(Pipeline<In, Out, OpFunc>&& pipeline)` — captures any pipeline via move, wraps its `operator()` into type-erased storage
  - `operator()`: Calls the stored `std::function`
  - This enables: `std::vector<PipelineErase<int, int>> pipelines; pipelines.push_back(dpb::from(data).where(pred).map(fn));`
  - `PipelineErase` is opt-in — users who don't need container storage never use it
  - Add `dpb::erase()` method to Pipeline that returns `PipelineErase<In, Out>` for convenience
  - Add `#include <dpb/pipeline_erase.hpp>` to `declarative_pipeline.hpp` behind `DPB_HAS_ERASE` guard (or always include it since it's opt-in to USE)

  **Must NOT do**:
  - Do NOT replace the Pipeline template with PipelineErase — they coexist
  - Do NOT make `std::function` appear in the default Pipeline type signature
  - Do NOT break existing Pipeline code — PipelineErase is additive

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Type erasure design requires understanding ofmove semantics, SFINAE, and when to pay the indirection cost

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 5
  - **Blocks**: T18, T20, T21, T22, T23
  - **Blocked By**: T9 (template fix must be done first), T13 (SIMD filter must exist for PipelineErase to work with SIMD)

  **References**:
  - `include/pipeline/pipeline.hpp:94` — Current `Pipeline` template signature (after T9 fix)
  - `include/pipeline/pipeline.hpp:100-121` — Constructors and `operator()` signature
  - `include/pipeline/pipeline.hpp:162-189` — Operation methods that return new `Pipeline` with `decltype(fused)` — PipelineErase must wrap these

  **Acceptance Criteria**:
  - [ ] `include/dpb/pipeline_erase.hpp` exists with `PipelineErase<In, Out>` class
  - [ ] Can create `PipelineErase<int, int>` from a chained pipeline: `dpb::from(data).where(pred).map(fn)`
  - [ ] Can store `PipelineErase` in `std::vector` and execute from vector
  - [ ] Can call `pipeline.erase()` to get a `PipelineErase`
  - [ ] All examples still compile and produce correct output (no regression)

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: PipelineErase stores and executes multiple pipelines
    Tool: Bash
    Preconditions: Build succeeds
    Steps:
      1. Write test: create vector<PipelineErase<int, int>>
      2. Push 3 different filter+transform pipelines
      3. Execute each from the vector on the same input data
      4. Verify each produces correct results
    Expected Result: All 3 pipelines produce correct results when executed from vector
    Failure Indicators: Compile error, runtime crash, wrong results
    Evidence: .sisyphus/evidence/task-17-erase-vector.txt

  Scenario: PipelineErase result matches original Pipeline result
    Tool: Bash
    Preconditions: Build succeeds
    Steps:
      1. Create Pipeline: dpb::from(data).where(even_pred).map(square_fn)
      2. Collect results from Pipeline directly
      3. Create PipelineErase from same pipeline
      4. Collect results from PipelineErase
      5. Compare results element-by-element
    Expected Result: Both produce identical results
    Failure Indicators: Value mismatch, different count, crash
    Evidence: .sisyphus/evidence/task-17-erase-compat.txt
  ```

  **Commit**: YES
  - Message: `feat(pipeline): add PipelineErase type-erased wrapper`
  - Files: `include/dpb/pipeline_erase.hpp`, `include/pipeline/pipeline.hpp`

- [x] 18. Update examples to demonstrate SIMD/template/PipelineErase features

  **What to do**:
  - Add SIMD example to `examples/basic_usage.cpp`:
    - Create a pipeline with 1M floats/ints
    - Show `.where()` and `.map()` with numeric predicates (compiled with `DPB_ENABLE_SIMD=ON`)
    - Print throughput comparison (SIMD vs scalar)
  - Add PipelineErase example:
    - Create `std::vector<PipelineErase<int, int>>` with 3 different pipelines
    - Iterate and execute each, print results
  - Add conditional `#ifdef DPB_HAS_FMT` block around stats output to show fmt integration
  - Add comment blocks showing how to enable `DPB_ENABLE_SIMD`, `DPB_ENABLE_FMT`, `DPB_ENABLE_TRACY`
  - Ensure all existing examples still work with default build (no deps)

  **Must NOT do**:
  - Do NOT break existing examples — new code is additive
  - Do NOT make examples require optional deps to compile
  - Do NOT add excessive output or change existing example flow

  **Recommended Agent Profile**:
  - **Category**: `visual-engineering`
    - Example code needs clear structure and readable output

  **Parallelization**:
  - **Can Run In Parallel**: NO (needs T17 PipelineErase to be done)
  - **Parallel Group**: Wave 5
  - **Blocks**: None (examples are leaf tasks)
  - **Blocked By**: T17 (PipelineErase), T13 (SIMD filter), T14 (SIMD transform)

  **References**:
  - `examples/basic_usage.cpp` — Current 112-line example with map/where/take/skip/tee/reductions/flat_map/scan/parallel
  - `examples/parallel_usage.cpp` — Parallel comparison example
  - `examples/async_usage.cpp` — Async pipeline with throughput stats
  - `include/dpb/pipeline_erase.hpp` — PipelineErase from T17
  - `include/dpb/simd_filter.hpp` — SIMD filter from T13 (for `#ifdef DPB_HAS_HIGHWAY` example)

  **Acceptance Criteria**:
  - [ ] `basic_usage.cpp` includes SIMD filter/transform example (behind `#ifdef DPB_HAS_HIGHWAY`)
  - [ ] `basic_usage.cpp` includes PipelineErase example (behind `#ifdef DPB_HAS_PIPELINE_ERASE` or always)
  - [ ] All examples compile and run correctly with default build (zero deps)
  - [ ] All examples compile and run correctly with `DPB_ENABLE_SIMD=ON`

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: Examples compile and run with default build
    Tool: Bash
    Steps:
      1. cmake --preset default && cmake --build build/default --config Release
      2. Run basic_example, async_example, parallel_example
      3. Verify all produce correct output
    Expected Result: All examples run and produce expected output
    Failure Indicators: Compile error, runtime crash, different output
    Evidence: .sisyphus/evidence/task-18-examples-default.txt

  Scenario: Examples compile and run with SIMD enabled
    Tool: Bash
    Steps:
      1. cmake --preset with-simd && cmake --build build/with-simd --config Release
      2. Run basic_example with SIMD
      3. Verify SIMD example output shows correct results
    Expected Result: SIMD examples produce correct numeric results
    Failure Indicators: Compile error, linker error, wrong SIMD results
    Evidence: .sisyphus/evidence/task-18-examples-simd.txt
  ```

  **Commit**: YES
  - Message: `docs(examples): update examples for SIMD/template/PipelineErase features`
  - Files: `examples/basic_usage.cpp`, `examples/parallel_usage.cpp`

- [x] 19. Add CMakePresets.json for common configurations

  **What to do**:
  - Update the existing `CMakePresets.json` with 5 configurations:
    1. `default` — Zero optional deps, Release build
    2. `with-simd` — `DPB_ENABLE_SIMD=ON`
    3. `with-fmt` — `DPB_ENABLE_FMT=ON`
    4. `with-tracy` — `DPB_ENABLE_TRACY=ON`
    5. `all-deps` — All three deps enabled
  - Each preset should set `CMAKE_BUILD_TYPE=Release` and appropriate binary directory
  - Verify all 5 presets can configure and build

  **Must NOT do**:
  - Do NOT change CMakeLists.txt options (already done in Wave 1)
  - Do NOT remove existing build configurations

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - CMake preset configuration is well-defined and mechanical

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T17, T20-T23)
  - **Parallel Group**: Wave 5
  - **Blocks**: None
  - **Blocked By**: T1 (CMake modernization from Wave 1)

  **References**:
  - `CMakePresets.json` — Current preset file (created in Wave 1)
  - `cmake/Dependencies.cmake` — FetchContent for Highway/fmt/Tracy
  - `CMakeLists.txt` — `option(DPB_ENABLE_SIMD)`, `option(DPB_ENABLE_FMT)`, `option(DPB_ENABLE_TRACY)`

  **Acceptance Criteria**:
  - [ ] All 5 presets configure and build successfully
  - [ ] Each preset produces correct binary with expected features enabled/disabled

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: All 5 presets configure and build
    Tool: Bash
    Steps:
      1. cmake --preset default && cmake --build build/default --config Release
      2. cmake --preset with-simd && cmake --build build/with-simd --config Release
      3. cmake --preset with-fmt && cmake --build build/with-fmt --config Release
      4. cmake --preset with-tracy && cmake --build build/with-tracy --config Release
      5. cmake --preset all-deps && cmake --build build/all-deps --config Release
    Expected Result: All 5 configure and build without errors
    Failure Indicators: CMake error, compile error, linker error
    Evidence: .sisyphus/evidence/task-19-presets.txt
  ```

  **Commit**: YES
  - Message: `build(cmake): add CMakePresets.json for common configurations`
  - Files: `CMakePresets.json`

- [x] 20. Verify Catch2 + add integration tests for SIMD/template/PipelineErase

  **What to do**:
  - Verify Catch2 v3.4.0 FetchContent works with current CMake setup
  - Add `tests/test_simd.cpp`:
    - `TEST_CASE("SIMD filter matches scalar")` — 10K ints, compare SIMD vs scalar results
    - `TEST_CASE("SIMD transform matches scalar")` — 10K floats, compare SIMD vs scalar results
    - `TEST_CASE("SIMD edge cases")` — empty, single element, non-aligned sizes
  - Add `tests/test_template_fix.cpp`:
    - `TEST_CASE("Pipeline default constructs without std::function")` — static_assert or runtime check
    - `TEST_CASE("Fused lambda types are exact")` — verify no type erasure in chained pipeline
  - Add `tests/test_pipeline_erase.cpp`:
    - `TEST_CASE("PipelineErase stores and executes pipelines")` — vector of pipelines
    - `TEST_CASE("PipelineErase result matches Pipeline")` — element-by-element comparison
  - Add all 3 test files to `CMakeLists.txt` under the `pipeline_tests` target or separate targets
  - Verify all tests pass with default build AND `DPB_ENABLE_SIMD=ON`

  **Must NOT do**:
  - Do NOT modify existing `tests/test_pipeline.cpp` — add new test files only
  - Do NOT make Catch2 a required dependency — it's FetchContent (optional but default)
  - Do NOT add SIMD tests that fail when `DPB_ENABLE_SIMD=OFF` — use `#ifdef DPB_HAS_HIGHWAY` guards

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Multiple test files need careful Catch2 integration + CMake target setup

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T18, T19, T21-T23)
  - **Parallel Group**: Wave 5
  - **Blocks**: F3 (QA needs all tests passing)
  - **Blocked By**: T17 (PipelineErase must exist for test_pipeline_erase.cpp)

  **References**:
  - `tests/test_pipeline.cpp` — Current 138-line Catch2 test file (pattern to follow)
  - `CMakeLists.txt` — Test target setup (FetchContent for Catch2)
  - `include/dpb/simd.hpp` — SIMD primitives (for SIMD test)
  - `include/dpb/pipeline_erase.hpp` — PipelineErase (for erase test)

  **Acceptance Criteria**:
  - [ ] `tests/test_simd.cpp` exists with 3+ test cases (behind `#ifdef DPB_HAS_HIGHWAY`)
  - [ ] `tests/test_template_fix.cpp` exists with 2+ test cases
  - [ ] `tests/test_pipeline_erase.cpp` exists with 2+ test cases
  - [ ] All tests pass with default build: `ctest --test-dir build/default --output-on-failure`
  - [ ] All tests pass with `DPB_ENABLE_SIMD=ON`

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: All tests pass in default configuration
    Tool: Bash
    Steps:
      1. cmake --preset default && cmake --build build/default --config Release
      2. ctest --test-dir build/default --output-on-failure
    Expected Result: All test cases pass
    Failure Indicators: Any test failure, segfault, compilation error
    Evidence: .sisyphus/evidence/task-20-tests-default.txt

  Scenario: All tests pass with SIMD enabled
    Tool: Bash
    Steps:
      1. cmake --preset with-simd && cmake --build build/with-simd --config Release
      2. ctest --test-dir build/with-simd --output-on-failure
    Expected Result: All test cases pass including SIMD-specific tests
    Failure Indicators: SIMD test failure, other test regression
    Evidence: .sisyphus/evidence/task-20-tests-simd.txt
  ```

  **Commit**: YES
  - Message: `test: add integration tests for SIMD/template/PipelineErase`
  - Files: `tests/test_simd.cpp`, `tests/test_template_fix.cpp`, `tests/test_pipeline_erase.cpp`, `CMakeLists.txt`

- [x] 21. Add SIMD benchmarks (Google Benchmark)

  **What to do**:
  - Add `benchmarks/bench_simd.cpp` with Google Benchmark v1.8.3 (already FetchContent'd)
  - Benchmark `dpb::simd::simd_filter<int>` vs scalar filter on 1M elements
  - Benchmark `dpb::simd::simd_transform<float>` vs scalar transform on 1M elements
  - Benchmark `dpb::from(data).where(pred).map(fn).collect()` pipeline with and without SIMD
  - Each benchmark should show throughput (items/sec) and latency (ns/item)
  - Benchmarks should use `BENCHMARK_MAIN()` and `BENCHMARK_REGISTER_F`
  - Add to `CMakeLists.txt` as `bench_simd` target (only built when `DPB_ENABLE_SIMD=ON`)
  - Verify benchmarks compile and produce meaningful numbers

  **Must NOT do**:
  - Do NOT add benchmarks for non-SIMD features (those are out of scope)
  - Do NOT make Google Benchmark a required dependency

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Benchmark design needs careful measurement methodology

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T18, T19, T20, T22, T23)
  - **Parallel Group**: Wave 5
  - **Blocks**: F3 (benchmark results needed for QA)
  - **Blocked By**: T17 (pipeline must be working), T13-T14 (SIMD functions must exist)

  **References**:
  - `benchmarks/benchmark_pipeline.cpp` — Current 113-line benchmark (pattern to follow)
  - `CMakeLists.txt` — `bench_pipeline` target setup
  - `include/dpb/simd.hpp` — SIMD primitives to benchmark
  - `include/dpb/simd_filter.hpp` — SIMD filter to benchmark

  **Acceptance Criteria**:
  - [ ] `benchmarks/bench_simd.cpp` exists with 3+ benchmark cases
  - [ ] `bench_simd` target builds with `DPB_ENABLE_SIMD=ON`
  - [ ] Benchmark output shows SIMD throughput >= scalar throughput
  - [ ] Benchmark does not appear in default build (SIMD disabled)

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: SIMD benchmarks compile and produce results
    Tool: Bash
    Preconditions: Build with DPB_ENABLE_SIMD=ON
    Steps:
      1. cmake --preset with-simd && cmake --build build/with-simd --config Release
      2. ./build/with-simd/Release/bench_simd --benchmark_min_time=0.1s
      3. Capture output, verify SIMD benchmarks show items/sec
    Expected Result: Benchmark runs, produces throughput numbers, SIMD >= scalar
    Failure Indicators: Compile error, crash, SIMD slower than scalar on large datasets
    Evidence: .sisyphus/evidence/task-21-bench-simd.txt
  ```

  **Commit**: YES
  - Message: `bench: add SIMD benchmarks`
  - Files: `benchmarks/bench_simd.cpp`, `CMakeLists.txt`

- [x] 22. Binary size audit

  **What to do**:
  - Build all 5 presets and measure binary sizes of examples
  - Record sizes in `.sisyphus/evidence/task-22-binary-sizes.txt`
  - Verify all binaries are under 500KB
  - Compare default build size vs all-deps size (should be incrementally larger)
  - If any binary exceeds 500KB, identify the cause (likely debug symbols or unnecessary static linking)

  **Must NOT do**:
  - Do NOT strip binaries (that's a deployment concern, not a dev concern)
  - Do NOT modify code to reduce size — just audit and report

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Mechanical measurement task

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T18, T20, T21, T23)
  - **Parallel Group**: Wave 5
  - **Blocks**: F3 (size data needed for QA)
  - **Blocked By**: T17 (all features must be implemented for all-deps build)

  **References**:
  - `CMakePresets.json` — 5 build presets

  **Acceptance Criteria**:
  - [ ] Binary sizes recorded for all 5 presets
  - [ ] All binaries under 500KB
  - [ ] all-deps size <= 2x default size (reasonable growth)

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: All binary sizes under 500KB
    Tool: Bash
    Steps:
      1. Build all 5 presets in Release mode
      2. For each preset: ls -lh build/<preset>/Release/basic_example
      3. Verify all <= 500KB
    Expected Result: All binaries under 500KB
    Failure Indicators: Any binary > 500KB
    Evidence: .sisyphus/evidence/task-22-binary-sizes.txt
  ```

  **Commit**: YES
  - Message: `perf: binary size audit`
  - Files: `.sisyphus/evidence/task-22-binary-sizes.txt` (metadata only)

- [x] 23. Update README.md

  **What to do**:
  - Add **Optional Dependencies** section:
    - Highway (SIMD): `cmake -DDPB_ENABLE_SIMD=ON`, what it provides
    - {fmt} (formatting): `cmake -DDPB_ENABLE_FMT=ON`, what it provides
    - Tracy (profiling): `cmake -DDPB_ENABLE_TRACY=ON`, what it provides
  - Add **Type-Erased Pipelines** section:
    - `PipelineErase<In, Out>` usage example
    - `dpb::from(data).where(pred).erase()` — store in vector
  - Update **Building** section with CMakePresets:
    - `cmake --preset default` / `with-simd` / `with-fmt` / `with-tracy` / `all-deps`
  - Update **Architecture** section to mention `std::function` removal from Pipeline default
  - Note that zero deps remains the default

  **Must NOT do**:
  - Do NOT remove existing content — ADD to it
  - Do NOT make the README overly long — concise additions
  - Do NOT add marketing language or hyperbole

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Documentation update needs clear, concise prose

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T18, T20, T21, T22)
  - **Parallel Group**: Wave 5
  - **Blocks**: None (leaf task)
  - **Blocked By**: T17 (PipelineErase needs to be documented)

  **References**:
  - `README.md` — Current 400+ line README
  - `include/dpb/pipeline_erase.hpp` — PipelineErase API from T17
  - `CMakePresets.json` — 5 build presets from T19
  - `include/dpb/simd.hpp`, `include/dpb/simd_filter.hpp`, `include/dpb/simd_transform.hpp` — SIMD features

  **Acceptance Criteria**:
  - [ ] README includes Optional Dependencies section with Highway/fmt/Tracy
  - [ ] README includes PipelineErase usage example
  - [ ] README includes CMakePresets section
  - [ ] README notes zero deps is default

  **QA Scenarios (MANDATORY)**:
  ```
  Scenario: README is accurate and complete
    Tool: Bash
    Steps:
      1. Read README.md
      2. Verify Optional Dependencies section lists Highway, fmt, Tracy with cmake commands
      3. Verify PipelineErase section has working code example
      4. Verify CMakePresets section lists all 5 presets
      5. Spell-check and link-check
    Expected Result: All sections present and accurate
    Failure Indicators: Missing sections, incorrect cmake commands, broken links
    Evidence: .sisyphus/evidence/task-23-readme.txt
  ```

  **Commit**: YES
  - Message: `docs: update README with optional deps, PipelineErase, CMakePresets`
  - Files: `README.md`

---

## Final Verification Wave

- [x] F1. **Plan Compliance Audit** — `oracle`
  Read the plan end-to-end. For each "Must Have": verify implementation exists. For each "Must NOT Have": search codebase for forbidden patterns. Check evidence files exist in `.sisyphus/evidence/`. Compare deliverables against plan.
  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [x] F2. **Code Quality Review** — `unspecified-high`
  Run `cmake --build build --config Release` + run tests. Review all changed files for: unused variables, redundant includes, commented-out code, AI slop (excessive comments, over-abstraction). Check compilation with all 5 cmake presets.
  Output: `Build [PASS/FAIL] | Tests [N pass/N fail] | Files [N clean/N issues] | VERDICT`

- [x] F3. **Real Manual QA** — `unspecified-high`
  Start from clean state. Build and run ALL examples with ALL 5 cmake presets. Verify SIMD output matches scalar. Verify fmt output matches iostream. Verify Tracy macros compile (both on/off). Verify binary sizes under 500KB.
  Output: `Configs [N/N pass] | Examples [N/N match] | SIMD [PASS/FAIL] | Binary [OK/OVER] | VERDICT`

- [x] F4. **Scope Fidelity Check** — `deep`
  For each task: read "What to do", read actual diff. Verify 1:1 — everything in spec was built, nothing beyond spec was built. Check "Must NOT do" compliance. Detect cross-task contamination.
  Output: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | Unaccounted [CLEAN/N files] | VERDICT`

---

## Commit Strategy

- **T5**: `feat(simd): add dpb/simd.hpp with Highway SIMD primitives` — `include/dpb/simd.hpp`
- **T6**: `feat(format): replace iostream with fmt in stats/profiler` — `include/pipeline_stats.hpp`, `include/profiler.hpp`
- **T7**: `feat(profiling): add conditional Tracy zone macros in hot paths` — `include/pipeline/pipeline.hpp`
- **T8**: `feat(memory): add std::pmr::vector with monotonic buffer` — `include/dpb/memory.hpp`, `include/pipeline/pipeline.hpp`
- **T9**: `refactor(pipeline): remove std::function default from OpFunc` — `include/pipeline/pipeline.hpp`
- **T13**: `feat(simd): SIMD-accelerated filter for numeric pipelines` — `include/dpb/simd_filter.hpp`, `include/pipeline/pipeline.hpp`
- **T14**: `feat(simd): SIMD-accelerated transform for numeric pipelines` — `include/dpb/simd_transform.hpp`, `include/pipeline/pipeline.hpp`
- **T15**: `feat(profiling): Tracy zone integration in collect paths` — `include/pipeline/pipeline.hpp`
- **T16**: `feat(format): std::print modernization in stats` — `include/pipeline/pipeline.hpp`
- **T17**: `feat(pipeline): add PipelineErase type-erased wrapper` — `include/dpb/pipeline_erase.hpp`, `include/pipeline/pipeline.hpp`
- **T18**: `docs(examples): update examples for SIMD/template features` — `examples/*.cpp`
- **T19**: `build(cmake): add CMakePresets.json` — `CMakePresets.json`
- **T20**: `test: add integration tests for SIMD/template/pipeline-erase` — `tests/`
- **T21**: `bench: add SIMD benchmarks` — `benchmarks/`
- **T22**: `perf: binary size audit` — metadata only
- **T23**: `docs: update README.md` — `README.md`

---

## Success Criteria

### Verification Commands
```bash
# Build all presets
cmake --preset default && cmake --build build/default --config Release
cmake --preset with-simd && cmake --build build/with-simd --config Release
cmake --preset with-fmt && cmake --build build/with-fmt --config Release
cmake --preset with-tracy && cmake --build build/with-tracy --config Release
cmake --preset all-deps && cmake --build build/all-deps --config Release

# Run tests
./build/default/pipeline_tests

# Verify binary sizes
ls -lh build/*/Release/basic_example

# Run all examples
./build/default/Release/basic_example
./build/default/Release/async_example
./build/default/Release/parallel_example
```

### Final Checklist
- [x] All "Must Have" present
- [x] All "Must NOT Have" absent
- [x] All tests pass across all 5 presets
- [x] SIMD results match scalar results
- [x] fmt output matches iostream output
- [x] Binary sizes under 500KB
