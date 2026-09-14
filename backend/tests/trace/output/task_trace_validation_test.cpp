#include <nlohmann/json.hpp>

#include <gtest/gtest.h>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>

#include "yarda/trace/mapped_trace.hpp"
#include "yarda/trace/resolved_access.hpp"

namespace
{

using Json = nlohmann::json;

Json access(std::string object, std::string operation = "load",
            int element_size = 4)
{
  return {
    {"type", "Array"},
    {"name", object},
    {"object", std::move(object)},
    {"indices", Json::array({"0"})},
    {"elem_size", element_size},
    {"op", std::move(operation)},
  };
}

Json call(std::string callee)
{
  return {
    {"type", "Call"},
    {"callee", std::move(callee)},
    {"args", Json::array()},
  };
}

Json function(std::string name, Json body, Json annotations = Json::array())
{
  return {
    {"function", std::move(name)},
    {"annotations", std::move(annotations)},
    {"body", std::move(body)},
  };
}

Json array_metadata(int element_size = 4)
{
  return {
    {"kind", "array"},
    {"shape", Json::array({1})},
    {"elem_size", element_size},
  };
}

Json module(Json functions, Json objects)
{
  return {
    {"schema_version", 2},
    {"metadata", {{"objects", std::move(objects)}}},
    {"functions", std::move(functions)},
  };
}

yarda::ObjectAddressModel object_addresses(
  std::initializer_list<std::pair<std::string, yarda::ObjectAddress>> objects)
{
  yarda::ObjectAddressModel result;
  for (const auto & [name, address] : objects)
  {
    result.objects.emplace(name, address);
  }
  return result;
}

template <typename Action>
std::string invalid_argument_message(const Action & action)
{
  try
  {
    action();
  }
  catch (const std::invalid_argument & error)
  {
    return error.what();
  }
  ADD_FAILURE() << "expected std::invalid_argument";
  return {};
}

TEST(TaskTraceValidationTest, RejectsInlineOnlyModuleWithoutAnalyzedRoot)
{
  const auto raw = module(Json::array({function("helper", Json::array(),
                                                Json::array({"ape.inline"}))}),
                          Json::object());

  const auto message = invalid_argument_message([&]() {
    static_cast<void>(
      yarda::resolved_task_traces(raw, yarda::ObjectAddressModel{}));
  });

  EXPECT_NE(message.find("no analyzed task root"), std::string::npos);
}

TEST(TaskTraceValidationTest, RejectsUnannotatedModuleWithoutAnalyzedRoot)
{
  const auto raw =
    module(Json::array({function("legacy", Json::array())}), Json::object());

  const auto message = invalid_argument_message([&]() {
    static_cast<void>(
      yarda::resolved_task_traces(raw, yarda::ObjectAddressModel{}));
  });

  EXPECT_NE(message.find("no analyzed task root"), std::string::npos);
}

TEST(TaskTraceValidationTest, ExpandsInlineAndSkipsKnownNonInlineCallees)
{
  const auto raw = module(
    Json::array({function("included", Json::array({access("global::included")}),
                          Json::array({"ape.inline"})),
                 function("excluded", Json::array({access("global::"
                                                          "excluded")})),
                 function("kernel",
                          Json::array({call("included"), call("excluded"),
                                       access("global::root")}),
                          Json::array({"ape.analyze"}))}),
    {{"global::included", array_metadata()},
     {"global::excluded", array_metadata()},
     {"global::root", array_metadata()}});
  const auto addresses = object_addresses({{"global::included", {0x1000, 4}},
                                           {"global::excluded", {0x1100, 4}},
                                           {"global::root", {0x1200, 4}}});

  const auto result = yarda::resolved_task_traces(raw, addresses);

  ASSERT_EQ(result.tasks.size(), 1U);
  ASSERT_EQ(result.tasks[0].accesses.size(), 2U);
  EXPECT_EQ(result.tasks[0].accesses[0].object_id, "global::included");
  EXPECT_EQ(result.tasks[0].accesses[1].object_id, "global::root");
  EXPECT_EQ(result.coverage.source_accesses, 2U);
}

TEST(TaskTraceValidationTest, RejectsUnknownTaskCallee)
{
  const auto raw = module(
    Json::array({function("kernel", Json::array({call("missing_helper")}),
                          Json::array({"ape.analyze"}))}),
    Json::object());

  const auto message = invalid_argument_message([&]() {
    static_cast<void>(
      yarda::resolved_task_traces(raw, yarda::ObjectAddressModel{}));
  });

  EXPECT_NE(message.find("Unknown call target"), std::string::npos);
}

TEST(TaskTraceValidationTest, RejectsAnalyzeAndInlineRoleOverlap)
{
  const auto raw = module(
    Json::array({function("helper", Json::array({access("global::helper")}),
                          Json::array({"ape.analyze", "ape.inline"})),
                 function("kernel", Json::array({call("helper")}),
                          Json::array({"ape.analyze"}))}),
    {{"global::helper", array_metadata()}});

  const auto message = invalid_argument_message([&]() {
    static_cast<void>(yarda::resolved_task_traces(
      raw, object_addresses({{"global::helper", {0x1000, 4}}})));
  });

  EXPECT_NE(message.find("both an analyzed root and inline"),
            std::string::npos);
}

TEST(TaskTraceValidationTest, MapsCrossLineStoreWithTaskProvenance)
{
  const auto raw = module(
    Json::array(
      {function("kernel", Json::array({access("global::wide", "store", 8)}),
                Json::array({"ape.analyze"}))}),
    {{"global::wide", array_metadata(8)}});
  const auto addresses = object_addresses({{"global::wide", {0x101c, 8}}});

  const auto result =
    yarda::mapped_task_traces(raw, yarda::CacheGeometry{32, 8, 2}, addresses);

  ASSERT_EQ(result.tasks.size(), 1U);
  ASSERT_EQ(result.tasks[0].accesses.size(), 2U);
  EXPECT_EQ(result.tasks[0].accesses[0].operation,
            yarda::AccessOperation::Store);
  EXPECT_EQ(result.tasks[0].accesses[0].source_access_ordinal, 0U);
  EXPECT_EQ(result.tasks[0].accesses[0].line_span_ordinal, 0U);
  EXPECT_EQ(result.tasks[0].accesses[1].source_access_ordinal, 0U);
  EXPECT_EQ(result.tasks[0].accesses[1].line_span_ordinal, 1U);
  EXPECT_EQ(result.tasks[0].accesses[1].object_byte_offset, 4U);
  EXPECT_EQ(result.tasks[0].coverage.emitted_line_references, 2U);
  EXPECT_EQ(result.mappings.size(), 2U);
}

TEST(TaskTraceValidationTest, RejectsInvalidTaskMappingGeometry)
{
  const auto raw = module(Json::array({function("kernel", Json::array(),
                                                Json::array({"ape.analyze"}))}),
                          Json::object());

  EXPECT_THROW(yarda::mapped_task_traces(raw, yarda::CacheGeometry{},
                                         yarda::ObjectAddressModel{}),
               std::invalid_argument);
}

TEST(TaskTraceValidationTest, NormalizesMalformedTaskLatException)
{
  const Json malformed_loop = {
    {"type", "Loop"}, {"var", 7}, {"bound", 1}, {"body", Json::array()}};
  const auto raw =
    module(Json::array({function("kernel", Json::array({malformed_loop}),
                                 Json::array({"ape.analyze"}))}),
           Json::object());

  const auto message = invalid_argument_message([&]() {
    static_cast<void>(
      yarda::resolved_task_traces(raw, yarda::ObjectAddressModel{}));
  });

  EXPECT_NE(message.find("malformed LAT input"), std::string::npos);
}

TEST(TaskTraceValidationTest, RejectsEmptyPublicRemappingInput)
{
  EXPECT_THROW(yarda::map_resolved_task_traces(yarda::ResolvedTaskTraceResult{},
                                               yarda::CacheGeometry{32, 8, 2}),
               std::invalid_argument);
}

TEST(TaskTraceValidationTest, RejectsDuplicatePublicRemappingTaskIds)
{
  yarda::ResolvedTaskTraceResult resolved;
  resolved.tasks = {{"duplicate", {}, {}}, {"duplicate", {}, {}}};

  EXPECT_THROW(
    yarda::map_resolved_task_traces(resolved, yarda::CacheGeometry{32, 8, 2}),
    std::invalid_argument);
}

TEST(TaskTraceValidationTest, RejectsEmptyPublicRemappingTaskId)
{
  yarda::ResolvedTaskTraceResult resolved;
  resolved.tasks = {{"", {}, {}}};

  EXPECT_THROW(
    yarda::map_resolved_task_traces(resolved, yarda::CacheGeometry{32, 8, 2}),
    std::invalid_argument);
}

TEST(TaskTraceValidationTest, RejectsRemappingAccessCountMismatch)
{
  yarda::ResolvedTaskTraceResult resolved;
  resolved.tasks = {
    {"task", {yarda::ResolvedAccess{}, yarda::ResolvedAccess{}}, {1, 1, 0, 0}}};
  resolved.coverage = {1, 1, 0, 0};

  EXPECT_THROW(
    yarda::map_resolved_task_traces(resolved, yarda::CacheGeometry{32, 8, 2}),
    std::invalid_argument);
}

TEST(TaskTraceValidationTest, RejectsIncompletePublicRemappingCoverage)
{
  yarda::ResolvedTaskTraceResult resolved;
  resolved.tasks = {{"task", {}, {1, 0, 1, 0}}};
  resolved.coverage = {1, 0, 1, 0};

  EXPECT_THROW(
    yarda::map_resolved_task_traces(resolved, yarda::CacheGeometry{32, 8, 2}),
    std::invalid_argument);
}

TEST(TaskTraceValidationTest, RejectsInconsistentAggregateRemappingCoverage)
{
  yarda::ResolvedTaskTraceResult resolved;
  resolved.tasks = {{"task", {}, {}}};
  resolved.coverage = {1, 1, 0, 0};

  EXPECT_THROW(
    yarda::map_resolved_task_traces(resolved, yarda::CacheGeometry{32, 8, 2}),
    std::invalid_argument);
}

}  // namespace
