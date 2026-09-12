#include "trace/module/call_substitution.hpp"
#include "trace/index/prepared_index.hpp"
#include "trace/prepared_access_test_support.hpp"

#include <limits>

namespace yarda::test
{
namespace
{
using namespace prepared_access;
using detail::LoopScope;
using detail::PreparedIndex;

TEST(AffineEvaluationTest, EvaluatesCoefficientsAndMultipleLexicalSlots)
{
  const LoopScope outer{"i", 0};
  const LoopScope inner{"j", 1, &outer};
  const PreparedIndex index("8*i+j-2", &inner);
  EXPECT_EQ(index.evaluate({2, 3}), "17");
  EXPECT_EQ(index.evaluate_numeric({2, 3}), 17);
  EXPECT_EQ(index.evaluate_numeric({0, 2}), 0);
}

TEST(AffineEvaluationTest, CombinesRepeatedTermsAndEliminatesZeroTerms)
{
  const LoopScope scope{"i", 0};
  EXPECT_EQ(PreparedIndex("2*i-i+3", &scope).evaluate_numeric({4}), 7);
  EXPECT_EQ(PreparedIndex("i-i+3", &scope).evaluate_numeric({4}), 3);
  EXPECT_EQ(PreparedIndex("0*missing+3", &scope).evaluate_numeric({4}), 3);
}

TEST(AffineEvaluationTest, UsesWideProductsBeforeCheckingFinalInt64)
{
  const LoopScope scope{"i", 0};
  const PreparedIndex index("2*i-9223372036854775807", &scope);
  const auto max = std::numeric_limits<std::int64_t>::max();
  EXPECT_EQ(index.evaluate_numeric({max}), max);
  EXPECT_FALSE(PreparedIndex("2*i", &scope).evaluate_numeric({max}));
  EXPECT_EQ(PreparedIndex("2*i", &scope).evaluate_numeric({3}), 6);
}

TEST(AffineEvaluationTest, DoesNotPartiallyEvaluateAnUnboundTerm)
{
  const LoopScope scope{"i", 0};
  const PreparedIndex index("2*i+j", &scope);
  EXPECT_EQ(index.evaluate({3}), "2*i+j");
  EXPECT_FALSE(index.evaluate_numeric({3}));
}

TEST(AffineEvaluationTest, SubstitutesExpressionsAsTerms)
{
  auto node = access("global::A", "2*x+1");
  node["name"] = "A[2*x+1]";
  node["access_path"] = Json::array({{{"kind", "index"}, {"value", "2*x+1"}}});
  const auto result =
    detail::substitute_call_node(node, {{"x", "i+1"}}, {}, {});
  EXPECT_EQ(result["indices"][0], "2*i+3");
  EXPECT_EQ(result["access_path"][0]["value"], "2*i+3");
  EXPECT_EQ(result["name"], "A[2*i+3]");
}

TEST(AffineEvaluationTest,
     KeepsUnsupportedBracketsAlignedWithSubstitutedIndices)
{
  auto node = access("global::A", "x/2");
  node["name"] = "A[x/2][2*y+1]";
  node["indices"] = {"x/2", "2*y+1"};
  node["access_path"] = Json::array({
    {{"kind", "index"}, {"value", "x/2"}},
    {{"kind", "index"}, {"value", "2*y+1"}},
  });
  for (const auto * actual : {"i", "k"})
  {
    SCOPED_TRACE(actual);
    const auto result =
      detail::substitute_call_node(node, {{"x", actual}, {"y", "j+1"}}, {}, {});
    EXPECT_EQ(result["name"], "A[x/2][2*j+3]");
    EXPECT_EQ(result["indices"], Json::array({"x/2", "2*j+3"}));
    EXPECT_EQ(result["access_path"][0]["value"], "x/2");
    EXPECT_EQ(result["access_path"][1]["value"], "2*j+3");
  }
}

TEST(AffineEvaluationTest, PreservesIndependentNestedSourceOffsets)
{
  const auto raw = kernel(Json::array(
    {loop(2, Json::array(
               {loop(3, Json::array({access("global::A", "8*i+j")}), "j")}))}));
  const auto result = resolved_task_traces(raw, addresses());
  ASSERT_EQ(result.tasks.size(), 1U);
  expect_offsets(result.tasks[0], {0, 4, 8, 32, 36, 40});
}

TEST(AffineEvaluationTest, DefersInvalidExpressionInsideZeroTripBody)
{
  const auto raw = kernel(Json::array(
    {loop(0, Json::array({access("global::A", "9223372036854775808*i")}))}));
  const auto result = resolved_task_traces(raw, addresses());
  EXPECT_TRUE(result.tasks[0].accesses.empty());
}

TEST(AffineEvaluationTest, ReportsOverflowWithOriginalSourceAccounting)
{
  const auto max = std::numeric_limits<std::int64_t>::max();
  const auto node =
    loop(max, Json::array({access("global::A", "2*i")}), "i", max - 1);
  expect_resolution_failure(kernel(Json::array({access(), node})), addresses(),
                            "runtime-dependent index cannot be resolved", 1,
                            ResolutionCategory::Unsupported);
}

TEST(AffineEvaluationTest, KeepsUnsupportedGrammarAndInvalidNumbersUnresolved)
{
  const LoopScope scope{"i", 0};
  for (const auto * expression :
       {"i*i", "n*i", "i/2", "i%2", "2*(i+1)", "i**2", "2*i+", "i+1x", "2*i+3 ",
        "9223372036854775808*i", "i+-1", "i--1", "2*i*2"})
  {
    SCOPED_TRACE(expression);
    const PreparedIndex index(expression, &scope);
    EXPECT_FALSE(index.evaluate_numeric({3}));
    EXPECT_EQ(index.evaluate({3}), expression);
  }
}

TEST(AffineEvaluationTest, ComposesNegativeMinimumCoefficientWithoutNegation)
{
  const LoopScope scope{"i", 0};
  const PreparedIndex index("-9223372036854775808*i+1", &scope);
  EXPECT_EQ(index.evaluate_numeric({1}),
            std::numeric_limits<std::int64_t>::min() + 1);
  EXPECT_FALSE(index.evaluate_numeric({-1}));
}

}  // namespace
}  // namespace yarda::test
