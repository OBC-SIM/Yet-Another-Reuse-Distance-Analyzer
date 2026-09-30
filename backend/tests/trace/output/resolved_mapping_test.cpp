#include "yarda/trace/resolved_mapping.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

namespace
{

TEST(ResolvedMappingTest, PreservesSourceProvenanceAcrossLineSpan)
{
  const yarda::ResolvedAccess access{
    "global::records",
    60,
    12,
    0x103c,
    yarda::AddressBasis::Absolute,
    yarda::AccessOperation::Store,
    7,
  };

  const auto mappings =
    yarda::map_cache_lines(access, yarda::CacheGeometry{64, 512, 8});

  ASSERT_EQ(mappings.size(), 2);
  EXPECT_EQ(mappings[0].source_object_byte_offset, 60U);
  EXPECT_EQ(mappings[0].source_access_size, 12U);
  EXPECT_EQ(mappings[0].source_linked_byte_address, 0x103cU);
  EXPECT_EQ(mappings[0].operation, yarda::AccessOperation::Store);
  EXPECT_EQ(mappings[0].source_access_ordinal, 7U);
  EXPECT_EQ(mappings[0].line_span_ordinal, 0U);
  EXPECT_EQ(mappings[1].source_access_ordinal, 7U);
  EXPECT_EQ(mappings[1].line_span_ordinal, 1U);
  EXPECT_EQ(mappings[1].object_byte_offset, 64U);
  EXPECT_EQ(mappings[1].decoded.address, 0x1040U);
}

TEST(ResolvedMappingTest, RemapsOneAccessAtDifferentGeometries)
{
  const yarda::ResolvedAccess access{
    "global::bytes",
    28,
    40,
    0x101c,
    yarda::AddressBasis::Absolute,
    yarda::AccessOperation::Load,
    3,
  };

  const auto lines_64 =
    yarda::map_cache_lines(access, yarda::CacheGeometry{64, 8, 2});
  const auto lines_32 =
    yarda::map_cache_lines(access, yarda::CacheGeometry{32, 8, 2});

  ASSERT_EQ(lines_64.size(), 2);
  ASSERT_EQ(lines_32.size(), 3);
  EXPECT_EQ(lines_64[0].source_linked_byte_address, 0x101cU);
  EXPECT_EQ(lines_32[0].source_linked_byte_address, 0x101cU);
  EXPECT_EQ(lines_32[2].line_span_ordinal, 2U);
}

TEST(ResolvedMappingTest, RejectsEmptyObjectIdentity)
{
  const yarda::ResolvedAccess access{"",
                                     0,
                                     4,
                                     0x1000,
                                     yarda::AddressBasis::Absolute,
                                     yarda::AccessOperation::Load,
                                     0};

  EXPECT_THROW(yarda::map_cache_lines(access, {64, 8, 2}),
               std::invalid_argument);
}

TEST(ResolvedMappingTest, RejectsZeroAccessSize)
{
  const yarda::ResolvedAccess access{"global::A",
                                     0,
                                     0,
                                     0x1000,
                                     yarda::AddressBasis::Absolute,
                                     yarda::AccessOperation::Load,
                                     0};

  EXPECT_THROW(yarda::map_cache_lines(access, {64, 8, 2}),
               std::invalid_argument);
}

TEST(ResolvedMappingTest, RejectsSourceObjectOffsetOverflow)
{
  const yarda::ResolvedAccess access{"global::A",
                                     std::numeric_limits<std::uint64_t>::max(),
                                     2,
                                     0x1000,
                                     yarda::AddressBasis::Absolute,
                                     yarda::AccessOperation::Load,
                                     0};

  EXPECT_THROW(yarda::map_cache_lines(access, {64, 8, 2}), std::overflow_error);
}

}  // namespace
