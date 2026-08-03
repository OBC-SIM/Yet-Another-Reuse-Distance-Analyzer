#include "yarda/cache_line_mapping_json.hpp"

#include <gtest/gtest.h>

namespace
{

TEST(CacheLineMappingJsonTest, SerializesRowsInDeterministicObjectOrder)
{
  yarda::CacheLineMappingTable table;
  table[{"global::B", 8}] = {
    "global::B", 8, yarda::AddressBasis::ImageRelative,
    {0x1048, 65, 1, 1, 8}};
  table[{"global::A", 0}] = {
    "global::A", 0, yarda::AddressBasis::ImageRelative,
    {0x1000, 64, 1, 0, 0}};

  const auto payload = yarda::cache_line_mapping_json(table);

  ASSERT_EQ(payload.size(), 2);
  EXPECT_EQ(payload[0]["object"], "global::A");
  EXPECT_EQ(payload[0]["tag"], 1);
  EXPECT_EQ(payload[0]["index"], 0);
  EXPECT_EQ(payload[0]["offset"], 0);
  EXPECT_EQ(payload[0]["address_basis"], "image-relative");
  EXPECT_EQ(payload[1]["object"], "global::B");
}

}  // namespace
