#include <cmath>
#include <functional>
#include <limits>

#include "artifact_test_support.hpp"

namespace
{
using namespace yarda;
using namespace yarda::test::artifact;

TEST(HierarchyResultValidationTest, RejectsNoTasks)
{
  EXPECT_THROW(hierarchy_result_json(metadata(), {}), std::invalid_argument);
}

TEST(HierarchyResultValidationTest, RejectsEmptyAndDuplicateTaskIdentity)
{
  auto value = result();
  value.tasks[0].task_id.clear();
  EXPECT_THROW(hierarchy_result_json(metadata(), value), std::invalid_argument);
  value = result();
  value.tasks.push_back(value.tasks[0]);
  value.coverage = {8, 8, 0, 8};
  EXPECT_THROW(hierarchy_result_json(metadata(), value), std::invalid_argument);
}

TEST(HierarchyResultValidationTest, RejectsIncompleteAndInconsistentCoverage)
{
  for (const auto mutate :
       std::vector<std::function<void(StreamingHierarchyResult &)>>{
           [](auto & r) { ++r.coverage.rejected_accesses; },
           [](auto & r) { ++r.coverage.source_accesses; },
           [](auto & r) { ++r.coverage.emitted_line_references; },
           [](auto & r) { ++r.tasks[0].coverage.rejected_accesses; },
           [](auto & r) { ++r.tasks[0].source_accesses; },
           [](auto & r) { ++r.tasks[0].coverage.emitted_line_references; }})
  {
    auto value = result();
    mutate(value);
    EXPECT_ANY_THROW(hierarchy_result_json(metadata(), value));
  }
}

TEST(HierarchyResultValidationTest, RejectsEachFalseInvariant)
{
  for (auto flag : {&HierarchyInvariants::level_conservation_l1,
                    &HierarchyInvariants::level_conservation_llc,
                    &HierarchyInvariants::llc_input_matches_l1_misses,
                    &HierarchyInvariants::first_service_conservation,
                    &HierarchyInvariants::all_passed})
  {
    auto value = result();
    value.tasks[0].invariants.*flag = false;
    EXPECT_THROW(hierarchy_result_json(metadata(), value), std::invalid_argument);
  }
}

TEST(HierarchyResultValidationTest, RejectsForgedPassingFlagsWithBadCounts)
{
  auto value = result();
  ++value.tasks[0].ehc_l1;
  EXPECT_THROW(hierarchy_result_json(metadata(), value), std::logic_error);
}

TEST(HierarchyResultValidationTest, RejectsHistogramAndUniqueLineMismatch)
{
  for (const auto mutate :
       std::vector<std::function<void(CacheLevelSummary &)>>{
           [](auto & l) { ++l.unique_lines; },
           [](auto & l) { l.csrd_histogram[0] += 1; },
           [](auto & l) { l.csrd_histogram[99] = 0; },
           [](auto & l) {
             l.csrd_histogram = {{2, 2}};
           }})
  {
    auto value = result();
    mutate(value.tasks[0].l1);
    EXPECT_ANY_THROW(hierarchy_result_json(metadata(), value));
  }
}

TEST(HierarchyResultValidationTest, RejectsAbsentNonfiniteAndIncorrectRatios)
{
  for (const auto bad :
       {std::optional<double>{}, std::optional<double>{-0.1},
        std::optional<double>{0.25}, std::optional<double>{std::nan("")},
        std::optional<double>{INFINITY}})
  {
    auto value = result();
    value.tasks[0].hr_l1 = bad;
    EXPECT_ANY_THROW(hierarchy_result_json(metadata(), value));
  }
}

TEST(HierarchyResultValidationTest, RejectsIncompleteOrUnsupportedMetadata)
{
  for (const auto mutate :
       std::vector<std::function<void(HierarchyResultMetadata &)>>{
           [](auto & m) { m.lat_schema_version = 0; },
           [](auto & m) { m.cache_schema_version = 2; },
           [](auto & m) { m.elf_address_size = 3; },
           [](auto & m) { m.elf_machine = 0; },
           [](auto & m) { m.hierarchy.core_id = 1; },
           [](auto & m) { m.hierarchy.l1.name.clear(); },
           [](auto & m) { m.hierarchy.llc.role = "L2"; },
           [](auto & m) { m.hierarchy.llc.name = m.hierarchy.l1.name; },
           [](auto & m) { m.hierarchy.llc.geometry.line_size = 64; }})
  {
    auto info = metadata();
    mutate(info);
    EXPECT_ANY_THROW(hierarchy_result_json(info, result()));
  }
}

TEST(HierarchyResultValidationTest, RejectsOverflowingCapacity)
{
  auto info = metadata();
  info.hierarchy.l1.geometry = {32, std::uint64_t{1} << 63, 1};
  EXPECT_THROW(hierarchy_result_json(info, result()), std::overflow_error);
}

TEST(HierarchyResultValidationTest, RejectsOverflowingHistogramCounts)
{
  auto value = result();
  value.tasks[0].l1.csrd_histogram = {
      {0, std::numeric_limits<std::uint64_t>::max()}, {1, 1}};
  EXPECT_THROW(hierarchy_result_json(metadata(), value), std::overflow_error);
}

} // namespace
