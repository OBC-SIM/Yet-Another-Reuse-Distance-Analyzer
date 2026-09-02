#include "yarda/trace/mapped_trace.hpp"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

namespace
{

using Json = nlohmann::json;

Json structured_span_module()
{
  return {
    {"metadata",
     {{"objects",
       {{"global::record",
         {{"kind", "struct"}, {"elem_type", "Record"}, {"elem_size", 68}}}}},
      {"structs",
       {{"Record",
         {{"name", "Record"},
          {"size", 68},
          {"fields", Json::array({{{"name", "tail"},
                                   {"index", 0},
                                   {"offset", 60},
                                   {"size", 8},
                                   {"kind", "scalar"},
                                   {"elem_size", 8}}})}}}}}}},
    {"functions",
     Json::array(
       {{{"function", "kernel"},
         {"body",
          Json::array({{{"type", "Array"},
                        {"name", "record.tail"},
                        {"object", "global::record"},
                        {"indices", Json::array()},
                        {"access_path", Json::array({{{"kind", "field"},
                                                      {"name", "tail"},
                                                      {"index", 0}}})}}})}}})},
  };
}

TEST(MappedTraceTest, PreservesMappedAccessOrder)
{
  const Json module = Json::array({{
    {"function", "kernel"},
    {"body", Json::array({
               {{"type", "Array"},
                {"name", "A"},
                {"object", "global::A"},
                {"indices", Json::array({"0"})},
                {"shape", Json::array({16})},
                {"elem_size", 4}},
               {{"type", "Array"},
                {"name", "B"},
                {"object", "global::B"},
                {"indices", Json::array({"0"})},
                {"shape", Json::array({16})},
                {"elem_size", 4}},
               {{"type", "Array"},
                {"name", "A"},
                {"object", "global::A"},
                {"indices", Json::array({"1"})},
                {"shape", Json::array({16})},
                {"elem_size", 4}},
             })},
  }});
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {0x1000, 64};
  objects.objects["global::B"] = {0x1080, 64};

  const auto result = yarda::mapped_block_traces(
    module, yarda::CacheGeometry{64, 512, 8}, objects);

  ASSERT_EQ(result.traces.size(), 1);
  const auto & accesses = result.traces.front().accesses;
  ASSERT_EQ(accesses.size(), 3);
  EXPECT_EQ(accesses[0].object_id, "global::A");
  EXPECT_EQ(accesses[1].object_id, "global::B");
  EXPECT_EQ(accesses[2].object_id, "global::A");
  EXPECT_EQ(accesses[2].object_byte_offset, 4U);
  EXPECT_EQ(accesses[0].decoded.set_index, 0U);
  EXPECT_EQ(accesses[1].decoded.set_index, 2U);
}

TEST(MappedTraceTest, FlattensMappedBlocksInProgramOrder)
{
  const auto loop = [](const std::string & variable, const std::string & name,
                       const std::string & object) {
    return Json{
      {"type", "Loop"},
      {"var", variable},
      {"bound", 1},
      {"body", Json::array({{
                 {"type", "Array"},
                 {"name", name},
                 {"object", object},
                 {"indices", Json::array({"0"})},
                 {"shape", Json::array({1})},
                 {"elem_size", 4},
               }})},
    };
  };

  const Json module = Json::array({{
    {"function", "kernel"},
    {"body", Json::array({
               loop("i", "A", "global::A"),
               loop("j", "B", "global::B"),
             })},
  }});

  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {0x1000, 4};
  objects.objects["global::B"] = {0x1080, 4};

  const auto mapped = yarda::mapped_block_traces(
    module, yarda::CacheGeometry{64, 512, 8}, objects);

  const auto accesses = yarda::flatten_mapped_traces(mapped.traces);

  ASSERT_EQ(mapped.traces.size(), 2);
  ASSERT_EQ(accesses.size(), 2);
  EXPECT_EQ(accesses[0].object_id, "global::A");
  EXPECT_EQ(accesses[1].object_id, "global::B");
  EXPECT_EQ(accesses[0].decoded.set_index, 0U);
  EXPECT_EQ(accesses[1].decoded.set_index, 2U);
}

TEST(MappedTraceTest, PreservesEveryLineOfOneStructSizedAccess)
{
  const Json module = Json::array({{
    {"function", "kernel"},
    {"body", Json::array({{
               {"type", "Array"},
               {"name", "records"},
               {"object", "global::records"},
               {"indices", Json::array({"5"})},
               {"shape", Json::array({6})},
               {"elem_size", 12},
               {"op", "store"},
             }})},
  }});
  yarda::ObjectAddressModel objects;
  objects.objects["global::records"] = {0x1000, 72};

  const auto result = yarda::mapped_block_traces(
    module, yarda::CacheGeometry{64, 512, 8}, objects);

  ASSERT_EQ(result.traces.size(), 1);
  const auto & accesses = result.traces.front().accesses;
  ASSERT_EQ(accesses.size(), 2);
  EXPECT_EQ(accesses[0].object_byte_offset, 60U);
  EXPECT_EQ(accesses[0].decoded.address, 0x103cU);
  EXPECT_EQ(accesses[1].object_byte_offset, 64U);
  EXPECT_EQ(accesses[1].decoded.address, 0x1040U);
  EXPECT_EQ(accesses[0].operation, yarda::AccessOperation::Store);
  EXPECT_EQ(accesses[0].source_access_ordinal, 0U);
  EXPECT_EQ(accesses[0].line_span_ordinal, 0U);
  EXPECT_EQ(accesses[1].source_access_ordinal, 0U);
  EXPECT_EQ(accesses[1].line_span_ordinal, 1U);
  EXPECT_EQ(result.mappings.size(), 2);
  EXPECT_EQ(result.mappings.at({"global::records", 64}).decoded.line_offset,
            0U);
}

TEST(MappedTraceTest, MapsStructuredFieldSpanToExactObjectOffsets)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::record"] = {0x1000, 68};

  const auto result = yarda::mapped_block_traces(
    structured_span_module(), yarda::CacheGeometry{64, 512, 8}, objects);

  ASSERT_EQ(result.traces.size(), 1);
  const auto & accesses = result.traces[0].accesses;
  ASSERT_EQ(accesses.size(), 2);
  EXPECT_EQ(accesses[0].object_byte_offset, 60U);
  EXPECT_EQ(accesses[0].decoded.address, 0x103cU);
  EXPECT_EQ(accesses[1].object_byte_offset, 64U);
  EXPECT_EQ(accesses[1].decoded.address, 0x1040U);
}

}  // namespace
