#pragma once

#include <vector>

#include "yarda/cache/line_mapping.hpp"
#include "yarda/trace/emission_budget.hpp"
#include "yarda/trace/resolved_access.hpp"

namespace yarda
{

/**
 * @brief Deliver touched lines with the resolved source's full provenance.
 * @param access Borrowed resolved source, unchanged throughout the call.
 * @param geometry Borrowed mapping geometry.
 * @param sink Required synchronous callback; row references expire on return.
 * @return Nothing.
 * @throws std::invalid_argument for invalid geometry, range or empty sink.
 * @throws std::overflow_error if the source range overflows.
 * @note Sink exceptions propagate unchanged; this primitive has no emission
 * limit. Use the budget overload to bound cumulative streaming work.
 */
void for_each_cache_line(const ResolvedAccess & access,
                         const CacheGeometry & geometry,
                         const CacheLineSink & sink);

/**
 * @brief Charge each mapped line to a shared module emission budget.
 * @param access Borrowed resolved source, unchanged throughout the call.
 * @param geometry Borrowed mapping geometry.
 * @param sink Required synchronous callback; row references expire on return.
 * @param budget Borrowed module budget, shared across tasks and accesses.
 * @return Nothing.
 * @throws std::invalid_argument for invalid input or exhausted line budget.
 * @throws std::overflow_error if the source range overflows.
 * @note A line is charged before delivery. On failure, discard partial consumer
 * state and the budget; exceptions stop delivery and propagate unchanged.
 */
void for_each_cache_line(const ResolvedAccess & access,
                         const CacheGeometry & geometry,
                         const CacheLineSink & sink,
                         TraceEmissionBudget & budget);

/**
 * @brief Decode every cache line touched by one resolved source access.
 *
 * Each row retains the source operation, emission ordinal, and zero-based
 * position within the cache-line span.
 *
 * @param access Geometry-independent linked source access.
 * @param geometry Cache geometry used to partition the linked address.
 * @return Ordered mapping rows, one for each touched cache line.
 * @throws std::invalid_argument for invalid geometry or access range.
 * @throws std::overflow_error if the source range overflows.
 */
std::vector<CacheLineMapping> map_cache_lines(const ResolvedAccess & access,
                                              const CacheGeometry & geometry);

}  // namespace yarda
