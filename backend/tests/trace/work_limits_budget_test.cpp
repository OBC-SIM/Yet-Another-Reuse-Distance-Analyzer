#include <limits>

#include "../../src/trace/expansion_budget.hpp"
#include "work_limits_test_support.hpp"

namespace
{

using yarda::detail::ExpansionBudget;
using yarda::test::work::expect_limit_error;

TEST(LoopWorkLimitsBudgetTest, FirstArgumentSetsStructuralNodeLimit)
{
  ExpansionBudget budget({2, 256});
  budget.consume_expanded_node();
  budget.consume_expanded_node();
  expect_limit_error([&] { budget.consume_expanded_node(); },
                     "inline call expansion exceeds 2 nodes");
}

TEST(LoopWorkLimitsBudgetTest, FirstArgumentSetsStructuralDepthLimit)
{
  ExpansionBudget budget({100'000, 1});
  EXPECT_NO_THROW(budget.validate_inline_call_depth(1));
  expect_limit_error([&] { budget.validate_inline_call_depth(2); },
                     "inline call depth exceeds 1");
}

TEST(LoopWorkLimitsBudgetTest, StructuralOverridesKeepDefaultLoopAllowance)
{
  ExpansionBudget budget({100'000, 256});
  EXPECT_NO_THROW(budget.consume_loop_iterations(300));
}

TEST(LoopWorkLimitsBudgetTest, AllowsExactUint64CumulativeReservation)
{
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  ExpansionBudget budget(yarda::detail::kExpansionLimits, {maximum, maximum});
  budget.consume_loop_iterations(maximum - 1);
  EXPECT_NO_THROW(budget.consume_loop_iterations(1));
  EXPECT_NO_THROW(budget.consume_loop_iterations(0));
}

TEST(LoopWorkLimitsBudgetTest, FullUint64AllowanceRejectsCumulativeOverflow)
{
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  ExpansionBudget budget(yarda::detail::kExpansionLimits, {maximum, maximum});
  budget.consume_loop_iterations(maximum);
  expect_limit_error([&] { budget.consume_loop_iterations(1); },
                     "cumulative loop iteration count exceeds "
                     "18446744073709551615");
}

}  // namespace
