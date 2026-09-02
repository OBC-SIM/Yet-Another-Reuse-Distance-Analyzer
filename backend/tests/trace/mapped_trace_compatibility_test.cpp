#include <nlohmann/json.hpp>

#include <gtest/gtest.h>
#include <type_traits>

#include "yarda/trace/mapped_trace.hpp"

namespace
{

using Json = nlohmann::json;

Json scalar_module(bool include_metadata)
{
  Json module = {
    {"functions",
     Json::array({{{"function", "kernel"},
                   {"body", Json::array({{{"type", "Scalar"},
                                          {"name", "flag"},
                                          {"object", "global::flag"},
                                          {"op", "load"}}})}}})},
  };
  if (include_metadata)
  {
    module["metadata"] = {
      {"objects", {{"global::flag", {{"kind", "scalar"}, {"elem_size", 4}}}}},
    };
  }
  return module;
}

Json repeated_access_module()
{
  const Json accesses = Json::array({{{"type", "Array"},
                                      {"name", "A"},
                                      {"object", "global::A"},
                                      {"indices", Json::array({"0"})},
                                      {"shape", Json::array({1})},
                                      {"elem_size", 4},
                                      {"op", "load"}},
                                     {{"type", "Array"},
                                      {"name", "A"},
                                      {"object", "global::A"},
                                      {"indices", Json::array({"0"})},
                                      {"shape", Json::array({1})},
                                      {"elem_size", 4},
                                      {"op", "store"}}});
  return Json::array({{{"function", "kernel"}, {"body", accesses}}});
}

Json ordinal_module()
{
  const auto array_access = [](const std::string & index) {
    return Json{{"type", "Array"},
                {"name", "A"},
                {"object", "global::A"},
                {"indices", Json::array({index})},
                {"shape", Json::array({3})},
                {"elem_size", 4},
                {"op", "load"}};
  };
  Json module = Json::array();
  module.push_back(
    {{"function", "kernel"},
     {"body", Json::array({array_access("0"),
                           {{"type", "Scalar"}, {"name", "temporary"}},
                           array_access("1")})}});
  module.push_back(
    {{"function", "helper"}, {"body", Json::array({array_access("2")})}});
  return module;
}

TEST(MappedTraceCompatibilityTest, SkipsGlobalScalarWithMetadata)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::flag"] = {0x1000, 4};

  const auto result = yarda::mapped_block_traces(
    scalar_module(true), yarda::CacheGeometry{64, 8, 2}, objects);

  EXPECT_TRUE(result.traces.empty());
  EXPECT_TRUE(result.mappings.empty());
}

TEST(MappedTraceCompatibilityTest, SkipsGlobalScalarWithoutMetadata)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::flag"] = {0x1000, 4};

  EXPECT_NO_THROW({
    const auto result = yarda::mapped_block_traces(
      scalar_module(false), yarda::CacheGeometry{64, 8, 2}, objects);
    EXPECT_TRUE(result.traces.empty());
    EXPECT_TRUE(result.mappings.empty());
  });
}

TEST(MappedTraceCompatibilityTest, KeepsDeduplicatedRowsAddressOnly)
{
  static_assert(std::is_same_v<yarda::CacheLineMappingTable::mapped_type,
                               yarda::CacheLineAddressMapping>);
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {0x1000, 4};

  const auto result = yarda::mapped_block_traces(
    repeated_access_module(), yarda::CacheGeometry{64, 8, 2}, objects);

  ASSERT_EQ(result.traces.size(), 1);
  ASSERT_EQ(result.traces[0].accesses.size(), 2);
  EXPECT_EQ(result.traces[0].accesses[0].operation,
            yarda::AccessOperation::Load);
  EXPECT_EQ(result.traces[0].accesses[1].operation,
            yarda::AccessOperation::Store);
  ASSERT_EQ(result.mappings.size(), 1);
  EXPECT_EQ(result.mappings.begin()->second.decoded.address, 0x1000U);
}

TEST(MappedTraceCompatibilityTest,
     KeepsModuleWideOrdinalsAcrossSkippedScalarAndFunctions)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {0x1000, 12};

  const auto result = yarda::mapped_block_traces(
    ordinal_module(), yarda::CacheGeometry{64, 8, 2}, objects);

  ASSERT_EQ(result.traces.size(), 2);
  ASSERT_EQ(result.traces[0].accesses.size(), 2);
  ASSERT_EQ(result.traces[1].accesses.size(), 1);
  EXPECT_EQ(result.traces[0].accesses[0].source_access_ordinal, 0U);
  EXPECT_EQ(result.traces[0].accesses[1].source_access_ordinal, 2U);
  EXPECT_EQ(result.traces[1].accesses[0].source_access_ordinal, 3U);
}

}  // namespace
