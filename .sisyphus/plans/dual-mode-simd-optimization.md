# Dual-Mode SIMD Optimization & Feature Expansion

## TL;DR

> **Quick Summary**: Integrate Highway SIMD into the Pipeline class, add runtime CPU dispatch, new DSL operations, custom allocators, pipeline composition, and error handling — all while keeping the stripped library under 850KB and targeting 5B items/sec throughput.
> 
> **Deliverables**:
> - SIMD-accelerated `.where()`/`.map()` auto-detected for numeric types in Pipeline class
> - Runtime CPU feature detection with Highway dynamic dispatch
> - New operations: `zip`, `join`, `sort_by`, `reduce_by_key`
> - Template-parameterized allocator support with `std::allocator` default
> - Zero-overhead pipeline composition via `compose()` / `then()`
> - Structured `Result<T, E>` error propagation replacing bool pass/fail
> - Dual-mode CMake presets: `scalar-test` and `simd-test`
> - Portable attribute macros (`DPB_HOT`, `DPB_INLINE`) replacing `[[gnu::hot]]`
> - Fixed `collect_parallel` O(n²) merge
> 
> **Estimated Effort**: Large (14 tasks, 5 waves)
> **Parallel Execution**: YES — 3 waves of 4-5 tasks each
> **Critical Path**: SIMD types → SIMD collect integration → runtime dispatch → new operations → composition

---

## Context

### Original Request
User wants maximum speed (~5B items/sec, up from ~1.07e9), SIMD integration into the DSL Pipeline class, runtime dispatch, new features, dual-mode testing, and strict 850KB stripped library size.

### Interview Summary
**Key Discussions**:
- **Size budget**: Stripped `.a`/`.lib` must stay under 850KB with SIMD enabled
- **SIMD integration depth**: Pipeline class must auto-detect numeric types and use Highway SIMD in its collect hot-loop — not just standalone helpers
- **Dispatch**: Runtime CPU detection at library init (Highway dynamic dispatch), not compile-time only
- **New features**: zip, join, sort_by, reduce_by_key, custom allocators, composition API, error handling
- **Testing**: Compile-time presets — same tests run under both `scalar-test` (no SIMD) and `simd-test` (Highway) presets

**Research Findings**:
- Highway dynamic dispatch uses per-ISA function pointer table, ~5KB overhead
- Extern templates + explicit instantiation reduce template bloat; {fmt} went 75KB → 14KB
- `collect_parallel` merge loop has O(n²) insert behavior — critical fix needed
- `[[gnu::hot]]` etc. are GCC/Clang-only; need portable `#ifdef` wrappers
- `concepts.hpp` defines unused `PipelineError`/`Result<T>` — reconcile before adding error handling
- Current SIMD tests are only standalone helpers; need Pipeline-level SIMD verification

### Metis Review
**Identified Gaps** (addressed):
- SIMD execution model: how does SIMD mesh with fused-lambda scalar chain? Resolved via per-stage SIMD detection — Pipeline checks at terminal `collect()` whether the entire chain is SIMD-eligible (numeric types, simple predicates), and if so uses a SIMD-optimized inner loop; otherwise falls back to scalar fused-lambda. This keeps the fluent API identical.
- `collect_parallel` merge bug: now Task 1, fixed first before any throughput work.
- Unused `PipelineError`/`Result<T>`: reconciled by evolving them into the new error handling system (Task 12).
- `parallelism_` field vs thread pool: investigation added to Task 1.
- MSVC attributes: portable `DPB_HOT`/`DPB_INLINE` macros added as Task 3.
- 850KB ambiguity: resolved — includes Highway code when SIMD enabled; `-Os -flto=thin -Wl,--gc-sections` applied in SIMD preset.

---

## Work Objectives

### Core Objective
Integrate Highway SIMD into the Pipeline class so `.where().map().collect()` auto-uses SIMD for numeric types, add runtime dispatch, expand the DSL, keep everything under 850KB, and achieve 5B items/sec.

### Concrete Deliverables
- `include/pipeline/simd_collect.hpp` — SIMD-accelerated collect path
- `include/dpb/cpu_detect.hpp` — runtime CPU detection
- `include/dpb/portable_attrs.hpp` — portable attribute macros
- `include/pipeline/operations/zip.hpp`, `join.hpp`, `sort_by.hpp`, `reduce_by_key.hpp`
- `include/dpb/result.hpp` — structured error type replacing bool
- Updated `include/pipeline/pipeline.hpp` — allocator template param, composition API
- CMake presets: `scalar-test`, `simd-test`
- Updated tests: all existing tests + new SIMD pipeline tests + new operation tests

### Definition of Done
- [ ] `cmake --preset simd-test && cmake --build build/simd-test && ctest --test-dir build/simd-test` passes all
- [ ] `cmake --preset scalar-test && cmake --build build/scalar-test && ctest --test-dir build/scalar-test` passes all
- [ ] Stripped `libdeclarative_pipeline.a` under 850KB (verified via `size` or `llvm-size`)
- [ ] Filter-map throughput ≥ 5B items/sec on manual 10M-int driver with SIMD

### Must Have
- SIMD auto-detection in Pipeline collect path
- Runtime CPU dispatch via Highway
- Dual-mode test presets with all tests passing
- 850KB stripped lib size
- Backward compatibility: existing scalar-only `dpb::from(data).where(...).map(...).collect(data)` still works unchanged

### Must NOT Have (Guardrails)
- Do NOT remove the scalar fused-lambda path — SIMD is an acceleration, not a replacement (Metis: scalar fallback must remain)
- Do NOT make Highway a required dependency — SIMD must stay opt-in via CMake flag (Metis: 850KB is ambiguous without this)
- Do NOT break the `from(data).op1().op2().collect(data)` fluent API
- Do NOT add new public dependencies beyond Highway
- Do NOT change the `ResultWithStats` public API shape (Metis: stability concern)
- Do NOT make the allocator template parameter change existing code — use default `= std::allocator<T>` (Metis: API break risk)

---

## Verification Strategy

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed.

### Test Decision
- **Infrastructure exists**: YES (Catch2 + Google Benchmark)
- **Automated tests**: TDD — RED failing test first, then GREEN implementation
- **Framework**: Catch2 for unit tests, Google Benchmark for throughput
- **TDD flow**: Write test → see it fail → implement → see it pass

### QA Policy
Every task includes agent-executed QA scenarios.
- **Build**: `cmake --preset {name} && cmake --build build/{name}`
- **Tests**: `ctest --test-dir build/{name} --output-on-failure`
- **Throughput**: Manual 10M-int driver, compiled with SIMD preset, verified ≥ target
- **Size**: `llvm-size --format=sysv build/simd-test/libdeclarative_pipeline.a` or equivalent
- **Evidence**: `.sisyphus/evidence/task-{N}-{slug}.txt`

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Start Immediately — foundation + fixes):
├── Task 1: Verify collect_parallel merge + investigate thread pool [medium]
├── Task 2: Portable attribute macros (DPB_HOT, DPB_INLINE) [quick]
├── Task 3: Reconcile PipelineError/Result<T> in concepts.hpp [quick]
├── Task 4: SIMD type traits + eligibility detection [medium]
└── Task 5: CMake scalar-test + simd-test presets [quick]

Wave 2 (After Wave 1 — SIMD integration, MAX PARALLEL):
├── Task 6: SIMD-accelerated collect path (depends: 4) [deep]
├── Task 7: Runtime CPU detection + Highway dispatch (depends: 4) [deep]
├── Task 8: Custom allocator template param (depends: 3) [medium]
├── Task 9: New operations: zip + join (depends: 3) [medium]
└── Task 10: New operations: sort_by + reduce_by_key (depends: 3) [medium]

Wave 3 (After Wave 2 — composition + polish, MAX PARALLEL):
├── Task 11: Pipeline composition API (depends: 8) [medium]
├── Task 12: Structured error handling via Result<T,E> (depends: 3) [medium]
├── Task 13: Dual-mode test expansion — SIMD pipeline tests (depends: 5,6,7) [deep]
└── Task 14: Size budget enforcement + strip verification (depends: 5,6) [quick]

Wave FINAL (After ALL):
├── Task F1: Plan compliance audit (oracle)
├── Task F2: Code quality review (unspecified-high)
├── Task F3: Real manual QA (unspecified-high)
└── Task F4: Scope fidelity check (deep)
```

Critical Path: Task 3 → Task 4 → Task 6 → Task 13 → F1-F4

---

## TODOs

- [x] 1. Verify `collect_parallel` merge efficiency + investigate thread pool

  **What to do**:
  - The merge at lines ~758-767 already uses `merged.reserve(total_out)` + `insert` with `std::make_move_iterator` (O(n)). Verify this is correct and efficient; if further optimization is possible (e.g., `std::uninitialized_move`), apply it.
  - Verify `parallelism_` field is actually respected by `ThreadPool::enqueue` (Metis raised potential latent bug) — round-robin at `thread_pool.hpp:195` uses `workers_.size()` not `parallelism_`
  - If thread pool ignores `parallelism_`, fix `enqueue` round-robin to respect the field or document the override
  - Update `BM_ParallelFilterTransform` and `BM_ParallelUnordered` benchmarks to validate merge performance

  **Must NOT do**:
  - Do NOT change the thread pool's Chase-Lev algorithm — only fix the parallelism field issue if confirmed
  - Do NOT remove the contiguous_range fast path

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: Multi-file bug investigation across pipeline.hpp and thread_pool.hpp with performance verification
  - **Skills**: []
  - **Skills Evaluated but Omitted**: N/A — pure C++ fix

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 1, with Tasks 2-5)
  - **Blocks**: Nothing critical — fixes must land before any parallel throughput benchmarks
  - **Blocked By**: None (can start immediately)

  **References**:
  - `include/pipeline/pipeline.hpp:748-758` — Current O(n²) merge loop to fix
  - `include/thread_pool.hpp:188-207` — `enqueue` round-robin to investigate
  - `include/thread_pool.hpp:150-151` — Thread pool constructor reads `hardware_concurrency`
  - `benchmarks/benchmark_pipeline.cpp:98-131` — Parallel benchmarks to update

  **Acceptance Criteria**:
  - [ ] `collect_parallel` merge uses single `reserve` + `std::move` — no repeated insert calls
  - [ ] Thread pool parallelism field confirmed working or documented as overridden
  - [ ] `BM_ParallelFilterTransform` and `BM_ParallelUnordered` benchmarks compile and run

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Parallel merge produces correct ordered results
    Tool: Bash (compile + run)
    Preconditions: CMake simd-test preset built
    Steps:
      1. Compile pipeline_tests with simd-test preset
      2. Run: ./pipeline_tests "[parallel]"
      3. Assert: "Parallel Execution Test" section shows "Results match: 1"
    Expected Result: All parallel tests pass, ordered merge correct
    Evidence: .sisyphus/evidence/task-1-parallel-merge-correct.txt

  Scenario: Parallel unordered merge produces correct element set
    Tool: Bash (compile + run)
    Steps:
      1. Run: ./pipeline_tests "[parallel]"
      2. Assert: "Parallel Unordered Test" section shows "Sorted results match: 1"
    Expected Result: Unordered elements match sequential set
    Evidence: .sisyphus/evidence/task-1-parallel-unordered-correct.txt
  ```

  **Commit**: YES
  - Message: `fix(pipeline): O(n^2) merge in collect_parallel, verify thread pool parallelism`
  - Files: `include/pipeline/pipeline.hpp`, `include/thread_pool.hpp`, `benchmarks/benchmark_pipeline.cpp`

- [x] 2. Portable attribute macros (DPB_HOT, DPB_INLINE, DPB_LIKELY)

  **What to do**:
  - Create `include/dpb/portable_attrs.hpp` with `#ifdef`-based macros:
    - `DPB_HOT` → `[[gnu::hot]]` on GCC/Clang, `[[msvc::noinline]]` on MSVC? No — on MSVC just empty
    - `DPB_INLINE` → `[[gnu::always_inline]] inline` on GCC/Clang, `__forceinline` on MSVC
    - `DPB_FLATTEN` → `[[gnu::flatten]]` on GCC/Clang, empty on MSVC
    - `DPB_LIKELY(x)` → `__builtin_expect(!!(x), 1)` on GCC/Clang, empty on MSVC
    - `DPB_UNLIKELY(x)` → `__builtin_expect(!!(x), 0)` on GCC/Clang, empty on MSVC
  - Replace all raw `[[gnu::hot]]`, `[[gnu::flatten]]`, `[[gnu::always_inline]]`, `[[likely]]`, `[[unlikely]]` in pipeline.hpp with portable macros
  - Replace `[[likely]]` with `DPB_LIKELY(...)` and `[[unlikely]]` with `DPB_UNLIKELY(...)`

  **Must NOT do**:
  - Do NOT change behavior — macros must expand to exactly the same attributes on GCC/Clang
  - Do NOT add compiler-specific optimizations beyond what currently exists

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Single new header + search-replace in pipeline.hpp, no logic changes
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 1, with Tasks 1,3,4,5)
  - **Blocks**: Nothing directly — other tasks can adopt macros after merge
  - **Blocked By**: None

  **References**:
  - `include/pipeline/pipeline.hpp:167` — `[[gnu::hot]]` usage
  - `include/pipeline/pipeline.hpp:619` — `[[gnu::hot]] [[gnu::flatten]] [[gnu::always_inline]]` on `collect_sequential`
  - `include/pipeline/pipeline.hpp:186` — `[[unlikely]]` in `transform`
  - `include/pipeline/pipeline.hpp:644` — `[[likely]]` in collect loop

  **Acceptance Criteria**:
  - [ ] `include/dpb/portable_attrs.hpp` exists with all macros
  - [ ] `pipeline.hpp` uses `DPB_HOT`/`DPB_INLINE`/`DPB_FLATTEN`/`DPB_LIKELY`/`DPB_UNLIKELY` — no raw GNU attributes
  - [ ] All existing tests pass with no behavioral change

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Portable macros compile on GCC/Clang and produce identical behavior
    Tool: Bash (compile + test)
    Steps:
      1. cmake --preset scalar-test && cmake --build build/scalar-test
      2. Run: ./pipeline_tests
      3. Assert: "All tests passed" — 196+ assertions, zero failures
    Expected Result: All tests pass, behavior identical
    Evidence: .sisyphus/evidence/task-2-portable-attrs.txt
  ```

  **Commit**: YES
  - Message: `refactor(attrs): portable DPB_HOT/DPB_INLINE/DPB_LIKELY macros, replace GNU attributes`
  - Files: `include/dpb/portable_attrs.hpp` (new), `include/pipeline/pipeline.hpp`

- [x] 3. Reconcile `PipelineError`/`Result<T>` in concepts.hpp

  **What to do**:
  - Read current `include/pipeline/concepts.hpp` — it defines `PipelineError` enum and `Result<T>` template but they are never used in pipeline.hpp
  - Either: (a) keep them as-is but add documentation that they'll be used by Task 12 (error handling), or (b) move them to a new `include/dpb/result.hpp` that both concepts and the future error system can include
  - Choose (b): create `include/dpb/result.hpp` with `PipelineError` enum + `Result<T>` (using `std::expected`-like pattern for C++23)
  - Update `concepts.hpp` to `#include "dpb/result.hpp"` and remove the duplicate definitions
  - No behavioral change to existing code — this is pure reorganization

  **Must NOT do**:
  - Do NOT change the `PipelineError` enum values or `Result<T>` semantics
  - Do NOT wire error handling into pipeline.hpp yet (that's Task 12)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Pure code move — extract definitions to new header, update includes
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (Wave 1, with Tasks 1,2,4,5)
  - **Blocks**: Task 12 (error handling relies on this being extracted)
  - **Blocked By**: None

  **References**:
  - `include/pipeline/concepts.hpp` — Current PipelineError + Result<T> definitions
  - `include/dpb/result.hpp` (new) — Target extraction location

  **Acceptance Criteria**:
  - [ ] `include/dpb/result.hpp` exists with `PipelineError` + `Result<T>`
  - [ ] `concepts.hpp` includes `dpb/result.hpp`, no longer defines them inline
  - [ ] All existing tests pass (no behavioral change)

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Extracted result types compile and tests pass unchanged
    Tool: Bash (compile + test)
    Steps:
      1. cmake --preset scalar-test && cmake --build build/scalar-test
      2. Run: ./pipeline_tests
      3. Assert: "All tests passed" — zero failures
    Expected Result: All tests pass, no behavioral change
    Evidence: .sisyphus/evidence/task-3-result-extraction.txt
  ```

  **Commit**: YES
  - Message: `refactor(result): extract PipelineError/Result<T> to dpb/result.hpp`
  - Files: `include/dpb/result.hpp` (new), `include/pipeline/concepts.hpp`

- [x] 4. SIMD type traits + eligibility detection

  **What to do**:
  - Create `include/dpb/simd_traits.hpp`: type traits that determine when a Pipeline chain is SIMD-eligible
  - Define `simd_eligible_op<OpFunc, In, Out>` trait: true when OpFunc is a simple transform and In/Out are `simd_numeric`
  - Add `simd_eligible_predicate<OpFunc, T>` trait: true when OpFunc is a simple predicate and T is `simd_numeric`
  - Add `simd_eligible_chain<Pipeline>` meta-function: recursively checks if chain is SIMD-eligible, evaluated at COMPILE TIME
  - Write static_assert / STATIC_REQUIRE tests

  **Must NOT do**: Do NOT add runtime type inspection — traits must be pure compile-time

  **Recommended Agent Profile**: `deep`
  **Parallelization**: Wave 1 | Blocks: Task 6,7 | Blocked By: None

  **Acceptance Criteria**:
  - [ ] `include/dpb/simd_traits.hpp` exists with header guards
  - [ ] `simd_eligible_chain<Pipeline<int,int,...>>` true for even+squre chain, false for string chain
  - [ ] STATIC_REQUIRE tests pass

  **QA Scenarios**:
  - Compile + test SIMD traits: `cmake --preset simd-test && make && ./pipeline_tests "[simd_traits]"` — traits tests pass
  - Evidence: `.sisyphus/evidence/task-4-simd-traits.txt`

  **Commit**: `feat(simd): compile-time SIMD eligibility traits` | Files: `include/dpb/simd_traits.hpp` (new), `tests/test_simd_traits.cpp`, `CMakeLists.txt`

- [x] 5. CMake scalar-test + simd-test presets

  **What to do**:
  - Add `scalar-test` preset (default + tests ON, SIMD OFF) and `simd-test` preset (with-simd + tests ON + benchmarks ON)
  - simd-test applies `-Os -flto=thin -Wl,--gc-sections` for size budget
  - Wire DPB_ENABLE_SIMD to test targets so `[simd]` tests auto-compile in simd-test

  **Must NOT do**: Do NOT remove existing presets

  **Recommended Agent Profile**: `quick`
  **Parallelization**: Wave 1 | Blocks: Task 13 | Blocked By: None

  **Acceptance Criteria**:
  - [ ] `cmake --list-presets` shows both new presets
  - [ ] Both presets configure and build `pipeline_tests`

  **QA Scenarios**:
  - Both presets build: `cmake --preset scalar-test && cmake --build build/scalar-test --target pipeline_tests` → exit 0
  - SIMD gating: scalar test no SIMD tests, simd test has SIMD tests
  - Evidence: `.sisyphus/evidence/task-5-dual-presets.txt`

  **Commit**: `build(cmake): add scalar-test and simd-test presets` | Files: `CMakePresets.json`, `CMakeLists.txt`

- [x] 6. SIMD-accelerated Pipeline collect path

  **What to do**:
  - Create `include/pipeline/simd_collect.hpp`: called from `collect_sequential` when chain is SIMD-eligible
  - Uses `dpb::simd::simd_filter` + `dpb::simd::simd_transform` in batch over chunked input
  - Falls back to scalar loop if non-eligible or no Highway
  - Identical results to scalar path required

  **Must NOT do**: Do NOT change scalar fallback path, do NOT require Highway to compile

  **Recommended Agent Profile**: `deep`
  **Parallelization**: Wave 2 (with Tasks 7-10) | Blocks: Task 13 | Blocked By: Task 4

  **Acceptance Criteria**:
  - [ ] `include/pipeline/simd_collect.hpp` exists
  - [ ] SIMD results == scalar results for int32/64, float, double
  - [ ] Filter-map throughput ≥ 5B items/sec with simd-test preset

  **QA Scenarios**:
  - Correctness: SIMD vs scalar identical for N=1K/10K/100K
  - Throughput: 10M-int driver median ≥ 5e9 items/sec
  - Fallback: non-numeric pipeline still works unchanged
  - Evidence: `.sisyphus/evidence/task-6-simd-{correctness,throughput,fallback}.txt`

  **Commit**: `feat(simd): SIMD-accelerated collect for numeric pipeline chains` | Files: `include/pipeline/simd_collect.hpp` (new), `include/pipeline/pipeline.hpp`, `tests/test_simd_pipeline.cpp`, `CMakeLists.txt`

- [x] 7. Runtime CPU detection + Highway dispatch

  **What to do**:
  - Create `include/dpb/cpu_detect.hpp`: `dpb::detect_cpu()` using `__builtin_cpu_supports` (GCC/Clang) / `IsProcessorFeaturePresent` (MSVC)
  - Expose `dpb::cpu_features` struct (has_avx2, has_avx512f, has_sse42)
  - Initialize once at library startup (static local, thread-safe)
  - Wire into simd_collect.hpp: check CPU features before using Highway ISA
  - Add HWY_DYNAMIC_DISPATCH for Highway to select best ISA at runtime

  **Must NOT do**: Do NOT require AVX2 at compile time — must run on non-AVX2 machines

  **Recommended Agent Profile**: `deep`
  **Parallelization**: Wave 2 (with Tasks 6,8,9,10) | Blocks: Task 13 | Blocked By: Task 4

  **Acceptance Criteria**:
  - [ ] `include/dpb/cpu_detect.hpp` exists
  - [ ] `detect_cpu()` cached after first call
  - [ ] SIMD path checks CPU features before ISA use
  - [ ] Library runs on non-AVX2 machines (scalar fallback)

  **QA Scenarios**:
  - CPU detect caches: same pointer on repeat calls
  - Graceful fallback: pipeline works on non-AVX2 or mocked
  - Evidence: `.sisyphus/evidence/task-7-cpu-detect.txt`

  **Commit**: `feat(cpu): runtime CPU detection with Highway dynamic dispatch` | Files: `include/dpb/cpu_detect.hpp` (new), `include/pipeline/simd_collect.hpp`, `tests/test_cpu_detect.cpp`, `CMakeLists.txt`

- [x] 8. Custom allocator template parameter

  **What to do**:
  - Add `template<typename Alloc = std::allocator<Out>>` to Pipeline class with default
  - `collect_buffer` accepts allocator and uses `std::vector<T, Alloc>` internally
  - `ResultWithStats` stores `std::vector<T, Alloc>` when custom allocator specified
  - Existing `from(data).collect(data)` unchanged (default allocator)

  **Must NOT do**: Do NOT break existing code — default `std::allocator` must be transparent

  **Recommended Agent Profile**: `medium`
  **Parallelization**: Wave 2 (with Tasks 6,7,9,10) | Blocks: Task 11 | Blocked By: Task 3

  **Acceptance Criteria**:
  - [ ] Pipeline accepts custom allocator: `Pipeline<int,int,DefaultOp,MyAlloc>`
  - [ ] Default allocator works unchanged
  - [ ] Custom allocator test: pmr vector emerges correctly

  **QA Scenarios**:
  - Default: `from(data).collect(data)` compiles and works unchanged
  - Custom: `Pipeline<int,int,DefaultOp,MyAlloc>::from(data).collect(data)` uses custom alloc
  - Evidence: `.sisyphus/evidence/task-8-allocator.txt`

  **Commit**: `feat(alloc): custom allocator template parameter on Pipeline` | Files: `include/pipeline/pipeline.hpp`, `include/dpb/memory.hpp`

- [ ] 9. New operations: zip + join

  **What to do**:
  - `zip(input, other_range)`: pairs elements from two ranges, terminal operation. Type: `zip(input, other) → vector<pair<Out, OtherT>>`
  - `join(input, other, keyFn)` (inner join): matches by key, terminal. Type: `join(input, other, keyFn) → vector<pair<Out, OtherT>>`
  - Follow existing fused-lambda pattern for consistency
  - Both are terminal (collecting) operations — they produce output vectors

  **Must NOT do**: Do NOT add lazy/streaming variants — keep consistent with existing terminal model

  **Recommended Agent Profile**: `medium`
  **Parallelization**: Wave 2 (with Tasks 6,7,8,10) | Blocks: None | Blocked By: Task 3

  **Acceptance Criteria**:
  - [ ] `zip` test: pairs [1,2,3] with [a,b,c] → [(1,a),(2,b),(3,c)]
  - [ ] `join` test: inner join on matching key produces correct pairs
  - [ ] Both compile and pass under scalar-test and simd-test presets

  **QA Scenarios**:
  - zip correctness: `REQUIRE(result[0].first == 1); REQUIRE(result[0].second == a);`
  - join correctness: inner join on int key
  - Evidence: `.sisyphus/evidence/task-9-zip-join.txt`

  **Commit**: `feat(ops): zip and join terminal operations` | Files: `include/pipeline/operations/zip.hpp`, `include/pipeline/operations/join.hpp`, `tests/test_zip_join.cpp`, `CMakeLists.txt`

- [ ] 10. New operations: sort_by + reduce_by_key

  **What to do**:
  - `sort_by(input, keyFn)`: sorts collected output by key, terminal. Type: `sort_by(input, keyFn) → vector<Out>` (sorted in place)
  - `reduce_by_key(input, keyFn, reduceFn)`: groups by key, reduces each group, terminal. Type: `reduce_by_key(input, keyFn, reduceFn) → vector<pair<Key, ReducedT>>`
  - Follow existing terminal operation patterns (e.g., `group_by`, `fold`)

  **Must NOT do**: Do NOT add in-place mutation variants — keep functional

  **Recommended Agent Profile**: `medium`
  **Parallelization**: Wave 2 (with Tasks 6,7,8,9) | Blocks: None | Blocked By: Task 3

  **Acceptance Criteria**:
  - [ ] `sort_by` test: sorts by key function
  - [ ] `reduce_by_key` test: groups and reduces correctly
  - [ ] Both pass under both presets

  **QA Scenarios**:
  - sort_by: `from(data).sort_by(data, [](int x){ return -x; })` → descending order
  - reduce_by_key: `from(data).reduce_by_key(data, id, sum)` → per-key sums
  - Evidence: `.sisyphus/evidence/task-10-sortby-reduce.txt`

  **Commit**: `feat(ops): sort_by and reduce_by_key terminal operations` | Files: `include/pipeline/operations/sort_by.hpp`, `reduce_by_key.hpp`, `tests/test_sort_by_reduce.cpp`, `CMakeLists.txt`

- [ ] 11. Pipeline composition API

  **What to do**:
  - `compose(p1, p2)`: chains two pipelines together without materializing intermediate results
  - `then(pipeline, nextOp)`: appends a single operation to a pipeline (returns new Pipeline)
  - Both are zero-overhead — they fuse operations at the type level like current `.where().map()` chain
  - Store pre-built pipelines in containers using `PipelineErase` (existing), but `compose`/`then` keep full type information

  **Must NOT do**: Do NOT introduce std::function or virtual dispatch in the zero-overhead path

  **Recommended Agent Profile**: `medium`
  **Parallelization**: Wave 3 (with Tasks 12,13,14) | Blocks: None | Blocked By: Task 8

  **Acceptance Criteria**:
  - [ ] `auto p3 = compose(p1, p2); auto result = p3.collect(data);` works
  - [ ] `auto p2 = p1.then(map(square));` works
  - [ ] Composition produces same results as direct chaining

  **QA Scenarios**:
  - compose: `compose(even_pipeline, square_pipeline).collect(data) == data | where(even) | map(square)`
  - then: `base_pipeline.then(map(square)).collect(data) == base_pipeline.map(square).collect(data)`
  - Evidence: `.sisyphus/evidence/task-11-composition.txt`

  **Commit**: `feat(compose): zero-overhead pipeline composition API` | Files: `include/pipeline/pipeline.hpp`, `tests/test_composition.cpp`, `CMakeLists.txt`

- [ ] 12. Structured error handling via Result<T,E>

  **What to do**:
  - Evolve `dpb::Result<T>` (from Task 3 extraction) into a proper `std::expected`-like type for C++23
  - Replace `bool operation_(const In&, Out&)` return with `Result<void>` pattern — if operation fails, pipeline can propagate error
  - Add `try_collect()` variant that returns `Result<ResultWithStats<T>>` instead of throwing/ignoring errors
  - Keep backward compatibility: existing `bool` return pattern still works; new code can opt into `Result<T>` returns
  - Wire into `collect_sequential`: if operation returns failure, increment error counter and optionally abort

  **Must NOT do**: Do NOT break existing `bool op(const In&, Out&)` signature — it must still work

  **Recommended Agent Profile**: `deep`
  **Parallelization**: Wave 3 (with Tasks 11,13,14) | Blocks: None | Blocked By: Task 3

  **Acceptance Criteria**:
  - [ ] `Result<T>` compiles with `.has_value()`, `.value()`, `.error()`
  - [ ] `try_collect()` returns `Result<ResultWithStats<T>>` with error propagation
  - [ ] Existing bool-return operations still work unchanged
  - [ ] Error count correctly incremented when operation fails

  **QA Scenarios**:
  - Success path: `try_collect()` returns value
  - Failure path: operation returns error → `try_collect()` returns error with count
  - Backward compat: existing tests pass unchanged
  - Evidence: `.sisyphus/evidence/task-12-error-handling.txt`

  **Commit**: `feat(error): structured Result<T,E> error propagation in pipelines` | Files: `include/dpb/result.hpp`, `include/pipeline/pipeline.hpp`, `tests/test_error_handling.cpp`, `CMakeLists.txt`

- [ ] 13. Dual-mode test expansion — SIMD pipeline tests

  **What to do**:
  - Expand test coverage: add SIMD-vs-scalar comparison tests for ALL numeric pipeline operations
  - For each pipeline operation (.where, .map, .take, .skip, .fold, .sum, etc.), add a test case tagged `[simd]` that runs under simd-test preset and verifies SIMD result == scalar result
  - Add benchmark comparisons: `BM_SIMD_Pipeline_vs_Scalar` in `benchmark_pipeline.cpp`
  - Ensure `scalar-test` preset excludes `[simd]` tag tests; `simd-test` includes them
  - Add throughput assertion test: `REQUIRE(throughput >= 5e9)` for simd-test preset only

  **Must NOT do**: Do NOT duplicate every scalar test — only add comparison tests for SIMD-eligible operations

  **Recommended Agent Profile**: `deep`
  **Parallelization**: Wave 3 (with Tasks 11,12,14) | Blocks: Final Wave | Blocked By: Task 5,6,7

  **Acceptance Criteria**:
  - [ ] All `[simd]` tagged tests pass under simd-test preset
  - [ ] No `[simd]` tests found under scalar-test preset
  - [ ] Benchmark shows SIMD pipeline throughput improvement vs scalar
  - [ ] At least 10 new SIMD-vs-scalar comparison test cases

  **QA Scenarios**:
  - simd-test: `ctest --test-dir build/simd-test` → all pass including [simd] tests
  - scalar-test: `ctest --test-dir build/scalar-test` → all pass, no [simd] tests
  - Throughput: `BM_SIMD_Pipeline_vs_Scalar` shows SIMD faster
  - Evidence: `.sisyphus/evidence/task-13-dual-mode-tests.txt`

  **Commit**: `test(simd): dual-mode SIMD vs scalar pipeline comparison tests` | Files: `tests/test_simd_pipeline.cpp` (extend), `benchmarks/benchmark_pipeline.cpp`, `CMakeLists.txt`

- [ ] 14. Size budget enforcement + strip verification

  **What to do**:
  - Add CMake target `check-size`: builds the library as static archive, strips it, checks size ≤ 850KB
  - Use `llvm-strip` or `strip` to remove symbols, then `llvm-size` or `size` to measure
  - If over budget: apply `-Os` (Clang) or `/O1` (MSVC) for SIMD preset, or reduce Highway contrib surface
  - Document size budget in README with command to verify
  - Add CI check (or manual verification script) for size budget

  **Must NOT do**: Do NOT cripple performance to meet size budget — exhaust optimization flags first

  **Recommended Agent Profile**: `quick`
  **Parallelization**: Wave 3 (with Tasks 11,12,13) | Blocks: Final Wave | Blocked By: Task 5,6

  **Acceptance Criteria**:
  - [ ] `cmake --build build/simd-test --target check-size` exits 0 if ≤ 850KB
  - [ ] Stripped library size ≤ 850KB on build machine
  - [ ] README documents size budget and verification command

  **QA Scenarios**:
  - Size check: `llvm-size --format=sysv build/simd-test/libdeclarative_pipeline.a | tail -1` → total ≤ 870400 bytes
  - Over budget: size check target reports failure with current size
  - Evidence: `.sisyphus/evidence/task-14-size-budget.txt`

  **Commit**: `build(size): enforce 850KB stripped library size budget` | Files: `CMakeLists.txt`, `cmake/SizeCheck.cmake` (new), `README.md`

---

## Final Verification Wave

- [ ] F1. **Plan Compliance Audit** — `oracle`
- [ ] F2. **Code Quality Review** — `unspecified-high`
- [ ] F3. **Real Manual QA** — `unspecified-high`
- [ ] F4. **Scope Fidelity Check** — `deep`

---

## Commit Strategy

- **Wave 1**: Independent tasks, separate commits
- **Wave 2**: Each task separate commit; Task 6+7 share dependency but differ in files
- **Wave 3**: Each task separate commit
- **Waves FINAL**: Review group commit

---

## Success Criteria

### Verification Commands
```bash
cmake --preset simd-test && cmake --build build/simd-test && ctest --test-dir build/simd-test --output-on-failure
cmake --preset scalar-test && cmake --build build/scalar-test && ctest --test-dir build/scalar-test --output-on-failure
```
