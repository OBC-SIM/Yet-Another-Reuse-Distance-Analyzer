#include "evaluation.hpp"

#include <tuple>

namespace yarda::evaluation
{
namespace
{
auto mapping_key(const CacheLineMapping & m)
{
  return std::tie(m.object_id, m.object_byte_offset, m.address_basis,
    m.decoded.address, m.decoded.block_number, m.decoded.tag,
    m.decoded.set_index, m.decoded.line_offset, m.source_object_byte_offset,
    m.source_access_size, m.source_linked_byte_address, m.operation,
    m.source_access_ordinal, m.line_span_ordinal);
}

bool same_event(const HierarchyAccessEvent & a, const HierarchyAccessEvent & b)
{
  if (mapping_key(a.l1_mapping) != mapping_key(b.l1_mapping) ||
      a.l1.outcome != b.l1.outcome ||
      a.l1.reuse_distance != b.l1.reuse_distance ||
      a.first_service != b.first_service ||
      a.llc_mapping.has_value() != b.llc_mapping.has_value() ||
      a.llc.has_value() != b.llc.has_value()) return false;
  return (!a.llc_mapping || mapping_key(*a.llc_mapping) == mapping_key(*b.llc_mapping)) &&
    (!a.llc || (a.llc->outcome == b.llc->outcome &&
                a.llc->reuse_distance == b.llc->reuse_distance));
}
} // namespace

void verify_paths(const Inputs & inputs,
                  const StreamingHierarchyOptions & options)
{
  const auto & hierarchy = inputs.metadata.hierarchy;
  const auto batch = run_batch(inputs.raw, inputs.objects, hierarchy, options);
  auto checked = options;
  std::size_t task = 0, ordinal = 0;
  std::uint64_t seen = 0;
  checked.event_limit = std::numeric_limits<std::uint64_t>::max();
  checked.event_sink = [&](const std::string & id, const HierarchyAccessEvent & event) {
    while (task < batch.tasks.size() && ordinal == batch.tasks[task].events.size())
    {
      ++task;
      ordinal = 0;
    }
    if (task == batch.tasks.size() || id != batch.tasks[task].summary.task_id ||
        !same_event(event, batch.tasks[task].events[ordinal]))
      throw std::logic_error("batch/streaming event mismatch at " + std::to_string(seen));
    ++ordinal;
    ++seen;
  };
  const auto streaming = analyze_streaming_hierarchy(inputs.raw, inputs.objects,
                                                      hierarchy, checked);
  if (seen != batch.coverage.emitted_line_references ||
      hierarchy_result_json(inputs.metadata, streaming).dump() !=
      hierarchy_result_json(inputs.metadata, batch_summary(batch)).dump())
    throw std::logic_error("batch/streaming RESULT mismatch");
}

} // namespace yarda::evaluation
