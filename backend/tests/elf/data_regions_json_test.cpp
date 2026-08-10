#include "yarda/elf/data_regions_json.hpp"

#include <gtest/gtest.h>

#include "yarda/elf/data_regions.hpp"

namespace
{

TEST(ElfDataRegionsJsonTest, SerializesStableVersionedContract)
{
  const auto image = yarda::parse_elf_data_regions(YARDA_ELF_PIE_FIXTURE_PATH);
  const auto payload = yarda::elf_data_regions_json(image);

  EXPECT_EQ(payload["schema_version"], 1);
  EXPECT_EQ(payload["image"]["type"], "shared-object");
  EXPECT_EQ(payload["image"]["endianness"], "little");
  EXPECT_TRUE(payload["image"]["relative_addresses"]);
  ASSERT_FALSE(payload["regions"].empty());
  EXPECT_TRUE(payload["regions"][0].contains("virtual_address"));
  EXPECT_FALSE(payload["regions"][0].contains("thread_local"));
  ASSERT_FALSE(payload["symbols"].empty());
  EXPECT_TRUE(payload["symbols"][0].contains("virtual_address"));
  EXPECT_FALSE(payload["symbols"][0].contains("value_kind"));
  EXPECT_EQ(payload.dump(), yarda::elf_data_regions_json(image).dump());
}

}  // namespace
