#include "streaming_hierarchy_task.hpp"

#include <stdexcept>
#include <utility>

#include "hierarchy_service_summary.hpp"
#include "streaming_hierarchy_summary.hpp"
#include "yarda/trace/resolved_mapping.hpp"

namespace yarda::detail
{

StreamingHierarchyTask::StreamingHierarchyTask(
  std::string id, const AnalysisHierarchy & hierarchy,
  TraceEmissionBudget & budget, const StreamingHierarchyOptions & options,
  HierarchyEventDelivery & delivery)
  : hierarchy_(hierarchy)
  , budget_(budget)
  , options_(options)
  , delivery_(delivery)
  , l1_(hierarchy.l1.geometry)
  , llc_(hierarchy.llc.geometry)
  , line_sink_(
      [this](const CacheLineMapping & mapping) { observe_line(mapping); })
{
  summary_.task_id = std::move(id);
}

void StreamingHierarchyTask::accept(const ResolvedAccess & access)
{
  if (access.address_basis != AddressBasis::Absolute)
    throw std::invalid_argument("hierarchy task requires absolute addresses: " +
                                summary_.task_id);
  if (access.operation != AccessOperation::Load &&
      access.operation != AccessOperation::Store)
    throw std::invalid_argument(
      "hierarchy task requires load/store operations: " + summary_.task_id);
  if (access.source_access_ordinal != summary_.source_accesses)
    throw std::invalid_argument(
      "hierarchy task requires consecutive source ordinals: " +
      summary_.task_id);
  summary_.source_accesses = checked_service_sum(summary_.source_accesses, 1);
  next_span_ordinal_ = 0;
  for_each_cache_line(access, hierarchy_.l1.geometry, line_sink_, budget_);
}

void StreamingHierarchyTask::observe_line(const CacheLineMapping & mapping)
{
  if (mapping.source_access_ordinal != summary_.source_accesses - 1 ||
      mapping.line_span_ordinal != next_span_ordinal_)
    throw std::logic_error("hierarchy line provenance disagrees for task: " +
                           summary_.task_id);
  next_span_ordinal_ = checked_service_sum(next_span_ordinal_, 1);

  const auto l1 = l1_.observe(mapping.decoded);
  std::optional<DecodedCacheAddress> llc_address;
  std::optional<CsrdObservation> llc;
  auto service = FirstServiceLevel::L1;
  if (l1.outcome == LruAccessOutcome::Hit)
  {
    summary_.ehc_l1 = checked_service_sum(summary_.ehc_l1, 1);
  }
  else
  {
    llc_address =
      decode_cache_address(mapping.decoded.address, hierarchy_.llc.geometry);
    llc = llc_.observe(*llc_address);
    if (llc->outcome == LruAccessOutcome::Hit)
    {
      service = FirstServiceLevel::LLC;
      summary_.ehc_llc = checked_service_sum(summary_.ehc_llc, 1);
    }
    else
    {
      service = FirstServiceLevel::Memory;
      summary_.all_cache_misses =
        checked_service_sum(summary_.all_cache_misses, 1);
    }
  }
  summary_.modeled_accesses = checked_service_sum(summary_.modeled_accesses, 1);

  if (!options_.event_sink) return;
  if (delivery_.emitted_events == options_.event_limit)
  {
    delivery_.events_truncated = true;
    return;
  }
  HierarchyAccessEvent event;
  event.l1_mapping = mapping;
  event.l1 = streaming_lru_result(l1);
  event.first_service = service;
  if (llc)
  {
    event.llc_mapping = mapping;
    event.llc_mapping->decoded = *llc_address;
    event.llc = streaming_lru_result(*llc);
  }
  options_.event_sink(summary_.task_id, event);
  delivery_.emitted_events = checked_service_sum(delivery_.emitted_events, 1);
}

TaskHierarchySummary
StreamingHierarchyTask::finish(const TraceCoverage & coverage)
{
  if (!coverage.complete() ||
      coverage.source_accesses != summary_.source_accesses ||
      coverage.emitted_line_references != 0)
    throw std::logic_error(
      "hierarchy task coverage disagrees with source stream: " +
      summary_.task_id);
  summary_.coverage = coverage;
  summary_.coverage.emitted_line_references = summary_.modeled_accesses;
  summary_.l1 = summarize_streaming_level(l1_.summary());
  summary_.llc = summarize_streaming_level(llc_.summary());
  return finalize_hierarchy_service(std::move(summary_));
}

}  // namespace yarda::detail
