#include "trace/output/task_access_stream_test_support.hpp"
#include "yarda/trace/calls.hpp"

namespace yarda::test
{
namespace
{

using namespace stream;

Json scalar_call_module(bool nested)
{
  auto inner = function("inner",
                        Json::array({{{"type", "Scalar"},
                                      {"name", "p"},
                                      {"object", "function:inner::param:p"},
                                      {"op", "store"}}}),
                        "ape.inline");
  inner["params"] = Json::array({"p"});
  auto invoke = call("inner");
  invoke["args"] = Json::array({"A"});
  invoke["arg_objects"] = Json::array({"global::A"});
  auto outer = function("outer", Json::array(), "ape.inline");
  outer["params"] = Json::array({"q"});
  if (nested)
  {
    auto forwarded = invoke;
    forwarded["args"] = Json::array({"q"});
    forwarded["arg_objects"] = Json::array({"function:outer::param:q"});
    outer["body"].push_back(forwarded);
    invoke["callee"] = "outer";
  }
  auto raw = module(
    Json::array({inner, outer, function("kernel", Json::array({invoke}))}));
  raw["metadata"]["objects"]["global::A"] = {{"kind", "scalar"},
                                             {"elem_size", 4}};
  return raw;
}

TEST(ScalarCallsTest, BindsDirectAndForwardedPointerToActualObject)
{
  for (const auto nested : {false, true})
  {
    SCOPED_TRACE(nested);
    const auto expanded = expand_calls(scalar_call_module(nested));
    ASSERT_EQ(expanded.size(), 1U);
    const auto & node = expanded[0]["body"][0];
    EXPECT_EQ(node.at("object"), "global::A");
    EXPECT_EQ(node.at("name"), "A");
    ASSERT_TRUE(node.contains("elem_size"));
    EXPECT_EQ(node.at("elem_size"), 4);
    EXPECT_FALSE(node.contains("indices"));
  }
}

TEST(ScalarCallsTest, StreamsActualAddressWidthAndCoverage)
{
  for (const auto nested : {false, true})
  {
    SCOPED_TRACE(nested);
    Collector collector;
    const auto summary = stream_resolved_task_accesses(
      scalar_call_module(nested), addresses(), collector.sink());
    ASSERT_EQ(collector.result.tasks.size(), 1U);
    const auto & accesses = collector.result.tasks[0].accesses;
    ASSERT_EQ(accesses.size(), 1U);
    EXPECT_EQ(accesses[0].object_id, "global::A");
    EXPECT_EQ(accesses[0].linked_byte_address, 0x101cU);
    EXPECT_EQ(accesses[0].object_byte_offset, 0U);
    EXPECT_EQ(accesses[0].access_size, 4U);
    EXPECT_EQ(accesses[0].operation, AccessOperation::Store);
    EXPECT_EQ(summary.coverage.resolved_accesses, 1U);
    EXPECT_TRUE(summary.coverage.complete());
  }
}

TEST(ScalarCallsTest, PreservesAccessWidthWhenStorageHasDifferentElementSize)
{
  auto raw = scalar_call_module(false);
  raw["metadata"]["objects"]["function:inner::param:p"] = {{"kind", "pointer"},
                                                           {"elem_size", 1}};
  Collector collector;
  const auto summary =
    stream_resolved_task_accesses(raw, addresses(), collector.sink());
  const auto & actual = collector.result.tasks.at(0).accesses;
  ASSERT_EQ(actual.size(), 1U);
  EXPECT_EQ(actual[0].linked_byte_address, 0x101cU);
  EXPECT_EQ(actual[0].access_size, 1U);
  EXPECT_TRUE(summary.coverage.complete());
}

TEST(ScalarCallsTest, ExplicitAccessWidthOverridesFormalPointeeMetadata)
{
  auto raw = scalar_call_module(false);
  raw["functions"][0]["body"][0]["elem_size"] = 4;
  raw["metadata"]["objects"]["function:inner::param:p"] = {{"kind", "pointer"},
                                                           {"elem_size", 1}};
  raw["metadata"]["objects"]["global::A"]["elem_size"] = 1;
  Collector collector;
  const auto summary =
    stream_resolved_task_accesses(raw, addresses(1), collector.sink());
  const auto & actual = collector.result.tasks.at(0).accesses;
  ASSERT_EQ(actual.size(), 1U);
  EXPECT_EQ(actual[0].linked_byte_address, 0x101cU);
  EXPECT_EQ(actual[0].access_size, 4U);
  EXPECT_TRUE(summary.coverage.complete());
}

}  // namespace
}  // namespace yarda::test
