#pragma once

#include <fstream>

#include "../cache/streaming_hierarchy_test_support.hpp"
#include "yarda/elf/object_addresses.hpp"

namespace yarda::test::region
{

/** @brief Load real compiler output and matching linked ELF fixture addresses.
 */
struct Fixture
{
  nlohmann::json raw;
  ObjectAddressModel objects;

  explicit Fixture(const std::string & name)
  {
    const auto path = std::string(YARDA_REGION_FIXTURE_DIR) + "/" + name;
    std::ifstream input(path + ".json");
    input >> raw;
    const auto image = parse_elf_data_regions(path + ".elf");
    if (image.image_type != ElfImageType::Executable || image.image_relative)
      throw std::runtime_error(
        "region fixtures require absolute ET_EXEC addresses");
    objects = build_elf_object_addresses(image);
  }
};

/** @brief Compare one emitted reference with an independently specified access.
 */
inline void expect_access(const ResolvedAccess & actual,
                          const Fixture & fixture, const std::string & object,
                          std::uint64_t offset, std::uint64_t width,
                          AccessOperation operation, std::uint64_t ordinal)
{
  EXPECT_EQ(actual.object_id, object);
  EXPECT_EQ(actual.object_byte_offset, offset);
  EXPECT_EQ(actual.access_size, width);
  EXPECT_EQ(actual.operation, operation);
  EXPECT_EQ(actual.source_access_ordinal, ordinal);
  EXPECT_EQ(actual.linked_byte_address,
            fixture.objects.objects.at(object).base + offset);
  EXPECT_EQ(actual.address_basis, fixture.objects.basis);
}

}  // namespace yarda::test::region
