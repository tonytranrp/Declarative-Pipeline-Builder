#pragma once

#ifdef DPB_HAS_HIGHWAY

#include "dpb/simd.hpp"
#include "dpb/memory.hpp"
#include "dpb/portable_attrs.hpp"
#include "dpb/cpu_detect.hpp"
#include <ranges>
#include <vector>
#include <chrono>
#include <utility>

namespace dpb {

// ============================================================================
// SIMD-ACCELERATED COLLECT PATH
// ============================================================================
//
// Called from Pipeline::collect_sequential() when:
//   1. DPB_HAS_HIGHWAY is defined (compile-time)
//   2. simd::simd_numeric<In> && simd::simd_numeric<Out> (compile-time)
//   3. !stats_ (runtime — SIMD path is stats-free)
//
// Since the Pipeline's operation_ is a fused lambda (filter+transform composed
// together), we CANNOT decompose it into separate filter/transform stages.
// Instead, we process elements in SIMD-sized chunks, calling the fused lambda
// element-wise, and use hn::CompressBlendedStore to compress only the passing
// elements into the output vector.
//
// The SIMD acceleration comes from:
//   - Vector loads/stores for cache-friendly memory access patterns
//   - hn::CompressBlendedStore for filter output compaction
//   - Known-size chunk iteration (better branch prediction, auto-vectorization)
// ============================================================================

template<typename In, typename Out, typename OpFunc, std::ranges::input_range Range>
DPB_HOT DPB_FLATTEN DPB_INLINE
auto simd_collect_sequential(Pipeline<In, Out, OpFunc>&& pipe, Range&& input)
    -> ResultWithStats<Out>
{
    // Runtime CPU detection: Highway's HWY_DYNAMIC_DISPATCH handles ISA
    // selection internally at runtime. This call verifies cpu_detect.hpp
    // works and is available for future per-ISA optimization tuning.
    [[maybe_unused]] const auto& cpu = detect_cpu();

    std::vector<Out> result;

    auto first = std::ranges::begin(input);
    auto last  = std::ranges::end(input);

    if constexpr (std::ranges::sized_range<Range>) {
        result.reserve(std::ranges::size(input));
    }

    const hn::ScalableTag<Out> d;
    const size_t N = hn::Lanes(d);
    constexpr size_t kMaxLanes = HWY_MAX_LANES_D(hn::ScalableTag<Out>);

    while (first != last) {
        // Determine chunk size (up to one SIMD vector width)
        auto chunk_end = first;
        size_t count = 0;
        for (size_t i = 0; i < N && chunk_end != last; ++i, ++chunk_end) {
            ++count;
        }

        // Process each element in the chunk through the fused lambda.
        // Build two arrays in parallel:
        //   out_buf[i] — the output value (may be uninitialized if filtered)
        //   mask_bits  — bitmask of which elements passed the filter
        alignas(64) Out out_buf[kMaxLanes];
        uint8_t mask_bits[kMaxLanes] = {};

        for (size_t i = 0; i < count; ++i) {
            Out out_val;
            if (pipe.operation_(*(first + static_cast<ptrdiff_t>(i)), out_val)) {
                mask_bits[i / 8] |= static_cast<uint8_t>(1u << (i % 8));
            }
            out_buf[i] = out_val;
        }

        // Compress: keep only passing elements
        auto mask       = hn::LoadMaskBits(d, mask_bits);
        size_t pass_count = hn::CountTrue(d, mask);
        size_t old_size   = result.size();
        result.resize(old_size + pass_count);

        auto v_out = (count == N)
                         ? hn::LoadU(d, out_buf)
                         : hn::LoadN(d, out_buf, count);
        hn::CompressBlendedStore(v_out, mask, d, result.data() + old_size);

        first = chunk_end;
    }

    // Zero stats — SIMD path is stats-free
    const std::size_t result_size = result.size();
    return ResultWithStats<Out>{
        std::move(result),
        result_size, 0, 0, result_size,
        std::chrono::nanoseconds{0}
    };
}

} // namespace dpb

#endif // DPB_HAS_HIGHWAY
