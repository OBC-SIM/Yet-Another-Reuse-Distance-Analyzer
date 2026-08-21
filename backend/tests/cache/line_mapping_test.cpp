#include "yarda/cache/line_mapping.hpp"

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

  const auto mappings =
    yarda::map_cache_lines("global::A", 0x34, 4, objects, geometry);

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
  yarda::ObjectAddressModel objects;
  objects.objects["global::records"] = {0x1000, 72};

  const auto mappings = yarda::map_cache_lines(
    "global::records", 60, 12, objects, yarda::CacheGeometry{64, 512, 8});

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
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {0x103c, 132};

  const auto mappings = yarda::map_cache_lines(
    "global::A", 0, 132, objects, yarda::CacheGeometry{64, 512, 8});

  ASSERT_EQ(mappings.size(), 3);
  EXPECT_EQ(mappings[0].object_byte_offset, 0U);
  EXPECT_EQ(mappings[1].object_byte_offset, 4U);
  EXPECT_EQ(mappings[2].object_byte_offset, 68U);
  EXPECT_EQ(mappings[2].decoded.address, 0x1080U);
}

TEST(CacheLineMappingTest, KeepsExactBoundaryAccessInOneLine)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {0x1000, 64};

  const auto mappings = yarda::map_cache_lines(
    "global::A", 0, 64, objects, yarda::CacheGeometry{64, 512, 8});

  ASSERT_EQ(mappings.size(), 1);
  EXPECT_EQ(mappings.front().decoded.address, 0x1000U);
}

TEST(CacheLineMappingTest, RejectsAccessPastObjectExtent)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {0x1000, 16};

  EXPECT_THROW(yarda::map_cache_lines("global::A", 14, 4, objects,
                                      yarda::CacheGeometry{64, 512, 8}),
               std::invalid_argument);
}

TEST(CacheLineMappingTest, RejectsUnknownObject)
{
  const yarda::ObjectAddressModel objects;

  EXPECT_THROW(yarda::map_cache_lines("global::missing", 0, 4, objects,
                                      yarda::CacheGeometry{64, 512, 8}),
               std::invalid_argument);
}

TEST(CacheLineMappingTest, RejectsReconstructedAddressOverflow)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {std::numeric_limits<std::uint64_t>::max(), 2};

  EXPECT_THROW(yarda::map_cache_lines("global::A", 1, 1, objects,
                                      yarda::CacheGeometry{64, 512, 8}),
               std::overflow_error);
}

TEST(CacheLineMappingTest, RejectsAccessEndAddressOverflow)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {
    std::numeric_limits<std::uint64_t>::max() - 1, 3};

  EXPECT_THROW(yarda::map_cache_lines("global::A", 0, 3, objects,
                                      yarda::CacheGeometry{64, 512, 8}),
               std::overflow_error);
}

}  // namespace
