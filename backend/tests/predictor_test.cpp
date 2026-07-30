#include "yarda/predictor.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <map>

namespace
{

using Json = nlohmann::json;

Json repeated_access(const Json & indices)
{
  return {
    {"type", "Array"},
    {"name", "A"},
    {"indices", indices},
  };
}

TEST(PredictorTest, PredictsOneDimensionalStableReuse)
{
  const auto access = repeated_access(Json::array({"i"}));
  const Json module = Json::array({{
    {"function", "kernel"},
    {"body", Json::array({{
               {"type", "Loop"},
               {"var", "i"},
               {"bound", 4},
               {"body", Json::array({access, access})},
             }})},
  }});

  const auto result = yarda::predict_module(module);

  EXPECT_EQ(result.program.histogram,
            (std::map<std::size_t, std::uint64_t>{{0, 4}}));
  EXPECT_EQ(result.program.cold_misses.size(), 4);
}

TEST(PredictorTest, PredictsTwoDimensionalStableReuse)
{
  const auto access = repeated_access(Json::array({"i", "j"}));
  const Json module = Json::array({{
    {"function", "kernel"},
    {"body", Json::array({{
               {"type", "Loop"},
               {"var", "i"},
               {"bound", 3},
               {"body", Json::array({{
                          {"type", "Loop"},
                          {"var", "j"},
                          {"bound", 3},
                          {"body", Json::array({access, access})},
                        }})},
             }})},
  }});

  const auto result = yarda::predict_module(module);

  EXPECT_EQ(result.program.histogram.at(0), 9);
  EXPECT_EQ(result.program.cold_misses.size(), 9);
}

TEST(PredictorTest, PredictsThreeDimensionalStableReuse)
{
  const auto access = repeated_access(Json::array({"i", "j", "k"}));
  const Json module = Json::array({{
    {"function", "kernel"},
    {"body", Json::array({{
               {"type", "Loop"},
               {"var", "i"},
               {"bound", 2},
               {"body", Json::array({{
                          {"type", "Loop"},
                          {"var", "j"},
                          {"bound", 2},
                          {"body", Json::array({{
                                     {"type", "Loop"},
                                     {"var", "k"},
                                     {"bound", 2},
                                     {"body", Json::array({access, access})},
                                   }})},
                        }})},
             }})},
  }});

  const auto result = yarda::predict_module(module);

  EXPECT_EQ(result.program.histogram.at(0), 8);
  EXPECT_EQ(result.program.cold_misses.size(), 8);
}

}  // namespace
