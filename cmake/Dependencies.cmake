# Optional dependency integration via FetchContent
# All dependencies are opt-in — zero deps by default
# Populated by T2 (Highway), T3 (fmt), T4 (Tracy)

# ---- Highway SIMD ----
if(DPB_ENABLE_SIMD)
    include(FetchContent)
    FetchContent_Declare(
        highway
        GIT_REPOSITORY https://github.com/google/highway.git
        GIT_TAG 1.2.0
    )
    set(HWY_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(HWY_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(HWY_ENABLE_CONTRIB ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(highway)

    target_link_libraries(declarative_pipeline INTERFACE hwy hwy_contrib)
    target_link_libraries(fast_pipeline INTERFACE hwy hwy_contrib)
    target_compile_definitions(declarative_pipeline INTERFACE DPB_HAS_HIGHWAY)
    target_compile_definitions(fast_pipeline INTERFACE DPB_HAS_HIGHWAY)
endif()

# ---- {fmt} formatting ----
if(DPB_ENABLE_FMT)
    include(FetchContent)
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 11.1.4
    )
    set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
    set(FMT_DOC OFF CACHE BOOL "" FORCE)
    set(FMT_TEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(fmt)

    target_link_libraries(declarative_pipeline INTERFACE fmt::fmt)
    target_link_libraries(fast_pipeline INTERFACE fmt::fmt)
    target_compile_definitions(declarative_pipeline INTERFACE DPB_HAS_FMT)
    target_compile_definitions(fast_pipeline INTERFACE DPB_HAS_FMT)
endif()

# ---- Tracy profiling ----
if(DPB_ENABLE_TRACY)
    include(FetchContent)
    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG v0.11.1
    )
    FetchContent_MakeAvailable(tracy)

    # Build TracyClient as a STATIC library
    add_library(tracy_client STATIC
        ${tracy_SOURCE_DIR}/public/TracyClient.cpp
    )
    target_include_directories(tracy_client PUBLIC
        ${tracy_SOURCE_DIR}/public
    )
    target_link_libraries(tracy_client PUBLIC
        ${CMAKE_THREAD_LIBS_INIT}
    )
    if(APPLE)
        target_link_libraries(tracy_client PUBLIC
            "-framework CoreFoundation"
        )
    endif()
    target_compile_definitions(tracy_client PUBLIC TRACY_ENABLE=ON)

    target_link_libraries(declarative_pipeline INTERFACE tracy_client)
    target_link_libraries(fast_pipeline INTERFACE tracy_client)
    target_compile_definitions(declarative_pipeline INTERFACE DPB_HAS_TRACY TRACY_ENABLE=ON)
    target_compile_definitions(fast_pipeline INTERFACE DPB_HAS_TRACY TRACY_ENABLE=ON)
endif()
