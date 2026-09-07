#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <string>

#include "hierarchy_analysis_test_support.hpp"

namespace
{

using yarda::analyze_batch_hierarchy;
using yarda::test::support::batch_input;
using yarda::test::support::make_batch_hierarchy;
using yarda::test::support::make_batch_task;

class BatchHierarchyValidationTest : public testing::Test
{
protected:
  yarda::AnalysisHierarchy hierarchy = make_batch_hierarchy();
  yarda::ResolvedTaskTraceResult input =
    batch_input({make_batch_task({0, 32})});
};

TEST_F(BatchHierarchyValidationTest, RejectsNoTasks)
{
  EXPECT_THROW(analyze_batch_hierarchy({}, hierarchy), std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsEmptyTaskId)
{
  input.tasks[0].task_id.clear();
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsDuplicateTaskIds)
{
  input = batch_input({make_batch_task({0}), make_batch_task({32})});
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsIncompleteTaskCoverage)
{
  input.tasks[0].coverage.resolved_accesses = 1;
  input.coverage.resolved_accesses = 1;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsRejectedAccessDespiteEqualCounts)
{
  input.tasks[0].coverage.rejected_accesses = 1;
  input.coverage.rejected_accesses = 1;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsAccessCountMismatch)
{
  input.tasks[0].accesses.pop_back();
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsAggregateCoverageMismatch)
{
  input.coverage.source_accesses = 3;
  input.coverage.resolved_accesses = 3;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsIncompleteAggregateCoverage)
{
  input.coverage.resolved_accesses = 1;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsPremappedTaskCoverage)
{
  input.tasks[0].coverage.emitted_line_references = 2;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsPremappedAggregateCoverage)
{
  input.coverage.emitted_line_references = 2;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsConsistentOpaqueCallExclusions)
{
  input.tasks[0].excluded_opaque_call_sites = 1;
  input.excluded_opaque_call_sites = 1;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsOpaqueCallsHiddenByWrappedAggregate)
{
  input =
    batch_input({make_batch_task({}, "first"), make_batch_task({}, "second")});
  input.tasks[0].excluded_opaque_call_sites =
    std::numeric_limits<std::uint64_t>::max();
  input.tasks[1].excluded_opaque_call_sites = 1;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsModuleOnlyOpaqueCallExclusions)
{
  input.excluded_opaque_call_sites = 1;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsImageRelativeAddress)
{
  input.tasks[0].accesses[0].address_basis = yarda::AddressBasis::ImageRelative;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest,
       RejectsNonAbsoluteAddressBeforeRangeOverflow)
{
  auto & access = input.tasks[0].accesses[0];
  access.address_basis = yarda::AddressBasis::ImageRelative;
  access.linked_byte_address = std::numeric_limits<std::uint64_t>::max();
  access.access_size = 2;

  try
  {
    analyze_batch_hierarchy(input, hierarchy);
    ADD_FAILURE() << "expected non-absolute address rejection";
  }
  catch (const std::invalid_argument & error)
  {
    EXPECT_NE(std::string(error.what()).find("absolute addresses"),
              std::string::npos)
      << error.what();
  }
}

TEST_F(BatchHierarchyValidationTest, RejectsUnknownOperation)
{
  input.tasks[0].accesses[0].operation = yarda::AccessOperation::Unknown;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsSourceOrdinalNotStartingAtZero)
{
  input.tasks[0].accesses[0].source_access_ordinal = 4;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsDuplicateSourceOrdinal)
{
  input.tasks[0].accesses[1].source_access_ordinal = 0;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsSourceOrdinalGap)
{
  input.tasks[0].accesses[1].source_access_ordinal = 2;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsEmptyObjectIdentity)
{
  input.tasks[0].accesses[0].object_id.clear();
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsZeroAccessSize)
{
  input.tasks[0].accesses[0].access_size = 0;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsLinkedAddressOverflow)
{
  input.tasks[0].accesses[0].linked_byte_address =
    std::numeric_limits<std::uint64_t>::max();
  input.tasks[0].accesses[0].access_size = 2;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy), std::overflow_error);
}

TEST_F(BatchHierarchyValidationTest, RejectsObjectOffsetOverflow)
{
  input.tasks[0].accesses[0].object_byte_offset =
    std::numeric_limits<std::uint64_t>::max();
  input.tasks[0].accesses[0].access_size = 2;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy), std::overflow_error);
}

TEST_F(BatchHierarchyValidationTest, RejectsUnsupportedLaterTask)
{
  input = batch_input(
    {make_batch_task({0}, "first"), make_batch_task({32}, "second")});
  input.tasks[1].accesses[0].address_basis = yarda::AddressBasis::ImageRelative;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsMappingFailureInLaterTask)
{
  input = batch_input(
    {make_batch_task({0}, "first"), make_batch_task({32}, "second")});
  input.tasks[1].accesses[0].access_size = 0;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsInvalidLlcGeometryForEmptyTask)
{
  input = batch_input({make_batch_task({})});
  hierarchy.llc.geometry.line_count = 0;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

TEST_F(BatchHierarchyValidationTest, RejectsUnequalLineSizesForEmptyTask)
{
  input = batch_input({make_batch_task({})});
  hierarchy.llc.geometry.line_size = 64;
  EXPECT_THROW(analyze_batch_hierarchy(input, hierarchy),
               std::invalid_argument);
}

}  // namespace
