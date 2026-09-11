#include "../../src/trace/prepared_index.hpp"

#include <gtest/gtest.h>
#include <limits>

namespace
{

using namespace yarda::detail;

TEST(PreparedIndexTest, ReadsTheCurrentValueOfItsBoundSlot)
{
  const LoopScope scope{"i", 1};
  const PreparedIndex index("i", &scope);
  std::vector<std::int64_t> values{99, 2};
  EXPECT_EQ(index.evaluate(values), "2");
  values[1] = -7;
  EXPECT_EQ(index.evaluate(values), "-7");
}

TEST(PreparedIndexTest, UsesNearestBindingAndLeavesOuterBindingAvailable)
{
  const LoopScope outer{"i", 0};
  const LoopScope inner{"i", 1, &outer};
  EXPECT_EQ(PreparedIndex("i+3", &inner).evaluate({4, 10}), "13");
  EXPECT_EQ(PreparedIndex("i-2", &outer).evaluate({4, 10}), "2");
}

TEST(PreparedIndexTest, FindsOuterVariableThroughAnotherNamedScope)
{
  const LoopScope outer{"i", 0};
  const LoopScope inner{"j", 1, &outer};
  EXPECT_EQ(PreparedIndex("i-2", &inner).evaluate({4, 10}), "2");
}

TEST(PreparedIndexTest, ExactVariableNamePrecedesOffsetParsing)
{
  const LoopScope outer{"i", 0};
  const LoopScope inner{"i+1", 1, &outer};
  EXPECT_EQ(PreparedIndex("i+1", &inner).evaluate({4, 10}), "10");
}

TEST(PreparedIndexTest, PreservesFixedSpellingAndUnsupportedExpressions)
{
  const LoopScope scope{"i", 0};
  for (const auto * text : {"0007", "+3", "-1", "missing", "i*2", "i+j", "i+1x",
                            "i+9223372036854775808"})
  {
    SCOPED_TRACE(text);
    EXPECT_EQ(PreparedIndex(text, &scope).evaluate({2}), text);
  }
  EXPECT_EQ(PreparedIndex("i+1", nullptr).evaluate({}), "i+1");
}

TEST(PreparedIndexTest, PreservesFallbackOnlyWhenCurrentAdditionOverflows)
{
  const LoopScope scope{"i", 0};
  const PreparedIndex upper("i+1", &scope);
  const PreparedIndex lower("i-1", &scope);
  const auto max = std::numeric_limits<std::int64_t>::max();
  const auto min = std::numeric_limits<std::int64_t>::min();
  EXPECT_EQ(upper.evaluate({max - 1}), "9223372036854775807");
  EXPECT_EQ(upper.evaluate({max}), "i+1");
  EXPECT_EQ(lower.evaluate({min}), "i-1");
  EXPECT_EQ(lower.evaluate({min + 1}), "-9223372036854775808");
}

TEST(PreparedIndexTest, SupportsMinimumSignedOffsetWithoutNegatingIt)
{
  const LoopScope scope{"i", 0};
  const PreparedIndex index("i-9223372036854775808", &scope);
  EXPECT_EQ(index.evaluate({0}), "-9223372036854775808");
  EXPECT_EQ(index.evaluate({1}), "-9223372036854775807");
  EXPECT_EQ(index.evaluate({-1}), "i-9223372036854775808");
}

TEST(PreparedIndexTest, NumericLiteralsRetainExistingExactIntegerGrammar)
{
  for (const auto * text : {"0007", "+7", " 7", "7"})
    EXPECT_EQ(PreparedIndex(text, nullptr).evaluate_numeric({}), 7);
  EXPECT_FALSE(PreparedIndex("7 ", nullptr).evaluate_numeric({}));
  EXPECT_FALSE(
    PreparedIndex("9223372036854775808", nullptr).evaluate_numeric({}));
}

TEST(PreparedIndexTest, NumericEvaluationReadsCurrentShadowedSlot)
{
  const LoopScope outer{"i", 0};
  const LoopScope inner{"i", 1, &outer};
  const PreparedIndex index("i-2", &inner);
  EXPECT_EQ(index.evaluate_numeric({4, 10}), 8);
  EXPECT_EQ(index.evaluate_numeric({4, -1}), -3);
}

TEST(PreparedIndexTest, NumericEvaluationPrefersExactVariableNames)
{
  const LoopScope numeric_name{"0007", 0};
  EXPECT_EQ(PreparedIndex("0007", &numeric_name).evaluate_numeric({12}), 12);
  const LoopScope expression_name{"i+1", 0};
  EXPECT_EQ(PreparedIndex("i+1", &expression_name).evaluate_numeric({5}), 5);
}

TEST(PreparedIndexTest, NumericOverflowDoesNotPoisonSubsequentEvaluation)
{
  const LoopScope scope{"i", 0};
  const PreparedIndex index("i+1", &scope);
  EXPECT_FALSE(
    index.evaluate_numeric({std::numeric_limits<std::int64_t>::max()}));
  EXPECT_EQ(index.evaluate_numeric({2}), 3);
  EXPECT_FALSE(PreparedIndex("i*2", &scope).evaluate_numeric({2}));
}

TEST(PreparedIndexTest, NumericOverflowUsesExactOriginalFallbackWhenAvailable)
{
  const LoopScope scope{" ", 0};
  const PreparedIndex index(" +1", &scope);
  const auto max = std::numeric_limits<std::int64_t>::max();
  EXPECT_EQ(index.evaluate({max}), " +1");
  EXPECT_EQ(index.evaluate_numeric({max}), 1);
  EXPECT_EQ(index.evaluate_numeric({2}), 3);
}

}  // namespace
