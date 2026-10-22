#include "trace/output/task_access_stream_test_support.hpp"
#include "yarda/trace/calls.hpp"

namespace yarda::test
{
namespace
{

using namespace stream;

Json array_call_module(Json formal, Json actual,
                       Json indices = Json::array({"1"}))
{
  auto node = access("function:touch::param:p");
  node["name"] = "p";
  node["indices"] = std::move(indices);
  auto touch = function("touch", Json::array({node}), "ape.inline");
  touch["params"] = Json::array({"p"});
  auto invoke = call("touch");
  invoke["args"] = Json::array({"A"});
  invoke["arg_objects"] = Json::array({"global::A"});
  auto raw =
    module(Json::array({touch, function("kernel", Json::array({invoke}))}));
  raw["metadata"]["objects"]["function:touch::param:p"] = std::move(formal);
  raw["metadata"]["objects"]["global::A"] = std::move(actual);
  return raw;
}

void expect_binding_error(const Json & raw, const std::string & reason)
{
  const auto expected =
    reason +
    " (formal object 'function:touch::param:p', actual object 'global::A')";
  for (const auto expand : {expand_calls, expand_task_calls})
  {
    try
    {
      expand(raw);
      FAIL() << "Expected an incompatible inline binding to be rejected";
    }
    catch (const std::invalid_argument & error)
    {
      EXPECT_EQ(error.what(), expected);
    }
  }
}

TEST(ArrayBindingTest, RejectsDifferentElementStride)
{
  const auto raw = array_call_module(
    {{"kind", "pointer"}, {"elem_size", 8}},
    {{"kind", "array"}, {"shape", Json::array({16})}, {"elem_size", 4}});

  expect_binding_error(raw,
                       "inline object binding has incompatible element size");
}

TEST(ArrayBindingTest, RejectsDifferentRowStrideWithSameElementWidth)
{
  const auto raw = array_call_module(
    {{"kind", "pointer"}, {"shape", Json::array({2})}, {"elem_size", 4}},
    {{"kind", "array"}, {"shape", Json::array({2, 3})}, {"elem_size", 4}},
    Json::array({"1", "1"}));

  expect_binding_error(raw,
                       "inline object binding has incompatible row dimensions");
}

TEST(ArrayBindingTest, KnownScalarPointerCannotAcquireActualRowStrides)
{
  const auto raw = array_call_module(
    {{"kind", "pointer"}, {"elem_size", 4}},
    {{"kind", "array"}, {"shape", Json::array({16})}, {"elem_size", 4}},
    Json::array({"2", "3"}));

  expect_binding_error(raw, "inline object binding lacks row dimensions");
}

TEST(ArrayBindingTest, RejectsMissingActualRowDimensions)
{
  const auto raw = array_call_module(
    {{"kind", "pointer"}, {"shape", Json::array({3})}, {"elem_size", 4}},
    {{"kind", "array"}, {"elem_size", 4}}, Json::array({"1", "1"}));

  expect_binding_error(raw, "inline object binding lacks row dimensions");
}

TEST(ArrayBindingTest, ValidatesBindingsInsideZeroTripLoops)
{
  for (const auto function_index : {0, 1})
  {
    SCOPED_TRACE(function_index);
    auto raw = array_call_module(
      {{"kind", "pointer"}, {"elem_size", 8}},
      {{"kind", "array"}, {"shape", Json::array({16})}, {"elem_size", 4}});
    auto & body = raw["functions"][function_index]["body"];
    body = Json::array({loop(0, body)});

    expect_binding_error(raw,
                         "inline object binding has incompatible element size");
  }
}

TEST(ArrayBindingTest, BindsMatchingRowsDespiteDifferentOuterExtent)
{
  const auto raw = array_call_module(
    {{"kind", "pointer"}, {"shape", Json::array({3})}, {"elem_size", 4}},
    {{"kind", "array"}, {"shape", Json::array({2, 3})}, {"elem_size", 4}},
    Json::array({"1", "1"}));
  Collector collector;
  const auto summary =
    stream_resolved_task_accesses(raw, addresses(), collector.sink());

  const auto & actual = collector.result.tasks.at(0).accesses;
  ASSERT_EQ(actual.size(), 1U);
  EXPECT_EQ(actual[0].object_byte_offset, 16U);
  EXPECT_EQ(actual[0].access_size, 4U);
  EXPECT_TRUE(summary.coverage.complete());
}

TEST(ArrayBindingTest, FlatIndexDoesNotUseRowShape)
{
  const auto raw = array_call_module(
    {{"kind", "pointer"}, {"shape", Json::array({2})}, {"elem_size", 4}},
    {{"kind", "array"}, {"shape", Json::array({2, 3})}, {"elem_size", 4}});
  Collector collector;
  const auto summary =
    stream_resolved_task_accesses(raw, addresses(), collector.sink());

  const auto & actual = collector.result.tasks.at(0).accesses;
  ASSERT_EQ(actual.size(), 1U);
  EXPECT_EQ(actual[0].object_byte_offset, 4U);
  EXPECT_EQ(actual[0].access_size, 4U);
  EXPECT_TRUE(summary.coverage.complete());
}

Json field_call_module()
{
  auto raw = array_call_module(
    {{"kind", "pointer"}, {"elem_type", "S"}, {"elem_size", 8}},
    {{"kind", "struct"}, {"elem_type", "S"}, {"elem_size", 8}}, Json::array());
  auto & node = raw["functions"][0]["body"][0];
  node["access_path"] =
    Json::array({{{"kind", "field"}, {"name", "value"}, {"index", 1}}});
  raw["metadata"]["structs"]["S"] = {
    {"size", 8},
    {"fields", Json::array({{{"name", "value"},
                             {"index", 1},
                             {"offset", 4},
                             {"size", 4},
                             {"kind", "scalar"},
                             {"elem_size", 4}}})}};
  return raw;
}

TEST(ArrayBindingTest, FieldBindingComparesRootLayoutInsteadOfLeafWidth)
{
  Collector collector;
  const auto summary = stream_resolved_task_accesses(
    field_call_module(), addresses(), collector.sink());

  const auto & actual = collector.result.tasks.at(0).accesses;
  ASSERT_EQ(actual.size(), 1U);
  EXPECT_EQ(actual[0].object_byte_offset, 4U);
  EXPECT_EQ(actual[0].access_size, 4U);
  EXPECT_TRUE(summary.coverage.complete());
}

TEST(ArrayBindingTest, RejectsIncompatibleFieldRootLayout)
{
  for (const auto & change : Json::array({{{"elem_type", "Other"}},
                                          {{"elem_size", 16}},
                                          {{"shape", Json::array({2})}}}))
  {
    SCOPED_TRACE(change.dump());
    auto raw = field_call_module();
    raw["metadata"]["objects"]["global::A"].update(change);
    const auto reason = change.contains("shape")
                          ? "inline field binding has incompatible root shape"
                          : "inline field binding has incompatible root layout";
    expect_binding_error(raw, reason);
  }
}

TEST(ArrayBindingTest, StructuredRowsRetainTheirRootStride)
{
  auto raw = field_call_module();
  raw["metadata"]["objects"]["function:touch::param:p"]["shape"] =
    Json::array({3});
  raw["metadata"]["objects"]["global::A"]["shape"] = Json::array({2, 3});
  auto & node = raw["functions"][0]["body"][0];
  node["indices"] = Json::array({"1", "1"});
  node["access_path"] = Json::array({
    {{"kind", "index"}, {"value", "1"}},
    {{"kind", "index"}, {"value", "1"}},
    {{"kind", "field"}, {"name", "value"}, {"index", 1}},
  });
  Collector collector;
  const auto summary =
    stream_resolved_task_accesses(raw, addresses(), collector.sink());
  const auto & actual = collector.result.tasks.at(0).accesses;
  ASSERT_EQ(actual.size(), 1U);
  EXPECT_EQ(actual[0].object_byte_offset, 36U);
  EXPECT_EQ(actual[0].access_size, 4U);
  EXPECT_TRUE(summary.coverage.complete());

  raw["metadata"]["objects"]["global::A"]["shape"] = Json::array({2, 4});
  expect_binding_error(raw,
                       "inline object binding has incompatible row dimensions");
}

}  // namespace
}  // namespace yarda::test
