#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <vector>

#include "cache_line.hpp"
#include "yarda/trace/trace.hpp"

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

class MappedTraceUnroller
{
public:
  MappedTraceUnroller(const CacheGeometry & geometry,
                      const ObjectAddressModel & objects,
                      const AccessLayoutResolver & layouts);

  std::vector<CacheLineMapping> unroll(const nlohmann::json & node) const;

private:
  CacheLineMapper mapper_;
};

}  // namespace yarda::detail
