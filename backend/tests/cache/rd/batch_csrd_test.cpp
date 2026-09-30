#include "yarda/cache/batch_csrd.hpp"

#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

yarda::CacheLineMapping access(std::uint64_t tag, std::uint64_t set_index,
                               const std::string & object = "global::A")
{
  yarda::CacheLineMapping mapping;
  mapping.object_id = object;
  mapping.decoded.tag = tag;
  mapping.decoded.set_index = set_index;
  return mapping;
}

TEST(BatchCsrdTest, IgnoresInterveningAccessesToOtherSets)
{
  const std::vector<yarda::CacheLineMapping> accesses = {
    access(0, 0), access(0, 1), access(1, 0), access(1, 1), access(0, 0)};

  const auto result =
    yarda::analyze_batch_csrd(accesses, yarda::CacheGeometry{64, 4, 2});

  ASSERT_EQ(result.accesses.size(), accesses.size());
  EXPECT_EQ(result.accesses.back().outcome, yarda::LruAccessOutcome::Hit);
  EXPECT_EQ(result.accesses.back().reuse_distance, 1U);
  EXPECT_EQ(result.hits, 1U);
  EXPECT_EQ(result.cold_misses, 4U);
  EXPECT_EQ(result.replacement_misses, 0U);
}

TEST(BatchCsrdTest, MissesAtAssociativityBoundary)
{
  const std::vector<yarda::CacheLineMapping> accesses = {
    access(0, 0), access(1, 0), access(2, 0), access(0, 0)};

  const auto result =
    yarda::analyze_batch_csrd(accesses, yarda::CacheGeometry{64, 2, 2});

  EXPECT_EQ(result.accesses.back().outcome,
            yarda::LruAccessOutcome::ReplacementMiss);
  EXPECT_EQ(result.accesses.back().reuse_distance, 2U);
  EXPECT_EQ(result.cold_misses, 3U);
  EXPECT_EQ(result.replacement_misses, 1U);
}

TEST(BatchCsrdTest, TreatsSharedPhysicalLineAsOneIdentity)
{
  const std::vector<yarda::CacheLineMapping> accesses = {
    access(3, 0, "global::A"), access(3, 0, "global::B")};

  const auto result =
    yarda::analyze_batch_csrd(accesses, yarda::CacheGeometry{64, 1, 1});

  EXPECT_EQ(result.cold_misses, 1U);
  EXPECT_EQ(result.hits, 1U);
  EXPECT_EQ(result.accesses.back().reuse_distance, 0U);
}

TEST(BatchCsrdTest, CountsDistinctLinesOnceInFullyAssociativeCache)
{
  const std::vector<yarda::CacheLineMapping> accesses = {
    access(0, 0, "global::A"),
    access(1, 0, "global::B"),
    access(1, 0, "global::B"),
    access(0, 0, "global::A"),
  };

  const auto result =
    yarda::analyze_batch_csrd(accesses, yarda::CacheGeometry{32, 4, 4});

  EXPECT_EQ(result.cold_misses, 2U);
  EXPECT_EQ(result.hits, 2U);
  EXPECT_EQ(result.accesses[2].reuse_distance, 0U);
  EXPECT_EQ(result.accesses.back().reuse_distance, 1U);
}

TEST(BatchCsrdTest, ReplacesConflictingLineInDirectMappedCache)
{
  const std::vector<yarda::CacheLineMapping> accesses = {
    access(0, 0), access(1, 0), access(0, 0)};

  const auto result =
    yarda::analyze_batch_csrd(accesses, yarda::CacheGeometry{64, 1, 1});

  EXPECT_EQ(result.cold_misses, 2U);
  EXPECT_EQ(result.replacement_misses, 1U);
  EXPECT_EQ(result.accesses.back().reuse_distance, 1U);
}

TEST(BatchCsrdTest, HandlesEmptyTrace)
{
  const auto result =
    yarda::analyze_batch_csrd({}, yarda::CacheGeometry{64, 512, 8});

  EXPECT_TRUE(result.accesses.empty());
  EXPECT_TRUE(result.sets.empty());
  EXPECT_TRUE(result.histogram.empty());
}

TEST(BatchCsrdTest, RejectsOutOfRangeSetIndex)
{
  EXPECT_THROW(
    yarda::analyze_batch_csrd({access(0, 2)}, yarda::CacheGeometry{64, 4, 2}),
    std::invalid_argument);
}

}  // namespace
