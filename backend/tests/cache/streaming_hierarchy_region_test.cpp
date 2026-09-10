#include "../regions/region_fixture.hpp"
#include "yarda/trace/resolution_error.hpp"

namespace yarda::test::region
{
namespace
{

TEST(RegionHierarchy, MatchesBatchAndBothOraclesOnRealCompilerAndElfInputs)
{
  for (const auto * name : {"boundary", "global_values", "atax", "tasks"})
  {
    SCOPED_TRACE(name);
    const Fixture fixture(name);
    streaming::expect_stream_parity(fixture.raw, fixture.objects,
                                    streaming::make_batch_hierarchy());
  }
}

TEST(RegionHierarchy, OutsideReferencesDoNotWarmIndependentTasks)
{
  const Fixture fixture("tasks");
  const auto result = analyze_streaming_hierarchy(
    fixture.raw, fixture.objects, streaming::make_batch_hierarchy());
  ASSERT_EQ(result.tasks.size(), 4U);
  EXPECT_EQ(result.tasks[0].task_id, "whole");
  EXPECT_EQ(result.tasks[1].task_id, "region:5:first:APE_ANALYZE");
  EXPECT_EQ(result.tasks[2].task_id, "region:6:second:APE_ANALYZE");
  EXPECT_EQ(result.tasks[3].task_id, "region:5:empty:APE_ANALYZE");
  for (std::size_t i = 0; i < 3; ++i)
  {
    EXPECT_EQ(result.tasks[i].source_accesses, 2U);
    EXPECT_EQ(result.tasks[i].l1.cold_misses, 1U);
    EXPECT_EQ(result.tasks[i].llc.cold_misses, 1U);
    EXPECT_EQ(result.tasks[i].ehc_l1, 1U);
    EXPECT_EQ(result.tasks[i].all_cache_misses, 1U);
  }
  const auto & empty = result.tasks[3];
  EXPECT_EQ(empty.source_accesses, 0U);
  EXPECT_TRUE(empty.l1.csrd_histogram.empty());
  EXPECT_TRUE(empty.llc.csrd_histogram.empty());
  EXPECT_FALSE(empty.hr_l1.has_value());
  EXPECT_FALSE(empty.hr_llc.has_value());
  EXPECT_FALSE(empty.miss_ratio.has_value());
}

TEST(RegionHierarchy, LaterFailureAbortsAndCollectedEarlierEventsAreDiscarded)
{
  Fixture fixture("tasks");
  auto & functions = fixture.raw["functions"];
  functions.back()["body"] =
    nlohmann::json::array({streaming::access("global::missing")});
  streaming::EventCollector collector;
  bool completed = false;
  try
  {
    analyze_streaming_hierarchy(fixture.raw, fixture.objects,
                                streaming::make_batch_hierarchy(),
                                collector.options());
    completed = true;
  }
  catch (const ResolutionError & error)
  {
    EXPECT_FALSE(collector.events.empty());
    EXPECT_NE(std::string(error.what()).find("region:5:empty:APE_ANALYZE"),
              std::string::npos);
    collector.events.clear();
  }
  EXPECT_FALSE(completed);
  EXPECT_TRUE(collector.events.empty());
}

}  // namespace
}  // namespace yarda::test::region
