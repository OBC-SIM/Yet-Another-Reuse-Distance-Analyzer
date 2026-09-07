#include <gtest/gtest.h>

#include "hierarchy_analysis_oracle_support.hpp"
#include "hierarchy_lru_oracle_differential_support.hpp"

namespace
{

using namespace yarda::test::support;

TEST(BatchHierarchyDifferentialTest, MatchesIndependentOraclesOnSeededTraces)
{
  for (const auto & spec : seeded_oracle_trace_specs())
  {
    const auto trace = make_seeded_oracle_trace(spec);
    SCOPED_TRACE(describe_seeded_oracle_trace(spec, trace));
    auto task = make_batch_task({}, trace.task_id);
    for (const auto & row : trace.accesses)
    {
      append_batch_access(task, row.decoded.address, 1, row.operation,
                          row.object_id, row.decoded.address);
    }
    ASSERT_EQ(task.accesses.size(), spec.reference_count);
    auto repeated = task;
    repeated.task_id += "-repeated";
    expect_batch_matches_oracles(
      batch_input({task, make_batch_task({}, "empty"), repeated}),
      make_batch_hierarchy(spec.l1_geometry, spec.llc_geometry));
  }
}

TEST(BatchHierarchyDifferentialTest, MatchesOraclesForMixedCrossLineAccesses)
{
  for (const std::uint64_t line_size : {32U, 64U})
  {
    SCOPED_TRACE("line_size=" + std::to_string(line_size));
    auto task = make_batch_task({0});
    append_batch_access(task, line_size - 4, line_size + 8,
                        yarda::AccessOperation::Store, "global::wide", 12);
    append_batch_access(task, 4 * line_size + 1, 2 * line_size,
                        yarda::AccessOperation::Load, "global::other", 5);
    append_batch_access(task, line_size - 4, line_size + 8,
                        yarda::AccessOperation::Load, "global::wide", 12);
    expect_batch_matches_oracles(
      batch_input({task}),
      make_batch_hierarchy({line_size, 4, 2}, {line_size, 8, 2}));
  }
}

TEST(BatchHierarchyDifferentialTest, MatchesOraclesForLoadAndStoreRecency)
{
  auto loads = make_batch_task({0, 32, 0, 64, 0, 32}, "loads");
  auto stores = loads;
  stores.task_id = "stores";
  for (auto & access : stores.accesses)
    access.operation = yarda::AccessOperation::Store;
  expect_batch_matches_oracles(batch_input({loads, stores}),
                               make_batch_hierarchy({32, 2, 2}, {32, 2, 2}));
}

}  // namespace
