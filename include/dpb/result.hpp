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

} // namespace dpb