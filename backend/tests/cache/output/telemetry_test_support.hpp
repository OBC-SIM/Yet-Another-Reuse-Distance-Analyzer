#pragma once

#include "yarda/cache/analysis_telemetry.hpp"
#include "artifact_test_support.hpp"

namespace yarda::test::artifact
{

struct FakeMeasurements
{
  std::uint64_t clock = 100;
  std::uint64_t clock_calls = 0;

  AnalysisTelemetryProviders providers()
  {
    return {
        [this]
        {
          ++clock_calls;
          return clock++;
        },
        [] { return std::uint64_t{4096}; },
        [] {
          return AnalysisTelemetryHost{"test-host", "Linux", "test", "x86_64"};
        },
        [] { return "2026-09-12T12:34:56Z"; }};
  }

  /** @brief Keep time fixed until the test explicitly advances clock. */
  AnalysisTelemetryProviders manual_providers()
  {
    auto result = providers();
    result.now_ns = [this] { return clock; };
    return result;
  }
};

inline void record_stage(AnalysisTelemetryCollector & collector,
                         AnalysisStage stage)
{
  const auto start = collector.now_ns();
  collector.finish_stage(stage, start);
}

inline void record_external_stages(AnalysisTelemetryCollector & collector)
{
  for (const auto stage :
       {AnalysisStage::ParseMap, AnalysisStage::ParseCache,
        AnalysisStage::ParseElf, AnalysisStage::SerializeResult})
    record_stage(collector, stage);
}

inline void advance_stage(AnalysisTelemetryCollector & collector,
                           FakeMeasurements & fake, AnalysisStage stage,
                           std::uint64_t duration)
{
  const auto start = collector.now_ns();
  fake.clock += duration;
  collector.finish_stage(stage, start);
}

inline AnalysisTelemetry measured_telemetry()
{
  AnalysisTelemetry telemetry;
  telemetry.analysis_id = kAnalysisId;
  telemetry.total_time_ns = 20;
  for (auto & stage : telemetry.stage_time_ns)
    stage = 1;
  telemetry.peak_rss_bytes = 4096;
  telemetry.source_accesses_emitted = 2;
  telemetry.line_references_emitted = 3;
  telemetry.execution = {2, 5};
  telemetry.host = {"test-host", "Linux", "test", "x86_64"};
  telemetry.measured_at_utc = "2026-09-12T12:34:56Z";
  return telemetry;
}

} // namespace yarda::test::artifact
