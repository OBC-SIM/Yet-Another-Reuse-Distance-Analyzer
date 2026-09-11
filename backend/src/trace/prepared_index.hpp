#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace yarda::detail
{

/** @brief Borrow lexical bindings only while preparing a reached node. */
struct LoopScope
{
  std::string_view variable;
  std::size_t slot;
  const LoopScope * parent = nullptr;
};

/** @brief Bind supported expressions once, retaining legacy fallback text. */
class PreparedIndex
{
public:
  /**
   * @brief Bind the nearest matching loop variable without reading its value.
   * @param expression Owned original spelling, also used on evaluation failure.
   * @param scope Borrowed nullable lexical scope; never retained.
   */
  PreparedIndex(std::string expression, const LoopScope * scope);

  /**
   * @brief Substitute a slot value with checked addition at the access site.
   * @param values Borrowed current loop values from the owning traversal.
   * @return Substituted integer, or original text for fixed/unsupported input
   * and overflow. The existing consumer decides whether to reject that text.
   * @pre Every slot bound during preparation exists in values.
   */
  std::string evaluate(const std::vector<std::int64_t> & values) const;

private:
  std::string expression_;
  std::optional<std::size_t> slot_;
  std::int64_t offset_ = 0;
};

}  // namespace yarda::detail
