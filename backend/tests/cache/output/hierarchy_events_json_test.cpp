#include <algorithm>
#include <functional>

#include "hierarchy_event_test_support.hpp"

namespace
{
using namespace yarda;
using namespace yarda::test::artifact;
namespace stream = yarda::test::streaming;

class HierarchyEventsBoundaryTest
    : public ::testing::TestWithParam<std::uint64_t>
{
};

TEST_P(HierarchyEventsBoundaryTest, SerializesExactModulePrefixAndStableResult)
{
  const auto actual = collect(GetParam());
  const auto baseline = collect(5);
  const auto payload = hierarchy_events_json(actual.metadata, actual.events);
  EXPECT_EQ(payload["events"].size(), std::min<std::uint64_t>(GetParam(), 5));
  EXPECT_EQ(payload["events_truncated"], GetParam() < 5);
  EXPECT_EQ(payload["event_limit"], GetParam());
  EXPECT_EQ(payload["analysis_id"], kAnalysisId);
  EXPECT_EQ(hierarchy_result_json(metadata(), actual.result).dump(),
            hierarchy_result_json(metadata(), baseline.result).dump());
  const auto again = collect(GetParam());
  EXPECT_EQ(payload.dump(),
            hierarchy_events_json(again.metadata, again.events).dump());
}

INSTANTIATE_TEST_SUITE_P(Limits, HierarchyEventsBoundaryTest,
                         ::testing::Values(0U, 1U, 4U, 5U, 6U));

TEST(HierarchyEventsJsonTest, SerializesColdEventWithEveryRequiredField)
{
  const auto data = collect(5);
  const auto row =
      hierarchy_events_json(data.metadata, data.events)["events"][0];
  EXPECT_EQ(
      row.dump(),
      "{\"task_id\":\"first\",\"source_access_ordinal\":0,\"line_span_"
      "ordinal\":0,"
      "\"object_id\":\"global::A\",\"linked_address\":0,\"access_size\":1,"
      "\"operation\":\"load\",\"l1_set\":0,\"l1_tag\":0,\"l1_csrd\":null,"
      "\"l1_outcome\":\"cold-miss\",\"llc_set\":0,\"llc_tag\":0,\"llc_csrd\":"
      "null,"
      "\"llc_outcome\":\"cold-miss\",\"first_service_level\":\"Memory\"}");
}

TEST(HierarchyEventsJsonTest, L1HitKeepsAllLlcFieldsAsExplicitNull)
{
  const auto data = collect(5);
  const auto row =
      hierarchy_events_json(data.metadata, data.events)["events"][1];
  EXPECT_EQ(row["l1_csrd"], 0);
  EXPECT_EQ(row["l1_outcome"], "hit");
  EXPECT_EQ(row["first_service_level"], "L1");
  for (const auto * field : {"llc_set", "llc_tag", "llc_csrd", "llc_outcome"})
    EXPECT_TRUE(row.at(field).is_null());
}

TEST(HierarchyEventsJsonTest, SerializesReplacementAndLlcFirstService)
{
  const auto data = collect(5);
  const auto rows = hierarchy_events_json(data.metadata, data.events)["events"];
  EXPECT_EQ(rows[3]["l1_outcome"], "replacement-miss");
  EXPECT_EQ(rows[3]["l1_csrd"], 1);
  EXPECT_EQ(rows[3]["llc_outcome"], "hit");
  EXPECT_EQ(rows[3]["first_service_level"], "LLC");
  EXPECT_EQ(rows[4]["task_id"], "last");
  EXPECT_EQ(rows[4]["source_access_ordinal"], 0);
  EXPECT_EQ(rows[4]["first_service_level"], "Memory");
}

TEST(HierarchyEventsJsonTest, CrossLineStorePreservesSourceSizeAndRowAddress)
{
  const auto raw = stream::stream::module(
      stream::Json::array({stream::function(
          "wide", {stream::access("global::A", "0", "store")})}),
      8);
  std::vector<HierarchyEventRecord> events;
  StreamingHierarchyOptions options;
  options.event_limit = 2;
  options.event_sink = [&](const auto & id, const auto & event) {
    events.push_back({id, event});
  };
  const auto value = analyze_streaming_hierarchy(
      raw, stream::stream::addresses(8), metadata().hierarchy, options);
  const auto rows = hierarchy_events_json(
      {kAnalysisId, 2, 2, value.event_delivery}, events)["events"];
  ASSERT_EQ(rows.size(), 2U);
  EXPECT_EQ(rows[0]["linked_address"], 0x101c);
  EXPECT_EQ(rows[1]["linked_address"], 0x1020);
  EXPECT_EQ(rows[1]["access_size"], 8);
  EXPECT_EQ(rows[1]["line_span_ordinal"], 1);
  EXPECT_EQ(rows[1]["operation"], "store");
}

TEST(HierarchyEventsJsonTest, EmptyEnabledStreamIsNotTruncatedAtZeroLimit)
{
  const auto payload = hierarchy_events_json({kAnalysisId, 0, 0, {}}, {});
  EXPECT_TRUE(payload["events"].empty());
  EXPECT_EQ(payload["events_truncated"], false);
}

TEST(HierarchyEventsJsonTest, StartsNewTaskAtZeroAfterCrossLinePrefix)
{
  const auto raw = stream::stream::module(
      stream::Json::array({
          stream::function("wide", {stream::access("global::A", "0", "store")}),
          stream::function("next", {stream::access("global::B", "0"),
                                     stream::access("global::B", "1")})}),
      8);
  std::vector<HierarchyEventRecord> events;
  StreamingHierarchyOptions options;
  options.event_limit = 3;
  options.event_sink = [&](const auto & id, const auto & event) {
    events.push_back({id, event});
  };
  const auto value = analyze_streaming_hierarchy(
      raw, stream::stream::addresses(8), metadata().hierarchy, options);
  ASSERT_EQ(value.coverage.emitted_line_references, 4U);
  const auto payload = hierarchy_events_json(
      {kAnalysisId, 3, 4, value.event_delivery}, events);
  EXPECT_EQ(payload["events_truncated"], true);
  const auto & rows = payload["events"];
  ASSERT_EQ(rows.size(), 3U);
  EXPECT_EQ(rows[1]["task_id"], "wide");
  EXPECT_EQ(rows[1]["line_span_ordinal"], 1);
  EXPECT_EQ(rows[2]["task_id"], "next");
  EXPECT_EQ(rows[2]["source_access_ordinal"], 0);
  EXPECT_EQ(rows[2]["line_span_ordinal"], 0);
  EXPECT_EQ(rows[2]["linked_address"], 0x2000);
  EXPECT_EQ(rows[2]["first_service_level"], "Memory");
}

TEST(HierarchyEventsJsonTest, RejectsExtraEventsRetainedFromFailedRun)
{
  std::vector<HierarchyEventRecord> events;
  StreamingHierarchyOptions options;
  options.event_limit = 2;
  options.event_sink = [&](const auto & id, const auto & event) {
    events.push_back({id, event});
  };
  options.emission_limits.emitted_source_accesses = 1;
  const auto failing = stream::byte_module(stream::Json::array({
      stream::function("failed", stream::byte_body({0, 32}))}));
  EXPECT_THROW(analyze_streaming_hierarchy(failing, stream::byte_addresses(),
                                           metadata().hierarchy, options),
               std::invalid_argument);
  ASSERT_EQ(events.size(), 1U);
  EXPECT_EQ(events[0].task_id, "failed");
  options.emission_limits = {};
  const auto succeeding = stream::byte_module(stream::Json::array({
      stream::function("succeeded", stream::byte_body({0}))}));
  const auto value = analyze_streaming_hierarchy(
      succeeding, stream::byte_addresses(), metadata().hierarchy, options);
  ASSERT_EQ(events.size(), 2U);
  EXPECT_EQ(events[1].task_id, "succeeded");
  const HierarchyEventMetadata info{
      kAnalysisId, 2, value.coverage.emitted_line_references, value.event_delivery};
  EXPECT_THROW(hierarchy_events_json(info, events), std::invalid_argument);
}

TEST(HierarchyEventsJsonTest, RejectsInconsistentDeliveryMetadata)
{
  for (const auto mutate :
       std::vector<std::function<void(HierarchyEventMetadata &)>>{
           [](auto & m) { m.analysis_id.clear(); },
           [](auto & m) { --m.delivery.emitted_events; },
           [](auto & m) { m.delivery.events_truncated = true; },
           [](auto & m) { m.event_limit = 1; },
           [](auto & m) { ++m.total_line_references; }})
  {
    auto data = collect(5);
    mutate(data.metadata);
    EXPECT_THROW(hierarchy_events_json(data.metadata, data.events),
                 std::invalid_argument);
  }
}

TEST(HierarchyEventsJsonTest, RejectsInconsistentCacheObservations)
{
  for (const auto mutate :
       std::vector<std::function<void(HierarchyAccessEvent &)>>{
           [](auto & e) { e.l1.reuse_distance = 0; },
           [](auto & e) { e.llc.reset(); },
           [](auto & e) { e.llc_mapping.reset(); },
           [](auto & e) { e.first_service = FirstServiceLevel::L1; },
           [](auto & e) { e.llc_mapping->decoded.address += 1; },
           [](auto & e) { e.l1_mapping.operation = AccessOperation::Unknown; },
           [](auto & e) { e.l1_mapping.source_access_size = 0; },
           [](auto & e)
           { e.l1_mapping.address_basis = AddressBasis::ImageRelative; }})
  {
    auto data = collect(1);
    mutate(data.events[0].event);
    EXPECT_THROW(hierarchy_events_json(data.metadata, data.events),
                 std::invalid_argument);
  }
}

TEST(HierarchyEventsJsonTest, RejectsBrokenTaskSourceSpanOrder)
{
  for (const auto mutate :
       std::vector<std::function<void(std::vector<HierarchyEventRecord> &)>>{
           [](auto & e) { e[0].task_id.clear(); },
           [](auto & e) { e[1].event.l1_mapping.source_access_ordinal = 3; },
           [](auto & e) { e[1].event.l1_mapping.line_span_ordinal = 2; },
           [](auto & e) { e[1].task_id = "other"; },
           [](auto & e) { e.back().task_id = "first"; }})
  {
    auto data = collect(5);
    mutate(data.events);
    EXPECT_THROW(hierarchy_events_json(data.metadata, data.events),
                 std::invalid_argument);
  }
}

} // namespace
