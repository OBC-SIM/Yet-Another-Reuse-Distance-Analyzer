#include "trace/index/affine_expression.hpp"
#include "trace/index/prepared_index.hpp"

#include <gtest/gtest.h>
#include <limits>

namespace yarda::detail
{
namespace
{

TEST(AffineExpressionTest, CanonicalizesOrderDuplicatesSignsAndZero)
{
  for (const auto & [source, expected] : std::map<std::string, std::string>{
         {"j+2*i-i+3-1", "i+j+2"},
         {"7-i", "-i+7"},
         {"-0*i+0", "0"},
         {"i-i", "0"},
         {"-9223372036854775808*i", "-9223372036854775808*i"},
         {"i-9223372036854775808", "i-9223372036854775808"}})
  {
    SCOPED_TRACE(source);
    const auto expression = parse_affine_expression(source);
    ASSERT_TRUE(expression);
    EXPECT_EQ(format_affine_expression(*expression), expected);
  }
}

TEST(AffineExpressionTest, RejectsOverflowDuringNormalization)
{
  for (const auto * text :
       {"9223372036854775807*i+i", "9223372036854775807+1",
        "-9223372036854775808*i-i", "-9223372036854775808-1"})
  {
    SCOPED_TRACE(text);
    EXPECT_FALSE(parse_affine_expression(text));
  }
}

TEST(AffineExpressionTest, ScaledAdditionChecksBothConstantsAndTerms)
{
  for (const auto * text : {"9223372036854775807", "9223372036854775807*i"})
  {
    AffineExpression target;
    const auto source = parse_affine_expression(text);
    ASSERT_TRUE(source);
    EXPECT_FALSE(add_affine_expression(target, *source, 2));
  }
}

TEST(AffineExpressionTest, MultiTermBindingUsesNearestLexicalVariable)
{
  const LoopScope outer{"i", 0};
  const LoopScope middle{"j", 1, &outer};
  const LoopScope inner{"i", 2, &middle};
  const PreparedIndex index("2*i+j", &inner);
  EXPECT_EQ(index.evaluate_numeric({1, 3, 5}), 13);
  EXPECT_EQ(PreparedIndex("2*i+j", &middle).evaluate_numeric({1, 3, 5}), 5);
}

TEST(AffineExpressionTest, RejectsInt128AccumulationOverflowBeforeCancellation)
{
  const LoopScope a{"a", 0}, b{"b", 1, &a}, c{"c", 2, &b}, d{"d", 3, &c},
    e{"e", 4, &d}, f{"f", 5, &e};
  const auto max = std::numeric_limits<std::int64_t>::max();
  const PreparedIndex index(
    "9223372036854775807*a+9223372036854775807*b+9223372036854775807*c+"
    "9223372036854775807*d+9223372036854775807*e+9223372036854775807*f",
    &f);
  EXPECT_FALSE(index.evaluate_numeric({max, max, max, -max, -max, -max}));
  EXPECT_EQ(index.evaluate_numeric({1, 1, 1, -1, -1, -1}), 0);
}

TEST(AffineExpressionTest, MatchesIndependentSmallSignedArithmetic)
{
  const LoopScope outer{"i", 0}, inner{"j", 1, &outer};
  const PreparedIndex index("3*i-2*j+7", &inner);
  for (std::int64_t i = -12; i <= 12; ++i)
    for (std::int64_t j = -12; j <= 12; ++j)
    {
      SCOPED_TRACE(std::to_string(i) + "," + std::to_string(j));
      EXPECT_EQ(index.evaluate_numeric({i, j}), 3 * i - 2 * j + 7);
      EXPECT_EQ(index.evaluate({i, j}), std::to_string(3 * i - 2 * j + 7));
    }
}

}  // namespace
}  // namespace yarda::detail
