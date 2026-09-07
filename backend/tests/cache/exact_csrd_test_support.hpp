#pragma once

#include <algorithm>
#include <gtest/gtest.h>
#include <set>
#include <string>
#include <vector>

#include "exact_csrd_oracle.hpp"
#include "recency_index.hpp"
#include "yarda/cache/exact_csrd.hpp"

namespace yarda::test::support
{

/** @brief Test-only measurements of retained recency storage. */
struct RecencyStorage
{
  std::uint64_t active;
  std::uint64_t capacity;
  std::uint64_t next_slot;
  std::size_t allocated_elements;
  std::size_t hash_buckets;
};

}  // namespace yarda::test::support

namespace yarda::detail
{

/** @brief Inspect storage invariants without adding production diagnostics. */
struct RecencyIndexTestAccess
{
  /**
   * @brief Snapshot the index's retained allocation sizes.
   * @param index Borrowed index.
   * @return Counts independent of stored reference outcomes.
   */
  static test::support::RecencyStorage storage(const RecencyIndex & index)
  {
    return {index.active_count_, index.capacity_, index.next_slot_,
            index.tree_.capacity(), index.slots_.bucket_count()};
  }

  /**
   * @brief Check one active slot per key and agreement with Fenwick totals.
   * @param index Borrowed index.
   * @return Nothing; reports failures through Google Test.
   */
  static void expect_invariants(const RecencyIndex & index)
  {
    EXPECT_EQ(index.active_count_, index.slots_.size());
    EXPECT_EQ(index.prefix_sum(index.capacity_), index.active_count_);
    EXPECT_EQ(index.tree_.size(), index.capacity_ + 1);
    std::set<std::uint64_t> active_slots;
    for (const auto & [key, slot] : index.slots_)
    {
      SCOPED_TRACE("key=" + std::to_string(key));
      ASSERT_GT(slot, 0U);
      ASSERT_LE(slot, index.capacity_);
      EXPECT_LT(slot, index.next_slot_);
      EXPECT_TRUE(active_slots.insert(slot).second);
      EXPECT_EQ(index.prefix_sum(slot) - index.prefix_sum(slot - 1), 1U);
    }
  }
};

/** @brief Supply private state access for bounded-state and overflow tests. */
struct ExactCsrdTestAccess
{
  /**
   * @brief Seed otherwise unreachable counter boundaries.
   * @param analyzer Borrowed analyzer; use only to provoke a failing observe.
   * @return Mutable test-only counter state.
   */
  static ExactCsrdSummary & counters(ExactCsrdAnalyzer & analyzer)
  {
    return analyzer.summary_;
  }

  /**
   * @brief Count only the set states actually allocated by observation.
   * @param analyzer Borrowed analyzer.
   * @return Number of touched sets.
   */
  static std::size_t set_count(const ExactCsrdAnalyzer & analyzer)
  {
    return analyzer.sets_.size();
  }

  /**
   * @brief Inspect a touched set for fixed-domain storage tests.
   * @param analyzer Borrowed analyzer.
   * @param set_index Previously touched set.
   * @return Read-only index owned by the analyzer.
   */
  static const RecencyIndex & index(const ExactCsrdAnalyzer & analyzer,
                                    std::uint64_t set_index)
  {
    return *analyzer.sets_.at(set_index);
  }
};

}  // namespace yarda::detail

namespace yarda::test::support
{

/**
 * @brief Feed a small fixture block through normal address decoding.
 * @param analyzer Analyzer being tested.
 * @param geometry Its geometry.
 * @param block Block with a representable byte address.
 * @param offset In-line byte offset.
 * @return Production observation.
 */
inline CsrdObservation observe_block(ExactCsrdAnalyzer & analyzer,
                                     const CacheGeometry & geometry,
                                     std::uint64_t block,
                                     std::uint64_t offset = 0)
{
  return analyzer.observe(
    decode_cache_address(block * geometry.line_size + offset, geometry));
}

/**
 * @brief Compare all summary fields, including the full finite histogram.
 * @param actual Incremental summary.
 * @param expected Independently derived counts.
 * @return Nothing; reports failures through Google Test.
 */
inline void expect_csrd_summary(const ExactCsrdSummary & actual,
                                const ExactCsrdSummary & expected)
{
  EXPECT_EQ(actual.lookups, expected.lookups);
  EXPECT_EQ(actual.hits, expected.hits);
  EXPECT_EQ(actual.cold_misses, expected.cold_misses);
  EXPECT_EQ(actual.replacement_misses, expected.replacement_misses);
  EXPECT_EQ(actual.unique_lines, expected.unique_lines);
  EXPECT_EQ(actual.histogram, expected.histogram);
}

/**
 * @brief Check every observation and prefix summary against independent data.
 * @param mappings Ordered fixture references, retained only in tests.
 * @param geometry Mapping geometry.
 * @return Nothing; reports failures through Google Test.
 */
inline void
expect_incremental_parity(const std::vector<CacheLineMapping> & mappings,
                          const CacheGeometry & geometry)
{
  const auto oracle = naive_exact_csrd(mappings, geometry);
  const auto batch = analyze_lru_reuse(mappings, geometry);
  ExactCsrdAnalyzer analyzer(geometry);
  ExactCsrdSummary expected;
  std::set<std::uint64_t> blocks;
  expect_csrd_summary(analyzer.summary(), expected);
  ASSERT_EQ(oracle.size(), mappings.size());
  ASSERT_EQ(batch.accesses.size(), mappings.size());
  for (std::size_t i = 0; i < mappings.size(); ++i)
  {
    SCOPED_TRACE("reference=" + std::to_string(i));
    const auto actual = analyzer.observe(mappings[i].decoded);
    EXPECT_EQ(actual.outcome, oracle[i].outcome);
    EXPECT_EQ(actual.distance, oracle[i].distance);
    EXPECT_EQ(actual.outcome, batch.accesses[i].outcome);
    EXPECT_EQ(actual.distance, batch.accesses[i].reuse_distance);
    ++expected.lookups;
    expected.unique_lines +=
      blocks.insert(mappings[i].decoded.block_number).second;
    if (oracle[i].distance) ++expected.histogram[*oracle[i].distance];
    switch (oracle[i].outcome)
    {
      case LruAccessOutcome::Hit:
        ++expected.hits;
        break;
      case LruAccessOutcome::ColdMiss:
        ++expected.cold_misses;
        break;
      case LruAccessOutcome::ReplacementMiss:
        ++expected.replacement_misses;
        break;
    }
    expect_csrd_summary(analyzer.summary(), expected);
  }
  const auto & summary = analyzer.summary();
  EXPECT_EQ(summary.hits, batch.hits);
  EXPECT_EQ(summary.cold_misses, batch.cold_misses);
  EXPECT_EQ(summary.replacement_misses, batch.replacement_misses);
  const std::map<std::uint64_t, std::uint64_t> batch_histogram(
    batch.histogram.begin(), batch.histogram.end());
  EXPECT_EQ(summary.histogram, batch_histogram);
}

/**
 * @brief Give parameterized boundary cases stable descriptive names.
 * @param case_info Test parameter with a name field.
 * @return Name used by Google Test discovery.
 */
template <typename Case>
std::string csrd_case_name(const testing::TestParamInfo<Case> & case_info)
{
  return case_info.param.name;
}

}  // namespace yarda::test::support
