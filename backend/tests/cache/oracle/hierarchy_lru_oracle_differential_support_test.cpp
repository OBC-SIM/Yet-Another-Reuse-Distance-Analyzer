#include "hierarchy_lru_oracle_differential_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include "exact_csrd_oracle.hpp"

namespace
{

using yarda::test::support::describe_seeded_oracle_trace;
using yarda::test::support::kWitnessReferenceCount;
using yarda::test::support::make_seeded_oracle_trace;
using yarda::test::support::seeded_oracle_trace_specs;
using yarda::test::support::SeededOracleTraceSpec;

struct ExpectedGeneratedAccess
{
  std::uint64_t block = 0;
  std::uint64_t line_offset = 0;
  yarda::AccessOperation operation = yarda::AccessOperation::Unknown;
};

using ExpectedSuffix = std::array<ExpectedGeneratedAccess, 3>;

void expect_seeded_trace_contract(const SeededOracleTraceSpec & spec,
                                  const ExpectedSuffix & expected_suffix)
{
  const auto trace = make_seeded_oracle_trace(spec);
  SCOPED_TRACE(describe_seeded_oracle_trace(spec, trace));
  ASSERT_EQ(trace.accesses.size(), spec.reference_count);
  ASSERT_GE(trace.accesses.size(),
            kWitnessReferenceCount + expected_suffix.size());
  EXPECT_EQ(trace.task_id, "seed-" + std::to_string(spec.seed));

  for (std::size_t index = 0; index < trace.accesses.size(); ++index)
  {
    SCOPED_TRACE("reference=" + std::to_string(index));
    const auto & mapping = trace.accesses[index];
    ASSERT_NE(mapping.operation, yarda::AccessOperation::Unknown);
    EXPECT_EQ(mapping.source_access_ordinal, index);
  }
  for (std::size_t index = 0; index < expected_suffix.size(); ++index)
  {
    const auto & actual = trace.accesses[kWitnessReferenceCount + index];
    const auto & expected = expected_suffix[index];
    SCOPED_TRACE("suffix_reference=" + std::to_string(index));
    ASSERT_EQ(actual.decoded.block_number, expected.block);
    ASSERT_EQ(actual.decoded.line_offset, expected.line_offset);
    ASSERT_EQ(actual.operation, expected.operation);
  }

  std::unordered_set<std::uint64_t> suffix_blocks;
  std::unordered_set<std::uint64_t> suffix_offsets;
  bool has_load = false;
  bool has_store = false;
  for (std::size_t index = kWitnessReferenceCount;
       index < trace.accesses.size(); ++index)
  {
    const auto & mapping = trace.accesses[index];
    suffix_blocks.insert(mapping.decoded.block_number);
    suffix_offsets.insert(mapping.decoded.line_offset);
    has_load = has_load || mapping.operation == yarda::AccessOperation::Load;
    has_store = has_store || mapping.operation == yarda::AccessOperation::Store;
  }
  EXPECT_GT(suffix_blocks.size(), spec.block_domain / 2);
  EXPECT_GT(suffix_offsets.size(), spec.l1_geometry.line_size / 2);
  EXPECT_TRUE(has_load);
  EXPECT_TRUE(has_store);

  const auto replay = make_seeded_oracle_trace(spec);
  ASSERT_EQ(replay.accesses.size(), trace.accesses.size());
  for (std::size_t index = 0; index < trace.accesses.size(); ++index)
  {
    SCOPED_TRACE("replay_reference=" + std::to_string(index));
    ASSERT_EQ(replay.accesses[index].decoded.address,
              trace.accesses[index].decoded.address);
    ASSERT_EQ(replay.accesses[index].operation,
              trace.accesses[index].operation);
  }
}

TEST(SeededOracleTraceTest, PreservesSeededSuffixAndDiversity)
{
  const auto & specs = seeded_oracle_trace_specs();
  const std::array<ExpectedSuffix, 3> expected_suffixes{{
    ExpectedSuffix{{{52U, 25U, yarda::AccessOperation::Store},
                    {41U, 0U, yarda::AccessOperation::Store},
                    {4U, 8U, yarda::AccessOperation::Load}}},
    ExpectedSuffix{{{51U, 4U, yarda::AccessOperation::Load},
                    {112U, 13U, yarda::AccessOperation::Store},
                    {54U, 17U, yarda::AccessOperation::Load}}},
    ExpectedSuffix{{{214U, 23U, yarda::AccessOperation::Store},
                    {70U, 41U, yarda::AccessOperation::Load},
                    {114U, 16U, yarda::AccessOperation::Load}}},
  }};

  ASSERT_EQ(specs.size(), expected_suffixes.size());
  for (std::size_t index = 0; index < specs.size(); ++index)
  {
    expect_seeded_trace_contract(specs[index], expected_suffixes[index]);
  }
}

TEST(SeededOracleTraceTest, SharedSpecsIncludeValid32And64ByteLines)
{
  bool has_32_byte_lines = false;
  bool has_64_byte_lines = false;
  for (const auto & spec : seeded_oracle_trace_specs())
  {
    SCOPED_TRACE("seed=" + std::to_string(spec.seed));
    EXPECT_NO_THROW(yarda::cache_set_count(spec.l1_geometry));
    EXPECT_NO_THROW(yarda::cache_set_count(spec.llc_geometry));
    EXPECT_EQ(spec.l1_geometry.line_size, spec.llc_geometry.line_size);
    has_32_byte_lines = has_32_byte_lines || spec.l1_geometry.line_size == 32;
    has_64_byte_lines = has_64_byte_lines || spec.l1_geometry.line_size == 64;
  }
  EXPECT_TRUE(has_32_byte_lines);
  EXPECT_TRUE(has_64_byte_lines);
}

TEST(SeededOracleTraceTest, WitnessPrefixReusesBlocksAcrossOffsets)
{
  for (auto spec : seeded_oracle_trace_specs())
  {
    spec.reference_count = kWitnessReferenceCount;
    const auto trace = make_seeded_oracle_trace(spec);
    SCOPED_TRACE(describe_seeded_oracle_trace(spec, trace));
    ASSERT_EQ(trace.accesses.size(), kWitnessReferenceCount);
    const auto exact =
      yarda::test::naive_exact_csrd(trace.accesses, spec.l1_geometry);
    ASSERT_EQ(exact.size(), trace.accesses.size());

    EXPECT_EQ(trace.accesses[1].decoded.block_number,
              trace.accesses[2].decoded.block_number);
    EXPECT_NE(trace.accesses[1].decoded.line_offset,
              trace.accesses[2].decoded.line_offset);
    EXPECT_EQ(exact[2].distance, 0U);
    EXPECT_EQ(exact[2].outcome, yarda::LruAccessOutcome::Hit);
  }
}

TEST(SeededOracleTraceTest, WitnessPrefixRemembersEvictedBlocks)
{
  for (auto spec : seeded_oracle_trace_specs())
  {
    spec.reference_count = kWitnessReferenceCount;
    const auto trace = make_seeded_oracle_trace(spec);
    SCOPED_TRACE(describe_seeded_oracle_trace(spec, trace));
    ASSERT_EQ(trace.accesses.size(), kWitnessReferenceCount);
    const auto exact =
      yarda::test::naive_exact_csrd(trace.accesses, spec.l1_geometry);
    ASSERT_EQ(exact.size(), trace.accesses.size());

    EXPECT_EQ(trace.accesses[0].decoded.block_number,
              trace.accesses[6].decoded.block_number);
    EXPECT_EQ(exact[6].distance, 2U);
    EXPECT_EQ(exact[6].outcome, yarda::LruAccessOutcome::ReplacementMiss);
  }
}

TEST(SeededOracleTraceTest, WitnessPrefixExercisesStoreRecency)
{
  for (auto spec : seeded_oracle_trace_specs())
  {
    spec.reference_count = kWitnessReferenceCount;
    const auto trace = make_seeded_oracle_trace(spec);
    SCOPED_TRACE(describe_seeded_oracle_trace(spec, trace));
    ASSERT_EQ(trace.accesses.size(), kWitnessReferenceCount);
    const auto exact =
      yarda::test::naive_exact_csrd(trace.accesses, spec.l1_geometry);
    ASSERT_EQ(exact.size(), trace.accesses.size());

    for (std::size_t index = 7; index < trace.accesses.size(); ++index)
    {
      SCOPED_TRACE("store_reference=" + std::to_string(index));
      EXPECT_EQ(trace.accesses[index].operation, yarda::AccessOperation::Store);
    }
    EXPECT_EQ(trace.accesses[9].decoded.block_number,
              trace.accesses[11].decoded.block_number);
    EXPECT_EQ(trace.accesses[8].decoded.block_number,
              trace.accesses[12].decoded.block_number);
    EXPECT_EQ(exact[9].outcome, yarda::LruAccessOutcome::Hit);
    EXPECT_EQ(exact[11].distance, 1U);
    EXPECT_EQ(exact[11].outcome, yarda::LruAccessOutcome::Hit);
    EXPECT_EQ(exact[12].distance, 2U);
    EXPECT_EQ(exact[12].outcome, yarda::LruAccessOutcome::ReplacementMiss);
  }
}

TEST(SeededOracleTraceTest, DescribesCompleteReproductionContext)
{
  auto spec = seeded_oracle_trace_specs().front();
  spec.reference_count = kWitnessReferenceCount;
  const auto trace = make_seeded_oracle_trace(spec);

  EXPECT_EQ(describe_seeded_oracle_trace(spec, trace),
            "seed=12648430 length=13 block_domain=64"
            " l1={line=32,lines=8,ways=2} llc={line=32,lines=32,ways=4} trace="
            " 0@0:S 4@0:L 4@16:S 0@8:L 8@0:L 12@16:S 0@31:S"
            " 16@0:S 20@8:S 16@16:S 24@31:S 16@8:S 20@16:S");
}

TEST(SeededOracleTraceTest, RejectsZeroBlockDomain)
{
  // Exact witness length avoids reaching modulo if this guard regresses.
  const SeededOracleTraceSpec spec{
    0U, kWitnessReferenceCount, 0U, {32, 8, 2}, {32, 32, 4}};

  EXPECT_THROW(make_seeded_oracle_trace(spec), std::invalid_argument);
}

}  // namespace
