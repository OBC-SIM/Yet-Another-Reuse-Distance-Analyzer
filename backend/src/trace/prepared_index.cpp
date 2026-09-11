#include "prepared_index.hpp"

#include <utility>

#include "access_layout.hpp"

namespace yarda::detail
{

std::optional<std::int64_t>
PreparedIndex::evaluate_numeric(const std::vector<std::int64_t> & values) const
{
  if (!slot_) return literal_;
  std::int64_t resolved = 0;
  if (__builtin_add_overflow(values[*slot_], offset_, &resolved))
    return literal_;
  return resolved;
}

namespace
{

std::optional<std::size_t> find_slot(std::string_view name,
                                     const LoopScope * scope)
{
  for (; scope; scope = scope->parent)
    if (scope->variable == name) return scope->slot;
  return std::nullopt;
}

}  // namespace

PreparedIndex::PreparedIndex(std::string expression, const LoopScope * scope)
  : expression_(std::move(expression))
{
  slot_ = find_slot(expression_, scope);
  if (slot_) return;
  literal_ = parse_exact_integer(expression_);
  const auto position = expression_.find_first_of("+-", 1);
  if (position == std::string::npos) return;
  const auto base =
    find_slot(std::string_view(expression_).substr(0, position), scope);
  if (!base) return;
  const auto offset = parse_exact_integer(expression_.substr(position));
  if (offset)
  {
    slot_ = base;
    offset_ = *offset;
  }
}

std::string
PreparedIndex::evaluate(const std::vector<std::int64_t> & values) const
{
  std::int64_t resolved = 0;
  if (slot_ && !__builtin_add_overflow(values[*slot_], offset_, &resolved))
    return std::to_string(resolved);
  return expression_;
}

}  // namespace yarda::detail
