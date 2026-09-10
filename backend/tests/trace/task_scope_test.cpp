#include "task_access_stream_test_support.hpp"
#include "yarda/trace/calls.hpp"
#include "yarda/trace/resolution_error.hpp"
#include "yarda/trace/schema.hpp"
#include "yarda/trace/trace.hpp"

namespace yarda::test::stream
{
namespace
{

Json region(std::string name, Json body = Json::array())
{
  auto result = function(std::move(name), std::move(body));
  result["analysis_scope"] = {{"kind", "region"}, {"name", "APE_ANALYZE"}};
  return result;
}

TEST(TaskScope, DerivesRegionIdentityWithoutRenamingFunction)
{
  auto root = region("kernel", Json::array({access()}));
  const auto raw = module(Json::array({root}));
  const auto expanded = expand_task_calls(raw);
  EXPECT_EQ(expanded[0]["function"], "kernel");
  EXPECT_EQ(expanded[0]["analysis_scope"], root["analysis_scope"]);
  const auto result = resolved_task_traces(raw, addresses());
  ASSERT_EQ(result.tasks.size(), 1U);
  EXPECT_EQ(result.tasks[0].task_id, "region:6:kernel:APE_ANALYZE");
  ASSERT_EQ(result.tasks[0].accesses.size(), 1U);
  EXPECT_EQ(result.tasks[0].accesses[0].object_id, "global::A");
}

TEST(TaskScope, CountsUtf8BytesAndRetainsEmptyRegion)
{
  const auto raw = module(Json::array({region(u8"커널")}));
  Collector collector;
  stream_resolved_task_accesses(raw, addresses(), collector.sink());
  EXPECT_EQ(collector.notifications, (std::vector<std::string>{u8"begin:region:"
                                                               u8"6:커널:APE_"
                                                               u8"ANALYZE",
                                                               u8"end:region:6:"
                                                               u8"커널:APE_"
                                                               u8"ANALYZE"}));
}

TEST(TaskScope, RejectsMalformedScopesBeforeAnyTaskCallback)
{
  const std::vector<Json> invalid{
    nullptr,
    false,
    "region",
    Json::array(),
    Json::object(),
    {{"kind", "region"}},
    {{"name", "APE_ANALYZE"}},
    {{"kind", "function"}, {"name", "APE_ANALYZE"}},
    {{"kind", "region"}, {"name", "custom"}},
    {{"kind", 1}, {"name", "APE_ANALYZE"}},
    {{"kind", "region"}, {"name", "APE_ANALYZE"}, {"extra", true}}};
  for (const auto & scope : invalid)
  {
    SCOPED_TRACE(scope.dump());
    auto root = region("later");
    root["analysis_scope"] = scope;
    Collector collector;
    EXPECT_THROW(
      stream_resolved_task_accesses(
        module(Json::array({function("first", Json::array()), root})),
        addresses(), collector.sink()),
      std::invalid_argument);
    EXPECT_TRUE(collector.notifications.empty());
  }
}

TEST(TaskScope, RejectsInlineAndUnselectedScopes)
{
  for (const auto & roles : {Json::array(), Json::array({"ape.inline"}),
                             Json::array({"ape.analyze", "ape.inline"})})
  {
    auto root = region("bad");
    root["annotations"] = roles;
    EXPECT_THROW(expand_task_calls(module(
                   Json::array({function("valid", Json::array()), root}))),
                 std::invalid_argument);
  }
}

TEST(TaskScope, RejectsCollisionWithLegacyFunctionIdentity)
{
  for (const bool reverse : {false, true})
  {
    auto roots = Json::array({region("kernel"), function("region:6:kernel:APE_"
                                                         "ANALYZE",
                                                         Json::array())});
    if (reverse) std::swap(roots[0], roots[1]);
    Collector collector;
    EXPECT_THROW(stream_resolved_task_accesses(module(roots), addresses(),
                                               collector.sink()),
                 std::invalid_argument);
    EXPECT_TRUE(collector.notifications.empty());
  }
}

TEST(TaskScope, LegacyModuleExpansionRejectsRegionBody)
{
  const auto raw = module(Json::array({region("kernel")}));
  EXPECT_THROW(expand_calls(raw), std::invalid_argument);
  EXPECT_THROW(block_traces(raw, Granularity::Element, 32),
               std::invalid_argument);
}

TEST(TaskScope, PreservesInlineObjectBindingAndTaskLocalOrdinals)
{
  auto helper = function(
    "helper", Json::array({access("function:helper::param:p")}), "ape.inline");
  helper["params"] = Json::array({"p"});
  auto site = call("helper");
  site["args"] = Json::array({"A"});
  site["arg_objects"] = Json::array({"global::A"});
  const auto raw =
    module(Json::array({region("first", Json::array({site})),
                        region("second", Json::array({access()})), helper}));
  const auto result = resolved_task_traces(raw, addresses());
  ASSERT_EQ(result.tasks.size(), 2U);
  for (const auto & task : result.tasks)
  {
    ASSERT_EQ(task.accesses.size(), 1U);
    EXPECT_EQ(task.accesses[0].source_access_ordinal, 0U);
    EXPECT_EQ(task.accesses[0].object_id, "global::A");
  }
  EXPECT_EQ(result.tasks[0].task_id, "region:5:first:APE_ANALYZE");
  EXPECT_EQ(result.tasks[1].task_id, "region:6:second:APE_ANALYZE");
}

TEST(TaskScope, ResolutionFailureReportsDerivedTaskIdentity)
{
  const auto raw =
    module(Json::array({region("kernel", Json::array({access("global::"
                                                             "missing")}))}));
  try
  {
    resolved_task_traces(raw, addresses());
    FAIL() << "missing object must fail";
  }
  catch (const ResolutionError & error)
  {
    EXPECT_NE(std::string(error.what()).find("region:6:kernel:APE_ANALYZE"),
              std::string::npos);
  }
}

}  // namespace
}  // namespace yarda::test::stream
