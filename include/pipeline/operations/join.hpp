#pragma once

#include <vector>
#include <utility>
#include <unordered_map>
#include <ranges>

// ============================================================================
// JOIN TERMINAL OPERATION (INNER JOIN / HASH JOIN)
// ============================================================================
// Performs an inner join between pipeline output and another range.
// keyFn is applied to both sides to extract the join key.
// Builds a hash map from the other range, then probes with pipeline outputs.
// Produces all matching pairs (one output element may match multiple other
// elements, and vice versa).
//
// Implementation is inlined in Pipeline::join() member function.
// ============================================================================