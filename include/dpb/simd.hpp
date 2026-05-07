#pragma once

#include "highway_config.hpp"

#include <vector>
#include <ranges>
#include <concepts>
#include <type_traits>

namespace dpb::simd {

template <typename T>
concept simd_numeric = std::same_as<T, int32_t>  ||
                       std::same_as<T, int64_t>  ||
                       std::same_as<T, float>    ||
                       std::same_as<T, double>;


#ifdef DPB_HAS_HIGHWAY

template <std::ranges::input_range R, std::predicate<std::ranges::range_value_t<R>> Pred>
    requires simd_numeric<std::ranges::range_value_t<R>>
auto simd_filter(R&& range, Pred pred) -> std::vector<std::ranges::range_value_t<R>>
{
    using T = std::ranges::range_value_t<R>;
    std::vector<T> result;

    auto first = std::ranges::begin(range);
    auto last  = std::ranges::end(range);

    if constexpr (std::ranges::sized_range<R>) {
        result.reserve(std::ranges::size(range));
    }

    const hn::ScalableTag<T> d;
    const size_t N = hn::Lanes(d);

    while (first != last) {
        auto chunk_end = first;
        size_t count = 0;
        for (size_t i = 0; i < N && chunk_end != last; ++i, ++chunk_end) {
            ++count;
        }

        auto v = (count == N)
                     ? hn::LoadU(d, &*first)
                     : hn::LoadN(d, &*first, count);

        uint8_t bits[HWY_MAX_LANES_D(hn::ScalableTag<T>)] = {};
        for (size_t i = 0; i < count; ++i) {
            if (pred(*(first + static_cast<ptrdiff_t>(i)))) {
                bits[i / 8] |= static_cast<uint8_t>(1u << (i % 8));
            }
        }

        auto mask = hn::LoadMaskBits(d, bits);
        size_t old_size = result.size();
        size_t pass_count = hn::CountTrue(d, mask);
        result.resize(old_size + pass_count);
        hn::CompressBlendedStore(v, mask, d, result.data() + old_size);

        first = chunk_end;
    }

    return result;
}

#else

template <std::ranges::input_range R, std::predicate<std::ranges::range_value_t<R>> Pred>
auto simd_filter(R&& range, Pred pred) -> std::vector<std::ranges::range_value_t<R>>
{
    // Scalar fallback — also provided by dpb/simd_filter.hpp wrapper.
    // Kept here for direct dpb::simd::simd_filter() callers.
    using T = std::ranges::range_value_t<R>;
    std::vector<T> result;

    if constexpr (std::ranges::sized_range<R>) {
        result.reserve(std::ranges::size(range));
    }

    for (auto&& elem : range) {
        if (pred(elem)) {
            result.push_back(elem);
        }
    }

    return result;
}

#endif

#ifdef DPB_HAS_HIGHWAY

template <std::ranges::input_range R, std::invocable<std::ranges::range_value_t<R>> Fn>
    requires simd_numeric<std::ranges::range_value_t<R>> &&
             simd_numeric<std::invoke_result_t<Fn, std::ranges::range_value_t<R>>>
auto simd_transform(R&& range, Fn fn)
    -> std::vector<std::invoke_result_t<Fn, std::ranges::range_value_t<R>>>
{
    using Out = std::invoke_result_t<Fn, std::ranges::range_value_t<R>>;
    std::vector<Out> result;

    auto first = std::ranges::begin(range);
    auto last  = std::ranges::end(range);

    if constexpr (std::ranges::sized_range<R>) {
        result.reserve(std::ranges::size(range));
    }

    const hn::ScalableTag<Out> d_out;
    const size_t N = hn::Lanes(d_out);

    while (first != last) {
        auto chunk_end = first;
        size_t count = 0;
        for (size_t i = 0; i < N && chunk_end != last; ++i, ++chunk_end) {
            ++count;
        }

        alignas(64) Out buf[N];
        for (size_t i = 0; i < count; ++i) {
            buf[i] = fn(*(first + static_cast<ptrdiff_t>(i)));
        }

        size_t old_size = result.size();
        result.resize(old_size + count);
        auto v_out = (count == N)
                         ? hn::LoadU(d_out, buf)
                         : hn::LoadN(d_out, buf, count);
        hn::StoreU(v_out, d_out, result.data() + old_size);

        first = chunk_end;
    }

    return result;
}

#else

template <std::ranges::input_range R, std::invocable<std::ranges::range_value_t<R>> Fn>
auto simd_transform(R&& range, Fn fn)
    -> std::vector<std::invoke_result_t<Fn, std::ranges::range_value_t<R>>>
{
    // Scalar fallback — also provided by dpb/simd_transform.hpp wrapper.
    // Kept here for direct dpb::simd::simd_transform() callers.
    using Out = std::invoke_result_t<Fn, std::ranges::range_value_t<R>>;
    std::vector<Out> result;

    if constexpr (std::ranges::sized_range<R>) {
        result.reserve(std::ranges::size(range));
    }

    for (auto&& elem : range) {
        result.push_back(fn(elem));
    }

    return result;
}

#endif

}  // namespace dpb::simd
