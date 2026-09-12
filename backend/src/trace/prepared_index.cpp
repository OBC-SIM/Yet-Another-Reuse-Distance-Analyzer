#include "prepared_index.hpp"

#include <limits>
#include <utility>

#include "access_layout.hpp"
#include "affine_expression.hpp"

namespace yarda::detail
{

std::optional<std::int64_t>
PreparedIndex::evaluate_numeric(const std::vector<std::int64_t> & values) const
{
  if (constant_) return evaluate_affine(values);
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
  if (position != std::string::npos)
  {
    const auto base =
      find_slot(std::string_view(expression_).substr(0, position), scope);
    const auto offset = parse_exact_integer(expression_.substr(position));
    if (base && offset)
    {
      slot_ = base;
      offset_ = *offset;
      return;
    }
  }
  if (literal_) return;
  const auto affine = parse_affine_expression(expression_);
  if (!affine) return;
  for (const auto & [name, coefficient] : affine->terms)
  {
    const auto slot = find_slot(name, scope);
    if (!slot) return;
    terms_.emplace_back(*slot, coefficient);
  }
  constant_ = affine->constant;
}

std::optional<std::int64_t>
PreparedIndex::evaluate_affine(const std::vector<std::int64_t> & values) const
{
  __int128_t total = *constant_;
  for (const auto & [slot, coefficient] : terms_)
  {
    const auto product = static_cast<__int128_t>(coefficient) * values[slot];
    if (__builtin_add_overflow(total, product, &total)) return std::nullopt;
  }
  if (total < std::numeric_limits<std::int64_t>::min() ||
      total > std::numeric_limits<std::int64_t>::max())
    return std::nullopt;
  return static_cast<std::int64_t>(total);
}

std::string
PreparedIndex::evaluate(const std::vector<std::int64_t> & values) const
{
  if (constant_)
    if (const auto value = evaluate_affine(values))
      return std::to_string(*value);
  std::int64_t resolved = 0;
  if (slot_ && !__builtin_add_overflow(values[*slot_], offset_, &resolved))
    return std::to_string(resolved);
  return expression_;
}

}  // namespace yarda::detail
