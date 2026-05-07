#pragma once

#include "dpb/simd.hpp"

#include <vector>
#include <ranges>
#include <type_traits>

namespace dpb {

template <std::ranges::input_range R, typename Fn>
auto simd_transform(R&& range, Fn fn)
    -> std::vector<std::invoke_result_t<Fn, std::ranges::range_value_t<R>>>
{
    using In  = std::ranges::range_value_t<R>;
    using Out = std::invoke_result_t<Fn, In>;

#ifdef DPB_HAS_HIGHWAY
    if constexpr (simd::simd_numeric<In> && simd::simd_numeric<Out>) {
        return simd::simd_transform(std::forward<R>(range), fn);
    } else
#endif
    {
        std::vector<Out> result;

        if constexpr (std::ranges::sized_range<R>) {
            result.reserve(std::ranges::size(range));
        }

        for (auto&& elem : range) {
            result.push_back(fn(elem));
        }

        return result;
    }
}

}  // namespace dpb
