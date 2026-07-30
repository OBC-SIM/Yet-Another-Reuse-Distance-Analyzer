#include "yarda/calls.hpp"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

namespace
{

using Json = nlohmann::json;

TEST(CallsTest, ExpandsAnnotatedDirectCall)
{
  const Json module = Json::array({
    {
      {"function", "touch"},
      {"params", Json::array({"x", "idx"})},
      {"annotations", Json::array({"yard.inline"})},
      {"body", Json::array({{
                 {"type", "Array"},
                 {"name", "x"},
                 {"indices", Json::array({"idx"})},
                 {"access_path", Json::array({{
                                   {"kind", "index"},
                                   {"value", "idx"},
                                 }})},
               }})},
    },
    {
      {"function", "kernel"},
      {"annotations", Json::array({"yard.analyze"})},
      {"body", Json::array({{
                 {"type", "Call"},
                 {"callee", "touch"},
                 {"args", Json::array({"A", "i"})},
               }})},
    },
  });

  const auto expanded = yarda::expand_calls(module);

  ASSERT_EQ(expanded.size(), 1);
  const auto node = expanded[0]["body"][0];
  EXPECT_EQ(node["type"], "Array");
  EXPECT_EQ(node["name"], "A");
  EXPECT_EQ(node["indices"], Json::array({"i"}));
  EXPECT_EQ(node["access_path"][0]["value"], "i");
}

TEST(CallsTest, RejectsRecursion)
{
  const Json module = Json::array({{
    {"function", "recursive"},
    {"body", Json::array({{
               {"type", "Call"},
               {"callee", "recursive"},
               {"args", Json::array()},
             }})},
  }});

  EXPECT_THROW(yarda::expand_calls(module), std::invalid_argument);
}

}  // namespace
