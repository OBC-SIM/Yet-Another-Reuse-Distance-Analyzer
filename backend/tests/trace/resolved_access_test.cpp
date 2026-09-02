#include "yarda/trace/resolved_access.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>

namespace
{

using Json = nlohmann::json;

Json scalar_module()
{
  return {
    {"metadata",
     {{"objects", {{"global::flag", {{"kind", "scalar"}, {"elem_size", 4}}}}}}},
    {"functions",
     Json::array({{{"function", "kernel"},
                   {"body", Json::array({{{"type", "Scalar"},
                                          {"name", "flag"},
                                          {"object", "global::flag"},
                                          {"op", "load"}}})}}})},
  };
}

Json structured_module()
{
  return {
    {"metadata",
     {{"objects",
       {{"global::outer",
         {{"kind", "struct"}, {"elem_type", "Outer"}, {"elem_size", 72}}},
        {"global::values",
         {{"kind", "array"}, {"shape", Json::array({4})}, {"elem_size", 4}}}}},
      {"structs",
       {{"S",
         {{"name", "S"},
          {"size", 16},
          {"fields", Json::array({{{"name", "x"},
                                   {"index", 0},
                                   {"offset", 0},
                                   {"size", 4},
                                   {"kind", "scalar"},
                                   {"elem_size", 4}},
                                  {{"name", "y"},
                                   {"index", 1},
                                   {"offset", 8},
                                   {"size", 8},
                                   {"kind", "scalar"},
                                   {"elem_size", 8}}})}}},
        {"Outer",
         {{"name", "Outer"},
          {"size", 72},
          {"fields", Json::array({{{"name", "tag"},
                                   {"index", 0},
                                   {"offset", 0},
                                   {"size", 4},
                                   {"kind", "scalar"},
                                   {"elem_size", 4}},
                                  {{"name", "items"},
                                   {"index", 1},
                                   {"offset", 8},
                                   {"size", 64},
                                   {"kind", "array"},
                                   {"shape", Json::array({4})},
                                   {"elem_type", "S"},
                                   {"elem_size", 16}}})}}}}}}},
    {"functions",
     Json::array(
       {{{"function", "kernel"},
         {"body", Json::array(
                    {{{"type", "Array"},
                      {"name", "outer.items[2].y"},
                      {"object", "global::outer"},
                      {"indices", Json::array({"2"})},
                      {"op", "store"},
                      {"access_path",
                       Json::array(
                         {{{"kind", "field"}, {"name", "items"}, {"index", 1}},
                          {{"kind", "index"}, {"value", "2"}},
                          {{"kind", "field"}, {"name", "y"}, {"index", 1}}})}},
                     {{"type", "Array"},
                      {"name", "values[3]"},
                      {"object", "global::values"},
                      {"indices", Json::array({"3"})},
                      {"op", "load"}}})}}})},
  };
}

Json ordinal_module()
{
  const auto array = [](const std::string & object,
                        const std::string & operation) {
    return Json{{"type", "Array"},
                {"name", object},
                {"object", object},
                {"indices", Json::array({"0"})},
                {"shape", Json::array({1})},
                {"elem_size", 4},
                {"op", operation}};
  };
  return Json::array(
    {{{"function", "first"},
      {"body",
       Json::array({array("global::A", "load"),
                    {{"type", "Scalar"}, {"name", "local"}, {"op", "load"}},
                    array("global::B", "store")})}},
     {{"function", "second"},
      {"body", Json::array({array("global::A", "load")})}}});
}

TEST(ResolvedAccessTest, ResolvesGlobalScalarMetadataAndOperation)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::flag"] = {0x1010, 4};

  const auto result = yarda::resolved_block_traces(scalar_module(), objects);

  ASSERT_EQ(result.traces.size(), 1);
  ASSERT_EQ(result.traces[0].accesses.size(), 1);
  const auto & access = result.traces[0].accesses[0];
  EXPECT_EQ(access.object_id, "global::flag");
  EXPECT_EQ(access.object_byte_offset, 0U);
  EXPECT_EQ(access.access_size, 4U);
  EXPECT_EQ(access.linked_byte_address, 0x1010U);
  EXPECT_EQ(access.address_basis, yarda::AddressBasis::Absolute);
  EXPECT_EQ(access.operation, yarda::AccessOperation::Load);
  EXPECT_EQ(access.source_access_ordinal, 0U);
}

TEST(ResolvedAccessTest, PreservesStructuredOffsetsAndDeterministicOrdinals)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::outer"] = {0x1000, 72};
  objects.objects["global::values"] = {0x2000, 16};

  const auto result =
    yarda::resolved_block_traces(structured_module(), objects);

  ASSERT_EQ(result.traces.size(), 1);
  ASSERT_EQ(result.traces[0].accesses.size(), 2);
  const auto & field = result.traces[0].accesses[0];
  EXPECT_EQ(field.object_byte_offset, 48U);
  EXPECT_EQ(field.access_size, 8U);
  EXPECT_EQ(field.linked_byte_address, 0x1030U);
  EXPECT_EQ(field.operation, yarda::AccessOperation::Store);
  EXPECT_EQ(field.source_access_ordinal, 0U);
  const auto & array = result.traces[0].accesses[1];
  EXPECT_EQ(array.object_byte_offset, 12U);
  EXPECT_EQ(array.access_size, 4U);
  EXPECT_EQ(array.linked_byte_address, 0x200cU);
  EXPECT_EQ(array.operation, yarda::AccessOperation::Load);
  EXPECT_EQ(array.source_access_ordinal, 1U);
}

TEST(ResolvedAccessTest, RejectsAccessPastLinkedObjectExtent)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::outer"] = {0x1000, 72};
  objects.objects["global::values"] = {0x2000, 8};

  try
  {
    static_cast<void>(
      yarda::resolved_block_traces(structured_module(), objects));
    FAIL() << "expected object extent rejection";
  }
  catch (const std::invalid_argument & error)
  {
    EXPECT_STREQ(error.what(),
                 "access exceeds ELF object extent: global::values");
  }
}

TEST(ResolvedAccessTest, RejectsLinkedAddressRangeOverflow)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::flag"] = {
    std::numeric_limits<std::uint64_t>::max() - 1, 4};

  EXPECT_THROW(yarda::resolved_block_traces(scalar_module(), objects),
               std::overflow_error);
}

TEST(ResolvedAccessTest, RejectsUnresolvedElfObject)
{
  const yarda::ObjectAddressModel objects;

  try
  {
    static_cast<void>(yarda::resolved_block_traces(scalar_module(), objects));
    FAIL() << "expected unresolved ELF object rejection";
  }
  catch (const std::invalid_argument & error)
  {
    EXPECT_STREQ(error.what(), "ELF object is unresolved: global::flag");
  }
}

TEST(ResolvedAccessTest, RejectsReconstructedAddressOverflow)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::outer"] = {std::numeric_limits<std::uint64_t>::max(),
                                      72};

  try
  {
    static_cast<void>(
      yarda::resolved_block_traces(structured_module(), objects));
    FAIL() << "expected reconstructed address overflow";
  }
  catch (const std::overflow_error & error)
  {
    EXPECT_STREQ(error.what(), "ELF object address overflow: global::outer");
  }
}

TEST(ResolvedAccessTest, PreservesOrdinalsAcrossSkippedAccessesAndFunctions)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {0x1000, 4};
  objects.objects["global::B"] = {0x1040, 4};

  const auto result = yarda::resolved_block_traces(ordinal_module(), objects);

  ASSERT_EQ(result.traces.size(), 2);
  ASSERT_EQ(result.traces[0].accesses.size(), 2);
  ASSERT_EQ(result.traces[1].accesses.size(), 1);
  EXPECT_EQ(result.traces[0].accesses[0].source_access_ordinal, 0U);
  EXPECT_EQ(result.traces[0].accesses[1].source_access_ordinal, 2U);
  EXPECT_EQ(result.traces[1].accesses[0].source_access_ordinal, 3U);
}

TEST(ResolvedAccessTest, MarksMissingOperationAsUnknown)
{
  auto module = scalar_module();
  module["functions"][0]["body"][0].erase("op");
  yarda::ObjectAddressModel objects;
  objects.objects["global::flag"] = {0x1010, 4};

  const auto result = yarda::resolved_block_traces(module, objects);

  ASSERT_EQ(result.traces.size(), 1);
  ASSERT_EQ(result.traces[0].accesses.size(), 1);
  EXPECT_EQ(result.traces[0].accesses[0].operation,
            yarda::AccessOperation::Unknown);
}

TEST(ResolvedAccessTest, RejectsNegativeGlobalOffset)
{
  const Json access = {{"type", "Array"},
                       {"name", "A[-1]"},
                       {"object", "global::A"},
                       {"indices", Json::array({"-1"})},
                       {"shape", Json::array({4})},
                       {"elem_size", 4},
                       {"op", "load"}};
  const Json module =
    Json::array({{{"function", "kernel"}, {"body", Json::array({access})}}});
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {0x1000, 16};

  try
  {
    static_cast<void>(yarda::resolved_block_traces(module, objects));
    FAIL() << "expected negative offset rejection";
  }
  catch (const std::invalid_argument & error)
  {
    EXPECT_STREQ(error.what(), "global access offset is negative: global::A");
  }
}

}  // namespace
