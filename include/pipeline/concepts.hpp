#pragma once

#include <concepts>
#include "dpb/result.hpp"

namespace dpb {

// Concepts for pipeline stages

// Transform stage: takes input and produces output
template<typename F, typename In, typename Out>
concept TransformStage = std::invocable<F, In> &&
                        std::convertible_to<std::invoke_result_t<F, In>, Out>;

// Filter stage: takes input and returns bool or Result<bool>
template<typename F, typename T>
concept FilterStage = std::invocable<F, T> &&
                     (std::convertible_to<std::invoke_result_t<F, T>, bool> ||
                      std::convertible_to<std::invoke_result_t<F, T>, Result<bool>>);

} // namespace dpb
