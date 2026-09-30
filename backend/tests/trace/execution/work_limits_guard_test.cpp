#include "work_limits_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::work;

TEST(LoopWorkLimitsGuardTest, ConsumerStopsAtExactCumulativeReservation)
{
  const auto raw =
    loop_module(2, Json::array({loop(1, Json::array({access()}), "j")}));
  TraceEmissionBudget budget({1, 0});
  auto sink = discard_sink();
  struct Stop
  {
  };
  std::uint64_t delivered = 0;
  sink.access = [&](const std::string &, const ResolvedAccess & value) {
    ++delivered;
    EXPECT_EQ(value.source_access_ordinal, 0U);
    throw Stop{};
  };
  EXPECT_THROW(
    stream_resolved_task_accesses(raw, addresses(), sink, budget, {2, 3}),
    Stop);
  EXPECT_EQ(delivered, 1U);
}

TEST(LoopWorkLimitsGuardTest, RaisedLoopLimitsPreserveStructuralNodeGuard)
{
  Json functions =
    Json::array({function("helper_0", Json::array(), "ape.inline")});
  for (unsigned depth = 1; depth <= 16; ++depth)
  {
    const auto callee = "helper_" + std::to_string(depth - 1);
    functions.push_back(function("helper_" + std::to_string(depth),
                                 Json::array({call(callee), call(callee)}),
                                 "ape.inline"));
  }
  functions.push_back(function("kernel", Json::array({call("helper_16")})));
  TraceEmissionBudget budget;
  Collector collector;
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(module(functions), addresses(),
                                    collector.sink(), budget,
                                    {2'000'000, 2'000'000});
    },
    "inline call expansion exceeds 100000 nodes");
  EXPECT_TRUE(collector.notifications.empty());
}

TEST(LoopWorkLimitsGuardTest, RaisedLoopLimitsPreserveInlineDepthGuard)
{
  Json functions =
    Json::array({function("helper_0", Json::array(), "ape.inline")});
  for (unsigned depth = 1; depth < 257; ++depth)
    functions.push_back(
      function("helper_" + std::to_string(depth),
               Json::array({call("helper_" + std::to_string(depth - 1))}),
               "ape.inline"));
  functions.push_back(function("kernel", Json::array({call("helper_256")})));
  TraceEmissionBudget budget;
  Collector collector;
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(module(functions), addresses(),
                                    collector.sink(), budget,
                                    {2'000'000, 2'000'000});
    },
    "inline call depth exceeds 256");
  EXPECT_TRUE(collector.notifications.empty());
}

TEST(LoopWorkLimitsGuardTest, RaisedLoopLimitsPreserveRecursiveCallRejection)
{
  const auto raw = module(Json::array({
    function("kernel", Json::array({call("helper")})),
    function("helper", Json::array({call("helper")}), "ape.inline"),
  }));
  TraceEmissionBudget budget;
  Collector collector;
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), collector.sink(), budget,
                                    {2'000'000, 2'000'000});
    },
    "Recursive call expansion is not supported: helper");
  EXPECT_TRUE(collector.notifications.empty());
}

}  // namespace
