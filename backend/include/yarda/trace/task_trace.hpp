#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "yarda/trace/trace_coverage.hpp"

namespace yarda
{

/**
 * @brief Ordered accesses and coverage belonging to one analyzed task.
 *
 * @tparam Access Resolved source-access or mapped cache-line record type.
 */
template <typename Access>
struct TaskTrace
{
  /** @brief Stable identity of the analyzed root function. */
  std::string task_id;
  /** @brief Accesses in the concrete order emitted for this task. */
  std::vector<Access> accesses;
  /** @brief Source-to-output coverage restricted to this task. */
  TraceCoverage coverage;
  /** @brief Expanded static call sites omitted as known opaque calls. */
  std::uint64_t excluded_opaque_call_sites = 0;
};

}  // namespace yarda
