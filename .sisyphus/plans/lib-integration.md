# High-Performance Library Integration & Modernization Plan

## TL;DR

> **Quick Summary**: Integrate Highway SIMD, {fmt} formatting, and conditional Tracy profiling into Declarative-Pipeline-Builder, while migrating `std::function` to full templates and modernizing CMake. All new dependencies are optional via CMake flags — zero deps remains the default.
> 
> **Deliverables**:
> - Highway SIMD vectorization for numeric pipeline stages (2-8x throughput)
> - {fmt} library replacing iostream in stats/profiler output (faster, smaller binaries)
> - Conditional Tracy profiling (zero-cost when disabled)
> - Full template migration of `operation_` (removes `std::function` bottleneck, enables inlining+SIMD)
> - `std::pmr::vector` with monotonic buffer for collect paths
> - Modernized CMake with vcpkg integration, presets, and LTO flags
> - C++23 modernization: `std::print`, `std::format`, `std::assume`
> 
> **Estimated Effort**: Large (7+ waves, 25+ tasks)
> **Parallel Execution**: YES - 6 waves + final verification
> **Critical Path**: T1 (CMake) → T2 (Highway) → T5 (Template migration) → T8 (SIMD stages) → T11 (Full integration test)

---

## Context

### Original Request
"Research high-performance C++ libraries, make a plan so we can use them in our DSL builder instead of making everything our own. Binary size must be no more than ~500KB for a high-performance library."

### Interview Summary
**Key Discussions**:
- Dependency strategy: User chose **optional deps via CMake flags** (zero deps by default)
- SIMD library: User chose **Highway** (Google-backed, Apache-2.0, broader arch support)
- Template vs std::function: User chose **full template migration**, but **phased/incremental** (not one big breaking change)
- Profiling: User chose **conditional Tracy + keep simple profiler** as fallback
- Binary size: ~500KB target per architecture

**Research Findings**:
- Thread pool: KEEP custom (work-stealing tailor-made, TBB is 2-5MB)
- Pipeline stats: KEEP custom (too simple for dependency)
- moodycamel::ConcurrentQueue: NOT NEEDED (we have work-stealing, not MPMC)
- Intel TBB: TOO HEAVY (2-5MB, requires linking)
- Highway: ADD for SIMD (~50-100KB, Apache-2.0)
- {fmt}: ADD for formatting (~80KB, MIT)
- Tracy: ADD conditionally (0KB when disabled, BSD-3)
- Current binary sizes: 65-97KB for examples — significant headroom within 500KB budget

### Metis Review
**Identified Gaps** (addressed):
- Binary budget has headroom (current examples are 65-97KB vs 500KB target)
- `<iostream>` and `std::function<void()> ring[4096]` are dominant bloat sources
- Template migration must be phased to avoid breaking existing user code
- Tracy's NOASSERTION license needs verification — confirmed BSD-3 compatible

---

## Work Objectives

### Core Objective
Integrate battle-tested high-performance libraries (Highway, {fmt}, Tracy) into Declarative-Pipeline-Builder via optional CMake flags, while migrating from `std::function` to full templates for maximum inlining and SIMD vectorization. Keep binary size under 500KB and maintain zero-dependency default.

### Concrete Deliverables
- `include/dpb/simd.hpp` — Highway-based SIMD vectorization for numeric pipeline stages
- `include/dpb/format.hpp` — {fmt}-based output when `DPB_ENABLE_FMT=ON`
- Conditional Tracy integration in `profiler.hpp` when `DPB_ENABLE_TRACY=ON`
- Fully templated `Pipeline<In, Out, OpFunc>` with type-erased fallback
- CMakePresets.json + vcpkg.json for dependency management
- `std::pmr::vector` with monotonic buffer resource for collect paths
- `std::print` / `std::format`现代化的统计输出

### Definition of Done
- [ ] All examples compile and produce correct output with default deps (zero deps)
- [ ] All examples compile and produce correct output with `DPB_ENABLE_SIMD=ON`
- [ ] All examples compile and produce correct output with `DPB_ENABLE_FMT=ON`
- [ ] All examples compile and produce correct output with `DPB_ENABLE_TRACY=ON`
- [ ] Binary size with all deps enabled stays under 500KB
- [ ] SIMD benchmark shows ≥2x throughput improvement on numeric pipelines
- [ ] Template-migrated pipeline produces identical results to `std::function` version
- [ ] Tracy zones appear in Tracy Profiler GUI when enabled

### Must Have
- Zero dependencies by default (all new deps are opt-in via CMake flags)
- Fully templated `operation_` with type-erased fallback for existing users
- Highway SIMD for `filter` and `transform` on numeric types
- Conditional Tracy integration with zero overhead when disabled
- Binary size under 500KB with all deps enabled
- Backward API compatibility (existing `dpb::from().where().map().collect()` must still work)

### Must NOT Have (Guardrails)
- DO NOT break the fused lambda composition pattern (the killer feature)
- DO NOT make Highway/fmt/Tracy required dependencies
- DO NOT remove the simple `profiler.hpp` fallback
- DO NOT touch `thread_pool.hpp` (already optimized in previous plan)
- DO NOT touch `pipeline_stats.hpp` atomics (already optimized in previous plan)
- DO NOT add Intel TBB or Boost as dependencies (too heavy)
- DO NOT change the public API surface (Pipeline, from(), collect(), etc.)
- DO NOT exceed 500KB binary size for any example with all deps enabled

---

## Verification Strategy (MANDATORY)

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed. No exceptions.

### Test Decision
- **Infrastructure exists**: YES (Catch2, but FetchContent currently broken)
- **Automated tests**: Tests-after (add test tasks after implementation tasks)
- **Framework**: Catch2 (already in CMakeLists.txt)
- **Agent-Executed QA**: ALWAYS (mandatory for all tasks)

### QA Policy
Every task MUST include agent-executed QA scenarios.
Evidence saved to `.sisyphus/evidence/task-{N}-{scenario-slug}.{ext}`.

- **All tasks**: `cmake --build build --config Release` must succeed
- **SIMD tasks**: Benchmark comparison with/without SIMD
- **Template tasks**: Compile test with old and new API styles
- **CMake tasks**: Configure/build with each flag combination

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Start Immediately — CMake foundation):
├── T1: Modernize CMakeLists.txt (LTO, presets, vcpkg) [quick]
├── T2: Add Highway as optional dependency via FetchContent [quick]
├── T3: Add {fmt} as optional dependency via FetchContent [quick]
└── T4: Add Tracy as optional dependency via FetchContent [quick]

Wave 2 (After Wave 1 — core integration):
├── T5: Create dpb/simd.hpp with Highway SIMD primitives [deep]
├── T6: Replace iostream with fmt in stats/profiler (conditional) [unspecified-high]
├── T7: Add Tracy zone macros (conditional compilation) [quick]
└── T8: Add std::pmr::vector + monotonic buffer for collect paths [quick]

Wave 3 (After Wave 2 — template migration Phase 1):
├── T9: Template-migrate filter/transform operations [deep]
├── T10: Template-migrate stream control (take/skip/take_while/skip_while) [unspecified-high]
├── T11: Template-migrate terminal reductions (fold/sum/count/min/max) [unspecified-high]
└── T12: Template-migrate collect/flat_map/scan [deep]

Wave 4 (After Wave 3 — SIMD stages + profiling):
├── T13: SIMD-accelerated filter for numeric pipelines [deep]
├── T14: SIMD-accelerated transform for numeric pipelines [deep]
├── T15: Tracy integration in collect_sequential/collect_parallel [quick]
└── T16: std::print现代化 in pipeline stats output [quick]

Wave 5 (After Wave 4 — type-erased fallback + API compat):
├── T17: Create PipelineErase type-erased wrapper for backward compat [deep]
├── T18: Update examples to demonstrate new SIMD/template features [visual-engineering]
└── T19: Add CMakePresets.json for common configurations [quick]

Wave 6 (After Wave 5 — testing + documentation):
├── T20: Fix Catch2 FetchContent and add integration tests [unspecified-high]
├── T21: Add SIMD benchmarks with Highway vs scalar comparison [unspecified-high]
├── T22: Binary size audit — verify all configs under 500KB [quick]
└── T23: Update README.md with new features and CMake flags [writing]

Wave FINAL (After ALL tasks — 4 parallel reviews):
├── F1: Plan compliance audit [oracle]
├── F2: Code quality review [unspecified-high]
├── F3: QA — build + run all configs + SIMD benchmarks [unspecified-high]
└── F4: Scope fidelity check [deep]

Critical Path: T1 → T5 → T9 → T13 → T17 → F1-F4
Parallel Speedup: ~65% faster than sequential
Max Concurrent: 4 (Waves 2, 3, 4)
```

### Dependency Matrix

| Task | Depends On | Blocks |
|------|-----------|--------|
| T1 | - | T5, T6, T7, T8 |
| T2 | T1 | T5, T13, T14 |
| T3 | T1 | T6, T16 |
| T4 | T1 | T7, T15 |
| T5 | T2 | T13, T14 |
| T6 | T3 | T16 |
| T7 | T4 | T15 |
| T8 | T1 | T12 |
| T9 | T5 | T13 |
| T10 | T9 | T14 |
| T11 | T9 | - |
| T12 | T9, T8 | T17 |
| T13 | T5, T9 | T17 |
| T14 | T5, T10 | - |
| T15 | T7, T12 | - |
| T16 | T6 | - |
| T17 | T12, T13 | T20, T21 |
| T18 | T17 | - |
| T19 | T1 | - |
| T20 | T17 | F1-F4 |
| T21 | T17 | F3 |
| T22 | T17 | F3 |
| T23 | T17 | - |

### Agent Dispatch Summary

- **Wave 1** (4 tasks): T1-T4 → `quick`
- **Wave 2** (4 tasks): T5 → `deep`, T6 → `unspecified-high`, T7 → `quick`, T8 → `quick`
- **Wave 3** (4 tasks): T9 → `deep`, T10-T11 → `unspecified-high`, T12 → `deep`
- **Wave 4** (4 tasks): T13-T14 → `deep`, T15-T16 → `quick`
- **Wave 5** (3 tasks): T17 → `deep`, T18 → `visual-engineering`, T19 → `quick`
- **Wave 6** (4 tasks): T20-T21 → `unspecified-high`, T22 → `quick`, T23 → `writing`
- **FINAL** (4 tasks): F1 → `oracle`, F2-T3 → `unspecified-high`, F4 → `deep`

---

## TODOs

- [ ] 1. Modernize CMakeLists.txt — LTO flags, vcpkg skeleton, option guards

  **What to do**:
  - Add `CMAKE_INTERPROCEDURAL_OPTIMIZATION` flag (respects `$<CONFIG:Release>`)
  - Refactor existing aggressive optimization flags into `target_compile_options` with generator expressions
  - Add `option(DPB_ENABLE_SIMD "Enable Highway SIMD vectorization" OFF)`
  - Add `option(DPB_ENABLE_FMT "Use {fmt} for output formatting" OFF)`
  - Add `option(DPB_ENABLE_TRACY "Enable Tracy profiling integration" OFF)`
  - Create `cmake/Dependencies.cmake` with FetchContent declarations for Highway, fmt, Tracy
  - Each dependency should only be fetched when its option is ON
  - Add `CMakePresets.json` with configurations: default, with-simd, with-fmt, with-tracy, all-deps
  - Fix the Catch2 FetchContent that's currently broken (git checkout issue)

  **Must NOT do**:
  - Do NOT remove existing optimization flags, only reorganize
  - Do NOT make any dependency required — all must be opt-in
  - Do NOT change the existing API surface

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []
    - CMake configuration is well-defined and mechanical

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T2, T3, T4)
  - **Parallel Group**: Wave 1
  - **Blocks**: T5, T6, T7, T8
  - **Blocked By**: None

  **References**:

  **Pattern References**:
  - `CMakeLists.txt:29-52` — Current MSVC optimization flags
  - `CMakeLists.txt:67-97` — Current GCC/Clang optimization flags
  - `CMakeLists.txt:100-119` — Current Catch2 FetchContent (broken, needs fix)

  **Why Each Reference Matters**:
  - MSVC/GCC flags need to be guarded by DPB_ENABLE_* options
  - The Catch2 FetchContent issue must be fixed (currently fails on git checkout)
  - FetchContent pattern for Highway/fmt/Tracy should mirror the existing Catch2 pattern

  **Acceptance Criteria**:

  [ ] `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` succeeds (default, no deps)
  [ ] `cmake -S . -B build -DDPB_ENABLE_SIMD=ON` succeeds (Highway fetched)
  [ ] `cmake -S . -B build -DDPB_ENABLE_FMT=ON` succeeds (fmt fetched)
  [ ] `cmake -S . -B build -DDPB_ENABLE_TRACY=ON` succeeds (Tracy fetched)
  [ ] `cmake -S . -B build -DDPB_BUILD_TESTS=ON` succeeds (Catch2 fetched)

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Default build with no optional deps
    Tool: Bash
    Preconditions: Clean build directory
    Steps:
      1. `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
      2. `cmake --build build --config Release`
      3. `./build/Release/basic_example.exe`
    Expected Result: Build succeeds, example produces correct output
    Failure Indicators: Linker errors, missing headers, runtime crash
    Evidence: .sisyphus/evidence/task-1-default-build.txt

  Scenario: Build fails with no deps when optional flag is missing
    Tool: Bash
    Preconditions: Clean build directory
    Steps:
      1. `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
      2. Verify no Highway/fmt/Tracy headers are included
    Expected Result: Build succeeds WITHOUT Highway/fmt/Tracy
    Failure Indicators: Compilation errors referencing Highway/fmt headers
    Evidence: .sisyphus/evidence/task-1-no-optional-deps.txt
  ```

  **Commit**: YES (groups with T2, T3, T4)
  - Message: `build(cmake): add Highway, fmt, Tracy as optional deps via FetchContent`
  - Files: `CMakeLists.txt`, `cmake/Dependencies.cmake`, `CMakePresets.json`

- [ ] 2. Add Highway as optional dependency via FetchContent

  **What to do**:
  - Add `FetchContent_Declare` for Highway (google/highway, latest release tag) in `cmake/Dependencies.cmake`
  - When `DPB_ENABLE_SIMD=ON`: FetchContent_MakeAvailable(Highway), link `declarative_pipeline` and `fast_pipeline` to `Highway::hwy`
  - Create `include/dpb/highway_config.hpp` with `#ifdef DPB_HAS_HIGHWAY` guards
  - Verify Highway compiles on MSVC and GCC/Clang

  **Must NOT do**:
  - Do NOT make Highway a required dependency
  - Do NOT modify any existing pipeline source files yet

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T1, T3, T4)
  - **Parallel Group**: Wave 1
  - **Blocks**: T5, T13, T14
  - **Blocked By**: T1 (needs CMake options defined)

  **References**:

  **Pattern References**:
  - `CMakeLists.txt:100-107` — Existing FetchContent pattern for Catch2

  **External References**:
  - Highway GitHub: https://github.com/google/highway
  - Highway CMake integration docs: FetchContent pattern

  **Why Each Reference Matters**:
  - Follow existing FetchContent pattern for consistency
  - Highway's CMakeLists.txt handles multi-target SIMD compilation

  **Acceptance Criteria**:

  [ ] Highway fetches and builds when `DPB_ENABLE_SIMD=ON`
  [ ] Highway is NOT fetched when `DPB_ENABLE_SIMD=OFF` (default)
  [ ] `include/dpb/highway_config.hpp` exists with `#ifdef DPB_HAS_HIGHWAY` guards

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Highway builds with SIMD enabled
    Tool: Bash
    Preconditions: T1 completed, clean build
    Steps:
      1. `cmake -S . -B build -DDPB_ENABLE_SIMD=ON -DCMAKE_BUILD_TYPE=Release`
      2. `cmake --build build --config Release`
    Expected Result: Highway is fetched and compiled, build succeeds
    Failure Indicators: Highway fetch fails, compilation errors in Highway headers
    Evidence: .sisyphus/evidence/task-2-highway-build.txt

  Scenario: Build without SIMD flag
    Tool: Bash
    Preconditions: T1 completed, clean build
    Steps:
      1. `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
      2. Verify Highway is NOT in build/_deps
    Expected Result: Build succeeds without Highway
    Failure Indicators: Highway fetched even though flag is OFF
    Evidence: .sisyphus/evidence/task-2-no-simd.txt
  ```

  **Commit**: YES (groups with T1, T3, T4)
  - Message: `build(cmake): add Highway, fmt, Tracy as optional deps via FetchContent`
  - Files: `cmake/Dependencies.cmake`, `include/dpb/highway_config.hpp`

- [ ] 3. Add {fmt} as optional dependency via FetchContent

  **What to do**:
  - Add `FetchContent_Declare` for {fmt} (fmtlib/fmt, v11.x) in `cmake/Dependencies.cmake`
  - When `DPB_ENABLE_FMT=ON`: FetchContent_MakeAvailable(fmt), link `declarative_pipeline` to `fmt::fmt`
  - Create `include/dpb/fmt_config.hpp` with `#ifdef DPB_HAS_FMT` guards
  - Create `include/dpb/format.hpp` that either uses `fmt::print` or falls back to `std::cout`/`std::format`

  **Must NOT do**:
  - Do NOT make fmt a required dependency
  - Do NOT modify existing stats/profiler output yet (that's T6)

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T1, T2, T4)
  - **Parallel Group**: Wave 1
  - **Blocks**: T6, T16
  - **Blocked By**: T1 (needs CMake options defined)

  **References**:

  **Pattern References**:
  - `CMakeLists.txt:100-107` — Existing FetchContent pattern

  **External References**:
  - {fmt} GitHub: https://github.com/fmtlib/fmt
  - {fmt} CMake integration: `FMT_INSTALL`, `FMT_MASTER_PROJECT`

  **Acceptance Criteria**:

  [ ] {fmt} fetches and builds when `DPB_ENABLE_FMT=ON`
  [ ] {fmt} is NOT fetched when `DPB_ENABLE_FMT=OFF` (default)
  [ ] `include/dpb/format.hpp` provides `dpb::print()` that dispatches to fmt or iostream

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: fmt builds with FMT enabled
    Tool: Bash
    Preconditions: T1 completed, clean build
    Steps:
      1. `cmake -S . -B build -DDPB_ENABLE_FMT=ON -DCMAKE_BUILD_TYPE=Release`
      2. `cmake --build build --config Release`
    Expected Result: fmt is fetched and compiled, build succeeds
    Failure Indicators: fmt fetch fails, compilation errors
    Evidence: .sisyphus/evidence/task-3-fmt-build.txt
  ```

  **Commit**: YES (groups with T1, T2, T4)
  - Message: `build(cmake): add Highway, fmt, Tracy as optional deps via FetchContent`
  - Files: `cmake/Dependencies.cmake`, `include/dpb/fmt_config.hpp`, `include/dpb/format.hpp`

- [ ] 4. Add Tracy as optional dependency via FetchContent

  **What to do**:
  - Add `FetchContent_Declare` for Tracy (wolfpld/tracy, latest release) in `cmake/Dependencies.cmake`
  - When `DPB_ENABLE_TRACY=ON`: FetchContent_MakeAvailable(Tracy), add `TRACY_ENABLE=ON` define
  - Create `include/dpb/tracy_config.hpp` with `#ifdef DPB_HAS_TRACY` guards
  - Create profiling macros: `DPB_ZONE_SCOPED`, `DPB_ZONE_TEXT`, `DPB_FRAME_MARK` that expand to Tracy zones when enabled, no-ops when disabled
  - Verify Tracy compiles on MSVC (known to work, but needs AVX2 note)

  **Must NOT do**:
  - Do NOT make Tracy a required dependency
  - Do NOT modify existing profiler.hpp yet (that's T7)

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T1, T2, T3)
  - **Parallel Group**: Wave 1
  - **Blocks**: T7, T15
  - **Blocked By**: T1 (needs CMake options defined)

  **References**:

  **Pattern References**:
  - `CMakeLists.txt:100-107` — Existing FetchContent pattern

  **External References**:
  - Tracy GitHub: https://github.com/wolfpld/tracy
  - Tracy integration docs: Add `public/TracyClient.cpp` to source, include `tracy/Tracy.hpp`

  **Acceptance Criteria**:

  [ ] Tracy fetches and builds when `DPB_ENABLE_TRACY=ON`
  [ ] Tracy is NOT fetched when `DPB_ENABLE_TRACY=OFF` (default)
  [ ] `include/dpb/tracy_config.hpp` provides macros that are no-ops when Tracy is disabled
  [ ] `DPB_ZONE_SCOPED` expands to `ZoneScoped` when Tracy enabled, `((void)0)` when disabled

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Tracy builds with TRACY enabled
    Tool: Bash
    Preconditions: T1 completed, clean build
    Steps:
      1. `cmake -S . -B build -DDPB_ENABLE_TRACY=ON -DCMAKE_BUILD_TYPE=Release`
      2. `cmake --build build --config Release`
    Expected Result: Tracy client is fetched and compiled, build succeeds
    Failure Indicators: Tracy fetch fails, compilation errors in Tracy source
    Evidence: .sisyphus/evidence/task-4-tracy-build.txt

  Scenario: Tracy macros are no-ops when disabled
    Tool: Bash
    Preconditions: T1 completed, build WITHOUT Tracy
    Steps:
      1. Create a test file that uses `DPB_ZONE_SCOPED` and `DPB_ZONE_TEXT`
      2. Compile with `DPB_ENABLE_TRACY=OFF`
      3. Verify in assembly that macros expand to nothing
    Expected Result: No Tracy calls in generated assembly
    Failure Indicators: Undefined Tracy symbols, Tracy header includes
    Evidence: .sisyphus/evidence/task-4-tracy-noop.txt
  ```

  **Commit**: YES (groups with T1, T2, T3)
  - Message: `build(cmake): add Highway, fmt, Tracy as optional deps via FetchContent`
  - Files: `cmake/Dependencies.cmake`, `include/dpb/tracy_config.hpp`

**Commit**: YES (groups with T1, T2, T3)
  - Message: `build(cmake): add Highway, fmt, Tracy as optional deps via FetchContent`
  - Files: `cmake/Dependencies.cmake`, `include/dpb/tracy_config.hpp`

- [ ] 5. Create dpb/simd.hpp with Highway SIMD primitives

  **What to do**:
  - Create `include/dpb/simd.hpp` with `#ifdef DPB_HAS_HIGHWAY` guards
  - Define SIMD wrapper functions: `dpb::simd::filter()`, `dpb::simd::transform()`, `dpb::simd::mask_store()`
  - Each function should use Highway's `hwy::HWY_NAMESPACE` macros for multi-target compilation
  - Implement NUMERIC-ONLY SIMD filter: load N elements, apply predicate in SIMD lanes, compress-mask store results
  - Implement NUMERIC-ONLY SIMD transform: load N elements, apply function in SIMD lanes, store results
  - Fall back to scalar loop when `DPB_HAS_HIGHWAY` is not defined
  - Support types: `int32_t`, `int64_t`, `float`, `double` (the types Highway handles well)

  **Must NOT do**:
  - Do NOT modify existing pipeline.hpp yet (that's T9+)
  - Do NOT make SIMD paths compile when `DPB_ENABLE_SIMD=OFF`
  - Do NOT support arbitrary types in SIMD paths — only numeric types

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: []
    - Highway SIMD requires understanding of lane-based programming, multi-target compilation

  **Parallelization**:
  - **Can Run In Parallel**: NO (needs T2)
  - **Parallel Group**: Wave 2
  - **Blocks**: T13, T14
  - **Blocked By**: T2 (Highway must be available)

  **References**:

  **Pattern References**:
  - `include/pipeline/pipeline.hpp:459-491` — Current scalar `collect_sequential` hot loop (the target for SIMD)
  - `include/pipeline/pipeline.hpp:162-170` — Current `filter()` fused lambda (target for SIMD filter)

  **External References**:
  - Highway docs: https://github.com/google/highway — `hwy::ForEachFloatType`, `hwy::Mask`, `hwy::CompressBlendedStore`
  - Highway SIMD filter pattern: compress-masked store is the idiomatic way to implement SIMD filter

  **Why Each Reference Matters**:
  - The hot loop structure determines how SIMD can be integrated
  - Highway's `CompressBlendedStore` is the key primitive for SIMD filter (packes passing elements contiguously)

  **Acceptance Criteria**:

  [ ] `include/dpb/simd.hpp` exists with `#ifdef DPB_HAS_HIGHWAY` guards
  [ ] `dpb::simd::filter<int>` compiles and produces correct results
  [ ] `dpb::simd::transform<float>` compiles and produces correct results
  [ ] Scalar fallback compiles and works when `DPB_ENABLE_SIMD=OFF`

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: SIMD filter produces same results as scalar
    Tool: Bash
    Preconditions: T2 completed, Highway available
    Steps:
      1. Write a small test that calls dpb::simd::filter with a predicate on 10000 integers
      2. Compare output with scalar filter
      3. Verify all 10000 results match
    Expected Result: SIMD and scalar produce identical results
    Failure Indicators: Value mismatch, crash on unsupported types
    Evidence: .sisyphus/evidence/task-5-simd-filter-correctness.txt

  Scenario: SIMD transform produces same results as scalar
    Tool: Bash
    Preconditions: T2 completed, Highway available
    Steps:
      1. Write a small test that calls dpb::simd::transform with x*x on 10000 floats
      2. Compare output with scalar transform
      3. Verify all results match within floating-point epsilon
    Expected Result: SIMD and scalar produce identical results (within epsilon)
    Failure Indicators: Value mismatch beyond float epsilon
    Evidence: .sisyphus/evidence/task-5-simd-transform-correctness.txt
  ```

  **Commit**: YES
  - Message: `feat(simd): add dpb/simd.hpp with Highway SIMD primitives`
  - Files: `include/dpb/simd.hpp`

- [ ] 6. Replace iostream with fmt in stats/profiler output (conditional)

  **What to do**:
  - Modify `pipeline_stats.hpp::print()` to use `dpb::format()` instead of `std::cout` + `std::setw`/`std::setprecision`
  - Modify `profiler.hpp::print()` to use `dpb::format()` instead of `std::cout`
  - `dpb::format()` dispatches to `fmt::print` when `DPB_HAS_FMT`, otherwise falls back to `std::cout`
  - Include `dpb/format.hpp` which provides the dispatch

  **Must NOT do**:
  - Do NOT remove `std::cout` fallback — it must work without fmt
  - Do NOT change the output format — must remain identical
  - Do NOT modify `ResultWithStats::print_stats()` in pipeline.hpp yet (separate task)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T5, T7, T8)
  - **Parallel Group**: Wave 2
  - **Blocks**: T16
  - **Blocked By**: T3 (fmt must be available)

  **References**:

  **Pattern References**:
  - `include/pipeline_stats.hpp:31-63` — Current `print()` method using `std::cout`, `std::setw`, `std::setprecision`
  - `include/profiler.hpp:29-45` — Current profiler print using `std::cout`, `std::setw`
  - `include/dpb/format.hpp` (from T3) — Dispatch header

  **Acceptance Criteria**:

  [ ] `pipeline_stats.hpp::print()` uses `dpb::format()` conditional dispatch
  [ ] `profiler.hpp::print()` uses `dpb::format()` conditional dispatch
  [ ] Output is identical whether fmt is enabled or disabled
  [ ] Binary without fmt is smaller than binary with fmt (fmt replaces iostream bloat)

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Stats output is identical with and without fmt
    Tool: Bash
    Preconditions: T3 completed, both build configs available
    Steps:
      1. Build with DPB_ENABLE_FMT=OFF, run basic_example, capture output
      2. Build with DPB_ENABLE_FMT=ON, run basic_example, capture output
      3. diff the two outputs
    Expected Result: Outputs are identical (same content, possibly different formatting)
    Failure Indicators: Missing stats, garbled output, crash
    Evidence: .sisyphus/evidence/task-6-fmt-output-comparison.txt

  Scenario: Binary size reduction with fmt
    Tool: Bash
    Preconditions: Both build configs available
    Steps:
      1. Build with DPB_ENABLE_FMT=OFF, record binary size
      2. Build with DPB_ENABLE_FMT=ON, record binary size
      3. Compare sizes
    Expected Result: fmt build is smaller or similar (iostream bloat removed)
    Failure Indicators: fmt build significantly larger (>10% increase)
    Evidence: .sisyphus/evidence/task-6-fmt-binary-size.txt
  ```

  **Commit**: YES
  - Message: `feat(format): replace iostream with fmt for stats/profiler output`
  - Files: `include/pipeline_stats.hpp`, `include/profiler.hpp`

- [ ] 7. Add Tracy zone macros (conditional compilation)

  **What to do**:
  - Create `include/dpb/tracy_config.hpp` (if not already created in T4)
  - Define macros: `DPB_ZONE_SCOPED`, `DPB_ZONE_TEXT(name, text)`, `DPB_FRAME_MARK`
  - When `DPB_HAS_TRACY`: expand to `ZoneScoped`, `ZoneText(name, text)`, `FrameMark`
  - When `DPB_HAS_TRACY` is NOT defined: expand to `((void)0)`
  - Add Tracy zones to `collect_sequential()` and `collect_parallel()` hot paths
  - Add Tracy frame mark after each `collect()` call

  **Must NOT do**:
  - Do NOT modify the fused lambda pattern
  - Do NOT add any overhead when Tracy is disabled (macros must be pure no-ops)
  - Do NOT require Tracy server to be running for normal execution

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T5, T6, T8)
  - **Parallel Group**: Wave 2
  - **Blocks**: T15
  - **Blocked By**: T4 (Tracy must be available)

  **References**:

  **Pattern References**:
  - `include/pipeline/pipeline.hpp:459-519` — `collect_sequential()` — add `DPB_ZONE_SCOPED` at entry
  - `include/pipeline/pipeline.hpp:523-614` — `collect_parallel()` — add `DPB_ZONE_SCOPED` at entry

  **Acceptance Criteria**:

  [ ] Tracy macros expand to no-ops when `DPB_ENABLE_TRACY=OFF`
  [ ] Tracy macros expand to Tracy calls when `DPB_ENABLE_TRACY=ON`
  [ ] No measurable performance overhead when Tracy is disabled
  [ ] Build succeeds in both configurations

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Tracy zones appear in profiler when enabled
    Tool: Bash
    Preconditions: T4 completed, Tracy build available
    Steps:
      1. Build with DPB_ENABLE_TRACY=ON
      2. Run basic_example
      3. Verify Tracy zones appear in profiling output
    Expected Result: "collect_sequential" and "collect_parallel" zones visible
    Failure Indicators: No Tracy zones, crash, build failure
    Evidence: .sisyphus/evidence/task-7-tracy-zones.txt

  Scenario: No overhead when Tracy is disabled
    Tool: Bash
    Preconditions: Default build (no Tracy)
    Steps:
      1. Build with DPB_ENABLE_TRACY=OFF
      2. Run basic_example benchmark section
      3. Record timing
    Expected Result: Performance identical to pre-Tracy baseline (within 1%)
    Failure Indicators: Performance regression > 1%
    Evidence: .sisyphus/evidence/task-7-tracy-noop.txt
  ```

  **Commit**: YES
  - Message: `feat(profiling): add conditional Tracy zone macros`
  - Files: `include/dpb/tracy_config.hpp`, `include/pipeline/pipeline.hpp`

- [ ] 8. Add std::pmr::vector with monotonic buffer for collect paths

  **What to do**:
  - Create `include/dpb/memory.hpp` with a `dpb::collect_buffer<Out>` type
  - When types are trivially copyable and small (≤64 bytes): use `std::pmr::vector<Out>` with monotonic buffer
  - When types are complex: fall back to `std::vector<Out>` with `reserve()`
  - The monotonic buffer should be stack-allocated for small pipelines, heap-allocated for large ones
  - Use `std::pmr::monotonic_buffer_resource` as the upstream allocator
  - Replace `std::vector<Out> result; result.reserve(estimated);` in `collect_sequential()` and `collect_parallel()`

  **Must NOT do**:
  - Do NOT replace vectors in the thread pool or profiler
  - Do NOT break existing public API — this is internal only
  - Do NOT use PMR when it would hurt performance (type-erased allocators have overhead for small collections)

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T5, T6, T7)
  - **Parallel Group**: Wave 2
  - **Blocks**: T12
  - **Blocked By**: T1 (needs C++17+ PMR support confirmed in CMake)

  **References**:

  **Pattern References**:
  - `include/pipeline/pipeline.hpp:462-466` — Current `result.reserve(estimated)` pattern in `collect_sequential()`
  - `include/pipeline/pipeline.hpp:556-557` — Current `local_results[t].reserve(chunk_size)` in `collect_parallel()`

  **Why Each Reference Matters**:
  - These are the main allocation hot spots — replacing with monotonic buffers reduces heap fragmentation
  - The monotonic buffer is especially beneficial for the parallel path where threads allocate vectors frequently

  **Acceptance Criteria**:

  [ ] `include/dpb/memory.hpp` exists with `dpb::collect_buffer<Out>` type
  [ ] `collect_sequential()` uses monotonic buffer for trivially copyable types
  - [ ] `collect_parallel()` uses monotonic buffers for thread-local results
  - [ ] Existing tests/examples produce identical results
  - [ ] No memory leaks under AddressSanitizer

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Monotonic buffer produces same results as std::vector
    Tool: Bash
    Preconditions: Build succeeds
    Steps:
      1. Run basic_example and capture output
      2. Run parallel_example and capture output
    Expected Result: All results identical to pre-change baseline
    Failure Indicators: Different output, crash, hang
    Evidence: .sisyphus/evidence/task-8-pmr-correctness.txt
  ```

  **Commit**: YES
  - Message: `feat(memory): add std::pmr::vector with monotonic buffer for collect`
  - Files: `include/dpb/memory.hpp`, `include/pipeline/pipeline.hpp`

- Files: `include/dpb/memory.hpp`, `include/pipeline/pipeline.hpp`

- [ ] 9. Template-migrate filter/transform operations

  **What to do**:
  - The current `Pipeline<In, Out, OpFunc>` already uses a template parameter for `OpFunc`, but `OpFunc` defaults to `std::function<bool(const In&, Out&)>`. This task makes the default a compile-time lambda.
  - Refactor `filter()` and `transform()` to ensure the fused lambda is fully inlinable by removing any `std::function` type erasure
  - Verify with `-O2` that the compiler inlines the entire chain (check assembly for call instructions)
  - Add `[[gnu::always_inline]]` or `__forceinline` on fused lambda operators
  - Ensure backward compatibility: code like `dpb::from(data).where([](int x) {...}).map([](int x) {...}).collect(data)` must still compile

  **Must NOT do**:
  - Do NOT change the public API — `.filter()`, `.transform()`, `.where()`, `.map()` must still work identically
  - Do NOT touch the FUSED LAMBDA PATTERN (the core composition logic in filter/transform)
  - Do NOT break `collect()`, `collect_sequential()`, or `collect_parallel()`

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO (foundation for T10-T12)
  - **Parallel Group**: Wave 3 (start first)
  - **Blocks**: T10, T11, T12, T13
  - **Blocked By**: T5 (SIMD primitives must be available for reference)

  **References**:

  **Pattern References**:
  - `include/pipeline/pipeline.hpp:94` — `template<typename In, typename Out = In, typename OpFunc = std::function<bool(const In&, Out&)>>` — the type-erased default that needs to change
  - `include/pipeline/pipeline.hpp:162-171` — `filter()` fused lambda — the KILLER FEATURE, DO NOT TOUCH the composition logic
  - `include/pipeline/pipeline.hpp:175-189` — `transform()` fused lambda — DO NOT TOUCH the composition logic

  **Why Each Reference Matters**:
  - Line 94 is the source of the `std::function` type erasure that prevents inlining
  - Lines 162-189 are the fused lambda patterns that MUST remain identical

  **Acceptance Criteria**:

  [ ] `Pipeline<In, Out>` deduces OpFunc from the first operation (no `std::function`)
  - [ ] `dpb::from(data).where(pred).map(fn).collect(data)` compiles and produces same results
  - [ ] Compiler inlines the entire chain at `-O2` (verify with `nm` or assembly check)
  - [ ] No `std::function` in the type signature of a chained pipeline

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Existing API still works
    Tool: Bash
    Preconditions: Build succeeds
    Steps:
      1. Run basic_example
      2. Verify all output matches baseline
    Expected Result: All results identical to pre-change
    Failure Indicators: Compile errors, different output, crash
    Evidence: .sisyphus/evidence/task-9-api-compat.txt

  Scenario: Compiler inlines fused lambdas
    Tool: Bash
    Preconditions: Build with -O2
    Steps:
      1. Compile basic_example with `-O2 -S` to get assembly
      2. Search for `call` instructions in the hot loop
      3. Verify no indirect calls through std::function
    Expected Result: No indirect calls in the fused lambda hot path
    Failure Indicators: `call` instructions referencing `std::function` vtable
    Evidence: .sisyphus/evidence/task-9-inline-check.txt
  ```

  **Commit**: YES
  - Message: `refactor(pipeline): template-migrate filter/transform operations`
  - Files: `include/pipeline/pipeline.hpp`

- [ ] 10. Template-migrate stream control (take/skip/take_while/skip_while)

  **What to do**:
  - Ensure `take()`, `skip()`, `take_while()`, `skip_while()` fully template the operation chain
  - These already use auto-deduced lambda types — verify they work correctly with the new OpFunc default
  - Add `[[gnu::always_inline]]` to ensure inlining

  **Must NOT do**:
  - Do NOT change the behavior of any stream control operation
  - Do NOT change the public API

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO (needs T9)
  - **Parallel Group**: Wave 3
  - **Blocks**: T14
  - **Blocked By**: T9

  **References**:

  **Pattern References**:
  - `include/pipeline/pipeline.hpp:206-229` — `take()` and `skip()` implementations

  **Acceptance Criteria**:

  [ ] All stream control operations compile and produce correct results
  [ ] Assembly check shows inlined operations

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Stream control operations work correctly
    Tool: Bash
    Preconditions: T9 completed
    Steps:
      1. Run basic_example (tests take, skip, etc.)
      2. Verify all output matches baseline
    Expected Result: All results identical to pre-change
    Failure Indicators: Wrong output count, crashes
    Evidence: .sisyphus/evidence/task-10-stream-control.txt
  ```

  **Commit**: YES
  - Message: `refactor(pipeline): template-migrate stream control operations`
  - Files: `include/pipeline/pipeline.hpp`

- [ ] 11. Template-migrate terminal reductions (fold/sum/count/min/max)

  **What to do**:
  - Ensure `fold()`, `sum()`, `count()`, `min()`, `max()`, `all_of()`, `any_of()`, `none_of()` use fully templated OpFunc
  - Verify all terminal reductions compile correctly with the new OpFunc default
  - Add `[[gnu::always_inline]]` where appropriate

  **Must NOT do**:
  - Do NOT change computed results for any reduction
  - Do NOT add new reduction operations (that's a future task)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO (needs T9)
  - **Parallel Group**: Wave 3
  - **Blocks**: None (reductions are leaf operations)
  - **Blocked By**: T9

  **References**:

  **Pattern References**:
  - `include/pipeline/pipeline.hpp:283-361` — All terminal reduction implementations

  **Acceptance Criteria**:

  [ ] `fold`, `sum`, `count`, `min`, `max`, `all_of`, `any_of`, `none_of` all produce correct results
  [ ] No `std::function` in type signatures

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: All reductions produce correct results
    Tool: Bash
    Preconditions: T9 completed
    Steps:
      1. Run basic_example (tests sum, count, min, all_of)
      2. Verify all reduction outputs match baseline
    Expected Result: All values identical
    Failure Indicators: Wrong sums, counts, min/max values
    Evidence: .sisyphus/evidence/task-11-reductions.txt
  ```

  **Commit**: YES
  - Message: `refactor(pipeline): template-migrate terminal reductions`
  - Files: `include/pipeline/pipeline.hpp`

- [ ] 12. Template-migrate collect/flat_map/scan

  **What to do**:
  - Ensure `collect()`, `collect_sequential()`, `collect_parallel()`, `flat_map()`, `scan()`, `to_vector()`, `collect_sorted()`, `collect_distinct()`, `collect_into()` use fully templated OpFunc
  - Use `dpb::collect_buffer<Out>` from T8 for memory allocation in collect paths
  - Verify parallel collect still works correctly with the thread pool

  **Must NOT do**:
  - Do NOT change the collect output format or ordering guarantees
  - Do NOT break `ParallelPreserveOrder` behavior

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO (needs T9 and T8)
  - **Parallel Group**: Wave 3 (last in wave, most complex)
  - **Blocks**: T17
  - **Blocked By**: T9, T8

  **References**:

  **Pattern References**:
  - `include/pipeline/pipeline.hpp:443-614` — `collect()`, `collect_sequential()`, `collect_parallel()`
  - `include/pipeline/pipeline.hpp:403-439` — `flat_map()` and `scan()`
  - `include/dpb/memory.hpp` (from T8) — `dpb::collect_buffer<Out>` type

  **Acceptance Criteria**:

  [ ] `collect()` produces identical results for sequential and parallel paths
  [ ] `flat_map()` and `scan()` produce identical results
  [ ] Parallel collect preserves order when `ExecutionPolicy::ParallelPreserveOrder` is used
  [ ] No `std::function` in type signatures of collected pipelines

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Collect produces identical results
    Tool: Bash
    Preconditions: T9, T8 completed
    Steps:
      1. Run basic_example and parallel_example
      2. Verify all results match baseline
    Expected Result: All results identical
    Failure Indicators: Different output, crashes, hangs
    Evidence: .sisyphus/evidence/task-12-collect-correctness.txt

  Scenario: Parallel collect preserves order
    Tool: Bash
    Preconditions: T9, T8 completed
    Steps:
      1. Run parallel_example
      2. Verify "Results match (ordered): yes"
    Expected Result: Ordered results match sequential results
    Failure Indicators: "Results match (ordered): no"
    Evidence: .sisyphus/evidence/task-12-parallel-order.txt
  ```

  **Commit**: YES
  - Message: `refactor(pipeline): template-migrate collect/flat_map/scan`
  - Files: `include/pipeline/pipeline.hpp`

- Message: `refactor(pipeline): template-migrate collect/flat_map/scan`
  - Files: `include/pipeline/pipeline.hpp`

- [ ] 13. SIMD-accelerated filter for numeric pipelines

  **What to do**:
  - Create `include/dpb/simd_filter.hpp` that provides SIMD-accelerated filter for numeric types
  - Use the `dpb::simd::filter` primitives from T5
  - When `DPB_ENABLE_SIMD=ON` and input type is numeric (int32, int64, float, double): use Highway SIMD filter
  - When `DPB_ENABLE_SIMD=OFF` or input type is non-numeric: fall back to scalar filter
  - Add a `simd_filter<In>` function that accepts a predicate and returns indices of passing elements
  - Integrate into `collect_sequential()` hot loop when pipeline is a simple filter

  **Must NOT do**:
  - Do NOT modify the fused lambda pattern — SIMD filter is applied as an optimization BEFORE the fused lambda for batch processing
  - Do NOT make SIMD filter the default for all types — only numeric types benefit
  - Do NOT break backward compatibility — SIMD is an optimization, not a replacement

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO (needs T5 and T9)
  - **Parallel Group**: Wave 4
  - **Blocks**: T17
  - **Blocked By**: T5 (SIMD primitives), T9 (template migration)

  **References**:

  **Pattern References**:
  - `include/dpb/simd.hpp` (from T5) — `dpb::simd::filter()` primitive
  - `include/pipeline/pipeline.hpp:459-519` — `collect_sequential()` hot loop to optimize

  **Why Each Reference Matters**:
  - SIMD filter must be integrated into the hot loop without changing its semantics
  - Highway's `CompressBlendedStore` is the key primitive for SIMD filter (packes passing elements contiguously)

  **Acceptance Criteria**:

  [ ] SIMD filter produces identical results to scalar filter for all numeric types
  - [ ] Benchmark shows ≥2x throughput improvement on int32/float filter operations
  - [ ] Non-numeric types fall back to scalar path correctly

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: SIMD filter correctness
    Tool: Bash
    Preconditions: Highway available, SIMD enabled
    Steps:
      1. Create test: filter 1M ints where x % 2 == 0
      2. Compare SIMD vs scalar output
    Expected Result: Identical results
    Failure Indicators: Value mismatch, crash on boundary cases
    Evidence: .sisyphus/evidence/task-13-simd-filter-correctness.txt

  Scenario: SIMD filter performance
    Tool: Bash
    Preconditions: Highway available, SIMD enabled
    Steps:
      1. Run benchmark with DPB_ENABLE_SIMD=ON and DPB_ENABLE_SIMD=OFF
      2. Compare throughput on int32/float filter operations
    Expected Result: ≥2x throughput improvement with SIMD
    Failure Indicators: SIMD slower than scalar, no improvement
    Evidence: .sisyphus/evidence/task-13-simd-filter-benchmark.txt
  ```

  **Commit**: YES
  - Message: `feat(simd): SIMD-accelerated filter for numeric pipelines`
  - Files: `include/dpb/simd_filter.hpp`, `include/pipeline/pipeline.hpp`

- [ ] 14. SIMD-accelerated transform for numeric pipelines

  **What to do**:
  - Create `include/dpb/simd_transform.hpp` that provides SIMD-accelerated transform for numeric types
  - Use the `dpb::simd::transform` primitives from T5
  - When `DPB_ENABLE_SIMD=ON` and input→output types are both numeric: use Highway SIMD transform
  - When `DPB_ENABLE_SIMD=OFF` or types are non-numeric: fall back to scalar transform
  - Add `simd_transform<In, Out>` function that applies a function to N elements at once

  **Must NOT do**:
  - Do NOT modify the fused lambda pattern
  - Do NOT break backward compatibility

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T13)
  - **Parallel Group**: Wave 4
  - **Blocks**: None (parallel optimization)
  - **Blocked By**: T5, T10

  **References**:

  **Pattern References**:
  - `include/dpb/simd.hpp` (from T5) — `dpb::simd::transform()` primitive
  - `include/pipeline/pipeline.hpp:475-491` — hot loop where transform is applied

  **Acceptance Criteria**:

  [ ] SIMD transform produces identical results to scalar transform for numeric types
  [ ] Floating-point results match within epsilon (1e-6)
  [ ] Benchmark shows ≥2x throughput improvement on float/double transforms

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: SIMD transform correctness
    Tool: Bash
    Steps:
      1. Create test: transform 1M floats with x * x + 1.0f
      2. Compare SIMD vs scalar output (within epsilon)
    Expected Result: All values match within float epsilon
    Failure Indicators: Value mismatch beyond epsilon
    Evidence: .sisyphus/evidence/task-14-simd-transform-correctness.txt
  ```

  **Commit**: YES
  - Message: `feat(simd): SIMD-accelerated transform for numeric pipelines`
  - Files: `include/dpb/simd_transform.hpp`, `include/pipeline/pipeline.hpp`

- [ ] 15. Tracy integration in collect_sequential/collect_parallel

  **What to do**:
  - Add `DPB_ZONE_SCOPED` macro at the top of `collect_sequential()` and `collect_parallel()`
  - Add `DPB_ZONE_TEXT` with stage names (filter, transform, etc.) if profiler is enabled
  - Add `DPB_FRAME_MARK` at the end of each pipeline execution
  - Verify that Tracy Profiler can connect and display zones when `DPB_ENABLE_TRACY=ON`

  **Must NOT do**:
  - Do NOT add any overhead when Tracy is disabled (macros must expand to `((void)0)`)
  - Do NOT modify the computation logic in any way

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T13, T14, T16)
  - **Parallel Group**: Wave 4
  - **Blocks**: None
  - **Blocked By**: T7 (Tracy macros must be defined), T12 (collect must be migrated)

  **References**:

  **Pattern References**:
  - `include/dpb/tracy_config.hpp` (from T4/T7) — `DPB_ZONE_SCOPED`, `DPB_ZONE_TEXT`, `DPB_FRAME_MARK`
  - `include/pipeline/pipeline.hpp:459` — `collect_sequential()` entry point
  - `include/pipeline/pipeline.hpp:523` — `collect_parallel()` entry point

  **Acceptance Criteria**:

  [ ] Tracy zones appear in Tracy Profiler when `DPB_ENABLE_TRACY=ON`
  [ ] No measurable overhead when `DPB_ENABLE_TRACY=OFF`
  [ ] Build succeeds in both configurations

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Tracy zones visible in profiler
    Tool: Bash
    Preconditions: Tracy build available
    Steps:
      1. Build with DPB_ENABLE_TRACY=ON
      2. Run basic_example with Tracy server
      3. Verify "collect_sequential" zone appears
    Expected Result: Tracy zones visible in profiler GUI
    Failure Indicators: No zones, crash
    Evidence: .sisyphus/evidence/task-15-tracy-zones-visible.txt
  ```

  **Commit**: YES
  - Message: `feat(profiling): Tracy integration in hot paths`
  - Files: `include/pipeline/pipeline.hpp`

- [ ] 16. std::print modernization in pipeline stats output

  **What to do**:
  - When `DPB_ENABLE_FMT=ON`: use `fmt::print` with format strings for stats output
  - When `DPB_ENABLE_FMT=OFF`: use `std::print` (C++23) or fall back to `std::cout`
  - Modernize `ResultWithStats::print_stats()` in pipeline.hpp to use the same `dpb::format()` dispatch
  - Replace `std::fixed << std::setprecision(2)` with `fmt::format("{:.2f}", val)` or `std::format("{:.2f}", val)`

  **Must NOT do**:
  - Do NOT change the output content or format — only the implementation
  - Do NOT break backward compatibility with C++23 platforms that have `std::print`

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T13, T14, T15)
  - **Parallel Group**: Wave 4
  - **Blocks**: None
  - **Blocked By**: T6 (fmt dispatch must be ready)

  **References**:

  **Pattern References**:
  - `include/pipeline/pipeline.hpp:45-70` — `ResultWithStats::print_stats()` using `std::cout << std::fixed << std::setprecision`
  - `include/dpb/format.hpp` (from T3) — `dpb::format()` dispatch

  **Acceptance Criteria**:

  [ ] Stats output is identical whether using fmt or iostream
  - [ ] `std::format` is used as fallback on C++23 compilers when fmt is not available
  [ ] Binary size with fmt is ≤ previous binary size with iostream

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Stats output identical across configurations
    Tool: Bash
    Steps:
      1. Build with DPB_ENABLE_FMT=OFF, run basic_example, capture stats output
      2. Build with DPB_ENABLE_FMT=ON, run basic_example, capture stats output
      3. Compare outputs line by line
    Expected Result: Outputs are identical
    Failure Indicators: Different formatting, missing numbers, crash
    Evidence: .sisyphus/evidence/task-16-format-output.txt
  ```

  **Commit**: YES
  - Message: `feat(format): std::print modernization in stats`
  - Files: `include/pipeline/pipeline.hpp`

- Message: `feat(format): std::print modernization in stats`
  - Files: `include/pipeline/pipeline.hpp`

- [ ] 17. Create PipelineErase type-erased wrapper for backward compatibility

  **What to do**:
  - Create `include/dpb/pipeline_erase.hpp` with a `PipelineErase<In, Out>` class
  - `PipelineErase` wraps a fully-templated `Pipeline<In, Out, OpFunc>` into a type-erased `std::function<bool(const In&, Out&)>`
  - This allows users who need to store pipelines in containers or pass them through type-erased boundaries
  - Add a `.erase()` method on `Pipeline` that converts to `PipelineErase`
  - Document that `PipelineErase` has ~20-40ns per-element overhead vs. fully-templated pipeline

  **Must NOT do**:
  - Do NOT make `PipelineErase` the default — full templates are the primary path
  - Do NOT remove `std::function` from the primary `Pipeline` template parameter (it should still be possible to construct with explicit `std::function`)

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO (needs T12 and T13)
  - **Parallel Group**: Wave 5
  - **Blocks**: T20, T21
  - **Blocked By**: T12 (all operations must be migrated), T13 (SIMD filter must be done)

  **References**:

  **Pattern References**:
  - `include/pipeline/pipeline.hpp:94` — Current `Pipeline<In, Out, OpFunc>` template signature

  **Why Each Reference Matters**:
  - Users who store pipelines in `std::vector` or pass them through `std::function` boundaries need the type-erased wrapper

  **Acceptance Criteria**:

  [ ] `PipelineErase<In, Out>` compiles and works
  - [ ] `.erase()` method on `Pipeline` converts to `PipelineErase`
  - [ ] `PipelineErase` produces identical results to fully-templated pipeline
  - [ ] Performance overhead is documented (~20-40ns per element)

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: PipelineErase produces correct results
    Tool: Bash
    Steps:
      1. Create a pipeline, call .erase(), store in PipelineErase<int, int>
      2. Run .collect() on the erased pipeline
      3. Compare results with non-erased pipeline
    Expected Result: Identical results
    Failure Indicators: Different output, crash, compile error
    Evidence: .sisyphus/evidence/task-17-pipeline-erase.txt
  ```

  **Commit**: YES
  - Message: `refactor(pipeline): add type-erased PipelineErase wrapper for backward compat`
  - Files: `include/dpb/pipeline_erase.hpp`, `include/pipeline/pipeline.hpp`

- [ ] 18. Update examples to demonstrate new SIMD/template features

  **What to do**:
  - Add a new example: `examples/simd_demo.cpp` that demonstrates SIMD-accelerated filter/transform on numeric data
  - Add `#ifdef DPB_HAS_HIGHWAY` sections showing SIMD performance comparison
  - Update `examples/basic_usage.cpp` to show new `dpb::format()` stats when available
  - Update `examples/parallel_usage.cpp` to show Tracy profiling zones when available
  - Ensure all examples compile in default mode (zero deps)

  **Must NOT do**:
  - Do NOT make examples require any dependency
  - Do NOT change existing example behavior — only ADD new functionality demonstrations

  **Recommended Agent Profile**:
  - **Category**: `visual-engineering`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T19)
  - **Parallel Group**: Wave 5
  - **Blocks**: None
  - **Blocked By**: T17 (PipelineErase for backward-compat demonstration)

  **References**:

  **Pattern References**:
  - `examples/basic_usage.cpp` — Current basic example
  - `examples/parallel_usage.cpp` — Current parallel example

  **Acceptance Criteria**:

  [ ] `examples/simd_demo.cpp` compiles and runs with `DPB_ENABLE_SIMD=ON`
  - [ ] All existing examples still compile and produce correct output in default mode
  - [ ] SIMD demo shows performance comparison between scalar and SIMD paths

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: SIMD demo shows performance improvement
    Tool: Bash
    Steps:
      1. Build with DPB_ENABLE_SIMD=ON
      2. Run simd_demo
      3. Verify SIMD path shows ≥2x improvement over scalar
    Expected Result: SIMD benchmark output shows improvement
    Failure Indicators: SIMD path slower, crash, wrong results
    Evidence: .sisyphus/evidence/task-18-simd-demo.txt
  ```

  **Commit**: YES
  - Message: `docs(examples): demonstrate SIMD and template features`
  - Files: `examples/simd_demo.cpp`, `examples/basic_usage.cpp`, `examples/parallel_usage.cpp`, `CMakeLists.txt`

- [ ] 19. Add CMakePresets.json for common configurations

  **What to do**:
  - Create `CMakePresets.json` with preset configurations:
    - `default` — Zero deps, Release build
    - `with-simd` — Highway enabled
    - `with-fmt` — {fmt} enabled
    - `with-tracy` — Tracy enabled
    - `with-all` — All deps enabled
    - `dev` — Debug build with all deps + tests
  - Each preset should set `CMAKE_BUILD_TYPE`, `DPB_ENABLE_*` flags, and compiler flags
  - Add `CMakeUserPresets.json` to `.gitignore`

  **Must NOT do**:
  - Do NOT modify `CMakeLists.txt` — presets are purely additive

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T17, T18)
  - **Parallel Group**: Wave 5
  - **Blocks**: None
  - **Blocked By**: T1 (needs CMake options defined)

  **References**:

  **Pattern References**:
  - `CMakeLists.txt:15-18` — Current option definitions

  **Acceptance Criteria**:

  [ ] `cmake --preset=with-simd` configures correctly
  [ ] `cmake --preset=with-all` configures correctly
  [ ] `cmake --preset=default` configures with zero deps

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: All presets configure successfully
    Tool: Bash
    Steps:
      1. `cmake --preset=default`
      2. `cmake --preset=with-simd`
      3. `cmake --preset=with-all`
      4. Verify each preset configures without errors
    Expected Result: All presets configure successfully
    Failure Indicators: Configuration errors, missing presets
    Evidence: .sisyphus/evidence/task-19-cmake-presets.txt
  ```

  **Commit**: YES
  - Message: `build(cmake): add CMakePresets.json`
  - Files: `CMakePresets.json`, `.gitignore`

- [ ] 20. Fix Catch2 FetchContent and add integration tests

  **What to do**:
  - Debug and fix the Catch2 FetchContent issue (currently fails on git checkout)
  - Options: pin a specific commit hash, use a local copy, or switch to vcpkg
  - Add integration tests for:
    - `dpb::from().where().map().collect()` — basic pipeline
    - `dpb::from().parallel(4).where().map().collect()` — parallel pipeline
    - SIMD filter/transform (when `DPB_ENABLE_SIMD=ON`)
    - Type-erased pipeline (`PipelineErase`)
    - Tracy zones (when `DPB_ENABLE_TRACY=ON`)
  - Add tests to `tests/test_pipeline.cpp`, `tests/test_simd.cpp` (new), `tests/test_integration.cpp` (new)

  **Must NOT do**:
  - Do NOT change existing test behavior — only ADD new tests
  - Do NOT make tests require optional dependencies unless guarded by `#ifdef`

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO (needs T17 for PipelineErase tests)
  - **Parallel Group**: Wave 6
  - **Blocks**: F1-F4
  - **Blocked By**: T17

  **References**:

  **Pattern References**:
  - `tests/test_pipeline.cpp` — Existing test structure
  - `tests/test_parallel_pipeline.cpp` — Existing parallel tests
  - `CMakeLists.txt:100-119` — Current broken Catch2 FetchContent

  **Acceptance Criteria**:

  [ ] Catch2 FetchContent works correctly
  [ ] `cmake --build build --target pipeline_tests` succeeds
  [ ] All existing tests pass
  [ ] New integration tests pass

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Catch2 tests build and run
    Tool: Bash
    Steps:
      1. cmake -S . -B build -DDPB_BUILD_TESTS=ON
      2. cmake --build build --config Release --target pipeline_tests
      3. Run tests
    Expected Result: All tests pass
    Failure Indicators: Catch2 fetch fails, test compilation errors, test failures
    Evidence: .sisyphus/evidence/task-20-catch2-tests.txt
  ```

  **Commit**: YES
  - Message: `test(catch2): fix FetchContent and add integration tests`
  - Files: `CMakeLists.txt`, `tests/test_pipeline.cpp`, `tests/test_simd.cpp`, `tests/test_integration.cpp`

- [ ] 21. Add SIMD benchmarks with Highway vs scalar comparison

  **What to do**:
  - Create `benchmarks/benchmark_simd.cpp` with comparison benchmarks:
    - Scalar filter vs SIMD filter on int32/float/double arrays
    - Scalar transform vs SIMD transform on int32/float/double arrays
    - Full pipeline: from().where().map().collect() with and without SIMD
  - Use existing `benchmark_pipeline.cpp` as a pattern for the benchmark structure
  - Report throughput in items/second and speedup factor
  - When `DPB_ENABLE_SIMD=OFF`: skip SIMD benchmarks and report scalar only

  **Must NOT do**:
  - Do NOT modify existing `benchmark_pipeline.cpp` — create a new file
  - Do NOT make benchmarks require SIMD — they must work in both modes

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T20)
  - **Parallel Group**: Wave 6
  - **Blocks**: F3
  - **Blocked By**: T17

  **Acceptance Criteria**:

  [ ] `benchmark_simd` compiles and runs with `DPB_ENABLE_SIMD=ON`
  - [ ] SIMD filter shows ≥2x throughput improvement
  - [ ] SIMD transform shows ≥2x throughput improvement
  - [ ] Benchmark works in scalar-only mode

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: SIMD benchmarks show improvement
    Tool: Bash
    Steps:
      1. Build with DPB_ENABLE_SIMD=ON -DDPB_BUILD_BENCHMARKS=ON
      2. Run benchmark_simd
      3. Verify SIMD throughput ≥ 2x scalar throughput
    Expected Result: SIMD shows measurable improvement
    Failure Indicators: SIMD slower, crash during benchmark
    Evidence: .sisyphus/evidence/task-21-simd-benchmark.txt
  ```

  **Commit**: YES
  - Message: `bench(simd): add Highway vs scalar benchmark comparison`
  - Files: `benchmarks/benchmark_simd.cpp`, `CMakeLists.txt`

- [ ] 22. Binary size audit — verify all configs under 500KB

  **What to do**:
  - Build all CMake preset configurations:
    - default (zero deps)
    - with-simd
    - with-fmt
    - with-tracy
    - with-all (all deps enabled)
  - Record binary size for `basic_example.exe` in each configuration
  - Create `.sisyphus/evidence/task-22-binary-sizes.md` documenting all sizes
  - If any configuration exceeds 500KB, identify bloat sources and optimize
  - Expected sizes:
    - default: ~100KB (current baseline)
    - with-simd: ~150KB
    - with-fmt: ~200KB
    - with-tracy: ~250KB
    - with-all: ~350-450KB

  **Must NOT do**:
  - Do NOT strip symbols or use `-s` linker flag just to meet size
  - Do NOT remove functionality to reduce size

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO (needs all implementation tasks done)
  - **Parallel Group**: Wave 6
  - **Blocks**: F3
  - **Blocked By**: T17

  **Acceptance Criteria**:

  [ ] All configurations build successfully
  [ ] All configurations have `basic_example.exe` under 500KB
  [ ] Size audit documented in `.sisyphus/evidence/task-22-binary-sizes.md`

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Binary size audit
    Tool: Bash
    Steps:
      1. Build default config, record size of basic_example.exe
      2. Build with-simd config, record size
      3. Build with-fmt config, record size
      4. Build with-tracy config, record size
      5. Build with-all config, record size
      6. Verify all sizes < 500KB
    Expected Result: All configurations under 500KB
    Failure Indicators: Any configuration exceeds 500KB
    Evidence: .sisyphus/evidence/task-22-binary-sizes.md
  ```

  **Commit**: YES
  - Message: `audit(binary): verify all configs under 500KB`
  - Files: `.sisyphus/evidence/task-22-binary-sizes.md`

- [ ] 23. Update README.md with new features and CMake flags

  **What to do**:
  - Add section on optional dependencies (Highway, {fmt}, Tracy)
  - Document CMake flags: `DPB_ENABLE_SIMD`, `DPB_ENABLE_FMT`, `DPB_ENABLE_TRACY`
  - Document CMakePresets configurations
  - Add SIMD performance benchmark results
  - Add Tracy profiling instructions
  - Update API reference to include `PipelineErase`
  - Update build instructions for vcpkg/FetchContent

  **Must NOT do**:
  - Do NOT remove existing README content — only ADD new sections
  - Do NOT add marketing language or unnecessary emoji

  **Recommended Agent Profile**:
  - **Category**: `writing`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T20-T22)
  - **Parallel Group**: Wave 6
  - **Blocks**: None
  - **Blocked By**: T17

  **Acceptance Criteria**:

  [ ] README documents all three optional dependencies
  [ ] CMake flags and presets are clearly documented
  [ ] SIMD performance claims are backed by benchmark numbers
  [ ] `PipelineErase` is documented with performance caveats

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: README accuracy
    Tool: Bash
    Steps:
      1. Verify all CMake flags mentioned in README exist in CMakeLists.txt
      2. Verify all preset names mentioned in README exist in CMakePresets.json
      3. Verify benchmark numbers match actual benchmark output
    Expected Result: README is accurate and complete
    Failure Indicators: Missing flags, wrong preset names, outdated numbers
    Evidence: .sisyphus/evidence/task-23-readme-accuracy.txt
  ```

  **Commit**: YES
  - Message: `docs(readme): update with new features and CMake flags`
  - Files: `README.md`

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

> 4 review agents run in PARALLEL. ALL must APPROVE.

- [ ] F1. **Plan Compliance Audit** — `oracle`
  Read the plan end-to-end. For each "Must Have": verify implementation exists. For each "Must NOT Have": search codebase for forbidden patterns. Check evidence files. Compare deliverables against plan.
  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [ ] F2. **Code Quality Review** — `unspecified-high`
  Build all CMake configurations (default, +SIMD, +FMT, +TRACY, all enabled). Check for: compiler warnings, `as any`, empty catches, unused includes, AI slop. Verify all `#ifdef` guards are correct.
  Output: `Build [N/N configs pass] | Warnings [N] | AI Slop [CLEAN/N issues] | VERDICT`

- [ ] F3. **Real Manual QA** — `unspecified-high` (+ `playwright` if UI)
  Build default config. Run all examples. Build SIMD config. Run SIMD benchmarks. Build with all flags. Compare binary sizes. Verify Tracy zones.
  Output: `Examples [N/N pass] | SIMD [N/N pass] | Binary Sizes [all < 500KB] | Tracy [working/not working] | VERDICT`

- [ ] F4. **Scope Fidelity Check** — `deep`
  For each task: verify everything in spec was built (no missing), nothing beyond spec was built (no creep). Check fused lambda pattern is untouched. Verify no required deps (all optional).
  Output: `Tasks [N/N compliant] | Fused Lambda [INTACT/MODIFIED] | Deps [ALL OPTIONAL/VIOLATION] | VERDICT`

---

## Commit Strategy

- **Wave 1**: `build(cmake): add Highway, fmt, Tracy as optional deps via FetchContent`
- **T5**: `feat(simd): add dpb/simd.hpp with Highway SIMD primitives`
- **T6**: `feat(format): replace iostream with fmt for stats/profiler output`
- **T7**: `feat(profiling): add conditional Tracy zone macros`
- **T8**: `feat(memory): add std::pmr::vector with monotonic buffer for collect`
- **T9**: `refactor(pipeline): template-migrate filter/transform operations`
- **T10**: `refactor(pipeline): template-migrate stream control operations`
- **T11**: `refactor(pipeline): template-migrate terminal reductions`
- **T12**: `refactor(pipeline): template-migrate collect/flat_map/scan`
- **T13**: `feat(simd): SIMD-accelerated filter for numeric pipelines`
- **T14**: `feat(simd): SIMD-accelerated transform for numeric pipelines`
- **T15**: `feat(profiling): Tracy integration in hot paths`
- **T16**: `feat(format): std::print modernization in stats`
- **T17**: `refactor(pipeline): add type-erased PipelineErase wrapper for backward compat`
- **T18**: `docs(examples): demonstrate SIMD and template features`
- **T19**: `build(cmake): add CMakePresets.json`
- **T20**: `test(catch2): fix FetchContent and add integration tests`
- **T21**: `bench(simd): add Highway vs scalar benchmark comparison`
- **T22**: `audit(binary): verify all configs under 500KB`
- **T23**: `docs(readme): update with new features and CMake flags`

---

## Success Criteria

### Verification Commands
```bash
# Default build (zero deps)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release
./build/Release/basic_example  # Must produce correct output

# With SIMD
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDPB_ENABLE_SIMD=ON && cmake --build build --config Release
./build/Release/basic_example  # Must produce correct output

# With all deps
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDPB_ENABLE_SIMD=ON -DDPB_ENABLE_FMT=ON -DDPB_ENABLE_TRACY=ON && cmake --build build --config Release
./build/Release/basic_example  # Must produce correct output

# Binary size check
ls -la build/Release/basic_example.exe  # Must be < 500KB

# SIMD benchmark comparison
./build/Release/pipeline_benchmarks  # Must show ≥2x improvement on numeric pipelines
```

### Final Checklist
- [ ] All "Must Have" present
- [ ] All "Must NOT Have" absent
- [ ] Fused lambda composition pattern is untouched
- [ ] All CMake flag combinations build successfully
- [ ] Binary size under 500KB with all deps
- [ ] SIMD shows measurable performance improvement
- [ ] Tracy zones work when enabled, zero overhead when disabled
- [ ] Existing `dpb::from().where().map().collect()` API still works