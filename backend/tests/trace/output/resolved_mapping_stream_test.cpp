#include <limits>
#include <stdexcept>

#include "task_access_stream_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::stream;

class ResolvedMappingStreamTest : public ::testing::TestWithParam<std::uint64_t>
{
};

TEST_P(ResolvedMappingStreamTest, PreservesAllFieldsAndIncreasingAddressOrder)
{
  const CacheGeometry geometry{GetParam(), 8, 2};
  const ResolvedAccess access{"global::A",
                              28,
                              40,
                              0x101c,
                              AddressBasis::Absolute,
                              AccessOperation::Store,
                              7};
  const auto batch = map_cache_lines(access, geometry);
  std::vector<CacheLineMapping> streamed;
  for_each_cache_line(access, geometry, [&](const CacheLineMapping & line) {
    streamed.push_back(line);
  });

  ASSERT_EQ(streamed.size(), batch.size());
  const std::vector<std::uint64_t> expected_addresses =
    GetParam() == 32 ? std::vector<std::uint64_t>{0x101c, 0x1020, 0x1040}
                     : std::vector<std::uint64_t>{0x101c, 0x1040};
  const std::vector<std::uint64_t> expected_offsets =
    GetParam() == 32 ? std::vector<std::uint64_t>{28, 32, 64}
                     : std::vector<std::uint64_t>{28, 64};
  ASSERT_EQ(streamed.size(), expected_addresses.size());
  for (std::size_t index = 0; index < streamed.size(); ++index)
  {
    SCOPED_TRACE(index);
    expect_mapping(streamed[index], batch[index]);
    EXPECT_EQ(streamed[index].decoded.address, expected_addresses[index]);
    EXPECT_EQ(streamed[index].object_byte_offset, expected_offsets[index]);
    EXPECT_EQ(streamed[index].decoded.block_number,
              (GetParam() == 32 ? 128U : 64U) + index);
    EXPECT_EQ(streamed[index].decoded.set_index, index);
    EXPECT_EQ(streamed[index].decoded.tag, GetParam() == 32 ? 32U : 16U);
    EXPECT_EQ(streamed[index].decoded.line_offset, index == 0 ? 28U : 0U);
    EXPECT_EQ(streamed[index].source_object_byte_offset, 28U);
    EXPECT_EQ(streamed[index].source_access_size, 40U);
    EXPECT_EQ(streamed[index].source_linked_byte_address, 0x101cU);
    EXPECT_EQ(streamed[index].source_access_ordinal, 7U);
    EXPECT_EQ(streamed[index].line_span_ordinal, index);
    EXPECT_EQ(streamed[index].object_id, "global::A");
    EXPECT_EQ(streamed[index].operation, AccessOperation::Store);
    EXPECT_EQ(streamed[index].address_basis, AddressBasis::Absolute);
  }
}

TEST_P(ResolvedMappingStreamTest, ExactLineBoundaryEmitsNoExtraRow)
{
  const ResolvedAccess access{
    "global::A",           0, GetParam(), 0x1000, AddressBasis::Absolute,
    AccessOperation::Load, 0};
  std::uint64_t count = 0;
  for_each_cache_line(access, {GetParam(), 8, 2},
                      [&](const CacheLineMapping &) { ++count; });

  EXPECT_EQ(count, 1U);
}

INSTANTIATE_TEST_SUITE_P(LineSizes, ResolvedMappingStreamTest,
                         ::testing::Values(32, 64));

TEST(CacheLineStreamTest, RangeOverloadPreservesProvenanceAndAddressBasis)
{
  const CacheLineAddressRange range{"global::A", 28, 40, 0x101c,
                                    AddressBasis::ImageRelative};
  const auto batch = map_cache_lines(range, {32, 8, 2});
  std::vector<CacheLineMapping> streamed;
  for_each_cache_line(range, {32, 8, 2}, [&](const CacheLineMapping & line) {
    streamed.push_back(line);
  });

  ASSERT_EQ(streamed.size(), 3U);
  ASSERT_EQ(streamed.size(), batch.size());
  for (std::size_t index = 0; index < streamed.size(); ++index)
    expect_mapping(streamed[index], batch[index]);
  EXPECT_EQ(streamed[0].address_basis, AddressBasis::ImageRelative);
  EXPECT_EQ(streamed[0].operation, AccessOperation::Unknown);
}

TEST(CacheLineStreamTest, PropagatesFirstCallbackFailureForHugeSourceSpan)
{
  const CacheLineAddressRange range{"global::A", 0, std::uint64_t{1} << 40, 0,
                                    AddressBasis::Absolute};
  struct Stop
  {
  };
  std::uint64_t count = 0;

  EXPECT_THROW(for_each_cache_line(range, {32, 8, 2},
                                   [&](const CacheLineMapping & line) {
                                     ++count;
                                     EXPECT_EQ(line.decoded.address, 0U);
                                     throw Stop{};
                                   }),
               Stop);
  EXPECT_EQ(count, 1U);
}

TEST(CacheLineStreamTest, SafelyEmitsTheLastRepresentableAddress)
{
  const CacheLineAddressRange range{"global::A", 0, 1,
                                    std::numeric_limits<std::uint64_t>::max(),
                                    AddressBasis::Absolute};
  std::uint64_t count = 0;
  for_each_cache_line(range, {32, 8, 2}, [&](const CacheLineMapping & line) {
    ++count;
    EXPECT_EQ(line.decoded.address, std::numeric_limits<std::uint64_t>::max());
    EXPECT_EQ(line.decoded.line_offset, 31U);
    EXPECT_EQ(line.line_span_ordinal, 0U);
  });

  EXPECT_EQ(count, 1U);
}

class CacheLineStreamInvalidTest : public ::testing::TestWithParam<int>
{
};

TEST_P(CacheLineStreamInvalidTest, RejectsInvalidRangeBeforeAnyCallback)
{
  ResolvedAccess access{
    "global::A",           0, 4, 0x1000, AddressBasis::Absolute,
    AccessOperation::Load, 0};
  CacheGeometry geometry{32, 8, 2};
  switch (GetParam())
  {
    case 0:
      access.object_id = "";
      break;
    case 1:
      access.access_size = 0;
      break;
    case 2:
      geometry.line_size = 0;
      break;
    case 3:
      access.linked_byte_address = UINT64_MAX;
      break;
    case 4:
      access.object_byte_offset = UINT64_MAX;
      break;
  }
  std::uint64_t count = 0;
  const auto sink = [&](const CacheLineMapping &) { ++count; };

  if (GetParam() >= 3)
    EXPECT_THROW(for_each_cache_line(access, geometry, sink),
                 std::overflow_error);
  else
    EXPECT_THROW(for_each_cache_line(access, geometry, sink),
                 std::invalid_argument);
  EXPECT_EQ(count, 0U);
}

INSTANTIATE_TEST_SUITE_P(InputValidation, CacheLineStreamInvalidTest,
                         ::testing::Values(0, 1, 2, 3, 4));

TEST(CacheLineStreamTest, RejectsEmptySinksInEveryOverload)
{
  const ResolvedAccess access{
    "global::A",           0, 4, 0x1000, AddressBasis::Absolute,
    AccessOperation::Load, 0};
  const CacheLineAddressRange range{"global::A", 0, 4, 0x1000,
                                    AddressBasis::Absolute};
  TraceEmissionBudget budget;

  EXPECT_THROW(for_each_cache_line(range, {32, 8, 2}, {}),
               std::invalid_argument);
  EXPECT_THROW(for_each_cache_line(access, {32, 8, 2}, {}),
               std::invalid_argument);
  EXPECT_THROW(for_each_cache_line(access, {32, 8, 2}, {}, budget),
               std::invalid_argument);
}

}  // namespace
