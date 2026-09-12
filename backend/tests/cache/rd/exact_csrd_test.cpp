#include <limits>
#include <stdexcept>
#include <tuple>

#include "exact_csrd_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::support;
constexpr CacheGeometry kOneSet{32, 2, 2};

TEST(ExactCsrdTest, EmptyStreamHasZeroCountsAndNoHistogram)
{
  ExactCsrdAnalyzer analyzer(kOneSet);
  expect_csrd_summary(analyzer.summary(), {});
  EXPECT_EQ(detail::ExactCsrdTestAccess::set_count(analyzer), 0U);
}

TEST(ExactCsrdTest, FirstObservationHasNoFiniteDistance)
{
  ExactCsrdAnalyzer analyzer(kOneSet);
  const auto observation = observe_block(analyzer, kOneSet, 7);
  EXPECT_EQ(observation.outcome, LruAccessOutcome::ColdMiss);
  EXPECT_FALSE(observation.distance);
  expect_csrd_summary(analyzer.summary(), {1, 0, 1, 0, 1, {}});
}

TEST(ExactCsrdTest, ImmediateReuseHitsAtDistanceZero)
{
  ExactCsrdAnalyzer analyzer(kOneSet);
  observe_block(analyzer, kOneSet, 7);
  const auto observation = observe_block(analyzer, kOneSet, 7);
  EXPECT_EQ(observation.outcome, LruAccessOutcome::Hit);
  EXPECT_EQ(observation.distance, 0U);
  expect_csrd_summary(analyzer.summary(), {2, 1, 1, 0, 1, {{0, 1}}});
}

TEST(ExactCsrdTest, RepeatedCompetitorContributesOnlyOneToDistance)
{
  ExactCsrdAnalyzer analyzer(kOneSet);
  for (const auto block : {0U, 1U, 1U, 1U})
    observe_block(analyzer, kOneSet, block);
  const auto observation = observe_block(analyzer, kOneSet, 0);
  EXPECT_EQ(observation.outcome, LruAccessOutcome::Hit);
  EXPECT_EQ(observation.distance, 1U);
  expect_csrd_summary(analyzer.summary(), {5, 3, 2, 0, 2, {{0, 2}, {1, 1}}});
}

TEST(ExactCsrdTest, OtherSetsDoNotContributeToDistance)
{
  const CacheGeometry geometry{32, 4, 2};
  ExactCsrdAnalyzer analyzer(geometry);
  for (const auto block : {0U, 1U, 3U, 5U})
    observe_block(analyzer, geometry, block);
  const auto observation = observe_block(analyzer, geometry, 0);
  EXPECT_EQ(observation.outcome, LruAccessOutcome::Hit);
  EXPECT_EQ(observation.distance, 0U);
  EXPECT_EQ(observe_block(analyzer, geometry, 1).distance, 2U);
}

TEST(ExactCsrdTest, ByteOffsetsShareOneBlockIdentity)
{
  ExactCsrdAnalyzer analyzer(kOneSet);
  observe_block(analyzer, kOneSet, 3, 1);
  const auto observation = observe_block(analyzer, kOneSet, 3, 31);
  EXPECT_EQ(observation.outcome, LruAccessOutcome::Hit);
  EXPECT_EQ(observation.distance, 0U);
  EXPECT_EQ(analyzer.summary().unique_lines, 1U);
}

TEST(ExactCsrdTest, RetainsDistanceSeventeenForAnEvictedTwoWayLine)
{
  ExactCsrdAnalyzer analyzer(kOneSet);
  for (std::uint64_t block = 0; block <= 17; ++block)
    observe_block(analyzer, kOneSet, block);
  const auto observation = observe_block(analyzer, kOneSet, 0);
  EXPECT_EQ(observation.outcome, LruAccessOutcome::ReplacementMiss);
  EXPECT_EQ(observation.distance, 17U);
  expect_csrd_summary(analyzer.summary(), {19, 0, 18, 1, 18, {{17, 1}}});
}

TEST(ExactCsrdTest, IndependentInstancesStartColdAndKeepSeparateRecency)
{
  ExactCsrdAnalyzer first(kOneSet);
  ExactCsrdAnalyzer second(kOneSet);
  observe_block(first, kOneSet, 0);
  EXPECT_EQ(observe_block(second, kOneSet, 0).outcome,
            LruAccessOutcome::ColdMiss);
  observe_block(first, kOneSet, 1);
  EXPECT_EQ(observe_block(second, kOneSet, 0).distance, 0U);
  EXPECT_EQ(observe_block(first, kOneSet, 0).distance, 1U);
  EXPECT_EQ(first.summary().unique_lines, 2U);
  EXPECT_EQ(second.summary().unique_lines, 1U);
}

TEST(ExactCsrdTest, SummaryViewTracksSubsequentObservations)
{
  ExactCsrdAnalyzer analyzer(kOneSet);
  const auto & view = analyzer.summary();
  observe_block(analyzer, kOneSet, 0);
  EXPECT_EQ(view.lookups, 1U);
  observe_block(analyzer, kOneSet, 0);
  EXPECT_EQ(view.hits, 1U);
}

TEST(ExactCsrdTest, HugeGeometryAllocatesOnlyTouchedSets)
{
  const CacheGeometry geometry{1, std::uint64_t{1} << 63, 1};
  ExactCsrdAnalyzer analyzer(geometry);
  observe_block(analyzer, geometry, 0);
  observe_block(analyzer, geometry, std::numeric_limits<std::uint64_t>::max());
  EXPECT_EQ(detail::ExactCsrdTestAccess::set_count(analyzer), 2U);
  EXPECT_EQ(analyzer.summary().unique_lines, 2U);
}

class ExactCsrdBoundaryTest
  : public testing::TestWithParam<std::tuple<std::uint64_t, bool>>
{
};

TEST_P(ExactCsrdBoundaryTest, ReuseHitsOnlyBelowAssociativity)
{
  const auto [ways, is_hit] = GetParam();
  const CacheGeometry geometry{32, ways, ways};
  const auto distance = is_hit ? ways - 1 : ways;
  ExactCsrdAnalyzer analyzer(geometry);
  for (std::uint64_t block = 0; block <= distance; ++block)
    observe_block(analyzer, geometry, block);
  const auto observation = observe_block(analyzer, geometry, 0);
  EXPECT_EQ(observation.distance, distance);
  EXPECT_EQ(observation.outcome,
            is_hit ? LruAccessOutcome::Hit : LruAccessOutcome::ReplacementMiss);
}

INSTANTIATE_TEST_SUITE_P(Ways, ExactCsrdBoundaryTest,
                         testing::Combine(testing::Values(1U, 2U, 4U, 8U),
                                          testing::Bool()));

class ExactCsrdGeometryTest : public testing::TestWithParam<CacheGeometry>
{
};

TEST_P(ExactCsrdGeometryTest, RejectsInvalidGeometryAtConstruction)
{
  EXPECT_THROW(ExactCsrdAnalyzer{GetParam()}, std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(Invalid, ExactCsrdGeometryTest,
                         testing::Values(CacheGeometry{0, 4, 2},
                                         CacheGeometry{24, 4, 2},
                                         CacheGeometry{32, 3, 1},
                                         CacheGeometry{32, 4, 3},
                                         CacheGeometry{32, 2, 4}));

TEST(ExactCsrdTest, RejectsBlockNumberInconsistentWithAddress)
{
  ExactCsrdAnalyzer analyzer(kOneSet);
  auto address = decode_cache_address(64, kOneSet);
  // Keep the other decoded relations coherent to isolate address/block
  // validation.
  address.block_number = 3;
  address.tag = 3;
  EXPECT_THROW(analyzer.observe(address), std::invalid_argument);
}

struct DecodedFieldCase
{
  const char * name;
  std::uint64_t DecodedCacheAddress::*field;
};

class ExactCsrdAddressTest : public testing::TestWithParam<DecodedFieldCase>
{
};

TEST_P(ExactCsrdAddressTest, RejectsInconsistentDecodedAddress)
{
  ExactCsrdAnalyzer analyzer(kOneSet);
  auto address = decode_cache_address(64, kOneSet);
  ++(address.*GetParam().field);
  EXPECT_THROW(analyzer.observe(address), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
  Fields, ExactCsrdAddressTest,
  testing::Values(DecodedFieldCase{"Address", &DecodedCacheAddress::address},
                  DecodedFieldCase{"Block", &DecodedCacheAddress::block_number},
                  DecodedFieldCase{"Tag", &DecodedCacheAddress::tag},
                  DecodedFieldCase{"Set", &DecodedCacheAddress::set_index},
                  DecodedFieldCase{"Offset",
                                   &DecodedCacheAddress::line_offset}),
  csrd_case_name<DecodedFieldCase>);

struct CounterCase
{
  const char * name;
  std::uint64_t ExactCsrdSummary::*field;
  std::uint64_t next_block;
};

class ExactCsrdOverflowTest : public testing::TestWithParam<CounterCase>
{
};

TEST_P(ExactCsrdOverflowTest, RejectsCounterIncrementBeyondUint64)
{
  const CacheGeometry geometry{32, 1, 1};
  ExactCsrdAnalyzer analyzer(geometry);
  observe_block(analyzer, geometry, 0);
  observe_block(analyzer, geometry, 1);
  auto & counters = detail::ExactCsrdTestAccess::counters(analyzer);
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  if (GetParam().field)
    counters.*GetParam().field = maximum;
  else
    counters.histogram[0] = maximum;
  EXPECT_THROW(observe_block(analyzer, geometry, GetParam().next_block),
               std::overflow_error);
}

INSTANTIATE_TEST_SUITE_P(
  Counters, ExactCsrdOverflowTest,
  testing::Values(CounterCase{"Lookups", &ExactCsrdSummary::lookups, 2},
                  CounterCase{"Cold", &ExactCsrdSummary::cold_misses, 2},
                  CounterCase{"Unique", &ExactCsrdSummary::unique_lines, 2},
                  CounterCase{"Hits", &ExactCsrdSummary::hits, 1},
                  CounterCase{"Replacement",
                              &ExactCsrdSummary::replacement_misses, 0},
                  CounterCase{"Histogram", nullptr, 1}),
  csrd_case_name<CounterCase>);

}  // namespace
