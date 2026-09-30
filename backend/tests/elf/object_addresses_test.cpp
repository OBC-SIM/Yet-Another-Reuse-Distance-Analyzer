#include "yarda/elf/object_addresses.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

namespace
{

TEST(ElfObjectAddressesTest, BuildsCanonicalGlobalObjectIds)
{
  yarda::ElfDataRegions image;
  image.image_relative = true;
  image.symbols = {{"A", 0x1000, 64, 0}, {"B", 0x1040, 32, 0}};

  const auto model = yarda::build_elf_object_addresses(image);

  EXPECT_EQ(model.basis, yarda::AddressBasis::ImageRelative);
  EXPECT_EQ(model.objects.at("global::A").base, 0x1000U);
  EXPECT_EQ(model.objects.at("global::A").size, 64U);
  EXPECT_EQ(model.objects.at("global::B").base, 0x1040U);
}

TEST(ElfObjectAddressesTest, RejectsAmbiguousSymbolNames)
{
  yarda::ElfDataRegions image;
  image.symbols = {{"A", 0x1000, 64, 0}, {"A", 0x2000, 64, 1}};

  EXPECT_THROW(yarda::build_elf_object_addresses(image), std::invalid_argument);
}

TEST(ElfObjectAddressesTest, CollapsesIdenticalSymbolTableDuplicates)
{
  yarda::ElfDataRegions image;
  image.symbols = {{"A", 0x1000, 64, 0}, {"A", 0x1000, 64, 0}};

  const auto model = yarda::build_elf_object_addresses(image);

  ASSERT_EQ(model.objects.size(), 1U);
  EXPECT_EQ(model.objects.at("global::A").base, 0x1000U);
  EXPECT_EQ(model.objects.at("global::A").size, 64U);
}

TEST(ElfObjectAddressesTest, RejectsImagesWithoutObjectSymbols)
{
  EXPECT_THROW(yarda::build_elf_object_addresses({}), std::invalid_argument);
}

}  // namespace
