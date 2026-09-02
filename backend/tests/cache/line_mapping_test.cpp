#include "yarda/cache/line_mapping.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

namespace
{

TEST(CacheLineMappingTest, MapsObjectOffsetToDecodedCacheAddress)
{
  const yarda::CacheGeometry geometry{64, 512, 8};
  const yarda::CacheLineAddressRange range{"global::A", 0x34, 4, 0x1034,
                                           yarda::AddressBasis::Absolute};

  const auto mappings = yarda::map_cache_lines(range, geometry);

  ASSERT_EQ(mappings.size(), 1);
  const auto & mapping = mappings.front();
  EXPECT_EQ(mapping.object_id, "global::A");
  EXPECT_EQ(mapping.object_byte_offset, 0x34U);
  EXPECT_EQ(mapping.decoded.address, 0x1034U);
  EXPECT_EQ(mapping.decoded.tag, 1U);
  EXPECT_EQ(mapping.decoded.set_index, 0U);
  EXPECT_EQ(mapping.decoded.line_offset, 0x34U);
}

TEST(CacheLineMappingTest, MapsEveryLineTouchedByStructSizedAccess)
{
  const yarda::CacheLineAddressRange range{"global::records", 60, 12, 0x103c,
                                           yarda::AddressBasis::Absolute};

  const auto mappings =
    yarda::map_cache_lines(range, yarda::CacheGeometry{64, 512, 8});

  ASSERT_EQ(mappings.size(), 2);
  EXPECT_EQ(mappings[0].object_byte_offset, 60U);
  EXPECT_EQ(mappings[0].decoded.address, 0x103cU);
  EXPECT_EQ(mappings[0].decoded.line_offset, 0x3cU);
  EXPECT_EQ(mappings[1].object_byte_offset, 64U);
  EXPECT_EQ(mappings[1].decoded.address, 0x1040U);
  EXPECT_EQ(mappings[1].decoded.line_offset, 0U);
}

TEST(CacheLineMappingTest, MapsAccessWiderThanTwoLines)
{
  const yarda::CacheLineAddressRange range{"global::A", 0, 132, 0x103c,
                                           yarda::AddressBasis::Absolute};

  const auto mappings =
    yarda::map_cache_lines(range, yarda::CacheGeometry{64, 512, 8});

  ASSERT_EQ(mappings.size(), 3);
  EXPECT_EQ(mappings[0].object_byte_offset, 0U);
  EXPECT_EQ(mappings[1].object_byte_offset, 4U);
  EXPECT_EQ(mappings[2].object_byte_offset, 68U);
  EXPECT_EQ(mappings[2].decoded.address, 0x1080U);
}

TEST(CacheLineMappingTest, KeepsExactBoundaryAccessInOneLine)
{
  const yarda::CacheLineAddressRange range{"global::A", 0, 64, 0x1000,
                                           yarda::AddressBasis::Absolute};

  const auto mappings =
    yarda::map_cache_lines(range, yarda::CacheGeometry{64, 512, 8});

  ASSERT_EQ(mappings.size(), 1);
  EXPECT_EQ(mappings.front().decoded.address, 0x1000U);
}

TEST(CacheLineMappingTest, RejectsLinkedAddressRangeOverflow)
{
  const yarda::CacheLineAddressRange range{
    "global::A", 0, 4, std::numeric_limits<std::uint64_t>::max() - 1,
    yarda::AddressBasis::Absolute};

  EXPECT_THROW(yarda::map_cache_lines(range, {64, 8, 2}), std::overflow_error);
}

}  // namespace
