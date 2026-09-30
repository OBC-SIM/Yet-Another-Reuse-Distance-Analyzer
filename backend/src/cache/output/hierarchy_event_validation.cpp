#include "hierarchy_event_validation.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_set>

#include "artifact_contract.hpp"

namespace yarda::detail
{
namespace
{

void validate_observation(const LruAccessResult & observation)
{
  if (observation.outcome == LruAccessOutcome::ColdMiss)
  {
    if (!observation.reuse_distance) return;
  }
  else if (observation.outcome == LruAccessOutcome::Hit ||
           observation.outcome == LruAccessOutcome::ReplacementMiss)
  {
    if (observation.reuse_distance) return;
  }
  throw std::invalid_argument("hierarchy event outcome and distance disagree");
}

bool same_source(const CacheLineMapping & left, const CacheLineMapping & right)
{
  return left.object_id == right.object_id &&
         left.source_object_byte_offset == right.source_object_byte_offset &&
         left.source_linked_byte_address == right.source_linked_byte_address &&
         left.source_access_size == right.source_access_size &&
         left.source_access_ordinal == right.source_access_ordinal &&
         left.operation == right.operation;
}

void validate_mapping(const CacheLineMapping & mapping)
{
  const auto base = mapping.source_linked_byte_address;
  const auto address = mapping.decoded.address;
  if (mapping.object_id.empty() ||
      mapping.address_basis != AddressBasis::Absolute ||
      (mapping.operation != AccessOperation::Load &&
       mapping.operation != AccessOperation::Store) ||
      mapping.source_access_size == 0 || address < base ||
      mapping.source_access_size - 1 >
          std::numeric_limits<std::uint64_t>::max() - base ||
      address - base >= mapping.source_access_size ||
      mapping.object_byte_offset < mapping.source_object_byte_offset ||
      mapping.object_byte_offset - mapping.source_object_byte_offset !=
          address - base ||
      (mapping.line_span_ordinal == 0 && address != base))
    throw std::invalid_argument(
        "hierarchy event has invalid source provenance");
}

void validate_event(const HierarchyAccessEvent & event)
{
  validate_mapping(event.l1_mapping);
  validate_observation(event.l1);
  auto expected = FirstServiceLevel::L1;
  if (event.l1.outcome == LruAccessOutcome::Hit)
  {
    if (event.llc || event.llc_mapping)
      throw std::invalid_argument(
          "hierarchy L1 hit must not contain LLC fields");
  }
  else
  {
    if (!event.llc || !event.llc_mapping)
      throw std::invalid_argument("hierarchy L1 miss requires LLC fields");
    validate_observation(*event.llc);
    validate_mapping(*event.llc_mapping);
    const auto & left = event.l1_mapping;
    const auto & right = *event.llc_mapping;
    if (!same_source(left, right) ||
        left.line_span_ordinal != right.line_span_ordinal ||
        left.object_byte_offset != right.object_byte_offset ||
        left.decoded.address != right.decoded.address ||
        left.decoded.block_number != right.decoded.block_number ||
        left.decoded.line_offset != right.decoded.line_offset)
      throw std::invalid_argument("hierarchy cache event provenance disagrees");
    expected = event.llc->outcome == LruAccessOutcome::Hit
                   ? FirstServiceLevel::LLC
                   : FirstServiceLevel::Memory;
  }
  if (event.first_service != expected)
    throw std::invalid_argument("hierarchy event first service disagrees");
}

void validate_next(const CacheLineMapping & previous,
                   const CacheLineMapping & current)
{
  if (current.source_access_ordinal == previous.source_access_ordinal)
  {
    if (same_source(previous, current) &&
        current.line_span_ordinal > previous.line_span_ordinal &&
        current.line_span_ordinal - previous.line_span_ordinal == 1 &&
        current.decoded.address > previous.decoded.address)
      return;
  }
  else if (current.source_access_ordinal > previous.source_access_ordinal &&
           current.source_access_ordinal - previous.source_access_ordinal ==
               1 &&
           current.line_span_ordinal == 0)
  {
    return;
  }
  throw std::invalid_argument("hierarchy events are not in source/span order");
}

} // namespace

void validate_hierarchy_events(const HierarchyEventMetadata & metadata,
                               const std::vector<HierarchyEventRecord> & events)
{
  require_sha256(metadata.analysis_id);
  const auto retained =
      std::min(metadata.event_limit, metadata.total_line_references);
  if (events.size() != retained ||
      metadata.delivery.emitted_events != retained ||
      metadata.delivery.events_truncated !=
          (metadata.total_line_references > metadata.event_limit))
    throw std::invalid_argument("hierarchy event delivery metadata disagrees");
  std::unordered_set<std::string> tasks;
  const HierarchyEventRecord * previous = nullptr;
  for (const auto & record : events)
  {
    validate_event(record.event);
    const auto & mapping = record.event.l1_mapping;
    if (!previous || record.task_id != previous->task_id)
    {
      if (record.task_id.empty() || !tasks.insert(record.task_id).second ||
          mapping.source_access_ordinal != 0 || mapping.line_span_ordinal != 0)
        throw std::invalid_argument("hierarchy events have invalid task order");
    }
    else
    {
      validate_next(previous->event.l1_mapping, mapping);
    }
    previous = &record;
  }
}

} // namespace yarda::detail
