#include <gtest/gtest.h>
#include <limits>
#include <map>
#include <vector>

#include "hierarchy_analysis_test_support.hpp"

namespace
{

using yarda::analyze_batch_hierarchy;
using yarda::LruAccessOutcome;
using yarda::test::support::append_batch_access;
using yarda::test::support::batch_input;
using yarda::test::support::make_batch_hierarchy;
using yarda::test::support::make_batch_task;

TEST(BatchHierarchyL1Test, PreservesEmptyTaskAndZeroSummaries)
{
  const auto result = analyze_batch_hierarchy(
    batch_input({make_batch_task({}, "empty")}), make_batch_hierarchy());
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & task = result.tasks[0];
  EXPECT_EQ(task.task_id, "empty");
  EXPECT_EQ(task.source_accesses, 0U);
  EXPECT_EQ(task.modeled_accesses, 0U);
  EXPECT_TRUE(task.coverage.complete());
  EXPECT_TRUE(result.coverage.complete());
  EXPECT_EQ(result.coverage.emitted_line_references, 0U);
  for (const auto * level : {&task.l1, &task.llc})
  {
    EXPECT_TRUE(level->mappings.empty());
    EXPECT_TRUE(level->accesses.empty());
    EXPECT_EQ(level->summary.lookups, 0U);
    EXPECT_EQ(level->summary.hits, 0U);
    EXPECT_EQ(level->summary.misses, 0U);
    EXPECT_EQ(level->summary.cold_misses, 0U);
    EXPECT_EQ(level->summary.replacement_misses, 0U);
    EXPECT_EQ(level->summary.unique_lines, 0U);
    EXPECT_TRUE(level->summary.csrd_histogram.empty());
  }
}

TEST(BatchHierarchyL1Test, CountsOneSourceRangeAsOrderedCrossLineReferences)
{
  auto input = make_batch_task({}, "wide");
  append_batch_access(input, 0x101c, 40, yarda::AccessOperation::Store,
                      "global::wide", 12);
  const auto result =
    analyze_batch_hierarchy(batch_input({input}), make_batch_hierarchy());
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & task = result.tasks[0];
  EXPECT_EQ(task.source_accesses, 1U);
  EXPECT_EQ(task.modeled_accesses, 3U);
  EXPECT_EQ(task.l1.summary.lookups, 3U);
  EXPECT_EQ(task.coverage.source_accesses, 1U);
  EXPECT_EQ(task.coverage.resolved_accesses, 1U);
  EXPECT_EQ(task.coverage.emitted_line_references, 3U);
  EXPECT_EQ(result.coverage.emitted_line_references, 3U);
  ASSERT_EQ(task.l1.mappings.size(), 3U);
  const std::vector<std::uint64_t> addresses{0x101c, 0x1020, 0x1040};
  const std::vector<std::uint64_t> offsets{12, 16, 48};
  for (std::size_t i = 0; i < 3; ++i)
  {
    const auto & row = task.l1.mappings[i];
    EXPECT_EQ(row.decoded.address, addresses[i]);
    EXPECT_EQ(row.object_byte_offset, offsets[i]);
    EXPECT_EQ(row.object_id, "global::wide");
    EXPECT_EQ(row.address_basis, yarda::AddressBasis::Absolute);
    EXPECT_EQ(row.source_access_ordinal, 0U);
    EXPECT_EQ(row.line_span_ordinal, i);
    EXPECT_EQ(row.source_linked_byte_address, 0x101cU);
    EXPECT_EQ(row.source_object_byte_offset, 12U);
    EXPECT_EQ(row.source_access_size, 40U);
    EXPECT_EQ(row.operation, yarda::AccessOperation::Store);
  }
}

TEST(BatchHierarchyL1Test, PreservesExactDistancesAcrossAssociativityBoundary)
{
  const auto result =
    analyze_batch_hierarchy(batch_input({make_batch_task({0, 32, 0, 64, 32})}),
                            make_batch_hierarchy({32, 2, 2}, {32, 8, 4}));
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & l1 = result.tasks[0].l1;
  ASSERT_EQ(l1.accesses.size(), 5U);
  EXPECT_FALSE(l1.accesses[0].reuse_distance);
  EXPECT_EQ(l1.accesses[2].reuse_distance, 1U);
  EXPECT_EQ(l1.accesses[2].outcome, LruAccessOutcome::Hit);
  EXPECT_EQ(l1.accesses[4].reuse_distance, 2U);
  EXPECT_EQ(l1.accesses[4].outcome, LruAccessOutcome::ReplacementMiss);
  EXPECT_EQ(l1.summary.hits, 1U);
  EXPECT_EQ(l1.summary.cold_misses, 3U);
  EXPECT_EQ(l1.summary.replacement_misses, 1U);
  EXPECT_EQ(l1.summary.misses, 4U);
  EXPECT_EQ(l1.summary.unique_lines, 3U);
  EXPECT_EQ(l1.summary.csrd_histogram,
            (std::map<std::uint64_t, std::uint64_t>{{1, 1}, {2, 1}}));
}

TEST(BatchHierarchyL1Test, RetainsDistanceFarBeyondResidentCapacity)
{
  auto task = make_batch_task({});
  for (std::uint64_t block = 0; block < 18; ++block)
    append_batch_access(task, block * 32);
  append_batch_access(task, 0);
  const auto result = analyze_batch_hierarchy(
    batch_input({task}), make_batch_hierarchy({32, 2, 2}, {32, 4, 4}));
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & l1 = result.tasks[0].l1;
  ASSERT_EQ(l1.accesses.size(), 19U);
  EXPECT_EQ(l1.accesses.back().reuse_distance, 17U);
  EXPECT_EQ(l1.accesses.back().outcome, LruAccessOutcome::ReplacementMiss);
  EXPECT_EQ(l1.summary.csrd_histogram,
            (std::map<std::uint64_t, std::uint64_t>{{17, 1}}));
}

TEST(BatchHierarchyL1Test, IgnoresOtherSetsAndDeduplicatesRepeatedCompetitors)
{
  const auto result =
    analyze_batch_hierarchy(batch_input({make_batch_task({0, 32, 64, 64, 0})}),
                            make_batch_hierarchy({32, 4, 2}, {32, 8, 2}));
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & accesses = result.tasks[0].l1.accesses;
  ASSERT_EQ(accesses.size(), 5U);
  EXPECT_EQ(accesses[3].reuse_distance, 0U);
  EXPECT_EQ(accesses[4].reuse_distance, 1U);
  EXPECT_EQ(accesses[4].outcome, LruAccessOutcome::Hit);
}

TEST(BatchHierarchyL1Test, PreservesTaskOrderAndResetsBothLevelsAndHistory)
{
  const auto input =
    batch_input({make_batch_task({0, 0}, "z-first"),
                 make_batch_task({}, "empty"), make_batch_task({0}, "a-last")});
  const auto result = analyze_batch_hierarchy(input, make_batch_hierarchy());
  ASSERT_EQ(result.tasks.size(), 3U);
  EXPECT_EQ(result.tasks[0].task_id, "z-first");
  EXPECT_EQ(result.tasks[1].task_id, "empty");
  EXPECT_EQ(result.tasks[2].task_id, "a-last");
  EXPECT_EQ(result.tasks[0].l1.summary.hits, 1U);
  const auto & last = result.tasks[2];
  ASSERT_EQ(last.l1.accesses.size(), 1U);
  ASSERT_EQ(last.llc.accesses.size(), 1U);
  EXPECT_EQ(last.l1.accesses[0].outcome, LruAccessOutcome::ColdMiss);
  EXPECT_EQ(last.llc.accesses[0].outcome, LruAccessOutcome::ColdMiss);
  EXPECT_FALSE(last.l1.accesses[0].reuse_distance);
  EXPECT_FALSE(last.llc.accesses[0].reuse_distance);
  EXPECT_EQ(last.l1.mappings[0].source_access_ordinal, 0U);
  EXPECT_EQ(result.coverage.source_accesses, 3U);
  EXPECT_EQ(result.coverage.resolved_accesses, 3U);
  EXPECT_EQ(result.coverage.emitted_line_references, 3U);
  EXPECT_TRUE(result.coverage.complete());
}

TEST(BatchHierarchyL1Test, UsesLinkedBlockIdentityAcrossObjectsAndOffsets)
{
  auto task = make_batch_task({});
  append_batch_access(task, 0x1008, 1, yarda::AccessOperation::Load,
                      "global::a");
  append_batch_access(task, 0x1018, 1, yarda::AccessOperation::Store,
                      "global::b");
  const auto result =
    analyze_batch_hierarchy(batch_input({task}), make_batch_hierarchy());
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & l1 = result.tasks[0].l1;
  ASSERT_EQ(l1.accesses.size(), 2U);
  EXPECT_EQ(l1.accesses[1].reuse_distance, 0U);
  EXPECT_EQ(l1.accesses[1].outcome, LruAccessOutcome::Hit);
  EXPECT_EQ(l1.summary.unique_lines, 1U);
}

TEST(BatchHierarchyL1Test, AcceptsLastRepresentableByteWithoutRangeOverflow)
{
  const auto result = analyze_batch_hierarchy(
    batch_input({make_batch_task({std::numeric_limits<std::uint64_t>::max()})}),
    make_batch_hierarchy());
  ASSERT_EQ(result.tasks.size(), 1U);
  ASSERT_EQ(result.tasks[0].l1.mappings.size(), 1U);
  EXPECT_EQ(result.tasks[0].l1.mappings[0].decoded.line_offset, 31U);
}

}  // namespace
