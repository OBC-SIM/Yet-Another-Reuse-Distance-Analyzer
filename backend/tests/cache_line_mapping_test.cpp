#include "yarda/cache_line_mapping.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

namespace
{

TEST(CacheLineMappingTest, MapsObjectOffsetToDecodedCacheAddress)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {0x1000, 128};
  const yarda::CacheGeometry geometry{64, 512, 8};

  const auto mapping =
    yarda::map_cache_line("global::A", 0x34, 4, objects, geometry);

  EXPECT_EQ(mapping.object_id, "global::A");
  EXPECT_EQ(mapping.object_byte_offset, 0x34U);
  EXPECT_EQ(mapping.decoded.address, 0x1034U);
  EXPECT_EQ(mapping.decoded.tag, 1U);
  EXPECT_EQ(mapping.decoded.set_index, 0U);
  EXPECT_EQ(mapping.decoded.line_offset, 0x34U);
}

TEST(CacheLineMappingTest, RejectsAccessPastObjectExtent)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {0x1000, 16};

  EXPECT_THROW(yarda::map_cache_line("global::A", 14, 4, objects,
                                     yarda::CacheGeometry{64, 512, 8}),
               std::invalid_argument);
}

TEST(CacheLineMappingTest, RejectsUnknownObject)
{
  const yarda::ObjectAddressModel objects;

  EXPECT_THROW(yarda::map_cache_line("global::missing", 0, 4, objects,
                                     yarda::CacheGeometry{64, 512, 8}),
               std::invalid_argument);
}

TEST(CacheLineMappingTest, RejectsReconstructedAddressOverflow)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {
    std::numeric_limits<std::uint64_t>::max(), 2};

  EXPECT_THROW(yarda::map_cache_line("global::A", 1, 1, objects,
                                     yarda::CacheGeometry{64, 512, 8}),
               std::overflow_error);
}

}  // namespace
