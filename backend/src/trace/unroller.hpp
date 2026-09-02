#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "cache_line.hpp"
#include "yarda/trace/resolved_access.hpp"
#include "yarda/trace/trace.hpp"

namespace yarda::detail
{

// TODO(feat/cpp-elf-cli-mapping A2): Remove this compatibility policy when
// unmapped task accesses are rejected instead of omitted.
enum class ScalarAccessPolicy
{
  Include,
  Omit,
};

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
  ResolvedTraceUnroller(
    const ObjectAddressModel & objects, const AccessLayoutResolver & layouts,
    ScalarAccessPolicy scalar_policy = ScalarAccessPolicy::Include);

  std::vector<ResolvedAccess> unroll(const nlohmann::json & node);

private:
  const ObjectAddressModel & objects_;
  const AccessLayoutResolver & layouts_;
  ScalarAccessPolicy scalar_policy_;
  std::uint64_t next_source_access_ordinal_ = 0;
};

}  // namespace yarda::detail
