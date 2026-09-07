#include "yarda/cache/hierarchy_analysis.hpp"

#include <stdexcept>
#include <utility>

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
    BatchTaskHierarchyResult task_result;
    task_result.task_id = std::move(task.task_id);
    task_result.source_accesses = task.coverage.source_accesses;
    task_result.coverage = task.coverage;
    task_result.l1 = detail::analyze_hierarchy_level(std::move(task.accesses),
                                                     hierarchy.l1.geometry);
    task_result.modeled_accesses = task_result.l1.summary.lookups;

    std::vector<CacheLineMapping> llc_mappings;
    for (std::size_t index = 0; index < task_result.l1.accesses.size(); ++index)
    {
      if (task_result.l1.accesses[index].outcome == LruAccessOutcome::Hit)
        continue;
      auto remapped = task_result.l1.mappings[index];
      remapped.decoded =
        decode_cache_address(remapped.decoded.address, hierarchy.llc.geometry);
      llc_mappings.push_back(std::move(remapped));
    }
    task_result.llc = detail::analyze_hierarchy_level(std::move(llc_mappings),
                                                      hierarchy.llc.geometry);
    if (task_result.llc.summary.lookups != task_result.l1.summary.misses)
    {
      throw std::logic_error("hierarchy LLC input disagrees with L1 misses");
    }
    result.tasks.push_back(std::move(task_result));
  }
  return result;
}

}  // namespace yarda
