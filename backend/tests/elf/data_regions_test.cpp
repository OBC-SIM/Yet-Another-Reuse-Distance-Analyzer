#include "yarda/elf/data_regions.hpp"

#include <algorithm>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

namespace
{

using yarda::ElfDataRegion;
using yarda::ElfDataRegionKind;
using yarda::ElfDataRegions;
using yarda::ElfImageType;
using yarda::ElfRegionOrigin;

const ElfDataRegion & find_region(const ElfDataRegions & image,
                                  const std::string & name)
{
  const auto found =
    std::find_if(image.regions.begin(), image.regions.end(),
                 [&](const auto & region) { return region.name == name; });
  if (found == image.regions.end())
  {
    throw std::runtime_error("missing fixture region: " + name);
  }
  return *found;
}

TEST(ElfDataRegionsTest, ParsesAllocatedNonExecutableSections)
{
  const auto image = yarda::parse_elf_data_regions(YARDA_ELF_PIE_FIXTURE_PATH);

  EXPECT_EQ(image.address_size, 8);
  EXPECT_EQ(image.image_type, ElfImageType::SharedObject);
  EXPECT_TRUE(image.image_relative);
  EXPECT_FALSE(image.used_program_headers);
  EXPECT_FALSE(image.regions.empty());
  EXPECT_TRUE(
    std::none_of(image.regions.begin(), image.regions.end(),
                 [](const auto & region) { return region.executable; }));
}

TEST(ElfDataRegionsTest, ClassifiesInitializedAndBssSections)
{
  const auto image = yarda::parse_elf_data_regions(YARDA_ELF_PIE_FIXTURE_PATH);
  const auto & data = find_region(image, ".data");
  const auto & bss = find_region(image, ".bss");

  EXPECT_EQ(data.kind, ElfDataRegionKind::Initialized);
  EXPECT_TRUE(data.writable);
  EXPECT_GT(data.file_size, 0U);
  EXPECT_EQ(data.file_size, data.memory_size);

  EXPECT_EQ(bss.kind, ElfDataRegionKind::ZeroInitialized);
  EXPECT_EQ(bss.file_size, 0U);
  EXPECT_GT(bss.memory_size, 0U);
  EXPECT_TRUE(std::none_of(
    image.regions.begin(), image.regions.end(), [](const auto & region) {
      return region.name == ".tdata" || region.name == ".tbss";
    }));
}

TEST(ElfDataRegionsTest, MapsObjectSymbolsAndIgnoresTls)
{
  const auto image = yarda::parse_elf_data_regions(YARDA_ELF_PIE_FIXTURE_PATH);
  const auto symbol = [&](const std::string & name) {
    return std::find_if(
      image.symbols.begin(), image.symbols.end(),
      [&](const auto & candidate) { return candidate.name == name; });
  };

  const auto data = symbol("yarda_data_value");
  const auto tls = symbol("yarda_tls_value");
  ASSERT_NE(data, image.symbols.end());
  EXPECT_EQ(tls, image.symbols.end());
  EXPECT_EQ(image.regions[data->region_index].name, ".data");
  EXPECT_EQ(data->size, sizeof(int));
}

TEST(ElfDataRegionsTest, ParsesExecutableImageType)
{
  const auto image = yarda::parse_elf_data_regions(YARDA_ELF_EXEC_FIXTURE_PATH);

  EXPECT_EQ(image.image_type, ElfImageType::Executable);
  EXPECT_FALSE(image.image_relative);
}

TEST(ElfDataRegionsTest, FallsBackToProgramHeadersWithoutSections)
{
  const auto image =
    yarda::parse_elf_data_regions(YARDA_ELF_SECTIONLESS_FIXTURE_PATH);

  EXPECT_TRUE(image.used_program_headers);
  EXPECT_FALSE(image.regions.empty());
  EXPECT_TRUE(std::all_of(image.regions.begin(), image.regions.end(),
                          [](const auto & region) {
                            return region.origin == ElfRegionOrigin::Segment;
                          }));
  for (std::size_t index = 1; index < image.regions.size(); ++index)
  {
    const auto & previous = image.regions[index - 1];
    const auto & current = image.regions[index];
    EXPECT_LE(previous.virtual_address + previous.memory_size,
              current.virtual_address);
  }
  EXPECT_TRUE(image.symbols.empty());
}

TEST(ElfDataRegionsTest, RejectsNonElfAndRelocatableInput)
{
  EXPECT_THROW(yarda::parse_elf_data_regions("/dev/null"),
               std::invalid_argument);
  EXPECT_THROW(
    yarda::parse_elf_data_regions(YARDA_ELF_RELOCATABLE_FIXTURE_PATH),
    std::invalid_argument);
}

}  // namespace
