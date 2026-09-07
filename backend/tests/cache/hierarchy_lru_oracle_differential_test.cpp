#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "exact_csrd_oracle.hpp"
#include "hierarchy_lru_oracle.hpp"
#include "hierarchy_lru_oracle_differential_support.hpp"
#include "hierarchy_lru_oracle_test_support.hpp"

namespace
{

using yarda::test::OracleCsrdObservation;
using yarda::test::support::describe_seeded_oracle_trace;
using yarda::test::support::expect_contract_invariants;
using yarda::test::support::make_seeded_oracle_trace;
using yarda::test::support::SeededOracleTraceSpec;

std::vector<std::optional<std::uint64_t>> independent_stack_distances(
  const std::vector<yarda::CacheLineMapping> & accesses)
{
  std::map<std::uint64_t, std::vector<std::uint64_t>> stacks;
  std::vector<std::optional<std::uint64_t>> distances;
  distances.reserve(accesses.size());
  for (const auto & mapping : accesses)
  {
    auto & stack = stacks[mapping.decoded.set_index];
    const auto found =
      std::find(stack.begin(), stack.end(), mapping.decoded.block_number);
    if (found == stack.end())
    {
      distances.push_back(std::nullopt);
    }
    else
    {
      distances.push_back(static_cast<std::uint64_t>(found - stack.begin()));
      stack.erase(found);
    }
    stack.insert(stack.begin(), mapping.decoded.block_number);
  }
  return distances;
}

void expect_exact_distances(
  const std::vector<OracleCsrdObservation> & exact,
  const std::vector<std::optional<std::uint64_t>> & independent,
  const char * level)
{
  SCOPED_TRACE(level);
  ASSERT_EQ(exact.size(), independent.size());
  for (std::size_t index = 0; index < exact.size(); ++index)
  {
    SCOPED_TRACE("distance_reference=" + std::to_string(index));
    ASSERT_EQ(exact[index].distance, independent[index]);
  }
}

std::uint64_t maximum_finite_distance(
  const std::vector<std::optional<std::uint64_t>> & distances)
{
  std::uint64_t maximum = 0;
  for (const auto distance : distances)
  {
    if (distance)
    {
      maximum = std::max(maximum, *distance);
    }
  }
  return maximum;
}

std::size_t
count_outcome(const std::vector<OracleCsrdObservation> & observations,
              yarda::LruAccessOutcome outcome)
{
  std::size_t count = 0;
  for (const auto & observation : observations)
  {
    if (observation.outcome == outcome)
    {
      ++count;
    }
  }
  return count;
}

bool has_store_outcome(const yarda::MappedTaskTrace & trace,
                       const std::vector<OracleCsrdObservation> & observations,
                       yarda::LruAccessOutcome outcome)
{
  for (std::size_t index = 0; index < observations.size(); ++index)
  {
    if (trace.accesses[index].operation == yarda::AccessOperation::Store &&
        observations[index].outcome == outcome)
    {
      return true;
    }
  }
  return false;
}

void expect_level_matches_exact(
  const yarda::test::OracleLevelCounts & counts,
  const std::vector<OracleCsrdObservation> & observations)
{
  EXPECT_EQ(counts.lookups, observations.size());
  EXPECT_EQ(counts.hits,
            count_outcome(observations, yarda::LruAccessOutcome::Hit));
  EXPECT_EQ(counts.cold_misses,
            count_outcome(observations, yarda::LruAccessOutcome::ColdMiss));
  EXPECT_EQ(
    counts.replacement_misses,
    count_outcome(observations, yarda::LruAccessOutcome::ReplacementMiss));
}

std::vector<yarda::CacheLineMapping>
make_llc_exact_input(const yarda::MappedTaskTrace & trace,
                     const std::vector<OracleCsrdObservation> & l1_exact,
                     const yarda::CacheGeometry & llc_geometry)
{
  std::vector<yarda::CacheLineMapping> llc_accesses;
  for (std::size_t index = 0; index < l1_exact.size(); ++index)
  {
    if (l1_exact[index].outcome == yarda::LruAccessOutcome::Hit)
    {
      continue;
    }
    auto remapped = trace.accesses[index];
    remapped.decoded =
      yarda::decode_cache_address(remapped.decoded.address, llc_geometry);
    llc_accesses.push_back(remapped);
  }
  return llc_accesses;
}

void expect_seeded_differential_agreement(
  const SeededOracleTraceSpec & spec, std::uint64_t & maximum_observed_distance)
{
  const auto trace = make_seeded_oracle_trace(spec);
  SCOPED_TRACE(describe_seeded_oracle_trace(spec, trace));
  ASSERT_EQ(trace.accesses.size(), spec.reference_count);
  ASSERT_GE(trace.accesses.size(), 13U);
  EXPECT_EQ(trace.accesses[1].decoded.block_number,
            trace.accesses[2].decoded.block_number);
  EXPECT_NE(trace.accesses[1].decoded.line_offset,
            trace.accesses[2].decoded.line_offset);

  const auto l1_exact =
    yarda::test::naive_exact_csrd(trace.accesses, spec.l1_geometry);
  const auto l1_independent = independent_stack_distances(trace.accesses);
  const auto llc_accesses =
    make_llc_exact_input(trace, l1_exact, spec.llc_geometry);
  const auto llc_exact =
    yarda::test::naive_exact_csrd(llc_accesses, spec.llc_geometry);
  const auto llc_independent = independent_stack_distances(llc_accesses);
  const auto hierarchy = yarda::test::analyze_with_explicit_lru(
    trace, spec.l1_geometry, spec.llc_geometry);

  expect_exact_distances(l1_exact, l1_independent, "L1 exact distance");
  expect_exact_distances(llc_exact, llc_independent, "LLC exact distance");
  maximum_observed_distance = std::max(maximum_observed_distance,
                                       maximum_finite_distance(l1_independent));
  ASSERT_EQ(l1_exact.size(), trace.accesses.size());
  ASSERT_EQ(hierarchy.events.size(), trace.accesses.size());
  EXPECT_EQ(hierarchy.task_id, trace.task_id);
  EXPECT_TRUE(has_store_outcome(trace, l1_exact, yarda::LruAccessOutcome::Hit));
  EXPECT_TRUE(has_store_outcome(trace, l1_exact,
                                yarda::LruAccessOutcome::ReplacementMiss));
  EXPECT_GT(count_outcome(llc_exact, yarda::LruAccessOutcome::Hit), 0U);
  EXPECT_GT(count_outcome(llc_exact, yarda::LruAccessOutcome::ReplacementMiss),
            0U);

  std::size_t llc_index = 0;
  for (std::size_t index = 0; index < l1_exact.size(); ++index)
  {
    SCOPED_TRACE("reference=" + std::to_string(index));
    const auto & event = hierarchy.events[index];
    ASSERT_EQ(event.l1_outcome, l1_exact[index].outcome);
    if (l1_exact[index].outcome == yarda::LruAccessOutcome::Hit)
    {
      ASSERT_FALSE(event.llc_outcome.has_value());
      ASSERT_EQ(event.first_service, yarda::test::OracleFirstServiceLevel::L1);
      continue;
    }

    ASSERT_LT(llc_index, llc_exact.size());
    ASSERT_TRUE(event.llc_outcome.has_value());
    ASSERT_EQ(*event.llc_outcome, llc_exact[llc_index].outcome);
    const auto expected_service =
      llc_exact[llc_index].outcome == yarda::LruAccessOutcome::Hit
        ? yarda::test::OracleFirstServiceLevel::LLC
        : yarda::test::OracleFirstServiceLevel::Memory;
    ASSERT_EQ(event.first_service, expected_service);
    ++llc_index;
  }
  EXPECT_EQ(llc_index, llc_exact.size());
  expect_level_matches_exact(hierarchy.l1, l1_exact);
  expect_level_matches_exact(hierarchy.llc, llc_exact);
  expect_contract_invariants(hierarchy, trace.accesses.size());
}

TEST(ExplicitHierarchyLruOracleDifferentialTest,
     MatchesExactOraclesOnSeededMixedTraces)
{
  const std::array<SeededOracleTraceSpec, 3> specs{{
    {0x00c0ffeeU, 256U, 64U, {32, 8, 2}, {32, 32, 4}},
    {0x5eed1234U, 320U, 256U, {32, 16, 2}, {32, 128, 8}},
    {0x000a11ceU, 384U, 256U, {64, 16, 2}, {64, 128, 8}},
  }};

  std::uint64_t maximum_observed_distance = 0;
  for (const auto & spec : specs)
  {
    expect_seeded_differential_agreement(spec, maximum_observed_distance);
  }
  EXPECT_GT(maximum_observed_distance, 17U);
}

}  // namespace
