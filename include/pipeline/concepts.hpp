#pragma once

#include <concepts>
#include <expected>

namespace dpb {

// Error types
enum class PipelineError {
    Filtered,
    InvalidInput,
    ProcessingFailed,
    BackpressureExceeded
};

// Result type for pipeline operations
template<typename T>
using Result = std::expected<T, PipelineError>;

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
