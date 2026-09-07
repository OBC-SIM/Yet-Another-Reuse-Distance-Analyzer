#pragma once

#include <array>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

#include "hierarchy_aggregation.hpp"
#include "hierarchy_analysis_test_support.hpp"

namespace yarda::test::support
{

/** @brief Literal L1/LLC observations for addresses 0,32,0,64,32. */
struct AggregationFixture
{
  std::string task_id = "service";
  TraceCoverage coverage{5, 5, 0, 5};
  BatchCacheLevelResult l1;
  BatchCacheLevelResult llc;
};

/**
 * @brief Build independent observations without invoking an LRU analyzer.
 *
 * @return Two streams for L1 {32,2,2} and LLC {32,8,4}; the last L1 miss
 * hits LLC, and the third L1 reference hits locally.
 */
inline AggregationFixture make_aggregation_fixture()
{
  AggregationFixture fixture;
  const std::array<std::uint64_t, 5> addresses{0, 32, 0, 64, 32};
  for (std::size_t i = 0; i < addresses.size(); ++i)
  {
    CacheLineMapping row;
    row.object_id = "global::service";
    row.object_byte_offset = addresses[i];
    row.decoded = decode_cache_address(addresses[i], {32, 2, 2});
    row.source_object_byte_offset = addresses[i];
    row.source_access_size = 1;
    row.source_linked_byte_address = addresses[i];
    row.operation = AccessOperation::Load;
    row.source_access_ordinal = i;
    fixture.l1.mappings.push_back(row);
  }
  for (const std::size_t index : {0U, 1U, 3U, 4U})
  {
    auto row = fixture.l1.mappings[index];
    row.decoded = decode_cache_address(row.decoded.address, {32, 8, 4});
    fixture.llc.mappings.push_back(row);
  }
  using Outcome = LruAccessOutcome;
  fixture.l1.accesses = {{Outcome::ColdMiss, std::nullopt},
                         {Outcome::ColdMiss, std::nullopt},
                         {Outcome::Hit, 1},
                         {Outcome::ColdMiss, std::nullopt},
                         {Outcome::ReplacementMiss, 2}};
  fixture.llc.accesses = {{Outcome::ColdMiss, std::nullopt},
                          {Outcome::ColdMiss, std::nullopt},
                          {Outcome::ColdMiss, std::nullopt},
                          {Outcome::Hit, 0}};
  fixture.l1.summary = {5, 1, 4, 3, 1, 3, {{1, 1}, {2, 1}}};
  fixture.llc.summary = {4, 1, 3, 3, 0, 3, {{0, 1}}};
  return fixture;
}

/**
 * @brief Require a specific aggregation failure without swallowing exceptions.
 *
 * @param fixture Malformed observations whose counts may still agree.
 * @param diagnostic Distinguishing substring of the expected logic error.
 * @return Nothing; reports differences through Google Test.
 */
inline void expect_aggregation_error(const AggregationFixture & fixture,
                                     const std::string & diagnostic)
{
  try
  {
    detail::aggregate_hierarchy_task(fixture.task_id, fixture.coverage,
                                     fixture.l1, fixture.llc);
    FAIL() << "aggregation accepted inconsistent observations";
  }
  catch (const std::logic_error & error)
  {
    EXPECT_NE(std::string(error.what()).find(diagnostic), std::string::npos)
      << error.what();
  }
}

/**
 * @brief Assert that every named conservation check was finalized.
 *
 * @param invariants Flags from successful task analysis.
 * @return Nothing; reports differences through Google Test.
 */
inline void expect_hierarchy_invariants(const HierarchyInvariants & invariants)
{
  EXPECT_TRUE(invariants.level_conservation_l1);
  EXPECT_TRUE(invariants.level_conservation_llc);
  EXPECT_TRUE(invariants.llc_input_matches_l1_misses);
  EXPECT_TRUE(invariants.first_service_conservation);
  EXPECT_TRUE(invariants.all_passed);
}

}  // namespace yarda::test::support
