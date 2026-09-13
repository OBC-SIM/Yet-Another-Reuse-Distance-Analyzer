#include "yarda/cache/hierarchy_result_json.hpp"

#include "artifact_contract.hpp"
#include "hierarchy_event_validation.hpp"

namespace yarda
{
namespace
{

using Json = nlohmann::ordered_json;

const char * outcome_name(LruAccessOutcome outcome)
{
  if (outcome == LruAccessOutcome::Hit) return "hit";
  return outcome == LruAccessOutcome::ColdMiss ? "cold-miss"
                                               : "replacement-miss";
}

const char * service_name(FirstServiceLevel service)
{
  if (service == FirstServiceLevel::L1) return "L1";
  return service == FirstServiceLevel::LLC ? "LLC" : "Memory";
}

Json distance_json(const LruAccessResult & observation)
{
  return observation.reuse_distance ? Json(*observation.reuse_distance)
                                    : Json(nullptr);
}

Json event_json(const HierarchyEventRecord & record)
{
  const auto & event = record.event;
  const auto & mapping = event.l1_mapping;
  return {{"task_id", record.task_id},
          {"source_access_ordinal", mapping.source_access_ordinal},
          {"line_span_ordinal", mapping.line_span_ordinal},
          {"object_id", mapping.object_id},
          {"linked_address", mapping.decoded.address},
          {"access_size", mapping.source_access_size},
          {"operation",
           mapping.operation == AccessOperation::Load ? "load" : "store"},
          {"l1_set", mapping.decoded.set_index},
          {"l1_tag", mapping.decoded.tag},
          {"l1_csrd", distance_json(event.l1)},
          {"l1_outcome", outcome_name(event.l1.outcome)},
          {"llc_set", event.llc_mapping
                          ? Json(event.llc_mapping->decoded.set_index)
                          : Json(nullptr)},
          {"llc_tag", event.llc_mapping ? Json(event.llc_mapping->decoded.tag)
                                        : Json(nullptr)},
          {"llc_csrd", event.llc ? distance_json(*event.llc) : Json(nullptr)},
          {"llc_outcome",
           event.llc ? Json(outcome_name(event.llc->outcome)) : Json(nullptr)},
          {"first_service_level", service_name(event.first_service)}};
}

} // namespace

nlohmann::ordered_json
hierarchy_events_json(const HierarchyEventMetadata & metadata,
                      const std::vector<HierarchyEventRecord> & events)
{
  detail::validate_hierarchy_events(metadata, events);
  auto rows = Json::array();
  for (const auto & record : events)
    rows.push_back(event_json(record));
  return {{"schema_version", detail::kArtifactSchemaVersion},
          {"analysis_id", metadata.analysis_id},
          {"event_limit", metadata.event_limit},
          {"events_truncated", metadata.delivery.events_truncated},
          {"events", std::move(rows)}};
}

} // namespace yarda
