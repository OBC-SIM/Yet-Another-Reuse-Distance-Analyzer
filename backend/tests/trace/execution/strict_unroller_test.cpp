#include <nlohmann/json.hpp>

#include <cstdint>
#include <gtest/gtest.h>
#include <limits>

#include "yarda/trace/resolved_access.hpp"
#include "yarda/trace/trace.hpp"

namespace
{

using Json = nlohmann::json;

Json loop_module(std::int64_t start, std::int64_t bound, std::int64_t step,
                 Json body)
{
  return Json::array({{{"function", "kernel"},
                       {"body", Json::array({{{"type", "Loop"},
                                              {"var", "i"},
                                              {"start", start},
                                              {"bound", bound},
                                              {"step", step},
                                              {"body", std::move(body)}}})}}});
}

TEST(StrictUnrollerTest, RejectsZeroLoopStep)
{
  EXPECT_THROW(yarda::block_traces(loop_module(0, 3, 0, Json::array())),
               std::invalid_argument);
}

TEST(StrictUnrollerTest, ExpandsExtremeLoopWithoutSignedOverflow)
{
  const Json access = {
    {"type", "Array"},
    {"name", "A"},
    {"indices", Json::array({"i"})},
    {"shape", Json::array({3})},
    {"elem_size", 4},
  };

  const auto traces = yarda::block_traces(loop_module(
    std::numeric_limits<std::int64_t>::min(),
    std::numeric_limits<std::int64_t>::max(),
    std::numeric_limits<std::int64_t>::max(), Json::array({access})));

  ASSERT_EQ(traces.size(), 1U);
  EXPECT_EQ(traces[0].accesses,
            (std::vector<std::string>{"A--9223372036854775808", "A--1",
                                      "A-9223372036854775806"}));
}

TEST(StrictUnrollerTest, RejectsLoopBeyondExpansionLimit)
{
  EXPECT_THROW(yarda::block_traces(loop_module(0, 1'000'001, 1, Json::array())),
               std::invalid_argument);
}

TEST(StrictUnrollerTest, CountsLoopAccessCoverageAndOrdinals)
{
  const Json access = {
    {"type", "Array"},       {"name", "A"},
    {"object", "global::A"}, {"indices", Json::array({"i"})},
    {"op", "load"},
  };
  auto raw = loop_module(0, 4, 1, Json::array({access}));
  raw = {
    {"metadata",
     {{"objects",
       {{"global::A",
         {{"kind", "array"},
          {"shape", Json::array({4})},
          {"elem_size", 4}}}}}}},
    {"functions", raw},
  };
  yarda::ObjectAddressModel addresses;
  addresses.objects["global::A"] = {0x1000, 16};

  const auto result = yarda::resolved_block_traces(raw, addresses);

  ASSERT_EQ(result.traces.size(), 1U);
  ASSERT_EQ(result.traces[0].accesses.size(), 4U);
  EXPECT_EQ(result.coverage.source_accesses, 4U);
  EXPECT_EQ(result.coverage.resolved_accesses, 4U);
  EXPECT_EQ(result.traces[0].accesses[0].source_access_ordinal, 0U);
  EXPECT_EQ(result.traces[0].accesses[3].source_access_ordinal, 3U);
}

}  // namespace
