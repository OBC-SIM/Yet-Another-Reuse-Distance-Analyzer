#include <limits>

#include "work_limits_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::work;

TEST(LoopWorkLimitsFailureTest, RejectsSingleLimitBeforeAnyBodyAccess)
{
  const auto raw = loop_module(3, Json::array({access()}));
  TraceEmissionBudget budget({100, 100});
  Collector collector;
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), collector.sink(), budget,
                                    {2, 10});
    },
    "loop iteration count exceeds 2");
  EXPECT_EQ(collector.notifications, (std::vector<std::string>{"begin:"
                                                               "kernel"}));
}

TEST(LoopWorkLimitsFailureTest, ZeroSingleLimitRejectsNonemptyTripCount)
{
  const auto raw = loop_module(1, Json::array());
  TraceEmissionBudget budget({0, 0});
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), discard_sink(), budget,
                                    {0, 1});
    },
    "loop iteration count exceeds 0");
}

TEST(LoopWorkLimitsFailureTest, ZeroCumulativeLimitRejectsFirstLoopReservation)
{
  const auto raw = loop_module(1, Json::array());
  TraceEmissionBudget budget({0, 0});
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), discard_sink(), budget,
                                    {1, 0});
    },
    "cumulative loop iteration count exceeds 0");
}

TEST(LoopWorkLimitsFailureTest, SingleLimitFailurePrecedesCumulativeFailure)
{
  const auto raw = loop_module(2, Json::array());
  TraceEmissionBudget budget({0, 0});
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), discard_sink(), budget,
                                    {1, 0});
    },
    "loop iteration count exceeds 1");
}

TEST(LoopWorkLimitsFailureTest, ReservesNestedTripCountBeforeItsNextBody)
{
  const auto raw =
    loop_module(2, Json::array({loop(3, Json::array({access()}), "j")}));
  TraceEmissionBudget budget({6, 0});
  Collector collector;
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), collector.sink(), budget,
                                    {3, 7});
    },
    "cumulative loop iteration count exceeds 7");
  EXPECT_EQ(collector.notifications,
            (std::vector<std::string>{"begin:kernel", "access:kernel:0",
                                      "access:kernel:1", "access:kernel:2"}));
}

TEST(LoopWorkLimitsFailureTest, EmptyInnerLoopsStillExhaustCumulativeAllowance)
{
  const auto raw = loop_module(2, Json::array({loop(3, Json::array(), "j")}));
  TraceEmissionBudget budget({0, 0});
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), discard_sink(), budget,
                                    {3, 7});
    },
    "cumulative loop iteration count exceeds 7");
}

TEST(LoopWorkLimitsFailureTest, ZeroTripInnerLoopsStillRequireOuterAllowance)
{
  const auto raw = loop_module(2, Json::array({loop(0, Json::array(), "j")}));
  TraceEmissionBudget budget({0, 0});
  Collector collector;
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), collector.sink(), budget,
                                    {2, 1});
    },
    "cumulative loop iteration count exceeds 1");
  EXPECT_EQ(collector.notifications, (std::vector<std::string>{"begin:"
                                                               "kernel"}));
}

TEST(LoopWorkLimitsFailureTest, LaterTaskFailureStopsAllFollowingCallbacks)
{
  const auto body = Json::array({loop(2, Json::array({access()}))});
  const auto raw =
    module(Json::array({function("first", body), function("second", body),
                        function("third", Json::array({access()}))}));
  TraceEmissionBudget budget({10, 0});
  Collector collector;
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), collector.sink(), budget,
                                    {2, 3});
    },
    "cumulative loop iteration count exceeds 3");
  EXPECT_EQ(
    collector.notifications,
    (std::vector<std::string>{"begin:first", "access:first:0", "access:first:1",
                              "end:first", "begin:second"}));
}

TEST(LoopWorkLimitsFailureTest, RaisingSingleLimitDoesNotRaiseCumulativeLimit)
{
  const auto raw = loop_module(1'000'001, Json::array());
  TraceEmissionBudget budget({0, 0});
  LoopWorkLimits limits;
  limits.single_loop_iterations = 1'000'001;
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), discard_sink(), budget,
                                    limits);
    },
    "cumulative loop iteration count exceeds 1000000");
}

TEST(LoopWorkLimitsFailureTest,
     LoopReservationPrecedesSourceAndResolutionFailure)
{
  const auto raw = loop_module(2, Json::array({access("global::missing")}));
  TraceEmissionBudget budget({0, 0});
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), discard_sink(), budget,
                                    {2, 1});
    },
    "cumulative loop iteration count exceeds 1");
}

TEST(LoopWorkLimitsFailureTest,
     SourceFailureStillPrecedesResolutionWithCustomLoops)
{
  const auto raw = loop_module(1, Json::array({access("global::missing")}));
  TraceEmissionBudget budget({0, 0});
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), discard_sink(), budget,
                                    {1, 1});
    },
    "emitted source accesses exceeds 0");
}

TEST(LoopWorkLimitsFailureTest, ZeroStepPrecedesZeroLoopAllowance)
{
  const auto raw = loop_module(0, Json::array(), 0, 0);
  TraceEmissionBudget budget({0, 0});
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), discard_sink(), budget,
                                    {0, 0});
    },
    "loop step must be non-zero");
}

TEST(LoopWorkLimitsFailureTest, RejectsExtremeSignedSpanUsingUnsignedTripCount)
{
  const auto raw =
    loop_module(std::numeric_limits<std::int64_t>::max(), Json::array(),
                std::numeric_limits<std::int64_t>::min(),
                std::numeric_limits<std::int64_t>::max());
  TraceEmissionBudget budget({0, 0});
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), discard_sink(), budget,
                                    {2, 4});
    },
    "loop iteration count exceeds 2");
}

TEST(LoopWorkLimitsFailureTest, ConsumerJsonExceptionPrecedesLaterLoopFailure)
{
  const auto raw = module(Json::array(
    {function("kernel", Json::array({access(), loop(1, Json::array())}))}));
  TraceEmissionBudget budget({1, 0});
  Collector collector;
  auto sink = collector.sink();
  sink.access = [](const std::string &, const ResolvedAccess &) {
    static_cast<void>(Json::object().at("consumer-token"));
  };
  EXPECT_THROW(
    stream_resolved_task_accesses(raw, addresses(), sink, budget, {0, 0}),
    Json::out_of_range);
  EXPECT_EQ(collector.notifications, (std::vector<std::string>{"begin:"
                                                               "kernel"}));
}

}  // namespace
