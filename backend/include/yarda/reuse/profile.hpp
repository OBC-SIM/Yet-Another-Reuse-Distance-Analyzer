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
 * @brief Own a Global Reuse-Distance Profile for a memory-reference trace.
 *
 * Distances count intervening distinct reference keys across the whole trace;
 * this profile does not partition references by cache set or cache level.
 */
struct ReuseProfile
{
  std::map<size_t, uint64_t> histogram;
  std::unordered_set<std::string> cold_misses;
};

/**
 * @brief Calculate exact Global Reuse Distances in O(N log N) time.
 *
 * @param trace Ordered cache-line or element reference keys.
 * @return Histogram and unique cold-reference set.
 */
ReuseProfile calculate_reuse_profile(const std::vector<std::string> & trace);

}  // namespace yarda
