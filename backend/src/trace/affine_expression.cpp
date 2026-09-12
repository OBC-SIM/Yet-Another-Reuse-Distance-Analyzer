#include "affine_expression.hpp"

#include <limits>

namespace yarda::detail
{
namespace
{

bool digit(char c) { return c >= '0' && c <= '9'; }
bool identifier_start(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool add_scaled(std::int64_t & target, std::int64_t value,
                std::int64_t coefficient)
{
  std::int64_t product;
  return !__builtin_mul_overflow(value, coefficient, &product) &&
         !__builtin_add_overflow(target, product, &target);
}

std::string signed_term(std::int64_t value, const std::string & variable,
                        bool first)
{
  const auto magnitude = value < 0 ? 0 - static_cast<std::uint64_t>(value)
                                   : static_cast<std::uint64_t>(value);
  std::string result = value < 0 ? "-" : (first ? "" : "+");
  if (variable.empty() || magnitude != 1)
    result += std::to_string(magnitude) + (variable.empty() ? "" : "*");
  return result + variable;
}

}  // namespace

bool add_affine_expression(AffineExpression & target,
                           const AffineExpression & source,
                           std::int64_t coefficient)
{
  if (!add_scaled(target.constant, source.constant, coefficient)) return false;
  for (const auto & [name, value] : source.terms)
  {
    auto & term = target.terms[name];
    if (!add_scaled(term, value, coefficient)) return false;
    if (term == 0) target.terms.erase(name);
  }
  return true;
}

std::optional<AffineExpression> parse_affine_expression(std::string_view text)
{
  if (text.empty()) return std::nullopt;
  AffineExpression result;
  std::size_t position = 0;
  while (position < text.size())
  {
    bool negative = false;
    if (text[position] == '+' || text[position] == '-')
      negative = text[position++] == '-';
    else if (position != 0)
      return std::nullopt;
    if (position == text.size()) return std::nullopt;
    std::int64_t coefficient = negative ? -1 : 1;
    const bool numeric = digit(text[position]);
    if (numeric)
    {
      const std::uint64_t limit =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) +
        (negative ? 1U : 0U);
      std::uint64_t magnitude = 0;
      while (position < text.size() && digit(text[position]))
      {
        const auto value = static_cast<unsigned>(text[position++] - '0');
        if (magnitude > (limit - value) / 10) return std::nullopt;
        magnitude = magnitude * 10 + value;
      }
      coefficient = negative && magnitude != 0
                      ? -static_cast<std::int64_t>(magnitude - 1) - 1
                      : static_cast<std::int64_t>(magnitude);
    }
    AffineExpression term;
    const bool product =
      numeric && position < text.size() && text[position] == '*';
    if (product) ++position;
    if (!numeric || product)
    {
      if (position == text.size() || !identifier_start(text[position]))
        return std::nullopt;
      const auto begin = position++;
      while (position < text.size() &&
             (identifier_start(text[position]) || digit(text[position])))
        ++position;
      term.terms.emplace(std::string(text.substr(begin, position - begin)),
                         coefficient);
    }
    else
      term.constant = coefficient;
    if (!add_affine_expression(result, term, 1)) return std::nullopt;
  }
  return result;
}

std::string format_affine_expression(const AffineExpression & expression)
{
  std::string result;
  for (const auto & [name, coefficient] : expression.terms)
    result += signed_term(coefficient, name, result.empty());
  if (expression.constant != 0 || result.empty())
    result += signed_term(expression.constant, "", result.empty());
  return result;
}

}  // namespace yarda::detail
