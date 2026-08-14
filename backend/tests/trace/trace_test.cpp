#include "yarda/trace/trace.hpp"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace
{

using Json = nlohmann::json;

Json matrix_loop()
{
  return {
    {"type", "Loop"},
    {"var", "i"},
    {"start", 0},
    {"bound", 2},
    {"step", 1},
    {"body", Json::array({{
               {"type", "Array"},
               {"name", "A"},
               {"indices", Json::array({"i", "0"})},
               {"shape", Json::array({2, 8})},
               {"elem_size", 4},
             }})},
  };
}

TEST(TraceTest, UnrollsActualLoopBounds)
{
  const auto trace = yarda::unroll_node_actual(matrix_loop());

  EXPECT_EQ(trace, (std::vector<std::string>{"A-0-0", "A-1-0"}));
}

TEST(TraceTest, MapsResolvedIndicesToCacheLines)
{
  const auto trace =
    yarda::unroll_node_actual(matrix_loop(), yarda::Granularity::CacheLine, 32);

  EXPECT_EQ(trace, (std::vector<std::string>{"A-line-0", "A-line-1"}));
}

TEST(TraceTest, UsesPythonFloorDivisionForNegativeOffsets)
{
  const Json access = {
    {"type", "Array"},
    {"name", "A"},
    {"indices", Json::array({"-1"})},
    {"shape", Json::array({8})},
    {"elem_size", 4},
  };

  const auto trace =
    yarda::unroll_node_actual(access, yarda::Granularity::CacheLine, 32);

  EXPECT_EQ(trace, (std::vector<std::string>{"A-line--1"}));
}

TEST(TraceTest, ReturnsTypedMappingForLinkedGlobalAccess)
{
  const Json access = {
    {"type", "Array"},
    {"name", "A"},
    {"object", "global::A"},
    {"indices", Json::array({"0"})},
    {"shape", Json::array({1})},
    {"elem_size", 4},
  };
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {0x1030, 4};

  const auto trace = yarda::unroll_node_actual(
    access, yarda::CacheGeometry{64, 512, 8}, objects);

  ASSERT_EQ(trace.size(), 1);
  EXPECT_EQ(trace[0].object_id, "global::A");
  EXPECT_EQ(trace[0].object_byte_offset, 0U);
  EXPECT_EQ(trace[0].decoded.tag, 1U);
  EXPECT_EQ(trace[0].decoded.set_index, 0U);
  EXPECT_EQ(trace[0].decoded.line_offset, 0x30U);
}

TEST(TraceTest, MapsDifferentGlobalObjectsToSharedCacheLine)
{
  const Json module = Json::array({{
    {"function", "kernel"},
    {"body", Json::array({
               {{"type", "Array"},
                {"name", "A"},
                {"object", "global::A"},
                {"indices", Json::array({"0"})},
                {"shape", Json::array({1})},
                {"elem_size", 4}},
               {{"type", "Array"},
                {"name", "B"},
                {"object", "global::B"},
                {"indices", Json::array({"0"})},
                {"shape", Json::array({1})},
                {"elem_size", 4}},
             })},
  }});
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {0x1030, 4};
  objects.objects["global::B"] = {0x1038, 4};

  const auto result = yarda::mapped_block_traces(
    module, yarda::CacheGeometry{64, 512, 8}, objects);

  ASSERT_EQ(result.traces.size(), 1);
  ASSERT_EQ(result.traces[0].accesses.size(), 2);
  EXPECT_EQ(result.traces[0].accesses[0].object_id, "global::A");
  EXPECT_EQ(result.traces[0].accesses[1].object_id, "global::B");
  EXPECT_EQ(result.traces[0].accesses[0].decoded.tag, 1U);
  EXPECT_EQ(result.traces[0].accesses[1].decoded.tag, 1U);
  EXPECT_EQ(result.traces[0].accesses[0].decoded.set_index, 0U);
  EXPECT_EQ(result.traces[0].accesses[1].decoded.set_index, 0U);
  ASSERT_EQ(result.mappings.size(), 2);
  EXPECT_EQ(result.mappings.at({"global::A", 0}).decoded.line_offset, 0x30U);
  EXPECT_EQ(result.mappings.at({"global::B", 0}).decoded.line_offset, 0x38U);
}

TEST(TraceTest, RejectsUnresolvedMappedGlobalObject)
{
  Json access = {
    {"type", "Array"},
    {"name", "A"},
    {"object", "global::A"},
    {"indices", Json::array({"0"})},
    {"shape", Json::array({1})},
    {"elem_size", 4},
  };

  EXPECT_THROW(yarda::unroll_node_actual(
                 access, yarda::CacheGeometry{64, 512, 8}, {}),
               std::invalid_argument);
}

TEST(TraceTest, RejectsStructuredGlobalAccessUntilPathOffsetsAreSupported)
{
  Json access = {
    {"type", "Array"},
    {"name", "value.items[0]"},
    {"object", "global::value"},
    {"indices", Json::array({"0"})},
    {"shape", Json::array({1})},
    {"elem_size", 4},
    {"access_path",
     Json::array({{{"kind", "field"}, {"name", "items"}, {"index", 0}}})},
  };
  yarda::ObjectAddressModel objects;
  objects.objects["global::value"] = {0x1000, 4};

  EXPECT_THROW(yarda::unroll_node_actual(
                 access, yarda::CacheGeometry{64, 512, 8}, objects),
               std::invalid_argument);
}

TEST(TraceTest, RejectsNegativeMappedGlobalOffset)
{
  Json access = {
    {"type", "Array"},
    {"name", "A"},
    {"object", "global::A"},
    {"indices", Json::array({"-1"})},
    {"shape", Json::array({1})},
    {"elem_size", 4},
  };
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {0x1000, 4};

  EXPECT_THROW(yarda::unroll_node_actual(
                 access, yarda::CacheGeometry{64, 512, 8}, objects),
               std::invalid_argument);
}

TEST(TraceTest, PreservesSequentialBlocks)
{
  const Json module = Json::array({{
    {"function", "kernel"},
    {"body", Json::array({
               {{"type", "Scalar"}, {"name", "x"}},
               matrix_loop(),
               {{"type", "Scalar"}, {"name", "y"}},
             })},
  }});

  const auto blocks = yarda::block_traces(module);

  ASSERT_EQ(blocks.size(), 3);
  EXPECT_EQ(blocks[0].accesses, (std::vector<std::string>{"x"}));
  EXPECT_EQ(blocks[1].accesses, (std::vector<std::string>{"A-0-0", "A-1-0"}));
  EXPECT_EQ(blocks[2].accesses, (std::vector<std::string>{"y"}));
}

}  // namespace
