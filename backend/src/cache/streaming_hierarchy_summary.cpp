#include "streaming_hierarchy_summary.hpp"

#include <limits>
#include <stdexcept>

#include "hierarchy_service_summary.hpp"

namespace yarda::detail
{

CacheLevelSummary summarize_streaming_level(const ExactCsrdSummary & source)
{
  const auto misses =
    checked_service_sum(source.cold_misses, source.replacement_misses);
  if (checked_service_sum(source.hits, misses) != source.lookups)
    throw std::logic_error("hierarchy level counts do not conserve lookups");
  std::uint64_t finite = 0;
  for (const auto & entry : source.histogram)
    finite = checked_service_sum(finite, entry.second);
  if (finite != checked_service_sum(source.hits, source.replacement_misses))
    throw std::logic_error(
      "hierarchy histogram does not conserve finite references");
  if (source.unique_lines != source.cold_misses)
    throw std::logic_error(
      "hierarchy unique-line count disagrees with cold misses");
  return {source.lookups,
          source.hits,
          misses,
          source.cold_misses,
          source.replacement_misses,
          source.unique_lines,
          source.histogram};
}

void add_streaming_coverage(TraceCoverage & total, const TraceCoverage & task)
{
  total.source_accesses =
    checked_service_sum(total.source_accesses, task.source_accesses);
  total.resolved_accesses =
    checked_service_sum(total.resolved_accesses, task.resolved_accesses);
  total.rejected_accesses =
    checked_service_sum(total.rejected_accesses, task.rejected_accesses);
  total.emitted_line_references = checked_service_sum(
    total.emitted_line_references, task.emitted_line_references);
}

LruAccessResult streaming_lru_result(const CsrdObservation & observation)
{
  LruAccessResult result;
  result.outcome = observation.outcome;
  if (observation.distance)
  {
    if constexpr (std::numeric_limits<std::size_t>::digits <
                  std::numeric_limits<std::uint64_t>::digits)
    {
      if (*observation.distance > std::numeric_limits<std::size_t>::max())
        throw std::overflow_error("hierarchy event distance overflows size_t");
    }
    result.reuse_distance = static_cast<std::size_t>(*observation.distance);
  }
  return result;
}

}  // namespace yarda::detail
