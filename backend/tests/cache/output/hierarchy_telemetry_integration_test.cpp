#include "artifact_test_support.hpp"
#include "telemetry_test_support.hpp"

namespace
{
using namespace yarda;
using namespace yarda::test::artifact;
namespace stream = yarda::test::streaming;

TEST(HierarchyTelemetryIntegrationTest, MeasuresNestedConsumersWithTickingClock)
{
  FakeMeasurements fake;
  AnalysisTelemetryCollector collector(fake.providers());
  record_external_stages(collector);
  StreamingHierarchyOptions options;
  options.telemetry = &collector;
  const auto raw = stream::byte_module(stream::Json::array({stream::function(
      "kernel", {stream::loop(2, {stream::loop(3, {stream::access()})})})}));
  const auto value = analyze_streaming_hierarchy(raw, stream::byte_addresses(),
                                                 metadata().hierarchy, options);
  const auto telemetry = collector.snapshot(kAnalysisId, value.coverage,
                                            value.execution_statistics);
  ASSERT_TRUE(telemetry.stage_time_ns[3]);
  ASSERT_TRUE(telemetry.stage_time_ns[4]);
  EXPECT_EQ(*telemetry.stage_time_ns[4], 8U);
  EXPECT_EQ(*telemetry.stage_time_ns[3], 17U);
  EXPECT_EQ(value.execution_statistics.loop_iterations_expanded, 8U);
  EXPECT_EQ(telemetry.source_accesses_emitted, 6U);
  const auto plain = analyze_streaming_hierarchy(raw, stream::byte_addresses(),
                                                 metadata().hierarchy);
  EXPECT_EQ(hierarchy_result_json(metadata(), value).dump(),
            hierarchy_result_json(metadata(), plain).dump());
}

TEST(HierarchyTelemetryIntegrationTest, AccumulatesKnownConsumerTimeWithoutTicks)
{
  FakeMeasurements fake;
  AnalysisTelemetryCollector collector(fake.manual_providers());
  record_external_stages(collector);
  fake.clock += 4;
  StreamingHierarchyOptions options;
  options.telemetry = &collector;
  options.event_limit = 2;
  options.event_sink = [&](const auto &, const auto &) { fake.clock += 17; };
  const auto value = analyze_streaming_hierarchy(stream::byte_trace({0, 32}),
                                                 stream::byte_addresses(),
                                                 metadata().hierarchy, options);
  const auto telemetry = collector.snapshot(kAnalysisId, value.coverage,
                                            value.execution_statistics);
  EXPECT_EQ(value.event_delivery.emitted_events, 2U);
  EXPECT_EQ(telemetry.stage_time_ns[4], 34U);
  EXPECT_EQ(telemetry.stage_time_ns[3], 34U);
  EXPECT_EQ(telemetry.total_time_ns, 38U);
  EXPECT_NO_THROW(hierarchy_telemetry_json(telemetry));
}

TEST(HierarchyTelemetryIntegrationTest,
     FailedAnalysisCannotProduceCompleteTelemetry)
{
  FakeMeasurements fake;
  AnalysisTelemetryCollector collector(fake.providers());
  record_external_stages(collector);
  StreamingHierarchyOptions options;
  options.telemetry = &collector;
  options.emission_limits.emitted_source_accesses = 1;
  EXPECT_THROW(analyze_streaming_hierarchy(stream::byte_trace({0, 0}),
                                           stream::byte_addresses(),
                                           metadata().hierarchy, options),
               std::invalid_argument);
  EXPECT_THROW(collector.snapshot(kAnalysisId, {1, 1, 0, 1}, {}),
               std::invalid_argument);
}

TEST(HierarchyTelemetryIntegrationTest, EmptyTaskStillMeasuresConsumers)
{
  FakeMeasurements fake;
  AnalysisTelemetryCollector collector(fake.providers());
  record_external_stages(collector);
  StreamingHierarchyOptions options;
  options.telemetry = &collector;
  const auto value = analyze_streaming_hierarchy(stream::byte_trace({}),
                                                 stream::byte_addresses(),
                                                 metadata().hierarchy, options);
  const auto telemetry = collector.snapshot(kAnalysisId, value.coverage,
                                            value.execution_statistics);
  EXPECT_EQ(telemetry.stage_time_ns[4], 2U);
  EXPECT_EQ(telemetry.execution.loop_iterations_expanded, 0U);
}

class HierarchyTelemetryInlineDepthTest : public ::testing::TestWithParam<int>
{
};

TEST_P(HierarchyTelemetryInlineDepthTest, PropagatesStructuralDepthToJson)
{
  const auto trips = GetParam();
  const auto raw = stream::byte_module(stream::Json::array({
      stream::function("root", {stream::loop(trips, {stream::call("helper")})}),
      stream::function("helper", {stream::call("leaf")}, "ape.inline"),
      stream::function("leaf", {stream::access()}, "ape.inline")}));
  FakeMeasurements fake;
  AnalysisTelemetryCollector collector(fake.manual_providers());
  record_external_stages(collector);
  StreamingHierarchyOptions options;
  options.telemetry = &collector;
  const auto value = analyze_streaming_hierarchy(raw, stream::byte_addresses(),
                                                 metadata().hierarchy, options);
  const auto telemetry = collector.snapshot(kAnalysisId, value.coverage,
                                            value.execution_statistics);
  EXPECT_TRUE(value.coverage.complete());
  EXPECT_EQ(value.coverage.source_accesses, trips);
  EXPECT_EQ(value.execution_statistics.maximum_inline_depth, 2U);
  EXPECT_EQ(telemetry.execution.maximum_inline_depth, 2U);
  EXPECT_EQ(hierarchy_telemetry_json(telemetry)["maximum_inline_depth"], 2);
  EXPECT_EQ(telemetry.execution.loop_iterations_expanded, trips);
}

INSTANTIATE_TEST_SUITE_P(ZeroAndNonzeroLoops, HierarchyTelemetryInlineDepthTest,
                         ::testing::Values(0, 3));

} // namespace
