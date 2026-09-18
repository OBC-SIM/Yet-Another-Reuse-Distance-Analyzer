#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

#include "yarda/trace/execution_statistics.hpp"
#include "yarda/trace/trace_coverage.hpp"

namespace yarda
{

/** @brief Stable measured intervals; hierarchy is a subset of streaming. */
enum class AnalysisStage
{
  ParseMap,
  ParseCache,
  ParseElf,
  ResolveAndStream,
  HierarchyAnalysis,
  SerializeResult,
  Count,
};

/** @brief Name the process host on which an analysis was measured. */
struct AnalysisTelemetryHost
{
  std::string hostname;
  std::string os;
  std::string release;
  std::string machine;
};

/** @brief Distinguish an unmeasured stage from a measured zero duration. */
using AnalysisStageTimes =
    std::array<std::optional<std::uint64_t>,
               static_cast<std::size_t>(AnalysisStage::Count)>;

/**
 * @brief Own process measurements separately from semantic result values.
 *
 * All stages must be measured before serialization. The hierarchy subinterval
 * must not be added to resolve_and_stream. Peak RSS is the process high-water
 * mark in bytes, not an invocation delta. Counters describe successful work.
 */
struct AnalysisTelemetry
{
  std::string analysis_id;
  std::uint64_t total_time_ns = 0;
  AnalysisStageTimes stage_time_ns{};
  std::uint64_t peak_rss_bytes = 0;
  std::uint64_t source_accesses_emitted = 0;
  std::uint64_t line_references_emitted = 0;
  TraceExecutionStatistics execution;
  AnalysisTelemetryHost host;
  std::string measured_at_utc;
};

/**
 * @brief Supply measurement observations, including deterministic test clocks.
 *
 * All four callbacks are required. now_ns must be monotonic nanoseconds in a
 * common epoch. peak_rss_bytes reports bytes. UTC format is
 * YYYY-MM-DDTHH:MM:SSZ. Captured state must outlive the collector; callback
 * exceptions propagate.
 */
struct AnalysisTelemetryProviders
{
  std::function<std::uint64_t()> now_ns;
  std::function<std::uint64_t()> peak_rss_bytes;
  std::function<AnalysisTelemetryHost()> host;
  std::function<std::string()> measured_at_utc;
};

/**
 * @brief Measure one invocation without imposing clocks on summary-only runs.
 *
 * Construct before input reading/hashing and snapshot after RESULT dumping,
 * before output I/O and optional-artifact serialization. Discard the collector
 * on any analysis/provider exception. It is not shared across threads or runs.
 */
class AnalysisTelemetryCollector
{
public:
  /** @brief Start with steady_clock, Linux process RSS, uname and UTC time. */
  AnalysisTelemetryCollector();

  /**
   * @brief Start an invocation using owned provider callbacks.
   * @param providers Complete callbacks; captured references remain borrowed.
   * @throws std::invalid_argument if any provider is missing.
   */
  explicit AnalysisTelemetryCollector(AnalysisTelemetryProviders providers);

  /**
   * @brief Read the provider's monotonic nanosecond clock.
   * @return Timestamp usable as the start of a measured stage.
   */
  std::uint64_t now_ns() const;

  /**
   * @brief Accumulate one completed interval into its stable stage.
   * @param stage Stage whose repeated intervals are disjoint.
   * @param started_at Timestamp from this collector, at or after construction.
   * @return Nothing.
   * @throws std::invalid_argument for invalid stage or backward timestamps.
   * @throws std::overflow_error for accumulated duration overflow.
   */
  void finish_stage(AnalysisStage stage, std::uint64_t started_at);

  /**
   * @brief Snapshot all completed measurements from successful analysis.
   * @param analysis_id Canonical identity shared with RESULT and EVENTS.
   * @param coverage Borrowed complete source-to-L1 coverage from the result.
   * @param execution Borrowed producer statistics from the same invocation.
   * @return Validated measurement values; does not create a file.
   * @throws std::invalid_argument for incomplete stages, coverage or metadata.
   * @throws std::overflow_error for duration arithmetic overflow.
   */
  AnalysisTelemetry snapshot(const std::string & analysis_id,
                             const TraceCoverage & coverage,
                             const TraceExecutionStatistics & execution) const;

private:
  AnalysisTelemetryProviders providers_;
  std::uint64_t started_at_ = 0;
  AnalysisStageTimes stages_{};
};

/**
 * @brief Serialize measured values without consulting clock or system state.
 * @param telemetry Borrowed complete measurements of a successful invocation.
 * @return Ordered schema-v1 JSON, intentionally excluded from RESULT equality.
 * @throws std::invalid_argument for missing or inconsistent measurement values.
 * @throws std::overflow_error for duration arithmetic overflow.
 * @note Host text is retained without UTF-8 validation. Supply valid UTF-8
 * for publication; the caller's subsequent dump() can otherwise throw
 * nlohmann::json::type_error. Complete dumping before opening an output file.
 */
nlohmann::ordered_json
hierarchy_telemetry_json(const AnalysisTelemetry & telemetry);

} // namespace yarda
