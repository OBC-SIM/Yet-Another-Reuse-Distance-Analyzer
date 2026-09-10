#include "../regions/region_fixture.hpp"

namespace yarda::test::region
{
namespace
{

TEST(RegionIntegration, EmitsExactlySixBoundarySourcesAtLinkedAddresses)
{
  const Fixture fixture("boundary");
  const auto result = resolved_task_traces(fixture.raw, fixture.objects);
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & task = result.tasks[0];
  EXPECT_EQ(task.task_id, "region:12:region_probe:APE_ANALYZE");
  ASSERT_EQ(task.accesses.size(), 6U);
  for (std::uint64_t i = 0; i < 3; ++i)
  {
    expect_access(task.accesses[2 * i], fixture, "global::inside", 4 * (i + 1),
                  4, AccessOperation::Load, 2 * i);
    expect_access(task.accesses[2 * i + 1], fixture, "global::inside",
                  4 * (i + 1), 4, AccessOperation::Store, 2 * i + 1);
  }
  EXPECT_EQ(result.coverage.source_accesses, 6U);
}

TEST(RegionIntegration, EmitsNineSourcesAndExcludesOutsideGlobalRead)
{
  const Fixture fixture("global_values");
  const auto result = resolved_task_traces(fixture.raw, fixture.objects);
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & task = result.tasks[0];
  EXPECT_EQ(task.task_id, "region:19:global_values_probe:APE_ANALYZE");
  ASSERT_EQ(task.accesses.size(), 9U);
  for (std::uint64_t i = 0; i < 3; ++i)
  {
    expect_access(task.accesses[3 * i], fixture, "global::external_value", 0, 4,
                  AccessOperation::Load, 3 * i);
    expect_access(task.accesses[3 * i + 1], fixture, "global::inside",
                  4 * (i + 1), 4, AccessOperation::Load, 3 * i + 1);
    expect_access(task.accesses[3 * i + 2], fixture, "global::inside",
                  4 * (i + 1), 4, AccessOperation::Store, 3 * i + 2);
  }
}

TEST(RegionIntegration, PreservesCompleteAtaxSiblingLoopsAndAll53Sources)
{
  const Fixture fixture("atax");
  const auto result = resolved_task_traces(fixture.raw, fixture.objects);
  ASSERT_EQ(result.tasks.size(), 1U);
  const auto & accesses = result.tasks[0].accesses;
  ASSERT_EQ(accesses.size(), 53U);
  std::uint64_t ordinal = 0;
  const auto expect = [&](const std::string & object, std::uint64_t offset,
                          AccessOperation operation) {
    ASSERT_LT(ordinal, accesses.size());
    expect_access(accesses[ordinal], fixture, object, offset, 8, operation,
                  ordinal);
    ++ordinal;
  };
  for (std::uint64_t j = 0; j < 3; ++j)
    expect("global::y", 8 * j, AccessOperation::Store);
  for (std::uint64_t i = 0; i < 2; ++i)
  {
    expect("global::tmp", 8 * i, AccessOperation::Store);
    for (std::uint64_t j = 0; j < 3; ++j)
    {
      // The fixed Clang pipeline evaluates compound-assignment RHS loads first.
      expect("global::A", 8 * (3 * i + j), AccessOperation::Load);
      expect("global::x", 8 * j, AccessOperation::Load);
      expect("global::tmp", 8 * i, AccessOperation::Load);
      expect("global::tmp", 8 * i, AccessOperation::Store);
    }
    for (std::uint64_t j = 0; j < 3; ++j)
    {
      expect("global::A", 8 * (3 * i + j), AccessOperation::Load);
      expect("global::tmp", 8 * i, AccessOperation::Load);
      expect("global::y", 8 * j, AccessOperation::Load);
      expect("global::y", 8 * j, AccessOperation::Store);
    }
  }
  EXPECT_EQ(ordinal, 53U);
}

}  // namespace
}  // namespace yarda::test::region
