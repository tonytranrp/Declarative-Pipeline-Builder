# Decisions

## 2026-05-07
- SIMD integrates INTO Pipeline class (compile-time eligibility, runtime dispatch) — not standalone helpers
- Scalar fallback always preserved — SIMD accelerates, never replaces
- Highway stays optional DPB_ENABLE_SIMD flag
- Allocators: template param with std::allocator default (no API break)
- Error handling: Result<T,E> backward-compatible with existing bool signature
- Size budget: -Os -flto=thin -Wl,--gc-sections on SIMD preset
- Dual-mode: compile-time CMake presets, not runtime toggle
- collect_parallel merge is already O(n) — verify efficiency, don't rewrite