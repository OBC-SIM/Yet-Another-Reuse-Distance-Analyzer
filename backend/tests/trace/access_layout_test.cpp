#include "../../src/trace/access_layout.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using Json = nlohmann::json;
using yarda::detail::AccessLayoutResolver;

Json layout_module()
{
  return {
    {"metadata",
     {{"objects",
       {{"global::s",
         {{"kind", "struct"}, {"elem_type", "S"}, {"elem_size", 16}}},
        {"global::outer",
         {{"kind", "struct"}, {"elem_type", "Outer"}, {"elem_size", 72}}},
        {"global::grid",
         {{"kind", "struct"}, {"elem_type", "Grid"}, {"elem_size", 104}}},
        {"function:f::param:rows",
         {{"kind", "pointer"},
          {"shape", Json::array({3})},
          {"elem_type", "S"},
          {"elem_size", 16}}}}},
      {"structs",
       {{"S",
         {{"name", "S"},
          {"size", 16},
          {"fields", Json::array({
                       {{"name", "x"},
                        {"index", 0},
                        {"offset", 0},
                        {"size", 4},
                        {"kind", "scalar"},
                        {"elem_type", "i32"},
                        {"elem_size", 4}},
                       {{"name", "y"},
                        {"index", 1},
                        {"offset", 8},
                        {"size", 8},
                        {"kind", "scalar"},
                        {"elem_type", "double"},
                        {"elem_size", 8}},
                     })}}},
        {"Outer",
         {{"name", "Outer"},
          {"size", 72},
          {"fields", Json::array({
                       {{"name", "tag"},
                        {"index", 0},
                        {"offset", 0},
                        {"size", 4},
                        {"kind", "scalar"},
                        {"elem_size", 4}},
                       {{"name", "items"},
                        {"index", 1},
                        {"offset", 8},
                        {"size", 64},
                        {"kind", "array"},
                        {"shape", Json::array({4})},
                        {"elem_type", "S"},
                        {"elem_size", 16}},
                     })}}},
        {"Grid",
         {{"name", "Grid"},
          {"size", 104},
          {"fields", Json::array({
                       {{"name", "tag"},
                        {"index", 0},
                        {"offset", 0},
                        {"size", 4},
                        {"kind", "scalar"},
                        {"elem_size", 4}},
                       {{"name", "cells"},
                        {"index", 1},
                        {"offset", 8},
                        {"size", 96},
                        {"kind", "array"},
                        {"shape", Json::array({2, 3})},
                        {"elem_type", "S"},
                        {"elem_size", 16}},
                     })}}}}}}},
  };
}

Json field(std::string object, std::string name, int index)
{
  return {
    {"type", "Array"},
    {"name", object + "." + name},
    {"object", std::move(object)},
    {"indices", Json::array()},
    {"access_path",
     Json::array(
       {{{"kind", "field"}, {"name", std::move(name)}, {"index", index}}})},
  };
}

Json indexed_field(std::string object, std::string array_name,
                   std::vector<std::string> indices, std::string leaf_name,
                   int leaf_index)
{
  Json path =
    Json::array({{{"kind", "field"}, {"name", array_name}, {"index", 1}}});
  for (const auto & index : indices)
  {
    path.push_back({{"kind", "index"}, {"value", index}});
  }
  path.push_back(
    {{"kind", "field"}, {"name", leaf_name}, {"index", leaf_index}});
  return {
    {"type", "Array"},
    {"name", object + "." + array_name},
    {"object", std::move(object)},
    {"indices", indices},
    {"access_path", std::move(path)},
  };
}

TEST(AccessLayoutTest, ResolvesPaddedFieldOffset)
{
  const AccessLayoutResolver resolver(layout_module());

  const auto access = resolver.resolve(field("global::s", "y", 1), {});

  ASSERT_TRUE(access);
  EXPECT_EQ(access->offset, 8);
  EXPECT_EQ(access->size, 8);
}

TEST(AccessLayoutTest, ResolvesStructArrayAndNestedField)
{
  const AccessLayoutResolver resolver(layout_module());
  const auto node = indexed_field("global::outer", "items", {"i"}, "y", 1);

  const auto access = resolver.resolve(node, {"2"});

  ASSERT_TRUE(access);
  EXPECT_EQ(access->offset, 48);
  EXPECT_EQ(access->size, 8);
}

TEST(AccessLayoutTest, ResolvesMultidimensionalFieldArray)
{
  const AccessLayoutResolver resolver(layout_module());
  const auto node = indexed_field("global::grid", "cells", {"i", "j"}, "x", 0);

  const auto access = resolver.resolve(node, {"1", "2"});

  ASSERT_TRUE(access);
  EXPECT_EQ(access->offset, 88);
  EXPECT_EQ(access->size, 4);
}

TEST(AccessLayoutTest, RejectsStructuredPointerObject)
{
  const AccessLayoutResolver resolver(layout_module());
  const Json node = {
    {"type", "Array"},
    {"name", "rows[i][j].x"},
    {"object", "function:f::param:rows"},
    {"indices", Json::array({"i", "j"})},
    {"access_path",
     Json::array({{{"kind", "index"}, {"value", "i"}},
                  {{"kind", "index"}, {"value", "j"}},
                  {{"kind", "field"}, {"name", "x"}, {"index", 0}}})},
  };

  EXPECT_THROW(resolver.resolve(node, {"2", "1"}), std::invalid_argument);
}

TEST(AccessLayoutTest, PreservesLegacyFlatArrayCalculation)
{
  const AccessLayoutResolver resolver;
  const Json node = {
    {"type", "Array"},
    {"name", "A"},
    {"shape", Json::array({2, 8})},
    {"elem_size", 4},
  };

  const auto access = resolver.resolve(node, {"1", "0"});

  ASSERT_TRUE(access);
  EXPECT_EQ(access->offset, 32);
  EXPECT_EQ(access->size, 4);
}

TEST(AccessLayoutTest, PreservesLegacyNegativeOffset)
{
  const AccessLayoutResolver resolver;
  const Json node = {{"type", "Array"}, {"elem_size", 4}};

  const auto access = resolver.resolve(node, {"-1"});

  ASSERT_TRUE(access);
  EXPECT_EQ(access->offset, -4);
}

TEST(AccessLayoutTest, ReturnsNoLegacyAccessForInexactIndex)
{
  const AccessLayoutResolver resolver;
  const Json node = {{"type", "Array"}, {"elem_size", 4}};

  EXPECT_FALSE(resolver.resolve(node, {"i"}));
}

TEST(AccessLayoutTest, RejectsMissingStructuredMetadata)
{
  const AccessLayoutResolver resolver;

  EXPECT_THROW(resolver.resolve(field("global::s", "y", 1), {}),
               std::invalid_argument);
}

TEST(AccessLayoutTest, RejectsMismatchedFieldIdentity)
{
  const AccessLayoutResolver resolver(layout_module());

  EXPECT_THROW(resolver.resolve(field("global::s", "x", 1), {}),
               std::invalid_argument);
}

TEST(AccessLayoutTest, RejectsNegativeStructuredIndex)
{
  const AccessLayoutResolver resolver(layout_module());
  const auto node = indexed_field("global::outer", "items", {"i"}, "x", 0);

  EXPECT_THROW(resolver.resolve(node, {"-1"}), std::invalid_argument);
}

TEST(AccessLayoutTest, RejectsStructuredIndexOutsideDimension)
{
  const AccessLayoutResolver resolver(layout_module());
  const auto node = indexed_field("global::outer", "items", {"i"}, "x", 0);

  EXPECT_THROW(resolver.resolve(node, {"4"}), std::invalid_argument);
}

TEST(AccessLayoutTest, RejectsStructuredOffsetOverflow)
{
  auto raw = layout_module();
  raw["metadata"]["structs"]["Outer"]["fields"][1]["shape"][0] =
    std::numeric_limits<std::int64_t>::max();
  raw["metadata"]["structs"]["Outer"]["fields"][1]["elem_size"] = 2;
  const AccessLayoutResolver resolver(raw);
  const auto node = indexed_field("global::outer", "items", {"i"}, "x", 0);

  EXPECT_THROW(resolver.resolve(node, {"1"}), std::invalid_argument);
}

TEST(AccessLayoutTest, RejectsAccessOutsideObjectExtent)
{
  auto raw = layout_module();
  raw["metadata"]["objects"]["global::s"]["elem_size"] = 12;
  const AccessLayoutResolver resolver(raw);

  EXPECT_THROW(resolver.resolve(field("global::s", "y", 1), {}),
               std::invalid_argument);
}

}  // namespace
