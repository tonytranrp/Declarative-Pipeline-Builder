#pragma once

#include <functional>
#include <memory>
#include <vector>

namespace dpb {

template<typename In, typename Out>
class PipelineErase {
    std::function<bool(const In&, Out&)> operation_;

public:
    template<typename OpFunc>
    PipelineErase(OpFunc&& op)
        : operation_(std::forward<OpFunc>(op)) {}

    bool operator()(const In& input, Out& output) const {
        return operation_(input, output);
    }

    template<std::ranges::input_range Range>
    auto collect(Range&& input) && {
        std::vector<Out> result;
        if constexpr (std::ranges::sized_range<Range>) {
            result.reserve(std::ranges::size(input));
        }
        for (auto&& item : input) {
            Out out_val;
            if (operation_(item, out_val)) {
                result.push_back(std::move(out_val));
            }
        }
        return result;
    }
};

} // namespace dpb