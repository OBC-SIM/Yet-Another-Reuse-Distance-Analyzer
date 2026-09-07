#pragma once

#include <gtest/gtest.h>
#include <map>
#include <string>

#include "exact_csrd_oracle.hpp"
#include "hierarchy_analysis_test_support.hpp"
#include "hierarchy_lru_oracle.hpp"
#include "yarda/trace/mapped_trace.hpp"

namespace yarda::test::support
{

/**
 * @brief Check mappings, exact distances and counts against independent input.
 *
 * @param actual Production cache-level result.
 * @param expected Mappings selected without production hierarchy outcomes.
 * @param geometry Geometry for the independent exact-distance oracle.
 * @param counts Explicit-LRU oracle counts.
 * @return Nothing; reports differences through Google Test.
 */
inline void expect_batch_level_matches_oracles(
  const BatchCacheLevelResult & actual,
  const std::vector<CacheLineMapping> & expected,
  const CacheGeometry & geometry, const OracleLevelCounts & counts)
{
  const auto exact = naive_exact_csrd(expected, geometry);
  ASSERT_EQ(actual.mappings.size(), expected.size());
  ASSERT_EQ(actual.accesses.size(), exact.size());
  std::map<std::uint64_t, std::uint64_t> histogram;
  for (std::size_t i = 0; i < expected.size(); ++i)
  {
    SCOPED_TRACE("reference=" + std::to_string(i));
    expect_batch_provenance(actual.mappings[i], expected[i]);
    EXPECT_EQ(actual.mappings[i].decoded.block_number,
              expected[i].decoded.block_number);
    EXPECT_EQ(actual.mappings[i].decoded.set_index,
              expected[i].decoded.set_index);
    EXPECT_EQ(actual.mappings[i].decoded.tag, expected[i].decoded.tag);
    EXPECT_EQ(actual.mappings[i].decoded.line_offset,
              expected[i].decoded.line_offset);
    EXPECT_EQ(actual.accesses[i].outcome, exact[i].outcome);
    EXPECT_EQ(actual.accesses[i].reuse_distance, exact[i].distance);
    if (exact[i].distance) ++histogram[*exact[i].distance];
  }
  EXPECT_EQ(actual.summary.lookups, counts.lookups);
  EXPECT_EQ(actual.summary.hits, counts.hits);
  EXPECT_EQ(actual.summary.misses, counts.misses);
  EXPECT_EQ(actual.summary.cold_misses, counts.cold_misses);
  EXPECT_EQ(actual.summary.replacement_misses, counts.replacement_misses);
  EXPECT_EQ(actual.summary.unique_lines, counts.cold_misses);
  EXPECT_EQ(actual.summary.csrd_histogram, histogram);
}

/**
 * @brief Compare each cold task to exact-distance and explicit-state oracles.
 *
 * Expected LLC rows come from the explicit oracle's decisions on independently
 * mapped input, so a production miss-filter error cannot alter the reference.
 *
 * @param input Resolved fixture tasks.
 * @param hierarchy Supported geometry snapshot.
 * @return Nothing; reports differences through Google Test.
 */
inline void expect_batch_matches_oracles(const ResolvedTaskTraceResult & input,
                                         const AnalysisHierarchy & hierarchy)
{
  const auto mapped = map_resolved_task_traces(input, hierarchy.l1.geometry);
  const auto actual = analyze_batch_hierarchy(input, hierarchy);
  ASSERT_EQ(actual.tasks.size(), mapped.tasks.size());
  EXPECT_EQ(actual.coverage.source_accesses, mapped.coverage.source_accesses);
  EXPECT_EQ(actual.coverage.resolved_accesses,
            mapped.coverage.resolved_accesses);
  EXPECT_EQ(actual.coverage.rejected_accesses, 0U);
  EXPECT_EQ(actual.coverage.emitted_line_references,
            mapped.coverage.emitted_line_references);
  for (std::size_t i = 0; i < mapped.tasks.size(); ++i)
  {
    const auto & reference = mapped.tasks[i];
    const auto & task = actual.tasks[i];
    const auto l1 = batch_l1_result(task);
    const auto llc = batch_llc_result(task);
    SCOPED_TRACE("task_id=" + reference.task_id);
    const auto oracle = analyze_with_explicit_lru(
      reference, hierarchy.l1.geometry, hierarchy.llc.geometry);
    ASSERT_EQ(oracle.events.size(), reference.accesses.size());
    ASSERT_EQ(l1.accesses.size(), oracle.events.size());
    std::vector<CacheLineMapping> llc_input;
    for (std::size_t j = 0; j < oracle.events.size(); ++j)
    {
      EXPECT_EQ(l1.accesses[j].outcome, oracle.events[j].l1_outcome);
      const auto & event = task.events[j];
      EXPECT_EQ(event.llc.has_value(),
                oracle.events[j].llc_outcome.has_value());
      EXPECT_EQ(event.llc_mapping.has_value(),
                oracle.events[j].llc_outcome.has_value());
      switch (oracle.events[j].first_service)
      {
        case OracleFirstServiceLevel::L1:
          EXPECT_EQ(event.first_service, FirstServiceLevel::L1);
          break;
        case OracleFirstServiceLevel::LLC:
          EXPECT_EQ(event.first_service, FirstServiceLevel::LLC);
          break;
        case OracleFirstServiceLevel::Memory:
          EXPECT_EQ(event.first_service, FirstServiceLevel::Memory);
          break;
      }

      if (!oracle.events[j].llc_outcome) continue;
      ASSERT_LT(llc_input.size(), llc.accesses.size());
      EXPECT_EQ(llc.accesses[llc_input.size()].outcome,
                *oracle.events[j].llc_outcome);
      auto row = reference.accesses[j];
      row.decoded =
        decode_cache_address(row.decoded.address, hierarchy.llc.geometry);
      llc_input.push_back(row);
    }
    EXPECT_EQ(task.summary.ehc_l1, oracle.ehc_l1);
    EXPECT_EQ(task.summary.ehc_llc, oracle.ehc_llc);
    EXPECT_EQ(task.summary.all_cache_misses, oracle.all_cache_misses);
    if (reference.accesses.empty())
    {
      EXPECT_FALSE(task.summary.hr_l1);
      EXPECT_FALSE(task.summary.hr_llc);
      EXPECT_FALSE(task.summary.miss_ratio);
    }
    else
    {
      ASSERT_TRUE(task.summary.hr_l1);
      ASSERT_TRUE(task.summary.hr_llc);
      ASSERT_TRUE(task.summary.miss_ratio);
      const auto denominator = static_cast<double>(reference.accesses.size());
      EXPECT_DOUBLE_EQ(*task.summary.hr_l1,
                       static_cast<double>(oracle.ehc_l1) / denominator);
      EXPECT_DOUBLE_EQ(*task.summary.hr_llc,
                       static_cast<double>(oracle.ehc_llc) / denominator);
      EXPECT_DOUBLE_EQ(*task.summary.miss_ratio,
                       static_cast<double>(oracle.all_cache_misses) /
                         denominator);
      EXPECT_NEAR(*task.summary.hr_l1 + *task.summary.hr_llc +
                    *task.summary.miss_ratio,
                  1.0, 1e-15);
    }
    EXPECT_TRUE(task.summary.invariants.level_conservation_l1);
    EXPECT_TRUE(task.summary.invariants.level_conservation_llc);
    EXPECT_TRUE(task.summary.invariants.llc_input_matches_l1_misses);
    EXPECT_TRUE(task.summary.invariants.first_service_conservation);
    EXPECT_TRUE(task.summary.invariants.all_passed);
    EXPECT_EQ(task.summary.task_id, reference.task_id);
    EXPECT_EQ(task.summary.source_accesses, reference.coverage.source_accesses);
    EXPECT_EQ(task.summary.modeled_accesses, reference.accesses.size());
    EXPECT_EQ(task.summary.coverage.source_accesses,
              reference.coverage.source_accesses);
    EXPECT_EQ(task.summary.coverage.resolved_accesses,
              reference.coverage.resolved_accesses);
    EXPECT_EQ(task.summary.coverage.rejected_accesses, 0U);
    EXPECT_EQ(task.summary.coverage.emitted_line_references,
              reference.accesses.size());
    expect_batch_level_matches_oracles(l1, reference.accesses,
                                       hierarchy.l1.geometry, oracle.l1);
    expect_batch_level_matches_oracles(llc, llc_input, hierarchy.llc.geometry,
                                       oracle.llc);
  }
}

}  // namespace yarda::test::support
