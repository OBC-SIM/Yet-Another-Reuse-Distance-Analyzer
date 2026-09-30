#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace yarda::detail
{

/** @brief Normalized integer affine terms in canonical identifier order. */
struct AffineExpression
{
  std::int64_t constant = 0;
  std::map<std::string, std::int64_t> terms;
};

/**
 * @brief Parse the restricted MAP affine grammar without observing a failure.
 * @param text Borrowed sum of constants, identifiers and integer*identifier.
 * @return Normalized expression, or nullopt for invalid syntax/int64 overflow.
 */
std::optional<AffineExpression> parse_affine_expression(std::string_view text);

/**
 * @brief Serialize a normalized expression with its constant last.
 * @param expression Borrowed normalized terms without zero coefficients.
 * @return Canonical MAP string, including "0" for the zero expression.
 */
std::string format_affine_expression(const AffineExpression & expression);

/**
 * @brief Add a scaled expression with checked coefficient/constant arithmetic.
 * @param target Borrowed accumulator; discard it if this operation fails.
 * @param source Borrowed expression, distinct from target.
 * @param coefficient Signed int64 scale.
 * @return False on int64 multiply/add overflow; no exception is raised.
 */
bool add_affine_expression(AffineExpression & target,
                           const AffineExpression & source,
                           std::int64_t coefficient);

}  // namespace yarda::detail
