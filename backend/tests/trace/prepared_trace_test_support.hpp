#pragma once

#include "../../src/trace/prepared_trace.hpp"
#include "task_access_stream_test_support.hpp"

namespace yarda::test::prepared
{

using namespace ::yarda::test::stream;
using IndexRows = std::vector<std::vector<std::string>>;

/** @brief Collect only the small expected index sequences in unit fixtures. */
inline IndexRows indices(const Json & node, LoopWorkLimits limits = {})
{
  detail::ExpansionBudget budget(detail::kExpansionLimits, limits);
  IndexRows result;
  detail::visit_prepared_trace(
    node, budget, [&](const Json &, const std::vector<std::string> & values) {
      result.push_back(values);
    });
  return result;
}

/** @brief Require all resolved fields against independently calculated rows. */
inline void expect_offsets(const ResolvedTaskTrace & task,
                           const std::vector<std::uint64_t> & offsets)
{
  ASSERT_EQ(task.accesses.size(), offsets.size());
  for (std::size_t i = 0; i < offsets.size(); ++i)
  {
    SCOPED_TRACE(i);
    expect_access(task.accesses[i],
                  {"global::A", offsets[i], 4, 0x101c + offsets[i],
                   AddressBasis::Absolute, AccessOperation::Load, i});
  }
}

}  // namespace yarda::test::prepared
