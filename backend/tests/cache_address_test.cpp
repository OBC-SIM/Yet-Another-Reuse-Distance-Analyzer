#include "yarda/cache_address.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

namespace
{

TEST(CacheAddressTest, DecodesTagIndexAndOffset)
{
  const yarda::CacheGeometry geometry{64, 512, 8};

  const auto decoded = yarda::decode_cache_address(0x1234, geometry);

  EXPECT_EQ(decoded.address, 0x1234U);
  EXPECT_EQ(decoded.block_number, 0x48U);
  EXPECT_EQ(decoded.tag, 1U);
  EXPECT_EQ(decoded.set_index, 8U);
  EXPECT_EQ(decoded.line_offset, 0x34U);
}

TEST(CacheAddressTest, KeepsOffsetsInOneCacheLineIdentity)
{
  const yarda::CacheGeometry geometry{64, 512, 8};
  const auto first = yarda::decode_cache_address(0x1030, geometry);
  const auto second = yarda::decode_cache_address(0x1038, geometry);

  EXPECT_EQ((yarda::CacheLineId{first.tag, first.set_index}),
            (yarda::CacheLineId{second.tag, second.set_index}));
  EXPECT_NE(first.line_offset, second.line_offset);
}

TEST(CacheAddressTest, RejectsInvalidCacheGeometry)
{
  EXPECT_THROW(yarda::cache_set_count({0, 512, 8}), std::invalid_argument);
  EXPECT_THROW(yarda::cache_set_count({64, 500, 8}), std::invalid_argument);
  EXPECT_THROW(yarda::cache_set_count({64, 512, 3}), std::invalid_argument);
  EXPECT_THROW(yarda::cache_set_count({64, 8, 16}), std::invalid_argument);
}

}  // namespace
