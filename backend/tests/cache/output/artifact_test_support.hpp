#pragma once

#include "cache/streaming/streaming_hierarchy_test_support.hpp"
#include "yarda/cache/hierarchy_result_json.hpp"

namespace yarda::test::artifact
{

inline constexpr const char * kAnalysisId =
    "42a9b555b42ac468fa6a0158825378cb56394848549dc20c686e5e2deb3c1a7b";

inline AnalysisIdentityInput identity()
{
  return {"test-build",
          std::string(64, 'a'),
          std::string(64, 'b'),
          std::string(64, 'c'),
          0,
          nlohmann::json::object()};
}

inline HierarchyResultMetadata metadata()
{
  return {identity(),
          {0, {"L1D0", "L1", {32, 2, 1}}, {"L2", "LLC", {32, 4, 1}}, "Memory"},
          2,
          1,
          8,
          62};
}

inline StreamingHierarchyResult result()
{
  return analyze_streaming_hierarchy(streaming::byte_trace({0, 0, 32, 0}),
                                     streaming::byte_addresses(),
                                     metadata().hierarchy);
}

inline StreamingHierarchyResult histogram_result()
{
  TaskHierarchySummary task;
  task.task_id = "histogram";
  task.source_accesses = task.modeled_accesses = 17;
  task.l1 = {17, 1, 16, 11, 5, 11, {{0, 1}, {2, 2}, {10, 3}}};
  task.llc = {16, 5, 11, 11, 0, 11, {{0, 5}}};
  task.l1_first_hit_count = 1;
  task.llc_first_hit_count = 5;
  task.all_cache_misses = 11;
  task.l1_first_hit_ratio = 1.0 / 17.0;
  task.llc_first_hit_ratio = 5.0 / 17.0;
  task.all_cache_miss_ratio = 11.0 / 17.0;
  task.coverage = {17, 17, 0, 17};
  task.invariants = {true, true, true, true, true};
  StreamingHierarchyResult report;
  report.tasks = {task};
  report.coverage = task.coverage;
  return report;
}

/** @brief Model 20,000 same-set blocks scanned forward, then in reverse.
 *
 * Reverse L1 distances are 0..19,999. The first revisit hits L1 and is absent
 * from LLC, whose reverse distances are therefore 1..19,999. Both are 1-way.
 */
inline StreamingHierarchyResult large_histogram_result()
{
  constexpr std::uint64_t lines = 20000;
  TaskHierarchySummary task;
  task.task_id = "reverse-scan";
  task.source_accesses = task.modeled_accesses = 2 * lines;
  task.l1 = {2 * lines, 1, 2 * lines - 1, lines, lines - 1, lines, {}};
  task.llc = {2 * lines - 1, 0, 2 * lines - 1, lines, lines - 1, lines, {}};
  for (std::uint64_t distance = 0; distance < lines; ++distance)
  {
    task.l1.csrd_histogram.emplace(distance, 1);
    if (distance != 0) task.llc.csrd_histogram.emplace(distance, 1);
  }
  task.l1_first_hit_count = 1;
  task.all_cache_misses = 2 * lines - 1;
  task.l1_first_hit_ratio = 1.0 / (2 * lines);
  task.llc_first_hit_ratio = 0.0;
  task.all_cache_miss_ratio = static_cast<double>(2 * lines - 1) / (2 * lines);
  task.coverage = {2 * lines, 2 * lines, 0, 2 * lines};
  task.invariants = {true, true, true, true, true};
  StreamingHierarchyResult report;
  report.tasks = {task};
  report.coverage = task.coverage;
  return report;
}

} // namespace yarda::test::artifact
