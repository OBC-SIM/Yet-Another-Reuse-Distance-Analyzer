#include "hierarchy_service_summary.hpp"

#include <limits>
#include <stdexcept>

namespace yarda::detail
{

std::uint64_t checked_service_sum(std::uint64_t left, std::uint64_t right)
{
  if (right > std::numeric_limits<std::uint64_t>::max() - left)
    throw std::overflow_error("hierarchy service counter overflows uint64_t");
  return left + right;
}

TaskHierarchySummary finalize_hierarchy_service(TaskHierarchySummary summary)
{
  const auto l1_misses =
    checked_service_sum(summary.l1.cold_misses, summary.l1.replacement_misses);
  const auto llc_misses = checked_service_sum(summary.llc.cold_misses,
                                              summary.llc.replacement_misses);
  const auto l1_lookups =
    checked_service_sum(summary.l1.hits, summary.l1.misses);
  const auto llc_lookups =
    checked_service_sum(summary.llc.hits, summary.llc.misses);
  const auto llc_services =
    checked_service_sum(summary.llc_first_hit_count, summary.all_cache_misses);
  const auto services =
    checked_service_sum(summary.l1_first_hit_count, llc_services);

  auto & checks = summary.invariants;
  checks.level_conservation_l1 =
    l1_misses == summary.l1.misses && l1_lookups == summary.l1.lookups;
  checks.level_conservation_llc =
    llc_misses == summary.llc.misses && llc_lookups == summary.llc.lookups;
  checks.llc_input_matches_l1_misses = summary.llc.lookups == summary.l1.misses;
  checks.first_service_conservation =
    summary.modeled_accesses == summary.l1.lookups &&
    summary.l1_first_hit_count == summary.l1.hits &&
    summary.llc_first_hit_count == summary.llc.hits &&
    summary.all_cache_misses == summary.llc.misses &&
    services == summary.modeled_accesses && llc_services == summary.llc.lookups;
  checks.all_passed =
    checks.level_conservation_l1 && checks.level_conservation_llc &&
    checks.llc_input_matches_l1_misses && checks.first_service_conservation;
  if (!checks.all_passed)
    throw std::logic_error("hierarchy service invariants disagree for task: " +
                           summary.task_id);
  if (!summary.coverage.complete() ||
      summary.source_accesses != summary.coverage.source_accesses ||
      summary.modeled_accesses != summary.coverage.emitted_line_references)
    throw std::logic_error("hierarchy service coverage disagrees for task: " +
                           summary.task_id);

  summary.l1_first_hit_ratio.reset();
  summary.llc_first_hit_ratio.reset();
  summary.all_cache_miss_ratio.reset();
  if (summary.modeled_accesses != 0)
  {
    const auto denominator = static_cast<double>(summary.modeled_accesses);
    summary.l1_first_hit_ratio =
      static_cast<double>(summary.l1_first_hit_count) / denominator;
    summary.llc_first_hit_ratio =
      static_cast<double>(summary.llc_first_hit_count) / denominator;
    summary.all_cache_miss_ratio =
      static_cast<double>(summary.all_cache_misses) / denominator;
  }
  return summary;
}

}  // namespace yarda::detail
