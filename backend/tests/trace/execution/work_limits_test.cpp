#include "work_limits_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::work;

TEST(LoopWorkLimitsTest,
     AllowsExactSingleLimitWithIndependentAccessExpectations)
{
  const auto raw = loop_module(3, Json::array({access("global::A", "i")}));
  TraceEmissionBudget budget({3, 0});
  Collector collector;
  collector.complete(stream_resolved_task_accesses(
    raw, addresses(), collector.sink(), budget, {3, 3}));

  expect_coverage(collector.result.coverage, {3, 3, 0, 0});
  ASSERT_EQ(collector.result.tasks.size(), 1U);
  const auto & accesses = collector.result.tasks[0].accesses;
  ASSERT_EQ(accesses.size(), 3U);
  const std::vector<ResolvedAccess> expected{
    {"global::A", 0, 4, 0x101c, AddressBasis::Absolute, AccessOperation::Load,
     0},
    {"global::A", 4, 4, 0x1020, AddressBasis::Absolute, AccessOperation::Load,
     1},
    {"global::A", 8, 4, 0x1024, AddressBasis::Absolute, AccessOperation::Load,
     2},
  };
  for (std::size_t index = 0; index < expected.size(); ++index)
    expect_access(accesses[index], expected[index]);
}

TEST(LoopWorkLimitsTest, ZeroLoopLimitsAllowFlatAccessesAndUnvisitedInvalidBody)
{
  const auto raw = module(Json::array({function(
    "kernel",
    Json::array(
      {loop(0, Json::array({loop(1, Json::array(), "j", 0, 0), access("global::"
                                                                      "missin"
                                                                      "g")})),
       access()}))}));
  TraceEmissionBudget budget({1, 0});
  Collector collector;
  collector.complete(stream_resolved_task_accesses(
    raw, addresses(), collector.sink(), budget, {0, 0}));

  expect_coverage(collector.result.coverage, {1, 1, 0, 0});
  EXPECT_EQ(collector.notifications,
            (std::vector<std::string>{"begin:kernel", "access:kernel:0",
                                      "end:kernel"}));
}

TEST(LoopWorkLimitsTest, EmptyTasksFitZeroLoopAndEmissionAllowances)
{
  const auto raw = module(Json::array({function("empty", Json::array())}));
  TraceEmissionBudget budget({0, 0});
  Collector collector;
  collector.complete(stream_resolved_task_accesses(
    raw, addresses(), collector.sink(), budget, {0, 0}));
  expect_coverage(collector.result.coverage, {});
  EXPECT_EQ(collector.notifications,
            (std::vector<std::string>{"begin:empty", "end:empty"}));
}

TEST(LoopWorkLimitsTest, CountsNestedEmptyLoopsAtEveryDynamicEntry)
{
  const auto raw = loop_module(2, Json::array({loop(3, Json::array(), "j")}));
  TraceEmissionBudget budget({0, 0});
  Collector collector;
  collector.complete(stream_resolved_task_accesses(
    raw, addresses(), collector.sink(), budget, {3, 8}));
  expect_coverage(collector.result.coverage, {});
  EXPECT_EQ(collector.notifications,
            (std::vector<std::string>{"begin:kernel", "end:kernel"}));
}

TEST(LoopWorkLimitsTest, ZeroTripInnerLoopsConsumeOnlyOuterReservations)
{
  const auto raw = loop_module(2, Json::array({loop(0, Json::array(), "j")}));
  TraceEmissionBudget budget({0, 0});
  Collector collector;
  collector.complete(stream_resolved_task_accesses(
    raw, addresses(), collector.sink(), budget, {2, 2}));
  expect_coverage(collector.result.coverage, {});
  EXPECT_EQ(collector.notifications,
            (std::vector<std::string>{"begin:kernel", "end:kernel"}));
}

TEST(LoopWorkLimitsTest,
     SharesExactCumulativeLimitAndRestartsOrdinalsAcrossTasks)
{
  const auto raw = module(Json::array({
    function("first", Json::array({loop(2, Json::array({access()}))})),
    function("second", Json::array({loop(2, Json::array({access()}))})),
  }));
  TraceEmissionBudget budget({4, 0});
  Collector collector;
  collector.complete(stream_resolved_task_accesses(
    raw, addresses(), collector.sink(), budget, {2, 4}));
  expect_coverage(collector.result.coverage, {4, 4, 0, 0});
  EXPECT_EQ(
    collector.notifications,
    (std::vector<std::string>{"begin:first", "access:first:0", "access:first:1",
                              "end:first", "begin:second", "access:second:0",
                              "access:second:1", "end:second"}));
}

TEST(LoopWorkLimitsTest, UsesTripCountForPositiveAndNegativeNonUnitSteps)
{
  for (const auto & bounds : {std::vector<std::int64_t>{1, 6, 2},
                              std::vector<std::int64_t>{5, 0, -2}})
  {
    SCOPED_TRACE(bounds[2]);
    const auto raw = loop_module(
      bounds[1], Json::array({access("global::A", "i")}), bounds[0], bounds[2]);
    TraceEmissionBudget budget({3, 0});
    Collector collector;
    collector.complete(stream_resolved_task_accesses(
      raw, addresses(), collector.sink(), budget, {3, 3}));
    ASSERT_EQ(collector.result.tasks[0].accesses.size(), 3U);
    const std::vector<std::uint64_t> expected =
      bounds[2] > 0 ? std::vector<std::uint64_t>{4, 12, 20}
                    : std::vector<std::uint64_t>{20, 12, 4};
    for (std::size_t index = 0; index < expected.size(); ++index)
      EXPECT_EQ(collector.result.tasks[0].accesses[index].object_byte_offset,
                expected[index]);
  }
}

TEST(LoopWorkLimitsTest, RaisingBothLoopLimitsRemovesFormerSingleLoopCeiling)
{
  const auto raw = loop_module(1'000'001, Json::array());
  TraceEmissionBudget budget({0, 0});
  Collector collector;
  collector.complete(stream_resolved_task_accesses(
    raw, addresses(), collector.sink(), budget, {1'000'001, 1'000'001}));
  expect_coverage(collector.result.coverage, {});
  EXPECT_EQ(collector.notifications,
            (std::vector<std::string>{"begin:kernel", "end:kernel"}));
}

TEST(LoopWorkLimitsTest, RaisingCumulativeLimitCompletesHundredCubedEmptyLoops)
{
  const auto raw = loop_module(
    100, Json::array(
           {loop(100, Json::array({loop(100, Json::array(), "k")}), "j")}));
  TraceEmissionBudget budget({0, 0});
  Collector collector;
  collector.complete(stream_resolved_task_accesses(
    raw, addresses(), collector.sink(), budget, {100, 1'010'100}));
  expect_coverage(collector.result.coverage, {});
  EXPECT_EQ(collector.notifications,
            (std::vector<std::string>{"begin:kernel", "end:kernel"}));
}

}  // namespace
