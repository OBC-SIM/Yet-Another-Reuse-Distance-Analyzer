#include <limits>

#include "artifact_test_support.hpp"
#include "telemetry_test_support.hpp"
#include "cache/output/runtime_measurement.hpp"

namespace
{
using namespace yarda;
using namespace yarda::test::artifact;

TEST(AnalysisTelemetryTest, SerializesExactMeasuredSchema)
{
  const auto payload = hierarchy_telemetry_json(measured_telemetry());
  EXPECT_EQ(
      payload.dump(),
      std::string("{\"schema_version\":1,\"analysis_id\":\"") + kAnalysisId +
          "\",\"total_time_ns\":20,\"stage_time_ns\":{\"parse_lat\":1,\"parse_"
          "cache\":1,"
          "\"parse_elf\":1,\"resolve_and_stream\":1,\"hierarchy_analysis\":1,"
          "\"serialize_result\":1},\"peak_rss_bytes\":4096,\"source_accesses_"
          "emitted\":2,"
          "\"line_references_emitted\":3,\"maximum_inline_depth\":2,"
          "\"loop_iterations_expanded\":5,\"host\":{\"hostname\":\"test-host\","
          "\"os\":\"Linux\",\"release\":\"test\",\"machine\":\"x86_64\"},"
          "\"measured_at_utc\":\"2026-09-12T12:34:56Z\"}");
}

TEST(AnalysisTelemetryTest, CollectsInjectedClockRssHostAndExecutionCounts)
{
  FakeMeasurements fake;
  AnalysisTelemetryCollector collector(fake.manual_providers());
  fake.clock += 11; // Input preparation before the first named interval.
  advance_stage(collector, fake, AnalysisStage::ParseLat, 2);
  advance_stage(collector, fake, AnalysisStage::ParseCache, 3);
  advance_stage(collector, fake, AnalysisStage::ParseElf, 5);
  const auto stream_start = collector.now_ns();
  fake.clock += 4;
  advance_stage(collector, fake, AnalysisStage::HierarchyAnalysis, 7);
  fake.clock += 6;
  collector.finish_stage(AnalysisStage::ResolveAndStream, stream_start);
  advance_stage(collector, fake, AnalysisStage::SerializeResult, 11);
  const auto value = collector.snapshot(kAnalysisId, {2, 2, 0, 3}, {2, 5});
  EXPECT_EQ(value.total_time_ns, 49U);
  EXPECT_EQ(value.peak_rss_bytes, 4096U);
  EXPECT_EQ(value.source_accesses_emitted, 2U);
  EXPECT_EQ(value.line_references_emitted, 3U);
  EXPECT_EQ(value.execution.maximum_inline_depth, 2U);
  EXPECT_EQ(value.execution.loop_iterations_expanded, 5U);
  const AnalysisStageTimes expected{2, 3, 5, 17, 7, 11};
  EXPECT_EQ(value.stage_time_ns, expected);
  EXPECT_EQ(value.host.hostname, "test-host");
  EXPECT_EQ(value.measured_at_utc, "2026-09-12T12:34:56Z");
}

TEST(AnalysisTelemetryTest, AccumulatesRepeatedDisjointStageIntervals)
{
  FakeMeasurements fake;
  AnalysisTelemetryCollector collector(fake.manual_providers());
  record_external_stages(collector);
  const auto begin = collector.now_ns();
  fake.clock += 5;
  advance_stage(collector, fake, AnalysisStage::HierarchyAnalysis, 3);
  fake.clock += 2;
  advance_stage(collector, fake, AnalysisStage::HierarchyAnalysis, 7);
  fake.clock += 13;
  collector.finish_stage(AnalysisStage::ResolveAndStream, begin);
  const auto value = collector.snapshot(kAnalysisId, {}, {});
  EXPECT_EQ(value.stage_time_ns[4], 10U);
  EXPECT_EQ(value.stage_time_ns[3], 30U);
  EXPECT_EQ(value.total_time_ns, 30U);
}

TEST(AnalysisTelemetryTest, RejectsMissingMeasurementInsteadOfWritingZero)
{
  auto value = measured_telemetry();
  value.stage_time_ns[0].reset();
  EXPECT_THROW(hierarchy_telemetry_json(value), std::invalid_argument);
}

TEST(AnalysisTelemetryTest, AcceptsActuallyMeasuredZeroDurations)
{
  auto value = measured_telemetry();
  for (auto & stage : value.stage_time_ns)
    stage = 0;
  value.total_time_ns = 0;
  EXPECT_NO_THROW(hierarchy_telemetry_json(value));
}

TEST(AnalysisTelemetryTest, RejectsSubintervalOutsideStreamingTime)
{
  auto value = measured_telemetry();
  value.stage_time_ns[4] = 2;
  EXPECT_THROW(hierarchy_telemetry_json(value), std::invalid_argument);
}

TEST(AnalysisTelemetryTest, RejectsExclusiveStagesExceedingTotal)
{
  auto value = measured_telemetry();
  value.total_time_ns = 4;
  EXPECT_THROW(hierarchy_telemetry_json(value), std::invalid_argument);
}

TEST(AnalysisTelemetryTest, RejectsDurationSumOverflow)
{
  auto value = measured_telemetry();
  value.stage_time_ns[0] = std::numeric_limits<std::uint64_t>::max();
  EXPECT_THROW(hierarchy_telemetry_json(value), std::overflow_error);
}

TEST(AnalysisTelemetryTest, RejectsInvalidIdentityHostTimestampAndCounts)
{
  for (const auto mutate :
       std::vector<std::function<void(AnalysisTelemetry &)>>{
           [](auto & v) { v.analysis_id.clear(); },
           [](auto & v) { v.host.os.clear(); },
           [](auto & v) { v.measured_at_utc = "2026-09-12"; },
           [](auto & v) { v.measured_at_utc = "2026-02-30T00:00:00Z"; },
           [](auto & v) { v.source_accesses_emitted = 4; }})
  {
    auto value = measured_telemetry();
    mutate(value);
    EXPECT_THROW(hierarchy_telemetry_json(value), std::invalid_argument);
  }
}

TEST(AnalysisTelemetryTest, RejectsIncompleteProviders)
{
  EXPECT_THROW(AnalysisTelemetryCollector(AnalysisTelemetryProviders{}),
               std::invalid_argument);
}

TEST(AnalysisTelemetryTest, AcceptsLeapDayIn2024)
{
  auto value = measured_telemetry();
  value.measured_at_utc = "2024-02-29T00:00:00Z";
  EXPECT_NO_THROW(hierarchy_telemetry_json(value));
}

TEST(AnalysisTelemetryTest, RejectsLeapDayIn2025)
{
  auto value = measured_telemetry();
  value.measured_at_utc = "2025-02-29T00:00:00Z";
  EXPECT_THROW(hierarchy_telemetry_json(value), std::invalid_argument);
}

TEST(AnalysisTelemetryTest, RejectsLeapDayIn1900)
{
  auto value = measured_telemetry();
  value.measured_at_utc = "1900-02-29T00:00:00Z";
  EXPECT_THROW(hierarchy_telemetry_json(value), std::invalid_argument);
}

TEST(AnalysisTelemetryTest, AcceptsLeapDayIn2000)
{
  auto value = measured_telemetry();
  value.measured_at_utc = "2000-02-29T00:00:00Z";
  EXPECT_NO_THROW(hierarchy_telemetry_json(value));
}

TEST(AnalysisTelemetryTest, RejectsBackwardClockAndInvalidStage)
{
  FakeMeasurements fake;
  AnalysisTelemetryCollector collector(fake.providers());
  const auto start = collector.now_ns();
  fake.clock = start - 1;
  EXPECT_THROW(collector.finish_stage(AnalysisStage::ParseLat, start),
               std::invalid_argument);
  EXPECT_THROW(collector.finish_stage(AnalysisStage::Count, start),
               std::invalid_argument);
}

TEST(AnalysisTelemetryTest, RejectsIncompleteCoverageAtSnapshot)
{
  FakeMeasurements fake;
  AnalysisTelemetryCollector collector(fake.providers());
  EXPECT_THROW(collector.snapshot(kAnalysisId, {2, 1, 1, 3}, {}),
               std::invalid_argument);
}

TEST(AnalysisTelemetryTest, NormalizesLinuxRssKibibytesToBytes)
{
  EXPECT_EQ(detail::linux_peak_rss_bytes(0), 0U);
  EXPECT_EQ(detail::linux_peak_rss_bytes(123), 125952U);
}

TEST(AnalysisTelemetryTest, RejectsNegativeAndOverflowingLinuxRss)
{
  EXPECT_THROW(detail::linux_peak_rss_bytes(-1), std::invalid_argument);
  EXPECT_THROW(
      detail::linux_peak_rss_bytes(std::numeric_limits<std::int64_t>::max()),
      std::overflow_error);
}

} // namespace
