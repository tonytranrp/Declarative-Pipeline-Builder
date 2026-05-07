# Learnings and Conventions

## Project Structure
- Header-only C++23 library
- Main headers in `include/` and `include/pipeline/`
- Examples in `examples/`, tests in `tests/`, benchmarks in `benchmarks/`
- CMake 3.20+, C++23 required

## Key Files
- `include/pipeline/pipeline.hpp` — Main Pipeline class (the KILLER FEATURE is the fused lambda pattern)
- `include/pipeline_stats.hpp` — Stats output
- `include/profiler.hpp` — Simple profiler
- `include/thread_pool.hpp` — Custom work-stealing thread pool (DO NOT TOUCH)
- `CMakeLists.txt` — Main build config

## Critical Rules
- DO NOT break the fused lambda composition pattern
- DO NOT make any dependency required — all must be opt-in via CMake flags
- DO NOT remove the simple profiler.hpp fallback
- DO NOT touch thread_pool.hpp
- DO NOT touch pipeline_stats.hpp atomics
- DO NOT change the public API surface
- Binary size target: < 500KB for any example with all deps enabled

## CMake Patterns
- Use `FetchContent` for optional dependencies
- Use `option(DPB_ENABLE_...)` flags
- Use generator expressions `$<$<CONFIG:Release>:...>` for optimization flags
- Include `cmake/Dependencies.cmake` for FetchContent declarations

## Build Verification
- Always test: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release`
- Run example: `./build/Release/basic_example`
- Test all flag combinations separately
