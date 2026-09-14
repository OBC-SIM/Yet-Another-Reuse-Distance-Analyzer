#include "yarda/cache/streaming_hierarchy.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

#include "streaming_hierarchy_summary.hpp"
#include "streaming_hierarchy_task.hpp"
#include "yarda/cache/analysis_telemetry.hpp"
#include "yarda/trace/task_access_stream.hpp"

namespace yarda
{

StreamingHierarchyResult analyze_streaming_hierarchy(
  const nlohmann::json & raw, const ObjectAddressModel & objects,
  const AnalysisHierarchy & hierarchy,
  const StreamingHierarchyOptions & options)
{
  static_cast<void>(cache_set_count(hierarchy.l1.geometry));
  static_cast<void>(cache_set_count(hierarchy.llc.geometry));
  if (hierarchy.l1.geometry.line_size != hierarchy.llc.geometry.line_size)
    throw std::invalid_argument(
      "hierarchy analysis requires matching line sizes");
  if (!options.event_sink && options.event_limit != 0)
    throw std::invalid_argument("hierarchy event limit requires an event sink");

  const auto stream_start = options.telemetry ? options.telemetry->now_ns() : 0;
  TraceEmissionBudget budget(options.emission_limits);
  StreamingHierarchyResult result;
  std::unique_ptr<detail::StreamingHierarchyTask> active;
  const TaskAccessSink sink{
    [&](const std::string & id, std::uint64_t excluded) {
      const auto start = options.telemetry ? options.telemetry->now_ns() : 0;
      if (excluded != 0)
        throw std::invalid_argument(
          "hierarchy task has opaque-call exclusions: " + id);
      active = std::make_unique<detail::StreamingHierarchyTask>(
        id, hierarchy, budget, options, result.event_delivery);
      if (options.telemetry)
        options.telemetry->finish_stage(AnalysisStage::HierarchyAnalysis, start);
    },
    [&](const std::string &, const ResolvedAccess & access) {
      const auto start = options.telemetry ? options.telemetry->now_ns() : 0;
      active->accept(access);
      if (options.telemetry)
        options.telemetry->finish_stage(AnalysisStage::HierarchyAnalysis, start);
    },
    [&](const std::string &, const TraceCoverage & coverage) {
      const auto start = options.telemetry ? options.telemetry->now_ns() : 0;
      auto task = active->finish(coverage);
      detail::add_streaming_coverage(result.coverage, task.coverage);
      result.tasks.push_back(std::move(task));
      active.reset();
      if (options.telemetry)
        options.telemetry->finish_stage(AnalysisStage::HierarchyAnalysis, start);
    },
  };
  const auto source = stream_resolved_task_accesses(raw, objects, sink, budget,
                                                    options.loop_limits);
  if (!source.coverage.complete() ||
      source.coverage.emitted_line_references != 0 ||
      source.coverage.source_accesses != result.coverage.source_accesses ||
      source.coverage.resolved_accesses != result.coverage.resolved_accesses ||
      source.coverage.rejected_accesses != result.coverage.rejected_accesses)
    throw std::logic_error(
      "hierarchy module coverage disagrees with source stream");
  result.execution_statistics = source.execution_statistics;
  if (options.telemetry)
    options.telemetry->finish_stage(AnalysisStage::ResolveAndStream, stream_start);
  return result;
}

}  // namespace yarda
