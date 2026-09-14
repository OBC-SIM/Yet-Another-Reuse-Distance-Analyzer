#include "yarda/cache/hierarchy_analysis.hpp"

#include <utility>

#include "hierarchy_aggregation.hpp"
#include "hierarchy_input_validation.hpp"
#include "hierarchy_level_analysis.hpp"
#include "yarda/trace/mapped_trace.hpp"

namespace yarda
{

BatchHierarchyResult analyze_batch_hierarchy(
  const ResolvedTaskTraceResult & resolved, const AnalysisHierarchy & hierarchy)
{
  detail::validate_hierarchy_input(resolved, hierarchy);
  auto mapped = map_resolved_task_traces(resolved, hierarchy.l1.geometry);
  BatchHierarchyResult result;
  result.coverage = mapped.coverage;
  for (auto & task : mapped.tasks)
  {
    auto l1 = detail::analyze_hierarchy_level(std::move(task.accesses),
                                              hierarchy.l1.geometry);

    std::vector<CacheLineMapping> llc_mappings;
    for (std::size_t index = 0; index < l1.accesses.size(); ++index)
    {
      if (l1.accesses[index].outcome == LruAccessOutcome::Hit) continue;
      auto remapped = l1.mappings[index];
      remapped.decoded =
        decode_cache_address(remapped.decoded.address, hierarchy.llc.geometry);
      llc_mappings.push_back(std::move(remapped));
    }
    auto llc = detail::analyze_hierarchy_level(std::move(llc_mappings),
                                               hierarchy.llc.geometry);
    result.tasks.push_back(detail::aggregate_hierarchy_task(
      std::move(task.task_id), task.coverage, std::move(l1), std::move(llc)));
  }
  return result;
}

}  // namespace yarda
