#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <vector>

#include "cache_line.hpp"
#include "yarda/trace/resolved_access.hpp"
#include "yarda/trace/trace.hpp"
#include "yarda/trace/trace_coverage.hpp"

namespace yarda::detail
{

class TraceUnroller
{
public:
  TraceUnroller(Granularity granularity, std::size_t cache_line_size,
                const AccessLayoutResolver & layouts);

  std::vector<std::string> unroll(const nlohmann::json & node) const;

private:
  Granularity granularity_;
  std::size_t cache_line_size_;
  const AccessLayoutResolver & layouts_;
};

class ResolvedTraceUnroller
{
public:
  ResolvedTraceUnroller(const ObjectAddressModel & objects,
                        const AccessLayoutResolver & layouts);

  std::vector<ResolvedAccess> unroll(const nlohmann::json & node,
                                     const std::string & task_id);

  const TraceCoverage & coverage() const noexcept;

private:
  const ObjectAddressModel & objects_;
  const AccessLayoutResolver & layouts_;
  TraceCoverage coverage_;
};

}  // namespace yarda::detail
