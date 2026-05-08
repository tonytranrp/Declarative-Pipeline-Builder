#pragma once

#include <cstdint>
#include <cstring>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace dpb {

// ============================================================================
// CPU Feature Detection
// ============================================================================
// Provides runtime CPU feature detection with thread-safe caching.
// Highway's HWY_DYNAMIC_DISPATCH handles ISA selection internally;
// this struct is for future per-ISA optimization tuning.
//
// Supported platforms:
//   GCC/Clang — __builtin_cpu_supports()
//   MSVC      — __cpuid intrinsic
// ============================================================================

struct cpu_features {
    bool has_sse42   = false;
    bool has_avx2    = false;
    bool has_avx512f = false;
};

inline const cpu_features& detect_cpu() {
    static const cpu_features features = []() {
        cpu_features f;

#if defined(__GNUC__) || defined(__clang__)
        f.has_sse42   = __builtin_cpu_supports("sse4.2");
        f.has_avx2    = __builtin_cpu_supports("avx2");
        f.has_avx512f = __builtin_cpu_supports("avx512f");
#elif defined(_MSC_VER)
        int regs[4];

        // CPUID leaf 1: SSE4.2 is in ECX bit 20
        __cpuid(regs, 1);
        f.has_sse42 = (regs[2] & (1 << 20)) != 0;

        // CPUID leaf 7, subleaf 0: AVX2 in EBX bit 5, AVX-512F in EBX bit 16
        __cpuid(regs, 7);
        f.has_avx2    = (regs[1] & (1 << 5))  != 0;
        f.has_avx512f = (regs[1] & (1 << 16)) != 0;
#else
        // Unknown platform: no feature detection available
        // All flags remain false — callers must handle scalar fallback
#endif

        return f;
    }();

    return features;
}

} // namespace dpb
