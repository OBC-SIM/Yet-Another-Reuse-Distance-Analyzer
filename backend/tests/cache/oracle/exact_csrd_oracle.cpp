#include "exact_csrd_oracle.hpp"

#include <limits>
#include <optional>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace yarda
{
namespace test
{
namespace
{

void validate_mapping(const CacheLineMapping & mapping,
                      const CacheGeometry & geometry, std::uint64_t set_count)
{
  const auto & decoded = mapping.decoded;
  if (decoded.block_number != decoded.address / geometry.line_size ||
      decoded.set_index != decoded.block_number % set_count ||
      decoded.tag != decoded.block_number / set_count ||
      decoded.line_offset != decoded.address % geometry.line_size)
  {
    throw std::invalid_argument(
      "exact CSRD oracle received an inconsistent cache mapping");
  }
}

void increment_distance(std::uint64_t & distance)
{
  if (distance == std::numeric_limits<std::uint64_t>::max())
  {
    throw std::overflow_error("exact CSRD oracle distance overflows");
  }
  ++distance;
}

}  // namespace

std::vector<OracleCsrdObservation>
naive_exact_csrd(const std::vector<CacheLineMapping> & accesses,
                 const CacheGeometry & geometry)
{
  const auto set_count = cache_set_count(geometry);
  std::vector<OracleCsrdObservation> result;
  result.reserve(accesses.size());

  for (std::size_t index = 0; index < accesses.size(); ++index)
  {
    validate_mapping(accesses[index], geometry, set_count);
    const auto & target = accesses[index].decoded;
    std::optional<std::size_t> previous;
    for (std::size_t cursor = index; cursor > 0; --cursor)
    {
      const auto candidate = cursor - 1;
      if (accesses[candidate].decoded.block_number == target.block_number)
      {
        previous = candidate;
        break;
      }
    }

    if (!previous)
    {
      result.push_back({std::nullopt, LruAccessOutcome::ColdMiss});
      continue;
    }

    std::unordered_set<std::uint64_t> distinct_blocks;
    std::uint64_t distance = 0;
    for (std::size_t cursor = *previous + 1; cursor < index; ++cursor)
    {
      const auto & competitor = accesses[cursor].decoded;
      if (competitor.set_index == target.set_index &&
          distinct_blocks.insert(competitor.block_number).second)
      {
        increment_distance(distance);
      }
    }
    const auto outcome = distance < geometry.associativity
                           ? LruAccessOutcome::Hit
                           : LruAccessOutcome::ReplacementMiss;
    result.push_back({distance, outcome});
  }
  return result;
}

}  // namespace test
}  // namespace yarda
