#include "hierarchy_lru_oracle_differential_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace
{

using yarda::test::support::describe_seeded_oracle_trace;
using yarda::test::support::make_seeded_oracle_trace;
using yarda::test::support::SeededOracleTraceSpec;

constexpr std::size_t kWitnessReferenceCount = 13;

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

  for (const auto & mapping : trace.accesses)
  {
    ASSERT_NE(mapping.operation, yarda::AccessOperation::Unknown);
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
  EXPECT_GT(suffix_blocks.size(), 16U);
  EXPECT_GT(suffix_offsets.size(), 8U);
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
  const std::array<SeededOracleTraceSpec, 3> specs{{
    {0x00c0ffeeU, 256U, 64U, {32, 8, 2}, {32, 32, 4}},
    {0x5eed1234U, 320U, 256U, {32, 16, 2}, {32, 128, 8}},
    {0x000a11ceU, 384U, 256U, {64, 16, 2}, {64, 128, 8}},
  }};
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

  for (std::size_t index = 0; index < specs.size(); ++index)
  {
    expect_seeded_trace_contract(specs[index], expected_suffixes[index]);
  }
}

TEST(SeededOracleTraceTest, RejectsZeroBlockDomain)
{
  // Exact witness length avoids reaching modulo if this guard regresses.
  const SeededOracleTraceSpec spec{0U, 13U, 0U, {32, 8, 2}, {32, 32, 4}};

  EXPECT_THROW(make_seeded_oracle_trace(spec), std::invalid_argument);
}

}  // namespace
