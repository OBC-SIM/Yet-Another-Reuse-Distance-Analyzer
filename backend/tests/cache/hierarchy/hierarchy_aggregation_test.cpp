#include <array>
#include <gtest/gtest.h>

#include "hierarchy_aggregation_test_support.hpp"

namespace
{

using yarda::analyze_batch_hierarchy;
using yarda::FirstServiceLevel;
using yarda::LruAccessOutcome;
using yarda::detail::aggregate_hierarchy_task;
using namespace yarda::test::support;

TEST(BatchHierarchyAggregationTest, ClassifiesFirstServiceInL1Order)
{
  const auto fixture = make_aggregation_fixture();
  const auto result = aggregate_hierarchy_task(
    fixture.task_id, fixture.coverage, fixture.l1, fixture.llc);
  const std::array<FirstServiceLevel, 5> expected{
    FirstServiceLevel::Memory, FirstServiceLevel::Memory, FirstServiceLevel::L1,
    FirstServiceLevel::Memory, FirstServiceLevel::LLC};
  ASSERT_EQ(result.events.size(), expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i)
    EXPECT_EQ(result.events[i].first_service, expected[i]);
}

TEST(BatchHierarchyAggregationTest, ConsumesLlcOnlyForL1Misses)
{
  const auto fixture = make_aggregation_fixture();
  const auto result = aggregate_hierarchy_task(
    fixture.task_id, fixture.coverage, fixture.l1, fixture.llc);
  ASSERT_EQ(result.events.size(), 5U);
  for (std::size_t i = 0; i < result.events.size(); ++i)
  {
    EXPECT_EQ(result.events[i].llc.has_value(), i != 2);
    EXPECT_EQ(result.events[i].llc_mapping.has_value(), i != 2);
  }
}

TEST(BatchHierarchyAggregationTest, PreservesMappingsAndExactObservations)
{
  const auto fixture = make_aggregation_fixture();
  const auto result = aggregate_hierarchy_task(
    fixture.task_id, fixture.coverage, fixture.l1, fixture.llc);
  ASSERT_EQ(result.events.size(), 5U);
  for (std::size_t i = 0; i < result.events.size(); ++i)
  {
    expect_batch_provenance(result.events[i].l1_mapping,
                            fixture.l1.mappings[i]);
    EXPECT_EQ(result.events[i].l1.outcome, fixture.l1.accesses[i].outcome);
    EXPECT_EQ(result.events[i].l1.reuse_distance,
              fixture.l1.accesses[i].reuse_distance);
  }
  const std::array<std::size_t, 4> paired{0, 1, 3, 4};
  for (std::size_t i = 0; i < paired.size(); ++i)
  {
    const auto & event = result.events[paired[i]];
    ASSERT_TRUE(event.llc_mapping);
    ASSERT_TRUE(event.llc);
    expect_batch_provenance(*event.llc_mapping, fixture.llc.mappings[i]);
    EXPECT_EQ(event.llc_mapping->decoded.set_index,
              fixture.llc.mappings[i].decoded.set_index);
    EXPECT_EQ(event.llc_mapping->decoded.tag,
              fixture.llc.mappings[i].decoded.tag);
    EXPECT_EQ(event.llc->outcome, fixture.llc.accesses[i].outcome);
    EXPECT_EQ(event.llc->reuse_distance,
              fixture.llc.accesses[i].reuse_distance);
  }
}

TEST(BatchHierarchyAggregationTest, OwnsPayloadAfterInputsAreDestroyed)
{
  const auto result = [] {
    auto fixture = make_aggregation_fixture();
    auto task = aggregate_hierarchy_task(fixture.task_id, fixture.coverage,
                                         fixture.l1, fixture.llc);
    fixture.l1.mappings[0].object_id = "changed";
    fixture.llc.mappings[0].decoded.address = 99;
    fixture.task_id = "changed";
    fixture.l1.summary.csrd_histogram.clear();
    return task;
  }();
  ASSERT_EQ(result.events.size(), 5U);
  EXPECT_EQ(result.summary.task_id, "service");
  EXPECT_EQ(result.events[0].l1_mapping.object_id, "global::service");
  ASSERT_TRUE(result.events[0].llc_mapping);
  EXPECT_EQ(result.events[0].llc_mapping->decoded.address, 0U);
  EXPECT_EQ(result.summary.l1.csrd_histogram,
            (std::map<std::uint64_t, std::uint64_t>{{1, 1}, {2, 1}}));
}

TEST(BatchHierarchyServiceTest, UsesAllModeledReferencesForFirstServiceRatios)
{
  const auto result =
    analyze_batch_hierarchy(batch_input({make_batch_task({0, 32, 0, 64, 32})}),
                            make_batch_hierarchy({32, 2, 2}, {32, 8, 4}));
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & summary = result.tasks[0].summary;
  EXPECT_EQ(summary.modeled_accesses, 5U);
  EXPECT_EQ(summary.llc.lookups, 4U);
  EXPECT_EQ(summary.ehc_l1, 1U);
  EXPECT_EQ(summary.ehc_llc, 1U);
  EXPECT_EQ(summary.all_cache_misses, 3U);
  ASSERT_TRUE(summary.hr_l1);
  ASSERT_TRUE(summary.hr_llc);
  ASSERT_TRUE(summary.miss_ratio);
  EXPECT_DOUBLE_EQ(*summary.hr_l1, 0.2);
  EXPECT_DOUBLE_EQ(*summary.hr_llc, 0.2);
  EXPECT_DOUBLE_EQ(*summary.miss_ratio, 0.6);
  EXPECT_NEAR(*summary.hr_l1 + *summary.hr_llc + *summary.miss_ratio, 1.0,
              1e-15);
  expect_hierarchy_invariants(summary.invariants);
}

TEST(BatchHierarchyServiceTest, RetainsEmptyTaskWithAbsentRatios)
{
  const auto result = analyze_batch_hierarchy(
    batch_input({make_batch_task({}, "empty")}), make_batch_hierarchy());
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & task = result.tasks[0];
  EXPECT_EQ(task.summary.task_id, "empty");
  EXPECT_TRUE(task.events.empty());
  EXPECT_EQ(task.summary.ehc_l1, 0U);
  EXPECT_EQ(task.summary.ehc_llc, 0U);
  EXPECT_EQ(task.summary.all_cache_misses, 0U);
  EXPECT_FALSE(task.summary.hr_l1);
  EXPECT_FALSE(task.summary.hr_llc);
  EXPECT_FALSE(task.summary.miss_ratio);
  expect_hierarchy_invariants(task.summary.invariants);
}

TEST(BatchHierarchyServiceTest, ClassifiesEachCrossLineSpanIndependently)
{
  auto task = make_batch_task({0x1000});
  append_batch_access(task, 0x101c, 8, yarda::AccessOperation::Store);
  const auto result = analyze_batch_hierarchy(
    batch_input({task}), make_batch_hierarchy({32, 4, 2}, {32, 8, 2}));
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & actual = result.tasks[0];
  ASSERT_EQ(actual.events.size(), 3U);
  EXPECT_EQ(actual.summary.source_accesses, 2U);
  EXPECT_EQ(actual.summary.modeled_accesses, 3U);
  EXPECT_EQ(actual.summary.coverage.source_accesses, 2U);
  EXPECT_EQ(result.coverage.emitted_line_references, 3U);
  EXPECT_EQ(actual.events[1].first_service, FirstServiceLevel::L1);
  EXPECT_FALSE(actual.events[1].llc);
  EXPECT_EQ(actual.events[2].first_service, FirstServiceLevel::Memory);
  ASSERT_TRUE(actual.events[2].llc_mapping);
  EXPECT_EQ(actual.events[2].llc_mapping->source_access_ordinal, 1U);
  EXPECT_EQ(actual.events[2].llc_mapping->line_span_ordinal, 1U);
  EXPECT_EQ(actual.events[2].llc_mapping->operation,
            yarda::AccessOperation::Store);
  EXPECT_EQ(actual.summary.ehc_l1, 1U);
  EXPECT_EQ(actual.summary.all_cache_misses, 2U);
  ASSERT_TRUE(actual.summary.hr_l1);
  EXPECT_DOUBLE_EQ(*actual.summary.hr_l1, 1.0 / 3.0);
}

TEST(BatchHierarchyServiceTest, CountsLlcReplacementMissAsMemoryService)
{
  const auto result =
    analyze_batch_hierarchy(batch_input({make_batch_task({0, 32, 64, 0})}),
                            make_batch_hierarchy({32, 1, 1}, {32, 2, 2}));
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & task = result.tasks[0];
  ASSERT_EQ(task.events.size(), 4U);
  ASSERT_TRUE(task.events.back().llc);
  EXPECT_EQ(task.events.back().llc->outcome, LruAccessOutcome::ReplacementMiss);
  EXPECT_EQ(task.events.back().first_service, FirstServiceLevel::Memory);
  EXPECT_EQ(task.summary.all_cache_misses, 4U);
  EXPECT_EQ(task.summary.ehc_l1, 0U);
  EXPECT_EQ(task.summary.ehc_llc, 0U);
  ASSERT_TRUE(task.summary.miss_ratio);
  EXPECT_DOUBLE_EQ(*task.summary.miss_ratio, 1.0);
}

TEST(BatchHierarchyServiceTest, ResetsServiceCountsAndKeepsInputTaskOrder)
{
  const auto result = analyze_batch_hierarchy(
    batch_input({make_batch_task({0, 32, 0, 0}, "z-first"),
                 make_batch_task({}, "empty"),
                 make_batch_task({0, 32, 0, 0}, "a-last")}),
    make_batch_hierarchy({32, 1, 1}, {32, 2, 2}));
  ASSERT_EQ(result.tasks.size(), 3U);
  EXPECT_EQ(result.tasks[0].summary.task_id, "z-first");
  EXPECT_EQ(result.tasks[1].summary.task_id, "empty");
  EXPECT_EQ(result.tasks[2].summary.task_id, "a-last");
  EXPECT_EQ(result.tasks[1].summary.all_cache_misses, 0U);
  for (const auto index : {0U, 2U})
  {
    const auto & task = result.tasks[index];
    EXPECT_EQ(task.summary.ehc_l1, 1U);
    EXPECT_EQ(task.summary.ehc_llc, 1U);
    EXPECT_EQ(task.summary.all_cache_misses, 2U);
    ASSERT_EQ(task.events.size(), 4U);
    EXPECT_EQ(task.events[0].first_service, FirstServiceLevel::Memory);
    EXPECT_EQ(task.events[0].l1_mapping.source_access_ordinal, 0U);
  }
  EXPECT_EQ(result.coverage.source_accesses, 8U);
  EXPECT_EQ(result.coverage.emitted_line_references, 8U);
}

}  // namespace
