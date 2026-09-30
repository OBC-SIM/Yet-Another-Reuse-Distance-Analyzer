#include <gtest/gtest.h>
#include <limits>

#include "yarda/trace/instruction_counts.hpp"

namespace
{
using Json = nlohmann::json;

Json function(const char * name, const char * role, Json opcodes,
              std::uint64_t executions = 1, Json calls = Json::array())
{
  return {{"function", name},
          {"annotations", {role}},
          {"ir_instructions",
           {{"version", 2},
            {"status", "exact"},
            {"scope", "function-exclusive"},
            {"basis", "map-extraction-ir"},
            {"excluded", "phi-debug-and-lifetime-intrinsics"},
            {"blocks", Json::array({{{"id", 0},
                                     {"name", "entry"},
                                     {"executions", executions},
                                     {"opcodes", std::move(opcodes)},
                                     {"calls", std::move(calls)}}})}}}};
}

Json call(const char * callee)
{
  return {{"callee", callee}, {"inline", true}};
}

TEST(InstructionInlineTest, SevenSweepsIncludeKernelLoadsAndInstructions)
{
  const auto input =
    Json::array({function("job", "ape.analyze", {{"call", 1}, {"br", 1}}, 7,
                          {call("kernel")}),
                 function("kernel", "ape.inline", {{"load", 3}, {"ret", 1}})});
  const auto result = yarda::count_ir_instructions(input);
  EXPECT_EQ(result["scope"], "inline-expanded");
  EXPECT_EQ(result["total"]["dynamic_instructions"], 42);
  EXPECT_EQ(result["total"]["opcodes"]["load"]["dynamic"], 21);
  EXPECT_EQ(result["functions"][0]["self"]["dynamic_instructions"], 14);
  EXPECT_EQ(result["functions"][0]["inline_callees"][0]["invocations"], 7);
  EXPECT_EQ(result["total"]["static_instructions"], 6);
}

TEST(InstructionInlineTest, NestedCallsAndRepeatedSitesMultiplyExactlyOnce)
{
  const auto input = Json::array(
    {function("job", "ape.analyze", {{"call", 2}}, 7,
              {call("middle"), call("middle")}),
     function("middle", "ape.inline", {{"call", 1}}, 3, {call("leaf")}),
     function("leaf", "ape.inline", {{"load", 2}})});
  const auto result = yarda::count_ir_instructions(input);
  EXPECT_EQ(result["total"]["opcodes"]["load"]["dynamic"], 84);
  EXPECT_EQ(result["total"]["dynamic_instructions"], 140);
  EXPECT_EQ(result["total"]["static_instructions"], 8);
}

TEST(InstructionInlineTest, NonInlineCalleeBodyIsExcluded)
{
  auto edge = call("other");
  edge["inline"] = false;
  const auto input =
    Json::array({function("job", "ape.analyze", {{"call", 1}}, 7, {edge}),
                 function("other", "", {{"load", 50}})});
  EXPECT_EQ(yarda::count_ir_instructions(input)["total"]["dynamic_"
                                                         "instructions"],
            7);
}

TEST(InstructionInlineTest, SharedCalleeContributesPerRootButNotAsAnotherRoot)
{
  const auto input = Json::array(
    {function("first", "ape.analyze", {{"call", 1}}, 2, {call("helper")}),
     function("second", "ape.analyze", {{"call", 1}}, 3, {call("helper")}),
     function("helper", "ape.inline", {{"load", 4}}),
     function("unused", "ape.inline", {{"load", 100}})});
  const auto result = yarda::count_ir_instructions(input);
  EXPECT_EQ(result["functions"].size(), 2U);
  EXPECT_EQ(result["total"]["opcodes"]["load"]["dynamic"], 20);
  EXPECT_EQ(result["total"]["dynamic_instructions"], 25);
}

TEST(InstructionInlineTest, ZeroTripCallRetainsOnlyStaticCalleeContribution)
{
  const auto input = Json::array(
    {function("job", "ape.analyze", {{"call", 1}}, 0, {call("helper")}),
     function("helper", "ape.inline", {{"load", 4}})});
  const auto result = yarda::count_ir_instructions(input);
  EXPECT_EQ(result["total"]["static_instructions"], 5);
  EXPECT_EQ(result["total"]["dynamic_instructions"], 0);
}

TEST(InstructionInlineTest, RejectsRecursionAndMissingInlineDefinition)
{
  auto input = Json::array(
    {function("job", "ape.analyze", {{"call", 1}}, 1, {call("helper")}),
     function("helper", "ape.inline", {{"call", 1}}, 1, {call("helper")})});
  EXPECT_THROW(yarda::count_ir_instructions(input), std::invalid_argument);
  input.erase(1);
  EXPECT_THROW(yarda::count_ir_instructions(input), std::invalid_argument);
}

TEST(InstructionInlineTest, RejectsInvocationOverflow)
{
  const auto input = Json::array(
    {function("job", "ape.analyze", {{"call", 1}},
              std::numeric_limits<std::uint64_t>::max(), {call("helper")}),
     function("helper", "ape.inline", {{"load", 2}})});
  EXPECT_THROW(yarda::count_ir_instructions(input), std::overflow_error);
}

TEST(InstructionInlineTest, LegacyCallsRequireRegenerationInsteadOfUndercount)
{
  auto root = function("job", "ape.analyze", {{"call", 1}});
  root["ir_instructions"]["version"] = 1;
  root["ir_instructions"]["excluded"] = "debug-and-lifetime-intrinsics";
  EXPECT_THROW(yarda::count_ir_instructions(Json::array({root})),
               std::invalid_argument);
}

TEST(InstructionInlineTest, RejectsMissingCallMetadata)
{
  const auto input =
    Json::array({function("job", "ape.analyze", {{"call", 1}})});
  EXPECT_THROW(yarda::count_ir_instructions(input), std::invalid_argument);
}
}  // namespace
