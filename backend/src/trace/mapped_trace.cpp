#include "yarda/trace/mapped_trace.hpp"

#include <vector>

namespace yarda
{

std::vector<CacheLineMapping>
flatten_mapped_traces(const std::vector<NamedMappedTrace> & traces)
{
  std::vector<CacheLineMapping> result;
  for (const auto & trace : traces)
  {
    result.insert(result.end(), trace.accesses.begin(), trace.accesses.end());
  }
  return result;
}

}  // namespace yarda
