#pragma once

#include "dpb/simd.hpp"

#include <vector>
#include <ranges>
#include <concepts>

namespace dpb {

template <std::ranges::input_range R, typename Pred>
auto simd_filter(R&& range, Pred pred) -> std::vector<std::ranges::range_value_t<R>>
{
    using T = std::ranges::range_value_t<R>;

#ifdef DPB_HAS_HIGHWAY
    if constexpr (simd::simd_numeric<T>) {
        return simd::simd_filter(std::forward<R>(range), pred);
    } else
#endif
    {
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
}

}  // namespace dpb
