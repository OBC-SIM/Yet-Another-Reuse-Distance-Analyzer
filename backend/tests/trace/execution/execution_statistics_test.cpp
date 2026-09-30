#include "trace/output/task_access_stream_test_support.hpp"

namespace
{
using namespace yarda;
using namespace yarda::test::stream;

TEST(TraceExecutionStatisticsTest, CountsNestedAndEmptyLoopsAcrossTasks)
{
  const auto raw =
      module(Json::array({function("first", {loop(2, {loop(3, {access()})})}),
                          function("second", {loop(5, Json::array())})}));
  const auto result =
      stream_resolved_task_accesses(raw, addresses(), discard_sink());
  EXPECT_EQ(result.execution_statistics.loop_iterations_expanded, 13U);
  EXPECT_EQ(result.execution_statistics.maximum_inline_depth, 0U);
  EXPECT_EQ(result.coverage.source_accesses, 6U);
}

TEST(TraceExecutionStatisticsTest, CountsStructuralDepthInZeroTripLoops)
{
  const auto raw =
      module(Json::array({function("root", {loop(0, {call("helper")})}),
                          function("helper", {call("leaf")}, "ape.inline"),
                          function("leaf", {access()}, "ape.inline")}));
  const auto result =
      stream_resolved_task_accesses(raw, addresses(), discard_sink());
  EXPECT_EQ(result.execution_statistics.maximum_inline_depth, 2U);
  EXPECT_EQ(result.execution_statistics.loop_iterations_expanded, 0U);
  EXPECT_EQ(result.coverage.source_accesses, 0U);
}

TEST(TraceExecutionStatisticsTest, StartsFreshStatisticsForEachInvocation)
{
  const auto raw =
      module(Json::array({function("root", {loop(3, {access()})})}));
  const auto first =
      stream_resolved_task_accesses(raw, addresses(), discard_sink());
  const auto second =
      stream_resolved_task_accesses(raw, addresses(), discard_sink());
  EXPECT_EQ(first.execution_statistics.loop_iterations_expanded, 3U);
  EXPECT_EQ(second.execution_statistics.loop_iterations_expanded, 3U);
}

TEST(TraceExecutionStatisticsTest, UnselectedFunctionsDoNotContributeDepth)
{
  const auto raw = module(
      Json::array({function("root", {access()}),
                   function("unselected", {call("helper")}, "ape.inline"),
                   function("helper", {call("leaf")}, "ape.inline"),
                   function("leaf", {access()}, "ape.inline")}));
  const auto result =
      stream_resolved_task_accesses(raw, addresses(), discard_sink());
  EXPECT_EQ(result.execution_statistics.maximum_inline_depth, 0U);
}

} // namespace
