#pragma once

#include "artifact_test_support.hpp"

namespace yarda::test::artifact
{

struct CollectedEvents
{
  StreamingHierarchyResult result;
  HierarchyEventMetadata metadata;
  std::vector<HierarchyEventRecord> events;
};

inline CollectedEvents collect(std::uint64_t limit)
{
  CollectedEvents collected;
  StreamingHierarchyOptions options;
  options.event_limit = limit;
  options.event_sink = [&](const std::string & id,
                           const HierarchyAccessEvent & event) {
    collected.events.push_back({id, event});
  };
  const auto raw = streaming::byte_module(streaming::Json::array(
      {streaming::function("first", streaming::byte_body({0, 0, 64, 0})),
       streaming::function("empty", streaming::Json::array()),
       streaming::function("last", streaming::byte_body({0}))}));
  collected.result = analyze_streaming_hierarchy(
      raw, streaming::byte_addresses(), metadata().hierarchy, options);
  collected.metadata = {kAnalysisId, limit,
                        collected.result.coverage.emitted_line_references,
                        collected.result.event_delivery};
  return collected;
}

} // namespace yarda::test::artifact
