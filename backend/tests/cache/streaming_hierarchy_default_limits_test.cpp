#include "../trace/work_limits_test_support.hpp"
#include "streaming_hierarchy_test_support.hpp"

namespace
{

using namespace yarda;
using namespace yarda::test::streaming;
using yarda::test::work::expect_limit_error;

TEST(StreamingHierarchyDefaultLimitsTest, AllowsExactSingleLoopAllowance)
{
  const auto raw = byte_module(Json::array(
    {function("kernel", Json::array({loop(1'000'000, Json::array())}))}));
  const StreamingHierarchyOptions options;
  const auto result = analyze_streaming_hierarchy(
    raw, byte_addresses(), make_batch_hierarchy(), options);
  ASSERT_EQ(result.tasks.size(), 1U);
  stream::expect_coverage(result.coverage, {});
  EXPECT_TRUE(result.tasks[0].invariants.all_passed);
}

TEST(StreamingHierarchyDefaultLimitsTest, RejectsSingleLoopAllowanceExceeded)
{
  const auto raw = byte_module(Json::array(
    {function("kernel", Json::array({loop(1'000'001, Json::array())}))}));
  const StreamingHierarchyOptions options;
  expect_limit_error(
    [&] {
      analyze_streaming_hierarchy(raw, byte_addresses(), make_batch_hierarchy(),
                                  options);
    },
    "loop iteration count exceeds 1000000");
}

TEST(StreamingHierarchyDefaultLimitsTest, AllowsExactCumulativeLoopAllowance)
{
  const auto raw = byte_module(Json::array({function(
    "kernel",
    Json::array({loop(1000, Json::array({loop(999, Json::array(), "j")}))}))}));
  const StreamingHierarchyOptions options;
  const auto result = analyze_streaming_hierarchy(
    raw, byte_addresses(), make_batch_hierarchy(), options);
  ASSERT_EQ(result.tasks.size(), 1U);
  stream::expect_coverage(result.coverage, {});
  EXPECT_TRUE(result.tasks[0].invariants.all_passed);
}

TEST(StreamingHierarchyDefaultLimitsTest,
     RejectsCumulativeLoopAllowanceExceeded)
{
  const auto raw = byte_module(Json::array({function(
    "kernel", Json::array({loop(
                1001, Json::array({loop(1000, Json::array(), "j")}))}))}));
  const StreamingHierarchyOptions options;
  expect_limit_error(
    [&] {
      analyze_streaming_hierarchy(raw, byte_addresses(), make_batch_hierarchy(),
                                  options);
    },
    "cumulative loop iteration count exceeds 1000000");
}

}  // namespace
