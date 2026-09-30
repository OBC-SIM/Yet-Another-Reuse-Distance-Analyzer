#include "prepared_trace_test_support.hpp"
#include "work_limits_test_support.hpp"
#include "yarda/trace/trace.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::prepared;

TEST(PreparedTraceBoundTest, SyrkInclusiveEndReevaluatesForEveryOuterIteration)
{
  auto read = access();
  read["indices"] = Json::array({"i", "j"});
  auto inner = loop(0, Json::array({read}), "j");
  inner["bound"] = "i+1";
  const auto node = loop(3, Json::array({inner}));
  EXPECT_EQ(indices(node, {3, 9}),
            (IndexRows{{"0", "0"}, {"1", "0"}, {"1", "1"},
                       {"2", "0"}, {"2", "1"}, {"2", "2"}}));
  test::work::expect_limit_error(
    [&] { indices(node, {3, 8}); },
    "cumulative loop iteration count exceeds 8");
}

TEST(PreparedTraceBoundTest, NussinovResolvesBothEndsFromDifferentOuterLoops)
{
  auto read = access();
  read["indices"] = Json::array({"i", "j", "k"});
  auto inner = loop(0, Json::array({read}), "k");
  inner["start"] = "i+1";
  inner["bound"] = "j";
  auto middle = loop(4, Json::array({inner}), "j");
  middle["start"] = "i+1";
  EXPECT_EQ(indices(loop(-1, Json::array({middle}), "i", 3, -1)),
            (IndexRows{{"1", "3", "2"}, {"0", "2", "1"},
                       {"0", "3", "1"}, {"0", "3", "2"}}));
}

TEST(PreparedTraceBoundTest, SupportsDescendingAndNonUnitSteps)
{
  auto inner = loop(0, Json::array({access("global::A", "j")}), "j", 5, -2);
  inner["bound"] = "i";
  EXPECT_EQ(indices(loop(3, Json::array({inner}))),
            (IndexRows{{"5"}, {"3"}, {"1"}, {"5"}, {"3"}, {"5"}, {"3"}}));
}

TEST(PreparedTraceBoundTest, BindsEndBeforeShadowingOuterVariable)
{
  const auto read = access("global::A", "i");
  auto inner = loop(0, Json::array({read}));
  inner["bound"] = "i";
  EXPECT_EQ(indices(loop(3, Json::array({inner, read}), "i", 1)),
            (IndexRows{{"0"}, {"1"}, {"0"}, {"1"}, {"2"}}));
}

TEST(PreparedTraceBoundTest, RejectsUnboundNonAffineAndOverflowingEnds)
{
  auto inner = loop(0, Json::array(), "j");
  for (const auto * bound : {"j", "missing", "i*i", "i+9223372036854775807"})
  {
    inner["bound"] = bound;
    EXPECT_THROW(indices(loop(2, Json::array({inner}))), std::invalid_argument);
  }
}

TEST(PreparedTraceBoundTest, DoesNotPrepareBodyOfDynamicZeroTripLoop)
{
  auto inner = loop(0, Json::array({{{"type", "Unknown"}}}), "j", 3);
  inner["bound"] = "i";
  EXPECT_TRUE(indices(loop(3, Json::array({inner}))).empty());
}

TEST(PreparedTraceBoundTest, StreamsResolvedAddressesWithDynamicEnd)
{
  auto inner = loop(0, Json::array({access("global::A", "j")}), "j");
  inner["bound"] = "i+1";
  const auto raw = module(Json::array({function(
    "kernel", Json::array({loop(3, Json::array({inner}))}))}));
  Collector collector;
  collector.complete(
    stream_resolved_task_accesses(raw, addresses(), collector.sink()));
  ASSERT_EQ(collector.result.tasks.size(), 1U);
  expect_offsets(collector.result.tasks[0], {0, 0, 4, 0, 4, 8});
  expect_coverage(collector.result.coverage, {6, 6, 0, 0});
}

TEST(PreparedTraceBoundTest, LabelsStringBoundWithoutNumericConversion)
{
  auto node = loop(0, Json::array({access("global::A", "i")}));
  node["bound"] = "2";
  const auto blocks =
    block_traces(Json::array({function("kernel", Json::array({node}))}));
  ASSERT_EQ(blocks.size(), 1U);
  EXPECT_EQ(blocks[0].name, "kernel  i-loop (bound=2)");
  EXPECT_EQ(blocks[0].accesses.size(), 2U);
}

}  // namespace
