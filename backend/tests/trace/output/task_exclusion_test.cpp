#include <nlohmann/json.hpp>

#include <gtest/gtest.h>
#include <stdexcept>

#include "yarda/trace/mapped_trace.hpp"
#include "yarda/trace/resolved_access.hpp"

namespace
{

using Json = nlohmann::json;

Json opaque_call_module()
{
  const Json call_excluded = {
    {"type", "Call"}, {"callee", "excluded"}, {"args", Json::array()}};
  const Json access = {
    {"type", "Array"},       {"name", "A[0]"},
    {"object", "global::A"}, {"indices", Json::array({"0"})},
    {"op", "load"},
  };
  const Json call_helper = {
    {"type", "Call"}, {"callee", "helper"}, {"args", Json::array()}};
  const Json loop = {
    {"type", "Loop"}, {"var", "i"}, {"start", 0},
    {"bound", 3},     {"step", 1},  {"body", Json::array({call_helper})},
  };
  return {
    {"schema_version", 2},
    {"metadata",
     {{"objects",
       {{"global::A",
         {{"kind", "array"},
          {"shape", Json::array({1})},
          {"elem_size", 4}}}}}}},
    {"functions",
     Json::array({{{"function", "excluded"}, {"body", Json::array()}},
                  {{"function", "helper"},
                   {"annotations", Json::array({"ape.inline"})},
                   {"body", Json::array({call_excluded, access})}},
                  {{"function", "kernel"},
                   {"annotations", Json::array({"ape.analyze"})},
                   {"body", Json::array({loop})}}})},
  };
}

yarda::ResolvedTaskTraceResult resolved_with_opaque_call()
{
  yarda::ObjectAddressModel objects;
  objects.objects["global::A"] = {0x1000, 4};
  return yarda::resolved_task_traces(opaque_call_module(), objects);
}

TEST(TaskExclusionTest, CountsExpandedCallSiteWithoutLoopMultiplication)
{
  const auto result = resolved_with_opaque_call();

  ASSERT_EQ(result.tasks.size(), 1U);
  EXPECT_EQ(result.tasks[0].accesses.size(), 3U);
  EXPECT_EQ(result.tasks[0].excluded_opaque_call_sites, 1U);
  EXPECT_EQ(result.excluded_opaque_call_sites, 1U);
  EXPECT_EQ(result.coverage.source_accesses, 3U);
  EXPECT_TRUE(result.coverage.complete());
}

TEST(TaskExclusionTest, PreservesAndValidatesCountDuringMapping)
{
  auto resolved = resolved_with_opaque_call();
  const auto mapped = yarda::map_resolved_task_traces(resolved, {32, 8, 2});

  ASSERT_EQ(mapped.tasks.size(), 1U);
  EXPECT_EQ(mapped.tasks[0].excluded_opaque_call_sites, 1U);
  EXPECT_EQ(mapped.excluded_opaque_call_sites, 1U);

  resolved.excluded_opaque_call_sites = 0;
  EXPECT_THROW(yarda::map_resolved_task_traces(resolved, {32, 8, 2}),
               std::invalid_argument);
}

}  // namespace
