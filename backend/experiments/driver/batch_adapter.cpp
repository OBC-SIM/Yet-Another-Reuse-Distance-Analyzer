#include "evaluation.hpp"

#include "yarda/trace/resolved_mapping.hpp"
#include "yarda/trace/task_access_stream.hpp"

namespace yarda::evaluation
{

BatchHierarchyResult run_batch(const nlohmann::json & raw,
  const ObjectAddressModel & objects, const AnalysisHierarchy & hierarchy,
  const StreamingHierarchyOptions & options)
{
  ResolvedTaskTraceResult resolved;
  TraceEmissionBudget budget(options.emission_limits);
  const CacheLineSink discard_line = [](const CacheLineMapping &) {};
  const TaskAccessSink sink{
    [&](const std::string & id, std::uint64_t excluded) {
      if (excluded != 0)
        throw std::invalid_argument("hierarchy task has opaque-call exclusions: " + id);
      resolved.tasks.push_back({id, {}, {}, excluded});
    },
    [&](const std::string &, const ResolvedAccess & access) {
      for_each_cache_line(access, hierarchy.l1.geometry, discard_line, budget);
      resolved.tasks.back().accesses.push_back(access);
    },
    [&](const std::string &, const TraceCoverage & coverage) {
      resolved.tasks.back().coverage = coverage;
    }};
  const auto source = stream_resolved_task_accesses(raw, objects, sink, budget,
                                                    options.loop_limits);
  resolved.coverage = source.coverage;
  resolved.excluded_opaque_call_sites = source.excluded_opaque_call_sites;
  return analyze_batch_hierarchy(resolved, hierarchy);
}

StreamingHierarchyResult batch_summary(const BatchHierarchyResult & batch)
{
  StreamingHierarchyResult result;
  result.coverage = batch.coverage;
  for (const auto & task : batch.tasks) result.tasks.push_back(task.summary);
  return result;
}

} // namespace yarda::evaluation
