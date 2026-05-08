#pragma once

#include <vector>
#include <utility>
#include <ranges>

// ============================================================================
// ZIP TERMINAL OPERATION
// ============================================================================
// Pairs pipeline output elements with elements from another range.
// Iterates through both ranges in lockstep; only pairs elements that pass
// the pipeline's operation (filter/transform). Elements filtered out do NOT
// consume an element from the other range.
// Truncates to the shorter of: (pipeline output count) vs (other range size).
//
// Implementation is inlined in Pipeline::zip() member function.
// ============================================================================