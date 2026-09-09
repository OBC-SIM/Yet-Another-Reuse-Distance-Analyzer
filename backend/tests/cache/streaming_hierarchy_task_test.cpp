#include "streaming_hierarchy_task.hpp"

#include <stdexcept>
#include <string>

#include "hierarchy_analysis_test_support.hpp"

namespace
{

using namespace yarda;

class StreamingHierarchyTaskTest : public ::testing::Test
{
protected:
  const AnalysisHierarchy hierarchy_ = test::support::make_batch_hierarchy();
  TraceEmissionBudget budget_;
  const StreamingHierarchyOptions options_;
  HierarchyEventDelivery delivery_;
  detail::StreamingHierarchyTask task_{"guarded-task", hierarchy_, budget_,
                                       options_, delivery_};
  const ResolvedAccess valid_access_{
    "global::A", 0, 1, 0, AddressBasis::Absolute, AccessOperation::Load, 0};
};

TEST_F(StreamingHierarchyTaskTest, RejectsUnknownOperationWithTaskIdentity)
{
  auto access = valid_access_;
  access.operation = AccessOperation::Unknown;
  try
  {
    task_.accept(access);
    FAIL() << "expected unsupported operation rejection";
  }
  catch (const std::invalid_argument & error)
  {
    EXPECT_NE(std::string(error.what()).find("guarded-task"),
              std::string::npos);
  }
}

TEST_F(StreamingHierarchyTaskTest, RejectsFirstSourceOrdinalOtherThanZero)
{
  auto access = valid_access_;
  access.source_access_ordinal = 1;

  EXPECT_THROW(task_.accept(access), std::invalid_argument);
}

TEST_F(StreamingHierarchyTaskTest, RejectsSourceCoverageMismatchAtTaskBoundary)
{
  task_.accept(valid_access_);

  try
  {
    static_cast<void>(task_.finish({2, 2, 0, 0}));
    FAIL() << "expected source coverage mismatch";
  }
  catch (const std::logic_error & error)
  {
    const std::string message = error.what();
    EXPECT_NE(message.find("source stream"), std::string::npos);
    EXPECT_NE(message.find("guarded-task"), std::string::npos);
  }
}

TEST_F(StreamingHierarchyTaskTest, RejectsLineCountInSourceOnlyCoverage)
{
  task_.accept(valid_access_);

  EXPECT_THROW(task_.finish({1, 1, 0, 1}), std::logic_error);
}

}  // namespace
