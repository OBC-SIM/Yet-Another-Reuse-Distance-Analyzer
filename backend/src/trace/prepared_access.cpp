#include "prepared_access.hpp"

namespace yarda::detail
{

bool PreparedAccess::evaluate_numeric(const std::vector<std::int64_t> & slots)
{
  numeric_values.resize(indices.size());
  bool exact = true;
  for (std::size_t i = 0; i < indices.size(); ++i)
  {
    const auto value = indices[i].evaluate_numeric(slots);
    exact = exact && value.has_value();
    numeric_values[i] = value.value_or(0);
  }
  return exact;
}

}  // namespace yarda::detail
