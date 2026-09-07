#include <limits>
#include <stdexcept>

#include "exact_csrd_arithmetic.hpp"
#include "exact_csrd_test_support.hpp"
#include "hierarchy_lru_oracle_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::support;
using detail::RecencyIndex;
using detail::RecencyIndexTestAccess;

TEST(RecencyIndexTest, EmptyIndexHasNoActiveSlots)
{
  RecencyIndex index(4);
  EXPECT_EQ(RecencyIndexTestAccess::storage(index).active, 0U);
  RecencyIndexTestAccess::expect_invariants(index);
}

TEST(RecencyIndexTest, NewKeyHasNoDistanceAndOneActiveSlot)
{
  RecencyIndex index(4);
  EXPECT_FALSE(index.observe(99));
  EXPECT_EQ(RecencyIndexTestAccess::storage(index).active, 1U);
  RecencyIndexTestAccess::expect_invariants(index);
}

struct CompactionCase
{
  const char * name;
  std::uint64_t key;
  std::optional<std::uint64_t> distance;
};

class RecencyCompactionTest : public testing::TestWithParam<CompactionCase>
{
};

TEST_P(RecencyCompactionTest, FirstCompactionPreservesEveryNextKeyDistance)
{
  RecencyIndex index(4);
  for (const auto key : {31U, 17U, 99U, 17U}) index.observe(key);
  const auto before = RecencyIndexTestAccess::storage(index);
  ASSERT_EQ(before.capacity, 4U);
  ASSERT_EQ(before.next_slot, 5U);
  EXPECT_EQ(index.observe(GetParam().key), GetParam().distance);
  const auto after = RecencyIndexTestAccess::storage(index);
  EXPECT_EQ(after.capacity, 6U);
  EXPECT_EQ(after.next_slot, 5U);
  EXPECT_EQ(after.active, GetParam().distance ? 3U : 4U);
  RecencyIndexTestAccess::expect_invariants(index);
}

INSTANTIATE_TEST_SUITE_P(NextKey, RecencyCompactionTest,
                         testing::Values(CompactionCase{"Oldest", 31, 2},
                                         CompactionCase{"Newest", 17, 0},
                                         CompactionCase{"Middle", 99, 1},
                                         CompactionCase{"Unseen", 5,
                                                        std::nullopt}),
                         csrd_case_name<CompactionCase>);

class RecencyFixedDomainTest : public testing::TestWithParam<std::uint64_t>
{
};

TEST_P(RecencyFixedDomainTest, RepeatedCompactionsKeepFixedDomainStorageBounded)
{
  const auto domain = GetParam();
  RecencyIndex index(2);
  std::uint64_t compactions = 0;
  for (std::uint64_t i = 0; i < 20000; ++i)
  {
    const auto before = RecencyIndexTestAccess::storage(index);
    if (before.next_slot > before.capacity) ++compactions;
    const auto distance = index.observe(i % domain);
    if (i < domain)
      ASSERT_FALSE(distance);
    else
      ASSERT_EQ(distance, domain - 1);
    const auto after = RecencyIndexTestAccess::storage(index);
    ASSERT_LE(after.capacity, 2 * domain);
    ASSERT_LE(after.allocated_elements, 2 * domain + 1);
  }
  EXPECT_GT(compactions, 100U);
  EXPECT_EQ(RecencyIndexTestAccess::storage(index).active, domain);
  RecencyIndexTestAccess::expect_invariants(index);
}

INSTANTIATE_TEST_SUITE_P(Keys, RecencyFixedDomainTest, testing::Values(1U, 2U));

TEST(RecencyIndexTest, GrowingHistoryMatchesNaiveDistanceAcrossCompactions)
{
  const CacheGeometry geometry{32, 2, 2};
  ASSERT_EQ(cache_set_count(geometry), 1U) << "oracle parity requires a single "
                                              "set";
  std::vector<CacheLineMapping> mappings;
  for (std::uint64_t key = 0; key < 64; ++key)
  {
    for (const auto block : {key, std::uint64_t{0}, key / 2})
      mappings.push_back(make_mapping(block, geometry, mappings.size()));
  }
  const auto expected = test::naive_exact_csrd(mappings, geometry);
  RecencyIndex index(2);
  std::uint64_t compactions = 0;
  for (std::size_t i = 0; i < mappings.size(); ++i)
  {
    SCOPED_TRACE("reference=" + std::to_string(i));
    const auto before = RecencyIndexTestAccess::storage(index);
    if (before.next_slot > before.capacity) ++compactions;
    EXPECT_EQ(index.observe(mappings[i].decoded.block_number),
              expected[i].distance);
    const auto after = RecencyIndexTestAccess::storage(index);
    EXPECT_LE(after.capacity, std::max<std::uint64_t>(2, 2 * after.active));
    RecencyIndexTestAccess::expect_invariants(index);
  }
  EXPECT_GT(compactions, 1U);
}

TEST(RecencyIndexTest, GrowingColdDomainRetainsOneEntryPerHistoricalKey)
{
  RecencyIndex index(2);
  for (std::uint64_t key = 0; key < 4096; ++key)
  {
    ASSERT_FALSE(index.observe(key));
    const auto state = RecencyIndexTestAccess::storage(index);
    ASSERT_EQ(state.active, key + 1);
    ASSERT_LE(state.capacity, 2 * state.active);
    ASSERT_LE(state.allocated_elements, 2 * state.active + 1);
  }
  RecencyIndexTestAccess::expect_invariants(index);
}

TEST(RecencyIndexTest, SlotCeilingDoesNotLimitTotalReferences)
{
  RecencyIndex index(2, 2);
  EXPECT_FALSE(index.observe(0));
  for (int i = 0; i < 1000; ++i) ASSERT_EQ(index.observe(0), 0U);
  EXPECT_EQ(RecencyIndexTestAccess::storage(index).capacity, 2U);
}

TEST(RecencyIndexTest, RejectsZeroCapacityFloor)
{
  EXPECT_THROW(RecencyIndex{0}, std::invalid_argument);
}

TEST(RecencyIndexTest, RejectsFloorAboveSlotCeiling)
{
  EXPECT_THROW((RecencyIndex{5, 4}), std::invalid_argument);
}

TEST(RecencyIndexTest, RejectsSentinelSizeOverflowBeforeAllocation)
{
  EXPECT_THROW(RecencyIndex{std::numeric_limits<std::uint64_t>::max()},
               std::overflow_error);
}

TEST(RecencyIndexTest, RejectsCompactionCapacityBeyondInjectedCeiling)
{
  RecencyIndex index(4, 4);
  for (const auto key : {0U, 1U, 2U, 3U}) index.observe(key);
  EXPECT_THROW(index.observe(0), std::overflow_error);
}

TEST(CsrdArithmeticTest, AcceptsSumExactlyAtUint64Maximum)
{
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  EXPECT_EQ(detail::csrd_checked_add(maximum - 1, 1), maximum);
}

TEST(CsrdArithmeticTest, RejectsUint64AdditionOverflow)
{
  EXPECT_THROW(
    detail::csrd_checked_add(std::numeric_limits<std::uint64_t>::max(), 1),
    std::overflow_error);
}

TEST(CsrdArithmeticTest, RejectsOperandAlreadyAboveInjectedBound)
{
  EXPECT_THROW(detail::csrd_checked_add(5, 0, 4), std::overflow_error);
}

TEST(CsrdArithmeticTest, AcceptsStorageExactlyAtContainerLimit)
{
  EXPECT_EQ(detail::csrd_storage_size(8, 8), 8U);
}

TEST(CsrdArithmeticTest, RejectsStorageAboveContainerLimit)
{
  EXPECT_THROW(detail::csrd_storage_size(9, 8), std::overflow_error);
}

TEST(CsrdArithmeticTest, RejectsUnrepresentableOrUnallocatableStorage)
{
  EXPECT_THROW(
    detail::csrd_storage_size(std::numeric_limits<std::uint64_t>::max(),
                              std::vector<std::uint64_t>{}.max_size()),
    std::overflow_error);
}

}  // namespace
