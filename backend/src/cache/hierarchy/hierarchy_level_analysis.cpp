#include "hierarchy_level_analysis.hpp"

#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace yarda::detail
{
namespace
{

std::uint64_t checked_count(std::size_t value)
{
  if constexpr (std::numeric_limits<std::size_t>::digits >
                std::numeric_limits<std::uint64_t>::digits)
  {
    if (value > std::numeric_limits<std::uint64_t>::max())
    {
      throw std::overflow_error(
        "hierarchy size or distance overflows uint64_t");
    }
  }
  return static_cast<std::uint64_t>(value);
}

std::uint64_t checked_sum(std::uint64_t left, std::uint64_t right)
{
  if (right > std::numeric_limits<std::uint64_t>::max() - left)
  {
    throw std::overflow_error("hierarchy counter overflows uint64_t");
  }
  return left + right;
}

}  // namespace

CacheLevelSummary
summarize_hierarchy_level(const LruRdAnalysis & analysis,
                          const std::vector<CacheLineMapping> & mappings)
{
  if (analysis.accesses.size() != mappings.size())
  {
    throw std::logic_error(
      "hierarchy level result size disagrees with mappings");
  }
  CacheLevelSummary result;
  result.lookups = checked_count(mappings.size());
  result.hits = analysis.hits;
  result.cold_misses = analysis.cold_misses;
  result.replacement_misses = analysis.replacement_misses;
  result.misses = checked_sum(result.cold_misses, result.replacement_misses);
  if (checked_sum(result.hits, result.misses) != result.lookups)
  {
    throw std::logic_error("hierarchy level counts do not conserve lookups");
  }

  std::uint64_t finite_references = 0;
  for (const auto & [distance, count] : analysis.histogram)
  {
    result.csrd_histogram.emplace(checked_count(distance), count);
    finite_references = checked_sum(finite_references, count);
  }
  if (finite_references != checked_sum(result.hits, result.replacement_misses))
  {
    throw std::logic_error(
      "hierarchy histogram does not conserve finite references");
  }

  std::unordered_set<std::uint64_t> blocks;
  for (const auto & mapping : mappings)
    blocks.insert(mapping.decoded.block_number);
  result.unique_lines = checked_count(blocks.size());
  if (result.unique_lines != result.cold_misses)
  {
    throw std::logic_error(
      "hierarchy unique-line count disagrees with cold misses");
  }
  return result;
}

BatchCacheLevelResult analyze_hierarchy_level(
  std::vector<CacheLineMapping> mappings, const CacheGeometry & geometry)
{
  auto analysis = analyze_lru_reuse(mappings, geometry);
  auto summary = summarize_hierarchy_level(analysis, mappings);
  return {std::move(summary), std::move(mappings),
          std::move(analysis.accesses)};
}

}  // namespace yarda::detail
