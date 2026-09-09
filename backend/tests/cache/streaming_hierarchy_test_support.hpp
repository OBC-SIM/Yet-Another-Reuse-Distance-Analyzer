#pragma once

#include <limits>

#include "../trace/task_access_stream_test_support.hpp"
#include "hierarchy_analysis_oracle_support.hpp"
#include "yarda/cache/streaming_hierarchy.hpp"

namespace yarda::test::streaming
{

namespace stream = ::yarda::test::stream;

using stream::access;
using stream::call;
using stream::function;
using stream::Json;
using stream::loop;
using support::make_batch_hierarchy;

/** @brief Build byte-array LAT fixtures with literal index/address identity. */
inline Json byte_module(Json functions)
{
  auto raw = stream::module(std::move(functions), 1);
  for (auto & object : raw["metadata"]["objects"])
    object["shape"] = Json::array({65536});
  return raw;
}

inline ObjectAddressModel byte_addresses()
{
  ObjectAddressModel objects;
  objects.objects = {{"global::A", {0, 65536}},
                     {"global::B", {65536, 65536}},
                     {"global::C", {131072, 65536}}};
  return objects;
}

inline Json byte_body(std::initializer_list<std::uint64_t> offsets,
                      const std::string & operation = "load")
{
  auto body = Json::array();
  for (const auto offset : offsets)
    body.push_back(access("global::A", std::to_string(offset), operation));
  return body;
}

inline Json byte_trace(std::initializer_list<std::uint64_t> offsets)
{
  return byte_module(Json::array({function("kernel", byte_body(offsets))}));
}

/** @brief Own diagnostics only for bounded test inputs. */
struct EventCollector
{
  std::vector<std::pair<std::string, HierarchyAccessEvent>> events;

  StreamingHierarchyOptions
  options(std::uint64_t limit = std::numeric_limits<std::uint64_t>::max(),
          TraceEmissionLimits limits = {})
  {
    return {limits,
            [this](const std::string & id, const HierarchyAccessEvent & event) {
              events.emplace_back(id, event);
            },
            limit};
  }
};

inline void expect_level(const CacheLevelSummary & actual,
                         const CacheLevelSummary & expected)
{
  EXPECT_EQ(actual.lookups, expected.lookups);
  EXPECT_EQ(actual.hits, expected.hits);
  EXPECT_EQ(actual.misses, expected.misses);
  EXPECT_EQ(actual.cold_misses, expected.cold_misses);
  EXPECT_EQ(actual.replacement_misses, expected.replacement_misses);
  EXPECT_EQ(actual.unique_lines, expected.unique_lines);
  EXPECT_EQ(actual.csrd_histogram, expected.csrd_histogram);
}

inline void expect_summary(const TaskHierarchySummary & actual,
                           const TaskHierarchySummary & expected)
{
  EXPECT_EQ(actual.task_id, expected.task_id);
  EXPECT_EQ(actual.source_accesses, expected.source_accesses);
  EXPECT_EQ(actual.modeled_accesses, expected.modeled_accesses);
  expect_level(actual.l1, expected.l1);
  expect_level(actual.llc, expected.llc);
  EXPECT_EQ(actual.ehc_l1, expected.ehc_l1);
  EXPECT_EQ(actual.ehc_llc, expected.ehc_llc);
  EXPECT_EQ(actual.all_cache_misses, expected.all_cache_misses);
  EXPECT_EQ(actual.hr_l1, expected.hr_l1);
  EXPECT_EQ(actual.hr_llc, expected.hr_llc);
  EXPECT_EQ(actual.miss_ratio, expected.miss_ratio);
  stream::expect_coverage(actual.coverage, expected.coverage);
  EXPECT_EQ(actual.invariants.level_conservation_l1,
            expected.invariants.level_conservation_l1);
  EXPECT_EQ(actual.invariants.level_conservation_llc,
            expected.invariants.level_conservation_llc);
  EXPECT_EQ(actual.invariants.llc_input_matches_l1_misses,
            expected.invariants.llc_input_matches_l1_misses);
  EXPECT_EQ(actual.invariants.first_service_conservation,
            expected.invariants.first_service_conservation);
  EXPECT_TRUE(actual.invariants.all_passed);
}

inline void expect_event(const HierarchyAccessEvent & actual,
                         const HierarchyAccessEvent & expected)
{
  stream::expect_mapping(actual.l1_mapping, expected.l1_mapping);
  EXPECT_EQ(actual.l1.outcome, expected.l1.outcome);
  EXPECT_EQ(actual.l1.reuse_distance, expected.l1.reuse_distance);
  ASSERT_EQ(actual.llc_mapping.has_value(), expected.llc_mapping.has_value());
  ASSERT_EQ(actual.llc.has_value(), expected.llc.has_value());
  if (expected.llc_mapping)
    stream::expect_mapping(*actual.llc_mapping, *expected.llc_mapping);
  if (expected.llc)
  {
    EXPECT_EQ(actual.llc->outcome, expected.llc->outcome);
    EXPECT_EQ(actual.llc->reuse_distance, expected.llc->reuse_distance);
  }
  EXPECT_EQ(actual.first_service, expected.first_service);
}

/** @brief Compare whole LAT-to-summary execution with batch and both oracles.
 */
inline void expect_stream_parity(const Json & raw,
                                 const ObjectAddressModel & objects,
                                 const AnalysisHierarchy & hierarchy)
{
  const auto resolved = resolved_task_traces(raw, objects);
  support::expect_batch_matches_oracles(resolved, hierarchy);
  const auto batch = analyze_batch_hierarchy(resolved, hierarchy);
  EventCollector collector;
  const auto actual =
    analyze_streaming_hierarchy(raw, objects, hierarchy, collector.options());
  const auto plain = analyze_streaming_hierarchy(raw, objects, hierarchy);
  stream::expect_coverage(actual.coverage, batch.coverage);
  stream::expect_coverage(plain.coverage, batch.coverage);
  ASSERT_EQ(actual.tasks.size(), batch.tasks.size());
  ASSERT_EQ(plain.tasks.size(), batch.tasks.size());
  std::size_t event_index = 0;
  for (std::size_t task = 0; task < batch.tasks.size(); ++task)
  {
    SCOPED_TRACE(batch.tasks[task].summary.task_id);
    expect_summary(actual.tasks[task], batch.tasks[task].summary);
    expect_summary(plain.tasks[task], batch.tasks[task].summary);
    for (const auto & expected : batch.tasks[task].events)
    {
      SCOPED_TRACE(event_index);
      ASSERT_LT(event_index, collector.events.size());
      const auto & [id, event] = collector.events[event_index++];
      EXPECT_EQ(id, batch.tasks[task].summary.task_id);
      expect_event(event, expected);
    }
  }
  EXPECT_EQ(event_index, collector.events.size());
  EXPECT_EQ(actual.event_delivery.emitted_events, event_index);
  EXPECT_FALSE(actual.event_delivery.events_truncated);
  EXPECT_EQ(plain.event_delivery.emitted_events, 0U);
  EXPECT_FALSE(plain.event_delivery.events_truncated);
}

}  // namespace yarda::test::streaming
