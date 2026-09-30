#include <nlohmann/json.hpp>

#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

#include "yarda/trace/mapped_trace.hpp"
#include "yarda/trace/resolution_error.hpp"

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
  return {
    {"metadata",
     {{"objects",
       {{"global::A",
         {{"kind", "array"},
          {"shape", Json::array({1})},
          {"elem_size", 4}}}}}}},
    {"functions", Json::array({{{"function", "kernel"}, {"body", accesses}}})},
  };
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
  Json functions = Json::array();
  functions.push_back(
    {{"function", "kernel"},
     {"body", Json::array({array_access("0"),
                           {{"type", "Scalar"}, {"name", "temporary"}},
                           array_access("1")})}});
  functions.push_back(
    {{"function", "helper"}, {"body", Json::array({array_access("2")})}});
  return {
    {"metadata",
     {{"objects",
       {{"global::A",
         {{"kind", "array"},
          {"shape", Json::array({3})},
          {"elem_size", 4}}}}}}},
    {"functions", std::move(functions)},
  };
}

Json successful_ordinal_module()
{
  const auto array_access = [](const std::string & object) {
    return Json{{"type", "Array"},
                {"name", object},
                {"object", object},
                {"indices", Json::array({"0"})},
                {"op", "load"}};
  };
  return {
    {"metadata",
     {{"objects",
       {{"global::A",
         {{"kind", "array"}, {"shape", Json::array({1})}, {"elem_size", 4}}},
        {"global::B",
         {{"kind", "array"},
          {"shape", Json::array({1})},
          {"elem_size", 4}}}}}}},
    {"functions",
     Json::array(
       {{{"function", "first"},
         {"body", Json::array({array_access("global::A"), array_access("global:"
                                                                       ":"
                                                                       "B")})}},
        {{"function", "second"},
         {"body", Json::array({array_access("global::A")})}}})},
  };
}

TEST(MappedTraceStrictTest, MapsGlobalScalarWithMetadata)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::flag"] = {0x1000, 4};

  const auto result = yarda::mapped_block_traces(
    scalar_module(true), yarda::CacheGeometry{64, 8, 2}, objects);

  ASSERT_EQ(result.traces.size(), 1U);
  ASSERT_EQ(result.traces[0].accesses.size(), 1U);
  EXPECT_EQ(result.traces[0].accesses[0].object_id, "global::flag");
  EXPECT_EQ(result.traces[0].accesses[0].operation,
            yarda::AccessOperation::Load);
  EXPECT_EQ(result.mappings.size(), 1U);
  EXPECT_TRUE(result.coverage.complete());
}

TEST(MappedTraceStrictTest, RejectsGlobalScalarWithoutMetadata)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::flag"] = {0x1000, 4};

  try
  {
    static_cast<void>(yarda::mapped_block_traces(
      scalar_module(false), yarda::CacheGeometry{64, 8, 2}, objects));
    FAIL() << "expected missing scalar layout rejection";
  }
  catch (const yarda::ResolutionError & error)
  {
    EXPECT_EQ(error.category(), yarda::ResolutionCategory::Unresolved);
    EXPECT_EQ(error.source_access_ordinal(), 0U);
  }
}

TEST(MappedTraceStrictTest, KeepsDeduplicatedRowsAddressOnly)
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

TEST(MappedTraceStrictTest, PreservesOrdinalsAcrossMappedFunctions)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {0x1000, 4};
  objects.objects["global::B"] = {0x1040, 4};

  const auto result = yarda::mapped_block_traces(
    successful_ordinal_module(), yarda::CacheGeometry{64, 8, 2}, objects);

  ASSERT_EQ(result.traces.size(), 2U);
  ASSERT_EQ(result.traces[0].accesses.size(), 2U);
  ASSERT_EQ(result.traces[1].accesses.size(), 1U);
  EXPECT_EQ(result.traces[0].accesses[0].source_access_ordinal, 0U);
  EXPECT_EQ(result.traces[0].accesses[1].source_access_ordinal, 1U);
  EXPECT_EQ(result.traces[1].accesses[0].source_access_ordinal, 2U);
}

TEST(MappedTraceStrictTest, RejectsNonGlobalAccessAtItsOrdinal)
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {0x1000, 12};

  try
  {
    static_cast<void>(yarda::mapped_block_traces(
      ordinal_module(), yarda::CacheGeometry{64, 8, 2}, objects));
    FAIL() << "expected non-global access rejection";
  }
  catch (const yarda::ResolutionError & error)
  {
    EXPECT_EQ(error.category(), yarda::ResolutionCategory::Unsupported);
    EXPECT_EQ(error.task_id(), "kernel");
    EXPECT_EQ(error.source_access_ordinal(), 1U);
    EXPECT_EQ(error.coverage().source_accesses, 2U);
    EXPECT_EQ(error.coverage().resolved_accesses, 1U);
    EXPECT_EQ(error.coverage().rejected_accesses, 1U);
    EXPECT_FALSE(error.coverage().complete());
  }
}

}  // namespace
