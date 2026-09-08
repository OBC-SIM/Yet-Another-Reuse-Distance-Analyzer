#include <stdexcept>

#include "task_access_stream_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::stream;

TEST(TaskAccessStreamBudgetTest, EmptyTasksConsumeNeitherSourceNorLineBudget)
{
  const auto raw = module(Json::array({
    function("first", Json::array()),
    function("second", Json::array()),
  }));
  TraceEmissionBudget budget({0, 0});
  Collector collector;
  collector.complete(
    stream_resolved_task_accesses(raw, addresses(), collector.sink(), budget));

  EXPECT_EQ(collector.notifications,
            (std::vector<std::string>{"begin:first", "end:first",
                                      "begin:second", "end:second"}));
  expect_coverage(collector.result.coverage, {});
}

TEST(TaskAccessStreamBudgetTest, AllowsExactSourceBoundaryAcrossTasks)
{
  const auto raw = module(Json::array({
    function("first", Json::array({access(), access()})),
    function("second", Json::array({access()})),
  }));
  TraceEmissionBudget budget({3, 0});
  Collector collector;
  collector.complete(
    stream_resolved_task_accesses(raw, addresses(), collector.sink(), budget));

  expect_coverage(collector.result.coverage, {3, 3, 0, 0});
  ASSERT_EQ(collector.result.tasks.size(), 2U);
  EXPECT_EQ(collector.result.tasks[1].accesses.size(), 1U);
}

TEST(TaskAccessStreamBudgetTest,
     RejectsSourceLimitAcrossTasksBeforeNextDelivery)
{
  const auto raw = module(Json::array({
    function("first", Json::array({access(), access()})),
    function("second", Json::array({access(), access()})),
  }));
  TraceEmissionBudget budget({3, 100});
  Collector collector;

  EXPECT_THROW(
    stream_resolved_task_accesses(raw, addresses(), collector.sink(), budget),
    std::invalid_argument);
  EXPECT_EQ(
    collector.notifications,
    (std::vector<std::string>{"begin:first", "access:first:0", "access:first:1",
                              "end:first", "begin:second", "access:second:0"}));
}

TEST(TaskAccessStreamBudgetTest, RejectsSourceLimitWithinNestedLoops)
{
  const auto raw = module(Json::array({function(
    "kernel", Json::array({
                loop(2, Json::array({loop(3, Json::array({access()}), "j")})),
              }))}));
  TraceEmissionBudget budget({5, 100});
  std::uint64_t count = 0;
  bool ended = false;
  auto sink = discard_sink();
  sink.access = [&](const std::string &, const ResolvedAccess &) { ++count; };
  sink.end_task = [&](const std::string &, const TraceCoverage &) {
    ended = true;
  };

  EXPECT_THROW(stream_resolved_task_accesses(raw, addresses(), sink, budget),
               std::invalid_argument);
  EXPECT_EQ(count, 5U);
  EXPECT_FALSE(ended);
}

TEST(TaskAccessStreamBudgetTest, ChecksSourceLimitBeforeResolvingNextAccess)
{
  const auto raw = module(Json::array({
    function("kernel", Json::array({access("global::missing")})),
  }));
  TraceEmissionBudget budget({0, 100});
  try
  {
    static_cast<void>(
      stream_resolved_task_accesses(raw, addresses(), discard_sink(), budget));
    FAIL() << "expected source limit failure";
  }
  catch (const ResolutionError &)
  {
    FAIL() << "source budget must be checked before resolution";
  }
  catch (const std::invalid_argument & error)
  {
    EXPECT_NE(
      std::string(error.what()).find("emitted source accesses exceeds 0"),
      std::string::npos);
  }
}

TEST(TaskAccessStreamBudgetTest, SharesLineLimitAcrossTasksAndStopsInsideSpan)
{
  const auto raw = module(Json::array({
                            function("first", Json::array({access()})),
                            function("second", Json::array({access()})),
                          }),
                          40);
  TraceEmissionBudget budget({2, 4});
  std::vector<std::string> delivered;
  std::vector<std::string> ended;
  auto sink = discard_sink();
  sink.access = [&](const std::string & id, const ResolvedAccess & value) {
    for_each_cache_line(
      value, {32, 8, 2},
      [&](const CacheLineMapping & line) {
        delivered.push_back(id + ":" + std::to_string(line.line_span_ordinal));
      },
      budget);
  };
  sink.end_task = [&](const std::string & id, const TraceCoverage &) {
    ended.push_back(id);
  };

  EXPECT_THROW(stream_resolved_task_accesses(raw, addresses(40), sink, budget),
               std::invalid_argument);
  EXPECT_EQ(delivered, (std::vector<std::string>{"first:0", "first:1",
                                                 "first:2", "second:0"}));
  EXPECT_EQ(ended, (std::vector<std::string>{"first"}));
}

TEST(TaskAccessStreamBudgetTest, AllowsExactLineBoundaryAcrossSourceAccesses)
{
  const auto raw =
    module(Json::array({
             function("kernel", Json::array({access(), access()})),
           }),
           40);
  TraceEmissionBudget budget({2, 6});
  std::uint64_t count = 0;
  auto sink = discard_sink();
  sink.access = [&](const std::string &, const ResolvedAccess & value) {
    for_each_cache_line(
      value, {32, 8, 2}, [&](const CacheLineMapping &) { ++count; }, budget);
  };

  const auto summary =
    stream_resolved_task_accesses(raw, addresses(40), sink, budget);
  EXPECT_EQ(count, 6U);
  expect_coverage(summary.coverage, {2, 2, 0, 0});
}

TEST(TaskAccessStreamBudgetTest, RejectsHugeSpanAtSmallLineBudget)
{
  const ResolvedAccess value{
    "global::A",           0, std::uint64_t{1} << 40, 0, AddressBasis::Absolute,
    AccessOperation::Load, 0};
  TraceEmissionBudget budget({0, 2});
  std::uint64_t count = 0;
  try
  {
    for_each_cache_line(
      value, {32, 8, 2}, [&](const CacheLineMapping &) { ++count; }, budget);
    FAIL() << "expected line limit failure";
  }
  catch (const std::invalid_argument & error)
  {
    EXPECT_NE(
      std::string(error.what()).find("emitted line references exceeds 2"),
      std::string::npos);
  }
  EXPECT_EQ(count, 2U);
}

TEST(TaskAccessStreamBudgetTest, PreservesCumulativeLoopLimitWithoutAccesses)
{
  const auto raw = module(Json::array({function(
    "kernel", Json::array({
                loop(1001, Json::array({loop(1000, Json::array(), "j")})),
              }))}));
  Collector collector;

  EXPECT_THROW(
    stream_resolved_task_accesses(raw, addresses(), collector.sink()),
    std::invalid_argument);
  EXPECT_EQ(collector.notifications, (std::vector<std::string>{"begin:"
                                                               "kernel"}));
}

TEST(TaskAccessStreamBudgetTest, PreservesStructuralLimitForEmptyInlineFanout)
{
  Json functions =
    Json::array({function("inline_0", Json::array(), "ape.inline")});
  for (unsigned depth = 1; depth <= 16; ++depth)
  {
    const auto callee = "inline_" + std::to_string(depth - 1);
    functions.push_back(function("inline_" + std::to_string(depth),
                                 Json::array({call(callee), call(callee)}),
                                 "ape.inline"));
  }
  functions.push_back(function("kernel", Json::array({call("inline_16")})));
  Collector collector;

  EXPECT_THROW(stream_resolved_task_accesses(module(functions), addresses(),
                                             collector.sink()),
               std::invalid_argument);
  EXPECT_TRUE(collector.notifications.empty());
}

TEST(TaskAccessStreamBudgetTest, DefaultSourceLimitIsCumulativeAndExplicit)
{
  const auto raw = module(Json::array(
    {function("kernel", Json::array({
                          loop(500'001, Json::array({access(), access()})),
                        }))}));
  std::uint64_t count = 0;
  auto sink = discard_sink();
  sink.access = [&](const std::string &, const ResolvedAccess &) { ++count; };

  EXPECT_THROW(stream_resolved_task_accesses(raw, addresses(), sink),
               std::invalid_argument);
  EXPECT_EQ(count, 1'000'000U);
}

TEST(TaskAccessStreamBudgetTest, LegacyVectorRetainsPreviousSourceDomain)
{
  const auto raw = module(Json::array(
    {function("kernel", Json::array({
                          loop(500'001, Json::array({access(), access()})),
                        }))}));
  const auto batch = resolved_task_traces(raw, addresses());
  TraceEmissionBudget budget({1'000'002, 0});
  std::uint64_t count = 0;
  auto sink = discard_sink();
  sink.access = [&](const std::string &, const ResolvedAccess &) { ++count; };
  const auto summary =
    stream_resolved_task_accesses(raw, addresses(), sink, budget);

  EXPECT_EQ(count, 1'000'002U);
  ASSERT_EQ(batch.tasks.size(), 1U);
  EXPECT_EQ(batch.tasks[0].accesses.size(), count);
  expect_coverage(summary.coverage, batch.coverage);
}

TEST(TraceEmissionBudgetTest, SourceAndLineReservationsHaveIndependentLimits)
{
  TraceEmissionBudget budget({1, 2});
  budget.consume_source_access();
  budget.consume_line_reference();
  budget.consume_line_reference();

  EXPECT_THROW(budget.consume_source_access(), std::invalid_argument);
  EXPECT_THROW(budget.consume_line_reference(), std::invalid_argument);
}

TEST(TraceEmissionBudgetTest, ZeroLimitsRejectTheFirstReservation)
{
  TraceEmissionBudget budget({0, 0});

  EXPECT_THROW(budget.consume_source_access(), std::invalid_argument);
  EXPECT_THROW(budget.consume_line_reference(), std::invalid_argument);
}

}  // namespace
