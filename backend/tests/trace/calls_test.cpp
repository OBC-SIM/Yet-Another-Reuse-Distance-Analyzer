#include "yarda/trace/calls.hpp"

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

TEST(CallsTest, BindsParameterAccessToActualGlobalObject)
{
  const Json module = {
    {"schema_version", 2},
    {"metadata",
     {{"objects",
       {{"global::A", {{"shape", Json::array({16})}, {"elem_size", 4}}},
        {"function:touch::param:x",
         {{"shape", Json::array({4})}, {"elem_size", 8}}}}}}},
    {"functions",
     Json::array({
       {
         {"function", "touch"},
         {"params", Json::array({"x"})},
         {"annotations", Json::array({"ape.inline"})},
         {"body", Json::array({{{"type", "Array"},
                                 {"name", "x"},
                                 {"object", "function:touch::param:x"},
                                 {"indices", Json::array({"0"})}}})},
       },
       {
         {"function", "kernel"},
         {"annotations", Json::array({"ape.analyze"})},
         {"body", Json::array({{{"type", "Call"},
                                 {"callee", "touch"},
                                 {"args", Json::array({"A"})},
                                 {"arg_objects", Json::array({"global::A"})}}})},
       },
     })},
  };

  const auto expanded = yarda::expand_calls(module);

  ASSERT_EQ(expanded.size(), 1);
  const auto & access = expanded[0]["body"][0];
  EXPECT_EQ(access["object"], "global::A");
  EXPECT_EQ(access["shape"], Json::array({16}));
  EXPECT_EQ(access["elem_size"], 4);
}

TEST(CallsTest, PropagatesActualObjectThroughNestedInlineCalls)
{
  const Json module = {
    {"schema_version", 2},
    {"metadata",
     {{"objects",
       {{"global::A", {{"shape", Json::array({16})}, {"elem_size", 4}}}}}}},
    {"functions",
     Json::array({
       {{"function", "inner"},
        {"params", Json::array({"y"})},
        {"annotations", Json::array({"ape.inline"})},
        {"body", Json::array({{{"type", "Array"},
                                {"name", "y"},
                                {"object", "function:inner::param:y"},
                                {"indices", Json::array({"0"})}}})}},
       {{"function", "outer"},
        {"params", Json::array({"x"})},
        {"annotations", Json::array({"ape.inline"})},
        {"body", Json::array({{{"type", "Call"},
                                {"callee", "inner"},
                                {"args", Json::array({"x"})},
                                {"arg_objects",
                                 Json::array({"function:outer::param:x"})}}})}},
       {{"function", "kernel"},
        {"annotations", Json::array({"ape.analyze"})},
        {"body", Json::array({{{"type", "Call"},
                                {"callee", "outer"},
                                {"args", Json::array({"A"})},
                                {"arg_objects", Json::array({"global::A"})}}})}},
     })},
  };

  const auto expanded = yarda::expand_calls(module);

  ASSERT_EQ(expanded.size(), 1);
  EXPECT_EQ(expanded[0]["body"][0]["object"], "global::A");
  EXPECT_EQ(expanded[0]["body"][0]["shape"], Json::array({16}));
}

TEST(CallsTest, RejectsTooFewArguments)
{
  const Json module = Json::array({
    {
      {"function", "touch"},
      {"params", Json::array({"x", "idx"})},
      {"body", Json::array()},
    },
    {
      {"function", "kernel"},
      {"body", Json::array({{
                 {"type", "Call"},
                 {"callee", "touch"},
                 {"args", Json::array({"A"})},
               }})},
    },
  });

  EXPECT_THROW(yarda::expand_calls(module), std::invalid_argument);
}

TEST(CallsTest, RejectsTooManyArguments)
{
  const Json module = Json::array({
    {
      {"function", "touch"},
      {"params", Json::array({"x"})},
      {"body", Json::array()},
    },
    {
      {"function", "kernel"},
      {"body", Json::array({{
                 {"type", "Call"},
                 {"callee", "touch"},
                 {"args", Json::array({"A", "i"})},
               }})},
    },
  });

  EXPECT_THROW(yarda::expand_calls(module), std::invalid_argument);
}

TEST(CallsTest, RejectsMismatchedArgumentObjects)
{
  const Json module = Json::array({
    {
      {"function", "touch"},
      {"params", Json::array({"x", "idx"})},
      {"body", Json::array()},
    },
    {
      {"function", "kernel"},
      {"body", Json::array({{
                 {"type", "Call"},
                 {"callee", "touch"},
                 {"args", Json::array({"A", "i"})},
                 {"arg_objects", Json::array({"global::A"})},
               }})},
    },
  });

  EXPECT_THROW(yarda::expand_calls(module), std::invalid_argument);
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
