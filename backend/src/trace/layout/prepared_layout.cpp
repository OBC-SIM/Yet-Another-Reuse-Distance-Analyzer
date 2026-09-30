#include "prepared_layout.hpp"

#include <stdexcept>

namespace yarda::detail
{
namespace
{

[[noreturn]] void reject(const std::string & object_id, const char * reason)
{
  throw std::invalid_argument(std::string("structured access ") + reason +
                              ": " + object_id);
}

}  // namespace

std::optional<ByteAccess>
PreparedLayout::resolve(const std::vector<std::int64_t> & indices,
                        const std::string & object_id) const
{
  std::int64_t offset = 0;
  for (const auto & step : steps)
  {
    auto amount = step.stride;
    if (step.index)
    {
      const auto index = indices[*step.index];
      if (structured)
      {
        if (index < 0)
          reject(object_id, "index is not an exact non-negative integer");
        if (index >= step.dimension)
          reject(object_id, "index exceeds its dimension");
      }
      if (__builtin_mul_overflow(index, step.stride, &amount))
      {
        if (structured) reject(object_id, "indexed byte offset overflows");
        return std::nullopt;
      }
    }
    if (__builtin_add_overflow(offset, amount, &offset))
    {
      if (structured) reject(object_id, "byte offset overflows");
      return std::nullopt;
    }
  }
  if (!structured)
  {
    if (__builtin_mul_overflow(offset, width, &offset)) return std::nullopt;
  }
  else
  {
    std::int64_t end = 0;
    if (__builtin_add_overflow(offset, width, &end) || end > extent)
      reject(object_id, "exceeds the object extent");
  }
  return ByteAccess{offset, width};
}

}  // namespace yarda::detail
