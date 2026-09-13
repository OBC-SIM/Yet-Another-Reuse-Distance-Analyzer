#include "artifact_test_support.hpp"

namespace
{
using namespace yarda;
using namespace yarda::test::artifact;
namespace stream = yarda::test::streaming;

TEST(HierarchyArtifactBoundaryTest, RejectsDistanceAtOrBeyondDistinctLineCount)
{
  auto value = histogram_result();
  value.tasks[0].l1.csrd_histogram.erase(10);
  value.tasks[0].l1.csrd_histogram[11] = 3;
  EXPECT_THROW(hierarchy_result_json(metadata(), value), std::invalid_argument);
}

TEST(HierarchyArtifactBoundaryTest, RejectsMoreLlcDistinctLinesThanL1)
{
  auto value = histogram_result();
  auto & task = value.tasks[0];
  task.llc = {16, 4, 12, 12, 0, 12, {{0, 4}}};
  task.ehc_llc = 4;
  task.all_cache_misses = 12;
  task.hr_llc = 4.0 / 17.0;
  task.miss_ratio = 12.0 / 17.0;
  EXPECT_THROW(hierarchy_result_json(metadata(), value), std::invalid_argument);
}

TEST(HierarchyArtifactBoundaryTest, RejectsNegativeZeroRatioEncoding)
{
  auto value = result();
  value.tasks[0].hr_llc = -0.0;
  EXPECT_THROW(hierarchy_result_json(metadata(), value), std::invalid_argument);
}

} // namespace
