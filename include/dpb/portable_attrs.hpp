#pragma once

// ============================================================================
// PORTABLE ATTRIBUTE MACROS
// ============================================================================
// Provides portable macros for compiler hints that work across GCC/Clang
// and MSVC. On MSVC, attributes that have no effect are omitted.
//
// Supported macros:
//   DPB_HOT         - Hint that a function is hot (frequently called)
//   DPB_INLINE      - Force inlining (combines always_inline + inline)
//   DPB_FLATTEN     - Hint that all calls in a function should be inlined
//   DPB_LIKELY(x)   - Hint that condition x is likely true
//   DPB_UNLIKELY(x) - Hint that condition x is unlikely true
// ============================================================================

#if defined(__GNUC__) || defined(__clang__)
    // GCC and Clang: use GNU attributes
    #define DPB_HOT [[gnu::hot]]
    #define DPB_INLINE [[gnu::always_inline]] inline
    #define DPB_FLATTEN [[gnu::flatten]]
    #define DPB_LIKELY(x) __builtin_expect(!!(x), 1)
    #define DPB_UNLIKELY(x) __builtin_expect(!!(x), 0)
#elif defined(_MSC_VER)
    // MSVC: use MSVC-specific equivalents
    #define DPB_HOT
    #define DPB_INLINE __forceinline
    #define DPB_FLATTEN
    #define DPB_LIKELY(x) (x)
    #define DPB_UNLIKELY(x) (x)
#else
    // Fallback: no hints
    #define DPB_HOT
    #define DPB_INLINE inline
    #define DPB_FLATTEN
    #define DPB_LIKELY(x) (x)
    #define DPB_UNLIKELY(x) (x)
#endif