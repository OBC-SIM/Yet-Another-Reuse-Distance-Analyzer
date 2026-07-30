#pragma once

#include <stddef.h>
#include <stdint.h>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

namespace yarda
{

/**
 * @brief Exact reuse-distance profile for a memory-reference trace.
 */
struct ReuseProfile
{
  std::map<size_t, uint64_t> histogram;
  std::unordered_set<std::string> cold_misses;
};

/**
 * @brief Calculate exact LRU stack distances in O(N log N) time.
 *
 * @param trace Ordered cache-line or element reference keys.
 * @return Histogram and unique cold-reference set.
 */
ReuseProfile calculate_reuse_profile(const std::vector<std::string> & trace);

}  // namespace yarda
