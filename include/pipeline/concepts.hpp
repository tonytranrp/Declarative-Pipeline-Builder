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

// Async transform stage: placeholder for future implementation
// template<typename F, typename In, typename Out>
// concept AsyncTransformStage = std::invocable<F, In> &&
//                              requires(std::invoke_result_t<F, In> coro) {
//                                  { coro.next() } -> std::convertible_to<Result<Out>>;
//                              };

// Sink concept: consumes final output
template<typename S, typename T>
concept Sink = requires(S sink, T value) {
    sink.write(value);
} || requires(S sink, T value) {
    sink.push_back(value);
} || requires(S sink, T value) {
    sink.insert(value);
} || std::invocable<S, T>;

// Source concept: provides initial input
template<typename S, typename T>
concept Source = requires(S source) {
    { source.begin() } -> std::input_iterator;
    { source.end() } -> std::input_iterator;
    { *source.begin() } -> std::convertible_to<T>;
} || requires(S source) {
    { source.next() } -> std::convertible_to<Result<T>>;
};

} // namespace dpb
