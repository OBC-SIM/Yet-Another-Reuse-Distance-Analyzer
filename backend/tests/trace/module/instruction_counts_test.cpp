#include "yarda/trace/instruction_counts.hpp"

#include <gtest/gtest.h>
#include <limits>

namespace
{
using Json = nlohmann::json;

Json fixture()
{
  return Json::array(
    {{{"function", "kernel"},
      {"ir_instructions",
       {{"version", 1},
        {"status", "exact"},
        {"scope", "function-exclusive"},
        {"basis", "map-extraction-ir"},
        {"excluded", "debug-and-lifetime-intrinsics"},
        {"blocks",
         Json::array({{{"id", 0},
                       {"name", "entry"},
                       {"executions", 1},
                       {"opcodes", {{"br", 1}}}},
                      {{"id", 1},
                       {"name", "header"},
                       {"executions", 4},
                       {"opcodes", {{"phi", 1}, {"icmp", 1}, {"br", 1}}}},
                      {{"id", 2},
                       {"name", "body"},
                       {"executions", 3},
                       {"opcodes", {{"add", 1}, {"br", 1}}}},
                      {{"id", 3},
                       {"name", "exit"},
                       {"executions", 1},
                       {"opcodes", {{"ret", 1}}}}})}}}}});
}

TEST(InstructionCountTest, AggregatesDynamicCountsIncludingLoopControl)
{
  const auto result = yarda::count_ir_instructions(fixture());
  EXPECT_EQ(result["total"]["static_instructions"], 6);
  EXPECT_EQ(result["total"]["dynamic_instructions"], 16);
  EXPECT_EQ(result["total"]["opcodes"]["br"]["dynamic"], 8);
  EXPECT_FALSE(result["total"]["opcodes"].contains("phi"));
  EXPECT_EQ(result["functions"][0]["blocks"][2]["dynamic_instructions"], 6);
}

TEST(InstructionCountTest, ZeroExecutionBlockRetainsStaticCount)
{
  auto input = fixture();
  input[0]["ir_instructions"]["blocks"][1]["executions"] = 1;
  input[0]["ir_instructions"]["blocks"][2]["executions"] = 0;
  const auto result = yarda::count_ir_instructions(input);
  EXPECT_EQ(result["total"]["static_instructions"], 6);
  EXPECT_EQ(result["total"]["dynamic_instructions"], 4);
}

TEST(InstructionCountTest, CountsOnlyAnnotatedRootsOnce)
{
  auto input = fixture();
  auto helper = input[0];
  input[0]["annotations"] = {"ape.analyze"};
  helper["function"] = "helper";
  helper.erase("ir_instructions");
  input.push_back(helper);
  const auto result = yarda::count_ir_instructions(input);
  EXPECT_EQ(result["functions"].size(), 1U);
  EXPECT_EQ(result["total"]["dynamic_instructions"], 16);
}

TEST(InstructionCountTest, LegacyMapRequiresRegeneration)
{
  auto input = fixture();
  input[0].erase("ir_instructions");
  EXPECT_THROW(yarda::count_ir_instructions(input), std::invalid_argument);
}

TEST(InstructionCountTest, RejectsUnsupportedControlFlow)
{
  auto input = fixture();
  input[0]["ir_instructions"] = {{"version", 1},
                                 {"status", "unsupported"},
                                 {"reason", "data-dependent IR branch"}};
  EXPECT_THROW(yarda::count_ir_instructions(input), std::invalid_argument);
}

TEST(InstructionCountTest, RejectsNegativeFractionalAndFloatingCounts)
{
  for (const Json value : {Json(-1), Json(1.5), Json(1e30), Json("4")})
  {
    auto input = fixture();
    input[0]["ir_instructions"]["blocks"][1]["executions"] = value;
    EXPECT_THROW(yarda::count_ir_instructions(input), std::invalid_argument);
  }
}

TEST(InstructionCountTest, RejectsMultiplicationOverflow)
{
  auto input = fixture();
  auto & block = input[0]["ir_instructions"]["blocks"][1];
  block["executions"] = std::numeric_limits<std::uint64_t>::max();
  block["opcodes"]["br"] = 2;
  EXPECT_THROW(yarda::count_ir_instructions(input), std::overflow_error);
}

TEST(InstructionCountTest, RejectsAdditionOverflow)
{
  auto input = fixture();
  input[0]["ir_instructions"]["blocks"][0]["executions"] =
    std::numeric_limits<std::uint64_t>::max();
  EXPECT_THROW(yarda::count_ir_instructions(input), std::overflow_error);
}

TEST(InstructionCountTest, RejectsDuplicateBlocksAndContractMismatch)
{
  auto input = fixture();
  input[0]["ir_instructions"]["blocks"][1]["id"] = 0;
  EXPECT_THROW(yarda::count_ir_instructions(input), std::invalid_argument);
  input = fixture();
  input[0]["ir_instructions"]["basis"] = "machine-code";
  EXPECT_THROW(yarda::count_ir_instructions(input), std::invalid_argument);
}

}  // namespace
