#include "hierarchy_json_fields.hpp"

#include "hierarchy_result_validation.hpp"

namespace yarda::detail
{
namespace
{

using Json = nlohmann::ordered_json;

Json level_summary_json(const CacheLevelSummary & level)
{
  Json::object_t histogram;
  histogram.reserve(level.csrd_histogram.size());
  // Source keys are unique and numerically sorted; skip linear duplicate scans.
  for (const auto & [distance, count] : level.csrd_histogram)
    histogram.emplace_back(std::to_string(distance), count);
  return {{"lookups", level.lookups},
          {"hits", level.hits},
          {"misses", level.misses},
          {"cold_misses", level.cold_misses},
          {"replacement_misses", level.replacement_misses},
          {"unique_lines", level.unique_lines},
          {"csrd_histogram", std::move(histogram)}};
}

Json coverage_json(const TraceCoverage & coverage)
{
  return {{"source_accesses", coverage.source_accesses},
          {"resolved_accesses", coverage.resolved_accesses},
          {"rejected_accesses", coverage.rejected_accesses},
          {"emitted_line_references", coverage.emitted_line_references},
          {"excluded_opaque_call_sites", std::uint64_t{0}},
          {"complete", coverage.complete()}};
}

Json invariant_json(const HierarchyInvariants & checks)
{
  return {{"level_conservation_l1", checks.level_conservation_l1},
          {"level_conservation_llc", checks.level_conservation_llc},
          {"llc_input_matches_l1_misses", checks.llc_input_matches_l1_misses},
          {"first_service_conservation", checks.first_service_conservation},
          {"all_passed", checks.all_passed}};
}

Json ratio_json(const std::optional<double> & ratio)
{
  return ratio ? Json(*ratio) : Json(nullptr);
}

} // namespace

nlohmann::ordered_json hierarchy_level_json(const AnalysisCacheLevel & level)
{
  return {{"name", level.name},
          {"role", level.role},
          {"size_bytes", hierarchy_level_capacity(level)},
          {"line_size_bytes", level.geometry.line_size},
          {"associativity", level.geometry.associativity},
          {"set_count", cache_set_count(level.geometry)},
          {"replacement", "LRU"}};
}

nlohmann::ordered_json hierarchy_task_json(const TaskHierarchySummary & task)
{
  return {{"task_id", task.task_id},
          {"source_accesses", task.source_accesses},
          {"modeled_accesses", task.modeled_accesses},
          {"l1_first_hit_count", task.l1_first_hit_count},
          {"llc_first_hit_count", task.llc_first_hit_count},
          {"all_cache_miss_count", task.all_cache_misses},
          {"l1_first_hit_ratio", ratio_json(task.l1_first_hit_ratio)},
          {"llc_first_hit_ratio", ratio_json(task.llc_first_hit_ratio)},
          {"all_cache_miss_ratio", ratio_json(task.all_cache_miss_ratio)},
          {"l1", level_summary_json(task.l1)},
          {"llc", level_summary_json(task.llc)},
          {"coverage", coverage_json(task.coverage)},
          {"invariants", invariant_json(task.invariants)}};
}

} // namespace yarda::detail
