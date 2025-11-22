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

# Library
add_library(declarative_pipeline INTERFACE)
target_include_directories(declarative_pipeline INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Aggressive optimization flags for performance
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(declarative_pipeline INTERFACE
        $<$<CONFIG:Release>:/O2>        # Maximum optimization
        $<$<CONFIG:Release>:/Ob3>       # Aggressive inlining
        $<$<CONFIG:Release>:/GL>        # Whole program optimization
        $<$<CONFIG:Release>:/Oi>        # Intrinsic functions
        $<$<CONFIG:Release>:/Ot>        # Favor fast code
        $<$<CONFIG:Release>:/arch:AVX2> # Enable AVX2 instructions
        $<$<CONFIG:Release>:/GS->       # Disable security checks for speed
    )
    target_link_options(declarative_pipeline INTERFACE
        $<$<CONFIG:Release>:/LTCG>      # Link-time code generation
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(declarative_pipeline INTERFACE
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-march=native>
        $<$<CONFIG:Release>:-flto>
        $<$<CONFIG:Release>:-ffast-math>
        $<$<CONFIG:Release>:-funroll-loops>
        $<$<CONFIG:Release>:-finline-functions>
        $<$<CONFIG:Release>:-fno-exceptions>
    )
endif()

# Coroutine support will be added later when MSVC fully supports C++23 coroutines
# if(MSVC)
#     target_compile_options(declarative_pipeline INTERFACE /await)
# endif()

# Fast pipeline library - zero overhead implementation
add_library(fast_pipeline INTERFACE)
target_include_directories(fast_pipeline INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(fast_pipeline INTERFACE cxx_std_23)

# Maximum optimization for fast pipeline
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        target_compile_options(fast_pipeline INTERFACE
            /O2          # Max optimization
            /Ob3         # Aggressive inlining
            /GL          # Whole program optimization
            /Oi          # Intrinsic functions
            /Ot          # Favor speed
            /GS-         # Disable security checks (if safe)
            /Oy          # Omit frame pointers
            /arch:AVX2   # Enable AVX2
        )
        target_link_options(fast_pipeline INTERFACE
            /LTCG        # Link-time code generation
        )
    else()  # GCC/Clang
        target_compile_options(fast_pipeline INTERFACE
            -O3
            -march=native
            -mtune=native
            -flto
            -ffast-math
            -funroll-loops
            -finline-functions
            -fomit-frame-pointer
            -fno-exceptions  # If you don't need them
            -fno-rtti        # If you don't need them
        )
        target_link_options(fast_pipeline INTERFACE
            -flto
        )
    endif()
endif()

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
        -O3
        -march=native
        -DNDEBUG
    )
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

# Install
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
