#include <fstream>
#include <gtest/gtest.h>
#include <map>

#include "../cache/streaming_hierarchy_test_support.hpp"
#include "yarda/elf/object_addresses.hpp"

namespace yarda::test
{
namespace
{

class AffineIndexContract : public testing::TestWithParam<const char *>
{
protected:
  nlohmann::json raw;
  ObjectAddressModel objects;

  void SetUp() override
  {
    const auto stem = std::string(YARDA_AFFINE_FIXTURE_DIR) + "/" + GetParam();
    std::ifstream input(stem + "_ape.json");
    ASSERT_TRUE(input.good());
    input >> raw;
    const auto image = parse_elf_data_regions(stem + ".elf");
    ASSERT_EQ(image.image_type, ElfImageType::Executable);
    ASSERT_FALSE(image.image_relative);
    objects = build_elf_object_addresses(image);
  }
};

TEST_P(AffineIndexContract, MatchesSourceElementOrderAndLinkedByteAddresses)
{
  // Independent source-level sequences; none are obtained from the LAT.
  const std::map<std::string, std::vector<uint64_t>> elements{
    {"offset", {1, 2, 3}},
    {"stride", {2, 4, 6}},
    {"descending", {4, 3, 2, 1, 0}},
    {"constant", {3, 3, 3}},
    {"dimensions", {0, 1, 2, 3, 4, 5}},
    {"caller", {0, 1, 0, 2, 0, 3}},
    {"fields", {1, 2, 3}},
    {"shadowed", {5, 0, 1}},
    {"capture", {0, 0, 1, 1, 2, 2}},
    {"unsigned_caller", {1, 2, 3}},
    {"unsigned_forward", {1, 2, 3}},
    {"unsigned_boundary", {383, 384, 511}},
    {"scaled_left", {0, 2, 4, 6}},
    {"scaled_right", {0, 2, 4, 6}},
    {"chained", {0, 4, 8, 12}},
    {"nested_product", {0, 4, 8, 12}},
    {"constant_product", {0, 4, 8, 12}},
    {"temporary", {0, 2, 4, 6}},
    {"scaled_stride", {5, 9, 13}},
    {"scaled_descending", {11, 7, 3}},
    {"negative_coefficient", {7, 6, 5, 4}},
    {"flattened", {0, 1, 2, 8, 9, 10}},
    {"scaled_fields", {0, 2}},
    {"affine_actual", {3, 5, 7}},
    {"affine_boundary", {255, 257, 511}}};
  std::vector<std::string> tasks;
  std::map<std::string, std::vector<ResolvedAccess>> accesses;
  const auto result = stream_resolved_task_accesses(
    raw, objects,
    {[&](const std::string & task, uint64_t excluded)
     {
       tasks.push_back(task);
       EXPECT_EQ(excluded, 0U);
     },
     [&](const std::string & task, const ResolvedAccess & access)
     { accesses[task].push_back(access); },
     [](const std::string &, const TraceCoverage & coverage)
     {
       EXPECT_TRUE(coverage.complete());
       EXPECT_EQ(coverage.rejected_accesses, 0U);
     }});
  EXPECT_EQ(tasks, (std::vector<std::string>{"offset",
                                             "stride",
                                             "descending",
                                             "constant",
                                             "dimensions",
                                             "caller",
                                             "fields",
                                             "shadowed",
                                             "capture",
                                             "unsigned_caller",
                                             "unsigned_forward",
                                             "unsigned_boundary",
                                             "scaled_left",
                                             "scaled_right",
                                             "chained",
                                             "nested_product",
                                             "constant_product",
                                             "temporary",
                                             "scaled_stride",
                                             "scaled_descending",
                                             "negative_coefficient",
                                             "flattened",
                                             "scaled_fields",
                                             "affine_actual",
                                             "affine_boundary"}));
  uint64_t count = 0;
  for (const auto & [task, expected] : elements)
  {
    SCOPED_TRACE(task);
    const auto & actual = accesses.at(task);
    ASSERT_EQ(actual.size(), expected.size());
    const bool field = task == "fields" || task == "scaled_fields";
    const std::string object =
      task.rfind("affine_", 0) == 0 ? "global::affine_data"
      : task == "unsigned_boundary" ? "global::unsigned_"
                                      "matrix"
      : task == "dimensions"        ? "global::matrix"
      : field                       ? "global::records"
                                    : "global::a";
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
      SCOPED_TRACE(i);
      const auto offset = field ? 8 * expected[i] + 4 : 4 * expected[i];
      EXPECT_EQ(actual[i].object_id, object);
      EXPECT_EQ(actual[i].object_byte_offset, offset);
      EXPECT_EQ(actual[i].linked_byte_address,
                objects.objects.at(object).base + offset);
      EXPECT_EQ(actual[i].access_size, 4U);
      EXPECT_EQ(actual[i].source_access_ordinal, i);
      EXPECT_EQ(actual[i].operation, task == "caller" && i % 2 == 0
                                       ? AccessOperation::Load
                                       : AccessOperation::Store);
      EXPECT_EQ(actual[i].address_basis, AddressBasis::Absolute);
    }
    count += expected.size();
  }
  EXPECT_EQ(result.coverage.source_accesses, count);
  EXPECT_TRUE(result.coverage.complete());
}

TEST_P(AffineIndexContract, PreservesHierarchyResultsAgainstBothOracles)
{
  streaming::expect_stream_parity(raw, objects,
                                  streaming::make_batch_hierarchy());
}

INSTANTIATE_TEST_SUITE_P(CompilerDebugModes, AffineIndexContract,
                         testing::Values("debug", "nodebug"));

}  // namespace
}  // namespace yarda::test
