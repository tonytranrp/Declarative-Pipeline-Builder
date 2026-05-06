# Compiler flags for declarative/fast pipeline targets
# Extracted from CMakeLists.txt to avoid duplication

function(dpb_apply_optimization_flags target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${target} INTERFACE
            $<$<CONFIG:Release>:/O2>
            $<$<CONFIG:Release>:/Ob3>
            $<$<CONFIG:Release>:/GL>
            $<$<CONFIG:Release>:/Oi>
            $<$<CONFIG:Release>:/Ot>
            $<$<CONFIG:Release>:/GS->
            $<$<CONFIG:Release>:/arch:AVX2>
        )
        if("${target}" STREQUAL "fast_pipeline")
            target_compile_options(${target} INTERFACE
                $<$<CONFIG:Release>:/Oy>
            )
        endif()
        target_link_options(${target} INTERFACE
            $<$<CONFIG:Release>:/LTCG>
        )
    else()
        target_compile_options(${target} INTERFACE
            $<$<CONFIG:Release>:-O3>
            $<$<CONFIG:Release>:-march=native>
            $<$<CONFIG:Release>:-flto>
            $<$<CONFIG:Release>:-ffast-math>
            $<$<CONFIG:Release>:-funroll-loops>
            $<$<CONFIG:Release>:-finline-functions>
            $<$<CONFIG:Release>:-fno-exceptions>
        )
        if("${target}" STREQUAL "fast_pipeline")
            target_compile_options(${target} INTERFACE
                $<$<CONFIG:Release>:-mtune=native>
                $<$<CONFIG:Release>:-fomit-frame-pointer>
                $<$<CONFIG:Release>:-fno-rtti>
            )
            target_link_options(${target} INTERFACE
                $<$<CONFIG:Release>:-flto>
            )
        endif()
    endif()
endfunction()
