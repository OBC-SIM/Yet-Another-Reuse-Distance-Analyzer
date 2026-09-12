#include "prepared_access_test_support.hpp"

namespace
{

using namespace yarda::test::prepared_access;
using yarda::detail::AccessLayoutResolver;
using yarda::detail::PreparedLayout;

TEST(PreparedAccessTest, ScalarPlanKeepsItsActualWidth)
{
  const Json node = {{"type", "Scalar"}, {"elem_size", 8}};
  PreparedLayout plan;
  expect_bytes(AccessLayoutResolver().prepare(node, {}, plan), 0, 8);
  expect_bytes(plan.resolve({}, "global::A"), 0, 8);
}

TEST(PreparedAccessTest, LegacyPlanReevaluatesEachRowWithOriginalFlattening)
{
  const Json node = {{"type", "Array"}, {"elem_size", 4}, {"shape", {2, 3}}};
  PreparedLayout plan;
  expect_bytes(AccessLayoutResolver().prepare(node, {0, 1}, plan), 4, 4);
  expect_bytes(plan.resolve({1, 2}, "global::A"), 20, 4);
}

TEST(PreparedAccessTest, LegacyPlanAcceptsLeadingGepIndex)
{
  const Json node = {{"type", "Array"}, {"elem_size", 4}, {"shape", {2, 3}}};
  PreparedLayout plan;
  expect_bytes(AccessLayoutResolver().prepare(node, {0, 1, 2}, plan), 20, 4);
  expect_bytes(plan.resolve({1, 0, 1}, "global::A"), 28, 4);
}

TEST(PreparedAccessTest, LegacyFlatIndexDoesNotAcquireDimensionChecks)
{
  const Json node = {{"type", "Array"}, {"elem_size", 4}, {"shape", {2, 3}}};
  PreparedLayout plan;
  expect_bytes(AccessLayoutResolver().prepare(node, {5}, plan), 20, 4);
  expect_bytes(plan.resolve({-1}, "global::A"), -4, 4);
}

TEST(PreparedAccessTest, LegacyKeepsIgnoredLeadingShapeAndSignedDimensions)
{
  const Json node = {
    {"type", "Array"}, {"elem_size", 4}, {"shape", {"ignored", -2}}};
  PreparedLayout plan;
  expect_bytes(AccessLayoutResolver().prepare(node, {-1, 1}, plan), 12, 4);
  expect_bytes(plan.resolve({-2, 1}, "global::A"), 20, 4);
}

TEST(PreparedAccessTest, StructuredPlanPreservesPaddedFieldsAndCurrentIndices)
{
  const AccessLayoutResolver resolver(structured_module());
  PreparedLayout plan;
  expect_bytes(resolver.prepare(structured_access(), {0, 0}, plan), 8, 8);
  expect_bytes(plan.resolve({1, 2}, "global::A"), 88, 8);
  expect_bytes(plan.resolve({0, 1}, "global::A"), 24, 8);
}

TEST(PreparedAccessTest, PreparedLayoutOwnsItsMetadataAfterResolverDestruction)
{
  PreparedLayout plan;
  {
    const AccessLayoutResolver resolver(structured_module());
    expect_bytes(resolver.prepare(structured_access(), {0, 0}, plan), 8, 8);
  }
  expect_bytes(plan.resolve({1, 1}, "global::A"), 72, 8);
}

TEST(PreparedAccessTest, LegacyRejectsMissingIndicesAndRankMismatch)
{
  const AccessLayoutResolver resolver;
  const Json node = {{"type", "Array"}, {"elem_size", 4}, {"shape", {2}}};
  PreparedLayout plan;
  EXPECT_FALSE(resolver.prepare(node, {}, plan));
  EXPECT_FALSE(resolver.prepare(node, {0, 1, 2}, plan));
}

}  // namespace
