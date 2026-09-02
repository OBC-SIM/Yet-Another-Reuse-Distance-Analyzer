#pragma once

#include <vector>

#include "yarda/cache/line_mapping.hpp"
#include "yarda/trace/resolved_access.hpp"

namespace yarda
{

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
