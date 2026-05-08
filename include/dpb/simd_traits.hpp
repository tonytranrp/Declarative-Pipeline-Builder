#pragma once

#include <type_traits>
#include <concepts>
#include "dpb/simd.hpp"
#include "pipeline/pipeline.hpp"

namespace dpb {

// ============================================================================
// SIMD TYPE TRAITS — compile-time eligibility detection for SIMD execution
// ============================================================================

// ── simd_eligible_op ──────────────────────────────────────────────────────
// True when OpFunc is a simple numeric transform: In → Out
// where both In and Out are simd_numeric types.
//
// OpFunc must be invocable with In and produce a result convertible to Out.
//
// Example:
//   auto square = [](int x) { return x * x; };
//   simd_eligible_op<decltype(square), int, int>         → true
//   simd_eligible_op<decltype(to_string), int, std::string> → false

template<typename OpFunc, typename In, typename Out>
struct simd_eligible_op
    : std::bool_constant<
        simd::simd_numeric<In> &&
        simd::simd_numeric<Out> &&
        std::invocable<OpFunc, In> &&
        std::convertible_to<std::invoke_result_t<OpFunc, In>, Out>
    > {};

template<typename OpFunc, typename In, typename Out>
inline constexpr bool simd_eligible_op_v =
    simd_eligible_op<OpFunc, In, Out>::value;


// ── simd_eligible_predicate ───────────────────────────────────────────────
// True when OpFunc is a simple numeric predicate: T → bool
// where T is a simd_numeric type.
//
// OpFunc must satisfy std::predicate<T> (invocable, returns testable-bool).
//
// Example:
//   auto is_even = [](int x) { return x % 2 == 0; };
//   simd_eligible_predicate<decltype(is_even), int>         → true
//   simd_eligible_predicate<decltype(non_empty), std::string> → false

template<typename OpFunc, typename T>
struct simd_eligible_predicate
    : std::bool_constant<
        simd::simd_numeric<T> &&
        std::predicate<OpFunc, T>
    > {};

template<typename OpFunc, typename T>
inline constexpr bool simd_eligible_predicate_v =
    simd_eligible_predicate<OpFunc, T>::value;


// ── simd_eligible_chain ───────────────────────────────────────────────────
// Meta-function: true when Pipeline<In, Out, OpFunc> is SIMD-eligible.
//
// Since the OpFunc lambda is opaque (type-erased composition), we can only
// inspect the input/output types. A chain is SIMD-eligible when:
//   1. In and Out are both simd_numeric
//   2. (Runtime check needed: chain does not use stats or parallel execution)
//
// Condition (2) must be checked at runtime since stats_/exec_policy_ are
// runtime state, not encoded in the Pipeline type template parameters.
//
// Convenience variable template:
//   template<typename P>
//   inline constexpr bool simd_eligible_chain_v = simd_eligible_chain<P>::value;

template<typename T>
struct simd_eligible_chain : std::false_type {};

template<typename In, typename Out, typename OpFunc>
struct simd_eligible_chain<Pipeline<In, Out, OpFunc>>
    : std::bool_constant<
        simd::simd_numeric<In> && simd::simd_numeric<Out>
    > {};

template<typename P>
inline constexpr bool simd_eligible_chain_v =
    simd_eligible_chain<P>::value;

} // namespace dpb
