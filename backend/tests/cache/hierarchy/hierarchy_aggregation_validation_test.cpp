#include <gtest/gtest.h>
#include <utility>

#include "hierarchy_aggregation_test_support.hpp"

namespace
{

using namespace yarda::test::support;

class HierarchyAggregationLevelTest : public testing::TestWithParam<bool>
{
};

TEST_P(HierarchyAggregationLevelTest, RejectsMappingDecisionLengthMismatch)
{
  auto fixture = make_aggregation_fixture();
  auto & level = GetParam() ? fixture.llc : fixture.l1;
  level.accesses.pop_back();
  expect_aggregation_error(fixture,
                           GetParam() ? "LLC result size" : "L1 result size");
}

TEST_P(HierarchyAggregationLevelTest, RejectsSummaryLookupLengthMismatch)
{
  auto fixture = make_aggregation_fixture();
  auto & level = GetParam() ? fixture.llc : fixture.l1;
  ++level.summary.lookups;
  expect_aggregation_error(fixture,
                           GetParam() ? "LLC result size" : "L1 result size");
}

INSTANTIATE_TEST_SUITE_P(SelectedLevels, HierarchyAggregationLevelTest,
                         testing::Bool());

TEST(HierarchyAggregationValidationTest, RejectsMissingLlcRow)
{
  auto fixture = make_aggregation_fixture();
  fixture.llc.mappings.pop_back();
  fixture.llc.accesses.pop_back();
  --fixture.llc.summary.lookups;
  --fixture.llc.summary.hits;
  fixture.llc.summary.csrd_histogram.clear();
  expect_aggregation_error(fixture, "missing LLC");
}

TEST(HierarchyAggregationValidationTest, RejectsLeftoverLlcRow)
{
  auto fixture = make_aggregation_fixture();
  fixture.llc.mappings.push_back(fixture.llc.mappings.back());
  fixture.llc.accesses.push_back(fixture.llc.accesses.back());
  ++fixture.llc.summary.lookups;
  ++fixture.llc.summary.hits;
  ++fixture.llc.summary.csrd_histogram[0];
  expect_aggregation_error(fixture, "leftover LLC");
}

TEST(HierarchyAggregationValidationTest, RejectsReorderedEqualAddressRows)
{
  auto fixture = make_aggregation_fixture();
  ASSERT_EQ(fixture.llc.mappings[1].decoded.address,
            fixture.llc.mappings[3].decoded.address);
  std::swap(fixture.llc.mappings[1], fixture.llc.mappings[3]);
  std::swap(fixture.llc.accesses[1], fixture.llc.accesses[3]);
  expect_aggregation_error(fixture, "provenance");
}

TEST(HierarchyAggregationValidationTest, RejectsDecisionsDisagreeingWithCounts)
{
  auto fixture = make_aggregation_fixture();
  fixture.llc.accesses.back() = {yarda::LruAccessOutcome::ReplacementMiss, 4};
  expect_aggregation_error(fixture, "invariants disagree");
}

struct ProvenanceMutation
{
  const char * name;
  void (*mutate)(yarda::CacheLineMapping &);
};

class HierarchyAggregationProvenanceTest
  : public testing::TestWithParam<ProvenanceMutation>
{
};

TEST_P(HierarchyAggregationProvenanceTest, RejectsMismatchedPairedProvenance)
{
  auto fixture = make_aggregation_fixture();
  GetParam().mutate(fixture.llc.mappings[0]);
  expect_aggregation_error(fixture, "provenance");
}

INSTANTIATE_TEST_SUITE_P(
  PairedFields, HierarchyAggregationProvenanceTest,
  testing::Values(
    ProvenanceMutation{"SourceOrdinal",
                       [](auto & row) { ++row.source_access_ordinal; }},
    ProvenanceMutation{"SpanOrdinal",
                       [](auto & row) { ++row.line_span_ordinal; }},
    ProvenanceMutation{"Object", [](auto & row) { row.object_id = "other"; }},
    ProvenanceMutation{"ObjectOffset",
                       [](auto & row) { ++row.object_byte_offset; }},
    ProvenanceMutation{"Address", [](auto & row) { ++row.decoded.address; }},
    ProvenanceMutation{"Basis",
                       [](auto & row) {
                         row.address_basis = yarda::AddressBasis::ImageRelative;
                       }},
    ProvenanceMutation{"SourceOffset",
                       [](auto & row) { ++row.source_object_byte_offset; }},
    ProvenanceMutation{"SourceSize",
                       [](auto & row) { ++row.source_access_size; }},
    ProvenanceMutation{"SourceAddress",
                       [](auto & row) { ++row.source_linked_byte_address; }},
    ProvenanceMutation{
      "Operation",
      [](auto & row) { row.operation = yarda::AccessOperation::Store; }}),
  [](const testing::TestParamInfo<ProvenanceMutation> & case_info) {
    return case_info.param.name;
  });

}  // namespace
