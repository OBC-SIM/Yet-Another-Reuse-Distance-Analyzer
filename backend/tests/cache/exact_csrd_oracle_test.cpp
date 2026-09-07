#include "exact_csrd_oracle.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <initializer_list>
#include <stdexcept>
#include <vector>

namespace
{

yarda::CacheLineMapping line(std::uint64_t block,
                             const yarda::CacheGeometry & geometry,
                             std::uint64_t line_offset = 0)
{
  yarda::CacheLineMapping mapping;
  mapping.object_id = "global::oracle";
  mapping.address_basis = yarda::AddressBasis::Absolute;
  mapping.decoded = yarda::decode_cache_address(
    block * geometry.line_size + line_offset, geometry);
  mapping.operation = yarda::AccessOperation::Load;
  return mapping;
}

std::vector<yarda::CacheLineMapping>
trace(std::initializer_list<std::uint64_t> blocks,
      const yarda::CacheGeometry & geometry)
{
  std::vector<yarda::CacheLineMapping> result;
  for (const auto block : blocks)
  {
    result.push_back(line(block, geometry));
  }
  return result;
}

TEST(NaiveExactCsrdOracleTest, ReturnsNoObservationsForEmptyTrace)
{
  const auto result =
    yarda::test::naive_exact_csrd({}, yarda::CacheGeometry{32, 4, 2});

  EXPECT_TRUE(result.empty());
}

TEST(NaiveExactCsrdOracleTest, ClassifiesFirstReferenceAsCold)
{
  const yarda::CacheGeometry geometry{32, 4, 2};

  const auto result =
    yarda::test::naive_exact_csrd(trace({0}, geometry), geometry);

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].outcome, yarda::LruAccessOutcome::ColdMiss);
  EXPECT_FALSE(result[0].distance.has_value());
}

TEST(NaiveExactCsrdOracleTest, ReportsZeroForImmediateReuse)
{
  const yarda::CacheGeometry geometry{32, 4, 2};

  const auto result =
    yarda::test::naive_exact_csrd(trace({0, 0}, geometry), geometry);

  ASSERT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].outcome, yarda::LruAccessOutcome::ColdMiss);
  EXPECT_EQ(result[1].outcome, yarda::LruAccessOutcome::Hit);
  EXPECT_EQ(result[1].distance, 0U);
}

TEST(NaiveExactCsrdOracleTest, TreatsOffsetsWithinOneLineAsOneBlock)
{
  const yarda::CacheGeometry geometry{32, 4, 2};
  const std::vector<yarda::CacheLineMapping> accesses{line(3, geometry, 8),
                                                      line(3, geometry, 24)};

  const auto result = yarda::test::naive_exact_csrd(accesses, geometry);

  ASSERT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].outcome, yarda::LruAccessOutcome::ColdMiss);
  EXPECT_EQ(result[1].distance, 0U);
  EXPECT_EQ(result[1].outcome, yarda::LruAccessOutcome::Hit);
}

TEST(NaiveExactCsrdOracleTest, CountsSameSetCompetitor)
{
  const yarda::CacheGeometry geometry{32, 4, 2};

  const auto result =
    yarda::test::naive_exact_csrd(trace({0, 2, 0}, geometry), geometry);

  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result.back().distance, 1U);
}

TEST(NaiveExactCsrdOracleTest, IgnoresDifferentSetCompetitor)
{
  const yarda::CacheGeometry geometry{32, 4, 2};

  const auto result =
    yarda::test::naive_exact_csrd(trace({0, 1, 0}, geometry), geometry);

  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result.back().distance, 0U);
}

TEST(NaiveExactCsrdOracleTest, CountsRepeatedCompetitorOnce)
{
  const yarda::CacheGeometry geometry{32, 4, 2};

  const auto result =
    yarda::test::naive_exact_csrd(trace({0, 2, 2, 0}, geometry), geometry);

  ASSERT_EQ(result.size(), 4U);
  EXPECT_EQ(result.back().distance, 1U);
}

TEST(NaiveExactCsrdOracleTest, CountsMultiOffsetCompetitorOnce)
{
  const yarda::CacheGeometry geometry{32, 4, 2};
  const std::vector<yarda::CacheLineMapping> accesses{
    line(0, geometry), line(2, geometry), line(2, geometry, 16),
    line(0, geometry)};

  const auto result = yarda::test::naive_exact_csrd(accesses, geometry);

  ASSERT_EQ(result.size(), 4U);
  EXPECT_EQ(result.back().distance, 1U);
  EXPECT_EQ(result.back().outcome, yarda::LruAccessOutcome::Hit);
}

TEST(NaiveExactCsrdOracleTest, MeasuresFromMostRecentPreviousReference)
{
  const yarda::CacheGeometry geometry{32, 2, 2};

  const auto result =
    yarda::test::naive_exact_csrd(trace({0, 1, 0, 2, 0}, geometry), geometry);

  ASSERT_EQ(result.size(), 5U);
  EXPECT_EQ(result[2].distance, 1U);
  EXPECT_EQ(result[4].distance, 1U);
  EXPECT_EQ(result[4].outcome, yarda::LruAccessOutcome::Hit);
}

TEST(NaiveExactCsrdOracleTest, HitsBelowAssociativityBoundary)
{
  const yarda::CacheGeometry geometry{32, 2, 2};

  const auto result =
    yarda::test::naive_exact_csrd(trace({0, 1, 0}, geometry), geometry);

  ASSERT_EQ(result.size(), 3U);
  EXPECT_EQ(result.back().distance, 1U);
  EXPECT_EQ(result.back().outcome, yarda::LruAccessOutcome::Hit);
}

TEST(NaiveExactCsrdOracleTest, MissesAtAssociativityBoundary)
{
  const yarda::CacheGeometry geometry{32, 2, 2};

  const auto result =
    yarda::test::naive_exact_csrd(trace({0, 1, 2, 0}, geometry), geometry);

  ASSERT_EQ(result.size(), 4U);
  EXPECT_EQ(result.back().distance, 2U);
  EXPECT_EQ(result.back().outcome, yarda::LruAccessOutcome::ReplacementMiss);
}

TEST(NaiveExactCsrdOracleTest,
     MissesWhenDistanceReachesAssociativityBelowLineCount)
{
  const yarda::CacheGeometry geometry{32, 4, 2};

  const auto result =
    yarda::test::naive_exact_csrd(trace({0, 2, 4, 0}, geometry), geometry);

  ASSERT_EQ(result.size(), 4U);
  EXPECT_EQ(result.back().distance, 2U);
  EXPECT_EQ(result.back().outcome, yarda::LruAccessOutcome::ReplacementMiss);
}

TEST(NaiveExactCsrdOracleTest, PreservesDistanceAboveAssociativity)
{
  const yarda::CacheGeometry geometry{32, 2, 2};
  std::vector<yarda::CacheLineMapping> accesses;
  accesses.push_back(line(0, geometry));
  for (std::uint64_t block = 1; block <= 17; ++block)
  {
    accesses.push_back(line(block, geometry));
  }
  accesses.push_back(line(0, geometry));

  const auto result = yarda::test::naive_exact_csrd(accesses, geometry);

  ASSERT_EQ(result.size(), 19U);
  EXPECT_EQ(result.back().distance, 17U);
  EXPECT_EQ(result.back().outcome, yarda::LruAccessOutcome::ReplacementMiss);
}

TEST(NaiveExactCsrdOracleTest, RejectsOutOfRangeSetIndex)
{
  const yarda::CacheGeometry geometry{32, 4, 2};
  auto accesses = trace({0}, geometry);
  ASSERT_EQ(accesses.size(), 1U);
  // Set-index consistency also rejects this out-of-range value.
  accesses[0].decoded.set_index = yarda::cache_set_count(geometry);

  EXPECT_THROW(yarda::test::naive_exact_csrd(accesses, geometry),
               std::invalid_argument);
}

TEST(NaiveExactCsrdOracleTest, RejectsInconsistentBlockNumber)
{
  const yarda::CacheGeometry geometry{32, 4, 2};
  auto accesses = trace({0}, geometry);
  ASSERT_EQ(accesses.size(), 1U);
  accesses[0].decoded.block_number += yarda::cache_set_count(geometry);
  ++accesses[0].decoded.tag;

  EXPECT_THROW(yarda::test::naive_exact_csrd(accesses, geometry),
               std::invalid_argument);
}

TEST(NaiveExactCsrdOracleTest, RejectsInconsistentSetIndex)
{
  const yarda::CacheGeometry geometry{32, 4, 2};
  auto accesses = trace({0}, geometry);
  ASSERT_EQ(accesses.size(), 1U);
  ++accesses[0].decoded.set_index;

  EXPECT_THROW(yarda::test::naive_exact_csrd(accesses, geometry),
               std::invalid_argument);
}

TEST(NaiveExactCsrdOracleTest, RejectsInconsistentTag)
{
  const yarda::CacheGeometry geometry{32, 4, 2};
  auto accesses = trace({0}, geometry);
  ASSERT_EQ(accesses.size(), 1U);
  ++accesses[0].decoded.tag;

  EXPECT_THROW(yarda::test::naive_exact_csrd(accesses, geometry),
               std::invalid_argument);
}

TEST(NaiveExactCsrdOracleTest, RejectsInconsistentLineOffset)
{
  const yarda::CacheGeometry geometry{32, 4, 2};
  auto accesses = trace({0}, geometry);
  ASSERT_EQ(accesses.size(), 1U);
  ++accesses[0].decoded.line_offset;

  EXPECT_THROW(yarda::test::naive_exact_csrd(accesses, geometry),
               std::invalid_argument);
}

TEST(NaiveExactCsrdOracleTest, RejectsMappingDecodedForAnotherGeometry)
{
  const yarda::CacheGeometry source_geometry{32, 4, 2};
  const yarda::CacheGeometry supplied_geometry{64, 4, 2};

  EXPECT_THROW(yarda::test::naive_exact_csrd(trace({1}, source_geometry),
                                             supplied_geometry),
               std::invalid_argument);
}

TEST(NaiveExactCsrdOracleTest, RejectsInvalidGeometry)
{
  // Empty input ensures the exception originates inside the oracle.
  EXPECT_THROW(yarda::test::naive_exact_csrd({}, yarda::CacheGeometry{0, 4, 2}),
               std::invalid_argument);
}

}  // namespace
