#include "yarda/trace/schema.hpp"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

namespace
{

using Json = nlohmann::json;

TEST(SchemaTest, PreservesLegacyFunctionList)
{
  const Json raw = Json::array({{
    {"function", "kernel"},
    {"body", Json::array({{
               {"type", "Array"},
               {"name", "A"},
               {"indices", Json::array({"i"})},
             }})},
  }});

  const auto module = yarda::normalize_module(raw);

  ASSERT_EQ(module.size(), 1);
  EXPECT_EQ(module[0]["function"], "kernel");
}

TEST(SchemaTest, EnrichesV2ArrayMetadata)
{
  const Json raw = {
    {"schema_version", 2},
    {"metadata",
     {{"objects",
       {{"obj-A",
         {
           {"shape", Json::array({4, 8})},
           {"elem_size", 8},
         }}}}}},
    {"functions", Json::array({{
                    {"function", "kernel"},
                    {"body", Json::array({{
                               {"type", "Array"},
                               {"name", "A"},
                               {"object", "obj-A"},
                               {"indices", Json::array({"i", "j"})},
                             }})},
                  }})},
  };

  const auto node = yarda::normalize_module(raw)[0]["body"][0];

  EXPECT_EQ(node["shape"], Json::array({4, 8}));
  EXPECT_EQ(node["elem_size"], 8);
}

TEST(SchemaTest, RejectsUnsupportedRoot)
{
  EXPECT_THROW(yarda::normalize_module(Json::object()), std::invalid_argument);
}

}  // namespace
