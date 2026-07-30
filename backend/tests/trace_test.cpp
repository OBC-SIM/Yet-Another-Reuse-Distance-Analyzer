#include "yarda/trace.hpp"

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
