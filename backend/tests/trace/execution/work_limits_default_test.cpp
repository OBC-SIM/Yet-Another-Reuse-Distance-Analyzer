#include "work_limits_test_support.hpp"
#include "yarda/trace/mapped_trace.hpp"
#include "yarda/trace/trace.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::work;

TEST(LoopWorkLimitsDefaultTest, ProducerRetainsOriginalSingleLoopCeiling)
{
  const auto raw = loop_module(1'000'001, Json::array());
  expect_limit_error(
    [&] { stream_resolved_task_accesses(raw, addresses(), discard_sink()); },
    "loop iteration count exceeds 1000000");
}

TEST(LoopWorkLimitsDefaultTest, EmissionBudgetKeepsDefaultSingleLoopCeiling)
{
  const auto raw = loop_module(1'000'001, Json::array());
  TraceEmissionBudget budget({0, 0});
  expect_limit_error(
    [&] {
      stream_resolved_task_accesses(raw, addresses(), discard_sink(), budget);
    },
    "loop iteration count exceeds 1000000");
}

TEST(LoopWorkLimitsDefaultTest, ResolvedTasksRetainOriginalSingleLoopCeiling)
{
  const auto raw = loop_module(1'000'001, Json::array());
  expect_limit_error([&] { resolved_task_traces(raw, addresses()); },
                     "loop iteration count exceeds 1000000");
}

TEST(LoopWorkLimitsDefaultTest, LegacyBlocksRetainOriginalSingleLoopCeiling)
{
  const auto raw = loop_module(1'000'001, Json::array());
  expect_limit_error([&] { block_traces(raw); },
                     "loop iteration count exceeds 1000000");
}

TEST(LoopWorkLimitsDefaultTest, ResolvedBlocksRetainOriginalSingleLoopCeiling)
{
  const auto raw = loop_module(1'000'001, Json::array());
  expect_limit_error([&] { resolved_block_traces(raw, addresses()); },
                     "loop iteration count exceeds 1000000");
}

TEST(LoopWorkLimitsDefaultTest, MappedBlocksRetainOriginalSingleLoopCeiling)
{
  const auto raw = loop_module(1'000'001, Json::array());
  expect_limit_error(
    [&] {
      mapped_block_traces(raw, CacheGeometry{64, 8, 2}, addresses());
    },
    "loop iteration count exceeds 1000000");
}

}  // namespace
