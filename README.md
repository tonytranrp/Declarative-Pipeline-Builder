# Declarative Pipeline Builder

A high-performance, zero-overhead C++ library for building composable data processing pipelines with optional parallelism. Inspired by functional programming and Unix pipes, but designed for maximum performance with compile-time optimizations.

## ✨ Features

- **🚀 Zero-overhead abstractions**: Direct function calls with no virtual dispatch
- **🔄 Fluent API**: Method chaining for readable, declarative pipeline construction
- **📊 Built-in performance monitoring**: Detailed statistics and benchmarking
- **⚡ Optional parallelism**: Multi-threaded execution with ordering guarantees
- **🔒 Thread-safe**: Atomic statistics and safe parallel execution
- **🎯 Type-safe**: Compile-time validation and error checking
- **🔀 Generic Type Transformations**: Transform between arbitrary types
- **📈 High throughput**: 350-470M items/second on modern hardware
- **🔧 Production-ready**: Works with any data type, not just integers

## 🧩 Type-Erased Pipelines

Use `.erase()` to convert a typed pipeline into `PipelineErase<In, Out>`, which can be stored in containers:

```cpp
#include <dpb/pipeline_erase.hpp>

std::vector<dpb::PipelineErase<int, int>> pipelines;
pipelines.push_back(dpb::from(data).where(is_even).map(square).erase());
pipelines.push_back(dpb::from(data).where(is_odd).map(cube).erase());

for (auto& p : pipelines) {
    auto results = std::move(p).collect(data);
}
```

> **Note**: `PipelineErase` uses `std::function` internally, which has a small overhead. Use typed `Pipeline` for maximum performance.

## 🧰 Optional Dependencies

All dependencies are **opt-in** — zero dependencies by default.

| Library | CMake Flag | Description |
|---------|-----------|-------------|
| [Highway](https://github.com/google/highway) | `DPB_ENABLE_SIMD=ON` | SIMD-accelerated filter/transform for numeric types |
| [{fmt}](https://github.com/fmtlib/fmt) | `DPB_ENABLE_FMT=ON` | High-performance formatting for stats/profiler output |
| [Tracy](https://github.com/wolfpld/tracy) | `DPB_ENABLE_TRACY=ON` | Real-time profiling zones in collect paths |

```bash
# Build with SIMD support
cmake --preset with-simd

# Build with all optional deps
cmake --preset all-deps
```

### CMake Presets

| Preset | Description |
|--------|-------------|
| `default` | No optional deps (zero deps) |
| `with-simd` | Highway SIMD enabled |
| `with-fmt` | {fmt} formatting enabled |
| `with-tracy` | Tracy profiling enabled |
| `all-deps` | All optional deps enabled |

## 📊 Performance Benchmarks

**Latest Results (Modern Hardware):**
- **Throughput**: 350-470M items/second for complex pipelines
- **Latency**: 2-4 ns per item (sub-nanosecond performance)
- **Overhead**: Often faster than hand-written loops due to compiler optimizations
- **Parallel scaling**: Effective for large datasets (100K+ elements)

## 🚀 Quick Start

### Basic Usage

```cpp
#include <declarative_pipeline.hpp>
#include <vector>

int main() {
    using namespace dpb;

    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto result = from(data)
        .where([](int x) { return x % 2 == 0; })
        .map([](int x) { return x * x; })
        .collect(data);

    for (int x : result) std::cout << x << " ";
    // Output: 4 16 36 64 100

    return 0;
}
```

### Parallel Execution

```cpp
auto result = dpb::from(data)
    .parallel(8)
    .where([](int x) { return x % 2 == 0; })
    .map([](int x) { return x * x; })
    .collect(data);
```

### With Stats

```cpp
auto result = dpb::from(data)
    .with_stats()
    .where([](int x) { return x % 2 == 0; })
    .map([](int x) { return x * x; })
    .collect(data);

result.print_stats();
```

## 🏗️ Building and Installation

### Requirements
- **C++23** or later
- **CMake 3.20+**
- **Modern C++ compiler** (GCC 13+, Clang 17+, MSVC 2022+)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/yourusername/Declarative-Pipeline-Builder.git
cd Declarative-Pipeline-Builder

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release

# Build all targets
cmake --build . --config Release

# Run tests
ctest --output-on-failure

# Run benchmarks
./Release/pipeline_benchmarks
```

### CMake Integration (for other projects)

Add to your `CMakeLists.txt`:

```cmake
# Add as subdirectory
add_subdirectory(path/to/declarative-pipeline-builder)

# Or find package (if installed)
find_package(DeclarativePipeline REQUIRED)

# Link to your target
target_link_libraries(your_target PRIVATE DeclarativePipeline::declarative_pipeline)
```

### Header-Only Usage

The library can also be used header-only:

```cpp
# Add include directory to your project
include_directories(path/to/declarative-pipeline-builder/include)

# Include in your code
#include <declarative_pipeline.hpp>
```

## 📚 API Reference

### Entry Point

```cpp
auto pipeline = dpb::from(data);  // Infers types automatically
```

### Core Operations

| Method | Description |
|--------|-------------|
| `filter(Fn)` / `where(Fn)` | Keep elements where predicate returns `true` |
| `transform(Fn)` / `map(Fn)` | Apply function to each element, may change type |
| `take(n)` | Keep first `n` elements |
| `skip(n)` | Skip first `n` elements |
| `take_while(Fn)` | Take elements while predicate holds |
| `skip_while(Fn)` | Skip elements while predicate holds |
| `tee(Fn)` / `inspect(Fn)` | Observe elements without modifying (side-effect tap) |
| `flat_map(input, Fn)` | One-to-many transform (terminal operation) |
| `scan(input, init, Fn)` | Running prefix accumulation (terminal operation) |

### Terminal Reductions

| Method | Description |
|--------|-------------|
| `fold(input, init, Fn)` | Generic left fold |
| `sum(input)` | Sum all values |
| `count(input)` | Count elements passing through |
| `min(input)` / `max(input)` | Find min/max (returns `std::optional`) |
| `all_of(input, Fn)` | True if all elements satisfy predicate |
| `any_of(input, Fn)` | True if any element satisfies predicate |
| `none_of(input, Fn)` | True if no element satisfies predicate |

### Collectors

| Method | Description |
|--------|-------------|
| `collect(input)` | Execute and return `ResultWithStats<T>` |
| `to_vector(input)` | Execute and return plain `std::vector<T>` |
| `collect_sorted(input)` | Execute and sort results |
| `collect_distinct(input)` | Execute and deduplicate results |
| `collect_into(input, iter)` | Zero-copy sink via output iterator |

### Pipeline Configuration

| Method | Description |
|--------|-------------|
| `parallel(n, policy)` | Enable multi-threaded execution |
| `with_stats()` | Enable performance statistics |
| `with_profiler()` | Enable per-stage profiling |

### Quick Start (v2 DSL)
```cpp
#include <declarative_pipeline.hpp>

auto result = dpb::from(data)
    .where([](int x) { return x % 2 == 0; })
    .map([](int x) { return x * x; })
    .collect(data);
```

## 🧪 Testing

```bash
# Build and run all tests
cmake --build build --target pipeline_tests
./build/pipeline_tests

# Run with verbose output
./build/pipeline_tests -v

# Run specific test
./build/pipeline_tests -k "parallel"
```

## 📁 Examples

### Basic Example Output
```bash
=== Declarative Pipeline Builder Examples ===

Test 1: Simple transform

Input: 1, 2, 3

Result: 2 4 6

Test 2: Simple filter

Input: 1, 2, 3, 4, 5

Filtered (> 3): 4 5

Test 3: Filter then transform

Filter (> 3) then transform (* 2): 8 10

Test 4: Transform then filter

Transform (* 2) then filter (> 6): 8 10

Test 5: Parallel execution

Sequential time: 0.0354 ms

Parallel time: 5.9453 ms

Speedup: 0.00595428x

Results match: 1
```

### Benchmark Tool Output
```bash
=== Pipeline Benchmark Tool ===

=== Benchmarking with 1000 items ===
=== Pipeline Statistics ===
Items processed: 159
Items filtered: 841
Errors: 0
Total input items: 1000
Pass rate: 15.90%
Total duration: 0.0026 ms
Average latency: 2 ns/item (per input)
Throughput: 391389432.49 items/sec

=== Benchmarking with 10000 items ===
=== Pipeline Statistics ===
Items processed: 159
Items filtered: 9841
Errors: 0
Total input items: 10000
Pass rate: 1.59%
Total duration: 0.0218 ms
Average latency: 2 ns/item (per input)
Throughput: 457959333.21 items/sec

=== Benchmarking with 100000 items ===
=== Pipeline Statistics ===
Items processed: 15966
Items filtered: 84034
Errors: 0
Total input items: 100000
Pass rate: 15.97%
Total duration: 0.4179 ms
Average latency: 4 ns/item (per input)
Throughput: 239315748.41 items/sec

=== Benchmarking with 1000000 items ===
=== Pipeline Statistics ===
Items processed: 241412
Items filtered: 758588
Errors: 0
Total input items: 1000000
Pass rate: 24.14%
Total duration: 2.1351 ms
Average latency: 2 ns/item (per input)
Throughput: 468358189.10 items/sec
```

### Test Suite Output
```bash
Randomness seeded to: 2267552208

=== Simple Transform Test ===
Input: 1 2 3 4 5
Output: 2 4 6 8 10
Expected: 2 4 6 8 10

=== Simple Filter Test ===
Input: 1 2 3 4 5
Filter: x > 3
Output: 4 5
Expected: 4 5

=== Filter then Transform Test ===
Input: 1 2 3 4 5
Filter: x > 3, then Transform: x * 2
Output: 8 10
Expected: 8 10

=== Empty Pipeline Test ===
Input: 1 2 3
No operations applied
Output: 1 2 3
Expected: 1 2 3

=== Custom Type Transformation Test ===
Input books:
  "The C++ Programming Language" by Bjarne Stroustrup (2013)
  "Effective Modern C++" by Scott Meyers (2014)
  "Clean Code" by Robert C. Martin (2008)
  "Dlsign Plttlrns" by Glng of Four (1994)
  "Code Complete" by Steve McConnell (2004)

Transformation: Book -> string -> filtered string
Filter: Titles containing letters 'A' or 'E' (case insensitive)
Result size: 4
Results:
  Title: "The C++ Programming Language" - Contains: A, E
  Title: "Effective Modern C++" - Contains: E
  Title: "Clean Code" - Contains: A, E
  Title: "Code Complete" - Contains: E

=== Pipeline Statistics ===
Items processed: 5000
Items filtered: 5000
Errors: 0
Total input items: 10000
Pass rate: 50.00%
Total duration: 0.0230 ms
Average latency: 2 ns/item (per input)
Throughput: 435691878.70 items/sec

=== Performance Comparison ===
Pipeline: 0.31 ms
Hand-written: 0.36 ms
Overhead: -11.83%

Sample results (first 10):
Pipeline:     0 4 8 12 16 20 24 28 32 36
Hand-written: 0 4 8 12 16 20 24 28 32 36
Results match: 1

=== Parallel Execution Test ===
Sequential result size: 500
Parallel result size: 500
First 10 sequential: 0 4 8 12 16 20 24 28 32 36
First 10 parallel: 0 4 8 12 16 20 24 28 32 36
Results match: 1

=== Parallel Unordered Test ===
Sequential result size: 500
Parallel unordered result size: 500
Sorted results match: 1

=== Parallel with Stats Test ===
Data size: 10000
Items processed: 5000
Items filtered: 5000
Result size: 5000
Total duration: 966900 ns

===============================================================================
All tests passed (35 assertions in 7 test cases)
```

Run examples:
```bash
# Build and run basic examples
cmake --build build --target basic_example
./build/Release/basic_example

# Build and run benchmarks
cmake --build build --target bench_tool
./build/Release/bench_tool

# Build and run tests
cmake --build build --target pipeline_tests
./build/Release/pipeline_tests
```

## 🏗️ Architecture

**Design Principles:**
- **Zero-cost abstractions**: No overhead vs hand-written loops
- **Compile-time safety**: Type checking and validation
- **Runtime efficiency**: Direct function calls, no virtual dispatch
- **Memory safety**: RAII, smart pointers, no raw allocations

**Key Components:**
- `Pipeline`: Main fluent API class with direct lambda storage
- `ExecutionPolicy`: Controls parallel execution semantics
- `ResultWithStats<T>`: Results container with performance metrics
- Statistics system: Atomic counters for thread-safe monitoring

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Add tests for new functionality
4. Ensure all tests pass (`cmake --build build --target pipeline_tests && ./build/Release/pipeline_tests`)
5. Submit a pull request

### Development Setup
```bash
git clone https://github.com/yourusername/Declarative-Pipeline-Builder.git
cd Declarative-Pipeline-Builder
mkdir build && cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Debug
cmake --build . --target pipeline_tests
./Release/pipeline_tests
```

## 🔀 Type Transformations

The pipeline supports transforming between arbitrary types:

```cpp
#include <declarative_pipeline.hpp>
#include <vector>
#include <string>

struct Person {
    std::string name;
    int age;
};

int main() {
    using namespace dpb;

    std::vector<Person> people = {
        {"Alice", 25}, {"Bob", 30}, {"Charlie", 35}
    };

    // Transform: Person → string (extract names)
    auto names = Pipeline<Person, Person>::from(people)
        .transform([](const Person& p) { return p.name; })
        .filter([](const std::string& name) { return name.length() > 3; })
        .collect(people);

    for (const auto& name : names) {
        std::cout << name << " ";
    }
    // Output: Alice Charlie

    return 0;
}
```

**Key Features:**
- Transform between any types at each pipeline step
- Compile-time type safety
- Zero overhead for type transformations
- Works with custom structs and classes

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

Inspired by functional programming concepts and Unix pipeline philosophy, built with modern C++ features for maximum performance in high-throughput data processing applications.
