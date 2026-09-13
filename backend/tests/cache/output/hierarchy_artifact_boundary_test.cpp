#include "artifact_test_support.hpp"
#include "telemetry_test_support.hpp"

namespace
{
using namespace yarda;
using namespace yarda::test::artifact;
namespace stream = yarda::test::streaming;

TEST(HierarchyArtifactBoundaryTest, RejectsDistanceAtOrBeyondDistinctLineCount)
{
  auto value = histogram_result();
  value.tasks[0].l1.csrd_histogram.erase(10);
  value.tasks[0].l1.csrd_histogram[11] = 3;
  EXPECT_THROW(hierarchy_result_json(metadata(), value), std::invalid_argument);
}

TEST(HierarchyArtifactBoundaryTest, RejectsMoreLlcDistinctLinesThanL1)
{
  auto value = histogram_result();
  auto & task = value.tasks[0];
  task.llc = {16, 4, 12, 12, 0, 12, {{0, 4}}};
  task.ehc_llc = 4;
  task.all_cache_misses = 12;
  task.hr_llc = 4.0 / 17.0;
  task.miss_ratio = 12.0 / 17.0;
  EXPECT_THROW(hierarchy_result_json(metadata(), value), std::invalid_argument);
}

TEST(HierarchyArtifactBoundaryTest, RejectsNegativeZeroRatioEncoding)
{
  auto value = result();
  value.tasks[0].hr_llc = -0.0;
  EXPECT_THROW(hierarchy_result_json(metadata(), value), std::invalid_argument);
}

TEST(HierarchyArtifactBoundaryTest, CombinedDiagnosticsPreserveResultBytes)
{
  const auto raw = stream::byte_trace({0, 0, 64, 0});
  const auto expected =
      hierarchy_result_json(
          metadata(), analyze_streaming_hierarchy(raw, stream::byte_addresses(),
                                                  metadata().hierarchy))
          .dump();
  for (const auto limit : {0U, 1U, 4U})
  {
    FakeMeasurements fake;
    AnalysisTelemetryCollector collector(fake.providers());
    record_external_stages(collector);
    std::vector<HierarchyEventRecord> events;
    StreamingHierarchyOptions options;
    options.telemetry = &collector;
    options.event_limit = limit;
    options.event_sink = [&](const auto & id, const auto & event) {
      events.push_back({id, event});
    };
    const auto value = analyze_streaming_hierarchy(
        raw, stream::byte_addresses(), metadata().hierarchy, options);
    EXPECT_EQ(hierarchy_result_json(metadata(), value).dump(), expected);
    const auto telemetry = collector.snapshot(kAnalysisId, value.coverage,
                                              value.execution_statistics);
    const auto event_json = hierarchy_events_json(
        {kAnalysisId, limit, value.coverage.emitted_line_references,
         value.event_delivery},
        events);
    EXPECT_EQ(event_json["analysis_id"],
              hierarchy_telemetry_json(telemetry)["analysis_id"]);
  }
}

TEST(HierarchyArtifactBoundaryTest, RuntimeProvidersProduceCompleteTelemetry)
{
  AnalysisTelemetryCollector collector;
  for (const auto stage : {AnalysisStage::ParseLat, AnalysisStage::ParseCache,
                           AnalysisStage::ParseElf})
    record_stage(collector, stage);
  StreamingHierarchyOptions options;
  options.telemetry = &collector;
  const auto value = analyze_streaming_hierarchy(stream::byte_trace({0, 0}),
                                                 stream::byte_addresses(),
                                                 metadata().hierarchy, options);
  const auto started = collector.now_ns();
  const auto bytes = hierarchy_result_json(metadata(), value).dump(2);
  collector.finish_stage(AnalysisStage::SerializeResult, started);
  const auto telemetry = collector.snapshot(kAnalysisId, value.coverage,
                                            value.execution_statistics);
  EXPECT_FALSE(bytes.empty());
  EXPECT_GT(telemetry.peak_rss_bytes, 0U);
  EXPECT_EQ(telemetry.host.os, "Linux");
  EXPECT_NO_THROW(hierarchy_telemetry_json(telemetry).dump());
}

} // namespace
