#include <gtest/gtest.h>
#include <map>

#include "hierarchy_analysis_test_support.hpp"

namespace
{

using yarda::analyze_batch_hierarchy;
using yarda::LruAccessOutcome;
using yarda::test::support::append_batch_access;
using yarda::test::support::batch_input;
using yarda::test::support::expect_batch_provenance;
using yarda::test::support::make_batch_hierarchy;
using yarda::test::support::make_batch_task;

TEST(BatchHierarchyLlcTest, FiltersL1HitsAndPreservesMissOrder)
{
  const auto result =
    analyze_batch_hierarchy(batch_input({make_batch_task({0, 0, 32, 32, 0})}),
                            make_batch_hierarchy({32, 1, 1}, {32, 2, 2}));
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & task = result.tasks[0];
  ASSERT_EQ(task.llc.mappings.size(), 3U);
  ASSERT_EQ(task.llc.accesses.size(), 3U);
  ASSERT_EQ(task.l1.mappings.size(), 5U);
  EXPECT_EQ(task.llc.summary.lookups, task.l1.summary.misses);
  EXPECT_EQ(task.llc.mappings[0].source_access_ordinal, 0U);
  EXPECT_EQ(task.llc.mappings[1].source_access_ordinal, 2U);
  EXPECT_EQ(task.llc.mappings[2].source_access_ordinal, 4U);
  EXPECT_EQ(task.llc.accesses[0].outcome, LruAccessOutcome::ColdMiss);
  EXPECT_EQ(task.llc.accesses[2].outcome, LruAccessOutcome::Hit);
  EXPECT_EQ(task.llc.accesses[2].reuse_distance, 1U);
}

TEST(BatchHierarchyLlcTest, FillsL1AfterLlcService)
{
  const auto result =
    analyze_batch_hierarchy(batch_input({make_batch_task({0, 32, 0, 0})}),
                            make_batch_hierarchy({32, 1, 1}, {32, 2, 2}));
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & task = result.tasks[0];
  ASSERT_EQ(task.l1.accesses.size(), 4U);
  ASSERT_EQ(task.llc.accesses.size(), 3U);
  EXPECT_EQ(task.l1.accesses[2].outcome, LruAccessOutcome::ReplacementMiss);
  EXPECT_EQ(task.llc.accesses[2].outcome, LruAccessOutcome::Hit);
  EXPECT_EQ(task.l1.accesses[3].outcome, LruAccessOutcome::Hit);
}

TEST(BatchHierarchyLlcTest, RemapsOriginalAddressAtDifferentSetCount)
{
  const auto result =
    analyze_batch_hierarchy(batch_input({make_batch_task({0, 64, 0})}),
                            make_batch_hierarchy({32, 2, 1}, {32, 8, 2}));
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & task = result.tasks[0];
  ASSERT_EQ(task.l1.mappings.size(), 3U);
  ASSERT_EQ(task.llc.mappings.size(), 3U);
  EXPECT_EQ(task.l1.mappings[1].decoded.set_index, 0U);
  EXPECT_EQ(task.l1.mappings[1].decoded.tag, 1U);
  EXPECT_EQ(task.llc.mappings[1].decoded.set_index, 2U);
  EXPECT_EQ(task.llc.mappings[1].decoded.tag, 0U);
  EXPECT_EQ(task.llc.summary.hits, 1U);
  EXPECT_EQ(task.llc.summary.replacement_misses, 0U);
}

TEST(BatchHierarchyLlcTest,
     RemapsEachMissedSpanWithoutRepeatingWholeSourceRange)
{
  auto input = make_batch_task({0x1000}, "mixed-span");
  append_batch_access(input, 0x101c, 8, yarda::AccessOperation::Store,
                      "global::wide", 12);
  const auto result = analyze_batch_hierarchy(
    batch_input({input}), make_batch_hierarchy({32, 4, 2}, {32, 8, 2}));
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & task = result.tasks[0];
  ASSERT_EQ(task.l1.mappings.size(), 3U);
  ASSERT_EQ(task.l1.accesses.size(), 3U);
  ASSERT_EQ(task.llc.mappings.size(), 2U);
  EXPECT_EQ(task.l1.accesses[1].outcome, LruAccessOutcome::Hit);
  const auto & remapped = task.llc.mappings[1];
  expect_batch_provenance(remapped, task.l1.mappings[2]);
  EXPECT_EQ(remapped.decoded.address, 0x1020U);
  EXPECT_EQ(remapped.decoded.set_index, 1U);
  EXPECT_EQ(remapped.decoded.tag, 32U);
  EXPECT_EQ(remapped.line_span_ordinal, 1U);
  EXPECT_EQ(remapped.source_access_ordinal, 1U);
  EXPECT_EQ(remapped.object_byte_offset, 16U);
  EXPECT_EQ(remapped.source_object_byte_offset, 12U);
  EXPECT_EQ(remapped.source_linked_byte_address, 0x101cU);
  EXPECT_EQ(remapped.source_access_size, 8U);
  EXPECT_EQ(remapped.operation, yarda::AccessOperation::Store);
}

TEST(BatchHierarchyLlcTest, PreservesExactLlcReplacementDistance)
{
  const auto result = analyze_batch_hierarchy(
    batch_input({make_batch_task({0, 32, 64, 96, 128, 0})}),
    make_batch_hierarchy({32, 1, 1}, {32, 2, 2}));
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & llc = result.tasks[0].llc;
  ASSERT_EQ(llc.accesses.size(), 6U);
  EXPECT_EQ(llc.accesses.back().outcome, LruAccessOutcome::ReplacementMiss);
  EXPECT_EQ(llc.accesses.back().reuse_distance, 4U);
  EXPECT_EQ(llc.summary.cold_misses, 5U);
  EXPECT_EQ(llc.summary.replacement_misses, 1U);
  EXPECT_EQ(llc.summary.unique_lines, 5U);
  EXPECT_EQ(llc.summary.csrd_histogram,
            (std::map<std::uint64_t, std::uint64_t>{{4, 1}}));
}

TEST(BatchHierarchyLlcTest, DoesNotTouchLlcRecencyOnL1Hit)
{
  const auto result = analyze_batch_hierarchy(
    batch_input({make_batch_task({0, 32, 0, 64, 96, 0})}),
    make_batch_hierarchy({32, 2, 2}, {32, 2, 2}));
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & task = result.tasks[0];
  ASSERT_EQ(task.llc.accesses.size(), 5U);
  EXPECT_EQ(task.llc.accesses.back().reuse_distance, 3U);
  EXPECT_EQ(task.llc.accesses.back().outcome,
            LruAccessOutcome::ReplacementMiss);
}

TEST(BatchHierarchyLlcTest, KeepsL1LineAfterLlcEviction)
{
  const auto result =
    analyze_batch_hierarchy(batch_input({make_batch_task({0, 32, 96, 0})}),
                            make_batch_hierarchy({32, 4, 2}, {32, 2, 2}));
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & task = result.tasks[0];
  ASSERT_EQ(task.l1.accesses.size(), 4U);
  EXPECT_EQ(task.l1.accesses.back().outcome, LruAccessOutcome::Hit);
  EXPECT_EQ(task.llc.summary.lookups, 3U);
}

TEST(BatchHierarchyLlcTest, DoesNotInsertL1VictimsIntoLlc)
{
  const auto result = analyze_batch_hierarchy(
    batch_input({make_batch_task({0, 32, 64, 96, 128, 0})}),
    make_batch_hierarchy({32, 4, 4}, {32, 2, 2}));
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & llc = result.tasks[0].llc;
  ASSERT_EQ(llc.accesses.size(), 6U);
  EXPECT_EQ(llc.accesses.back().outcome, LruAccessOutcome::ReplacementMiss);
  EXPECT_EQ(llc.accesses.back().reuse_distance, 4U);
}

TEST(BatchHierarchyLlcTest, AllocatesBothLevelsForStoreMisses)
{
  auto input = make_batch_task({0, 32, 0, 0});
  input.accesses[0].operation = yarda::AccessOperation::Store;
  input.accesses[3].operation = yarda::AccessOperation::Store;
  const auto result = analyze_batch_hierarchy(
    batch_input({input}), make_batch_hierarchy({32, 1, 1}, {32, 2, 2}));
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & task = result.tasks[0];
  ASSERT_EQ(task.l1.accesses.size(), 4U);
  ASSERT_EQ(task.llc.accesses.size(), 3U);
  EXPECT_EQ(task.llc.accesses[2].outcome, LruAccessOutcome::Hit);
  EXPECT_EQ(task.l1.accesses[3].outcome, LruAccessOutcome::Hit);
  ASSERT_EQ(task.llc.mappings.size(), 3U);
  EXPECT_EQ(task.llc.mappings[0].operation, yarda::AccessOperation::Store);
}

}  // namespace
