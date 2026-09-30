#include "yarda/cache/analysis_telemetry.hpp"

#include <stdexcept>
#include <utility>

#include "analysis_telemetry_validation.hpp"
#include "runtime_measurement.hpp"

namespace yarda
{

AnalysisTelemetryCollector::AnalysisTelemetryCollector()
    : AnalysisTelemetryCollector(detail::runtime_measurement_providers())
{
}

AnalysisTelemetryCollector::AnalysisTelemetryCollector(
    AnalysisTelemetryProviders providers)
    : providers_(std::move(providers))
{
  if (!providers_.now_ns || !providers_.peak_rss_bytes || !providers_.host ||
      !providers_.measured_at_utc)
    throw std::invalid_argument(
        "telemetry requires complete measurement providers");
  started_at_ = now_ns();
}

std::uint64_t AnalysisTelemetryCollector::now_ns() const
{
  return providers_.now_ns();
}

void AnalysisTelemetryCollector::finish_stage(AnalysisStage stage,
                                              std::uint64_t started_at)
{
  const auto index = static_cast<std::size_t>(stage);
  if (index >= stages_.size())
    throw std::invalid_argument("invalid telemetry stage");
  const auto ended_at = now_ns();
  if (started_at < started_at_ || ended_at < started_at)
    throw std::invalid_argument("telemetry clock moved backwards");
  stages_[index] = detail::checked_measurement_sum(stages_[index].value_or(0),
                                                   ended_at - started_at);
}

AnalysisTelemetry AnalysisTelemetryCollector::snapshot(
    const std::string & analysis_id, const TraceCoverage & coverage,
    const TraceExecutionStatistics & execution) const
{
  const auto ended_at = now_ns();
  if (ended_at < started_at_)
    throw std::invalid_argument("telemetry clock moved backwards");
  if (!coverage.complete())
    throw std::invalid_argument(
        "telemetry requires complete successful coverage");
  AnalysisTelemetry result;
  result.analysis_id = analysis_id;
  result.total_time_ns = ended_at - started_at_;
  result.stage_time_ns = stages_;
  result.source_accesses_emitted = coverage.source_accesses;
  result.line_references_emitted = coverage.emitted_line_references;
  result.execution = execution;
  result.peak_rss_bytes = providers_.peak_rss_bytes();
  result.host = providers_.host();
  result.measured_at_utc = providers_.measured_at_utc();
  detail::validate_analysis_telemetry(result);
  return result;
}

} // namespace yarda
