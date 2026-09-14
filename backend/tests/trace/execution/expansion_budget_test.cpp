#include <nlohmann/json.hpp>

#include <functional>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

#include "yarda/trace/calls.hpp"
#include "yarda/trace/resolved_access.hpp"
#include "yarda/trace/trace.hpp"

namespace
{

using Json = nlohmann::json;

Json scalar_access() { return {{"type", "Scalar"}, {"name", "value"}}; }

Json resolved_scalar_access()
{
  return {
    {"type", "Scalar"},
    {"name", "value"},
    {"object", "global::value"},
    {"op", "load"},
  };
}

Json loop(std::int64_t bound, Json body)
{
  return {
    {"type", "Loop"},
    {"var", "i"},
    {"bound", bound},
    {"body", std::move(body)},
  };
}

Json function(std::string name, Json body)
{
  return {{"function", std::move(name)}, {"body", std::move(body)}};
}

Json analyzed_function(std::string name, Json body)
{
  return {
    {"function", std::move(name)},
    {"annotations", Json::array({"ape.analyze"})},
    {"body", std::move(body)},
  };
}

Json resolved_module(Json functions)
{
  return {
    {"schema_version", 2},
    {"metadata",
     {{"objects",
       {{"global::value", {{"kind", "scalar"}, {"elem_size", 4}}}}}}},
    {"functions", std::move(functions)},
  };
}

yarda::ObjectAddressModel resolved_addresses()
{
  yarda::ObjectAddressModel result;
  result.objects["global::value"] = {0x1000, 4};
  return result;
}

Json repeated_inline_module(unsigned depth, bool leaf_has_access = true)
{
  Json functions = Json::array();
  functions.push_back({
    {"function", "inline_0"},
    {"annotations", Json::array({"ape.inline"})},
    {"body", leaf_has_access ? Json::array({scalar_access()}) : Json::array()},
  });
  for (unsigned level = 1; level <= depth; ++level)
  {
    const auto callee = "inline_" + std::to_string(level - 1);
    const Json call = {
      {"type", "Call"}, {"callee", callee}, {"args", Json::array()}};
    functions.push_back({
      {"function", "inline_" + std::to_string(level)},
      {"annotations", Json::array({"ape.inline"})},
      {"body", Json::array({call, call})},
    });
  }
  functions.push_back({
    {"function", "kernel"},
    {"annotations", Json::array({"ape.analyze"})},
    {"body", Json::array({{{"type", "Call"},
                           {"callee", "inline_" + std::to_string(depth)},
                           {"args", Json::array()}}})},
  });
  return functions;
}

Json deep_inline_module(unsigned depth)
{
  Json functions = Json::array();
  functions.push_back({
    {"function", "inline_0"},
    {"annotations", Json::array({"ape.inline"})},
    {"body", Json::array({scalar_access()})},
  });
  for (unsigned level = 1; level < depth; ++level)
  {
    functions.push_back({
      {"function", "inline_" + std::to_string(level)},
      {"annotations", Json::array({"ape.inline"})},
      {"body", Json::array({{{"type", "Call"},
                             {"callee", "inline_" + std::to_string(level - 1)},
                             {"args", Json::array()}}})},
    });
  }
  functions.push_back({
    {"function", "kernel"},
    {"annotations", Json::array({"ape.analyze"})},
    {"body", Json::array({{{"type", "Call"},
                           {"callee", "inline_" + std::to_string(depth - 1)},
                           {"args", Json::array()}}})},
  });
  return functions;
}

Json shared_inline_callee_module(bool nested_call_first)
{
  const Json call_leaf = {
    {"type", "Call"}, {"callee", "leaf"}, {"args", Json::array()}};
  const Json call_nested = {
    {"type", "Call"}, {"callee", "nested"}, {"args", Json::array()}};
  Json root_body = nested_call_first ? Json::array({call_nested, call_leaf})
                                     : Json::array({call_leaf, call_nested});
  return Json::array({
    {
      {"function", "leaf"},
      {"annotations", Json::array({"ape.inline"})},
      {"body", Json::array({scalar_access()})},
    },
    {
      {"function", "nested"},
      {"annotations", Json::array({"ape.inline"})},
      {"body", Json::array({call_leaf})},
    },
    {
      {"function", "kernel"},
      {"annotations", Json::array({"ape.analyze"})},
      {"body", std::move(root_body)},
    },
  });
}

void expect_invalid_argument(const std::function<void()> & operation,
                             const std::string & message_fragment)
{
  try
  {
    operation();
    FAIL() << "expected std::invalid_argument";
  }
  catch (const std::invalid_argument & error)
  {
    EXPECT_NE(std::string(error.what()).find(message_fragment),
              std::string::npos);
  }
}

TEST(ExpansionBudgetTest, AllowsMoreThanFormerSourceAccessLimit)
{
  const Json module = resolved_module(Json::array({analyzed_function(
    "kernel", Json::array({loop(
                101, Json::array({loop(
                       1000, Json::array({resolved_scalar_access()}))}))}))}));

  const auto result = yarda::resolved_task_traces(module, resolved_addresses());

  ASSERT_EQ(result.tasks.size(), 1U);
  EXPECT_EQ(result.tasks[0].accesses.size(), 101'000U);
  EXPECT_EQ(result.coverage.source_accesses, 101'000U);
}

TEST(ExpansionBudgetTest, RejectsCumulativeLoopWorkAcrossNestedLoops)
{
  const Json module = Json::array({function(
    "kernel", Json::array({loop(1001, Json::array({loop(1000, {})}))}))});

  expect_invalid_argument(
    [&]() { static_cast<void>(yarda::block_traces(module)); },
    "cumulative loop iteration count exceeds 1000000");
}

TEST(ExpansionBudgetTest, AllowsSourceAccessesAcrossModuleFunctions)
{
  Json body =
    Json::array({loop(50'001, Json::array({resolved_scalar_access()}))});
  const Json module = resolved_module(
    Json::array({analyzed_function("first", body),
                 analyzed_function("second", std::move(body))}));

  const auto result = yarda::resolved_task_traces(module, resolved_addresses());

  ASSERT_EQ(result.tasks.size(), 2U);
  EXPECT_EQ(result.tasks[0].accesses.size(), 50'001U);
  EXPECT_EQ(result.tasks[1].accesses.size(), 50'001U);
  EXPECT_EQ(result.coverage.source_accesses, 100'002U);
}

TEST(ExpansionBudgetTest, RejectsRepeatedInlineExpansionBeyondNodeLimit)
{
  const auto module = repeated_inline_module(17);

  expect_invalid_argument(
    [&]() { static_cast<void>(yarda::expand_task_calls(module)); },
    "inline call expansion exceeds 100000 nodes");
}

TEST(ExpansionBudgetTest, RejectsRepeatedExpansionOfEmptyInlineCallee)
{
  const auto module = repeated_inline_module(16, false);

  expect_invalid_argument(
    [&]() { static_cast<void>(yarda::expand_task_calls(module)); },
    "inline call expansion exceeds 100000 nodes");
  expect_invalid_argument(
    [&]() { static_cast<void>(yarda::expand_calls(module)); },
    "inline call expansion exceeds 100000 nodes");
}

TEST(ExpansionBudgetTest, RejectsDeepAcyclicInlineCallChain)
{
  const auto module = deep_inline_module(257);

  expect_invalid_argument(
    [&]() { static_cast<void>(yarda::expand_task_calls(module)); },
    "inline call depth exceeds 256");
}

TEST(ExpansionBudgetTest, AllowsInlineCallDepthAtLimit)
{
  const auto module = deep_inline_module(256);

  const auto expanded = yarda::expand_task_calls(module);

  ASSERT_EQ(expanded.size(), 1U);
  EXPECT_EQ(expanded[0]["body"], Json::array({scalar_access()}));
}

TEST(ExpansionBudgetTest, ExpandsSharedInlineCalleeInEitherSiblingOrder)
{
  const auto nested_first =
    yarda::expand_task_calls(shared_inline_callee_module(true));
  const auto leaf_first =
    yarda::expand_task_calls(shared_inline_callee_module(false));

  ASSERT_EQ(nested_first.size(), 1U);
  ASSERT_EQ(leaf_first.size(), 1U);
  const auto expected = Json::array({scalar_access(), scalar_access()});
  EXPECT_EQ(nested_first[0]["body"], expected);
  EXPECT_EQ(leaf_first[0]["body"], expected);
}

}  // namespace
