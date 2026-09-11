#include <limits>

#include "prepared_access_test_support.hpp"

namespace
{

using namespace yarda::test::prepared_access;
using yarda::detail::AccessLayoutResolver;
using yarda::detail::PreparedLayout;
constexpr auto kMax = std::numeric_limits<std::int64_t>::max();

TEST(PreparedLayoutFailureTest, RechecksStructuredNegativeIndexAfterSuccess)
{
  PreparedLayout plan;
  ASSERT_TRUE(AccessLayoutResolver(structured_module())
                .prepare(structured_access(), {0, 0}, plan));
  expect_layout_error(
    [&] {
      plan.resolve({-1, 0}, "global::A");
    },
    "index is not an exact non-negative integer");
}

TEST(PreparedLayoutFailureTest, RechecksStructuredDimensionAfterSuccess)
{
  PreparedLayout plan;
  ASSERT_TRUE(AccessLayoutResolver(structured_module())
                .prepare(structured_access(), {0, 0}, plan));
  expect_layout_error(
    [&] {
      plan.resolve({0, 3}, "global::A");
    },
    "index exceeds its dimension");
}

TEST(PreparedLayoutFailureTest, EarlierDynamicFailurePrecedesMalformedField)
{
  auto node = structured_access();
  node["access_path"][2]["name"] = "wrong";
  PreparedLayout plan;
  expect_layout_error(
    [&] {
      AccessLayoutResolver(structured_module()).prepare(node, {2, 0}, plan);
    },
    "index exceeds its dimension");
}

TEST(PreparedLayoutFailureTest, LegacyRechecksIndexMultiplicationOverflow)
{
  PreparedLayout plan;
  const Json node = {{"type", "Array"}, {"elem_size", 1}, {"shape", {2, 2}}};
  ASSERT_TRUE(AccessLayoutResolver().prepare(node, {0, 0}, plan));
  EXPECT_FALSE(plan.resolve({kMax, 0}, "global::A"));
}

TEST(PreparedLayoutFailureTest, LegacyRechecksLinearSumOverflow)
{
  PreparedLayout plan;
  const Json node = {{"type", "Array"}, {"elem_size", 1}, {"shape", {2, 1}}};
  ASSERT_TRUE(AccessLayoutResolver().prepare(node, {0, 0}, plan));
  EXPECT_FALSE(plan.resolve({kMax, 1}, "global::A"));
}

TEST(PreparedLayoutFailureTest, LegacyRechecksFinalWidthMultiplication)
{
  PreparedLayout plan;
  const Json node = {{"type", "Array"}, {"elem_size", 8}};
  ASSERT_TRUE(AccessLayoutResolver().prepare(node, {0}, plan));
  EXPECT_FALSE(plan.resolve({kMax}, "global::A"));
}

TEST(PreparedLayoutFailureTest,
     LegacyPreservesIntermediateOverflowBeforeCancellation)
{
  PreparedLayout plan;
  const Json node = {{"type", "Array"}, {"elem_size", 1}, {"shape", {2, 1, 1}}};
  ASSERT_TRUE(AccessLayoutResolver().prepare(node, {0, 0, 0}, plan));
  EXPECT_FALSE(plan.resolve({kMax, 1, -1}, "global::A"));
}

TEST(PreparedLayoutFailureTest, LegacyRejectsStaticStrideOverflow)
{
  PreparedLayout plan;
  const Json node = {
    {"type", "Array"}, {"elem_size", 1}, {"shape", {1, kMax, 2}}};
  EXPECT_FALSE(AccessLayoutResolver().prepare(node, {0, 0, 0}, plan));
}

}  // namespace
