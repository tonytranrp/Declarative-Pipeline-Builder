cmake_minimum_required(VERSION 3.20)
project(DeclarativePipelineBuilder VERSION 1.0.0 LANGUAGES CXX)

# Set C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Options
option(DPB_BUILD_TESTS "Build tests" ON)
option(DPB_BUILD_EXAMPLES "Build examples" ON)
option(DPB_BUILD_BENCHMARKS "Build benchmarks" ON)
option(DPB_BUILD_DOCS "Build documentation" OFF)
option(DPB_ENABLE_SIMD "Enable Highway SIMD vectorization" OFF)
option(DPB_ENABLE_FMT "Use {fmt} for output formatting" OFF)
option(DPB_ENABLE_TRACY "Enable Tracy profiling integration" OFF)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerFlags)

# Library
add_library(declarative_pipeline INTERFACE)
target_include_directories(declarative_pipeline INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

dpb_apply_optimization_flags(declarative_pipeline)

# Fast pipeline library - zero overhead implementation
add_library(fast_pipeline INTERFACE)
target_include_directories(fast_pipeline INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(fast_pipeline INTERFACE cxx_std_23)

dpb_apply_optimization_flags(fast_pipeline)

# Optional dependency integration (must come after library targets)
include(Dependencies)

# Tests
if(DPB_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(pipeline_tests
        tests/test_pipeline.cpp
        tests/test_async_pipeline.cpp
        tests/test_parallel_pipeline.cpp
        tests/test_integration.cpp
    )
    target_link_libraries(pipeline_tests PRIVATE Catch2::Catch2WithMain declarative_pipeline fast_pipeline)
    target_compile_features(pipeline_tests PRIVATE cxx_std_23)

    include(Catch)
    catch_discover_tests(pipeline_tests)
endif()

# Benchmarks
if(DPB_BUILD_BENCHMARKS)
    include(FetchContent)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
    )

    # Configure benchmark to download dependencies and avoid GTest issues
    set(BENCHMARK_DOWNLOAD_DEPENDENCIES ON CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(benchmark)

    add_executable(pipeline_benchmarks
        benchmarks/benchmark_pipeline.cpp
    )
    target_link_libraries(pipeline_benchmarks
        PRIVATE
        declarative_pipeline
        benchmark::benchmark
        benchmark::benchmark_main
    )

    target_compile_options(pipeline_benchmarks PRIVATE
        $<$<CONFIG:Release>:-DNDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:/O2 /Ob3 /arch:AVX2>
        $<$<CXX_COMPILER_ID:GNU,Clang>:-O3 -march=native>
    )

    if(DPB_ENABLE_SIMD)
        add_executable(bench_simd
            benchmarks/bench_simd.cpp
        )
        target_link_libraries(bench_simd
            PRIVATE
            declarative_pipeline
            benchmark::benchmark
            benchmark::benchmark_main
        )
        target_compile_options(bench_simd PRIVATE
            $<$<CONFIG:Release>:-DNDEBUG>
            $<$<CXX_COMPILER_ID:MSVC>:/O2 /Ob3 /arch:AVX2>
            $<$<CXX_COMPILER_ID:GNU,Clang>:-O3 -march=native>
        )
    endif()
endif()

# Examples
if(DPB_BUILD_EXAMPLES)
    add_executable(basic_example examples/basic_usage.cpp)
    target_link_libraries(basic_example PRIVATE declarative_pipeline)
    target_compile_features(basic_example PRIVATE cxx_std_23)

    add_executable(async_example examples/async_usage.cpp)
    target_link_libraries(async_example PRIVATE declarative_pipeline)
    target_compile_features(async_example PRIVATE cxx_std_23)

    add_executable(parallel_example examples/parallel_usage.cpp)
    target_link_libraries(parallel_example PRIVATE declarative_pipeline)
    target_compile_features(parallel_example PRIVATE cxx_std_23)
endif()

# Benchmark tool
if(DPB_BUILD_BENCHMARKS)
    add_executable(bench_tool tools/bench_tool.cpp)
    target_link_libraries(bench_tool PRIVATE declarative_pipeline)
    target_compile_features(bench_tool PRIVATE cxx_std_23)
endif()

# Install (skip when optional deps enabled — they're not in the export set)
if(NOT DPB_ENABLE_SIMD AND NOT DPB_ENABLE_FMT AND NOT DPB_ENABLE_TRACY)
install(TARGETS declarative_pipeline
    EXPORT DeclarativePipelineBuilderTargets
    INCLUDES DESTINATION include
)

install(DIRECTORY include/ DESTINATION include)

install(EXPORT DeclarativePipelineBuilderTargets
    FILE DeclarativePipelineBuilderTargets.cmake
    NAMESPACE DeclarativePipelineBuilder::
    DESTINATION lib/cmake/DeclarativePipelineBuilder
)
endif()
