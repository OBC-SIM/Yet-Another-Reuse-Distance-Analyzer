#include "cache_line.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace yarda::detail
{
namespace
{

std::int64_t floor_divide(std::int64_t dividend, std::int64_t divisor)
{
  auto quotient = dividend / divisor;
  if (dividend % divisor != 0 && dividend < 0)
  {
    --quotient;
  }
  return quotient;
}

}  // namespace

std::vector<std::string> trace_cache_line_keys(
  const nlohmann::json & node, const std::vector<std::string> & indices,
  std::size_t line_size, const AccessLayoutResolver & layouts)
{
  if (line_size == 0 || line_size > static_cast<std::size_t>(
                                      std::numeric_limits<std::int64_t>::max()))
  {
    return {};
  }
  const auto access = layouts.resolve(node, indices);
  if (!access)
  {
    return {};
  }
  std::int64_t last_byte = 0;
  if (__builtin_add_overflow(access->offset, access->size - 1, &last_byte))
  {
    return {};
  }
  const auto divisor = static_cast<std::int64_t>(line_size);
  const auto first_line = floor_divide(access->offset, divisor);
  const auto last_line = floor_divide(last_byte, divisor);
  const auto object = node.value("object", std::string{});
  const auto key_base =
    object.empty() ? node.value("name", std::string{}) : object;
  std::vector<std::string> keys;
  for (auto line = first_line;; ++line)
  {
    keys.push_back(key_base + "-line-" + std::to_string(line));
    if (line == last_line)
    {
      break;
    }
  }
  return keys;
}

}  // namespace yarda::detail
