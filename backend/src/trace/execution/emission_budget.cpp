#include "yarda/trace/emission_budget.hpp"

#include <stdexcept>
#include <string>

namespace yarda
{

TraceEmissionBudget::TraceEmissionBudget(TraceEmissionLimits limits)
  : limits_(limits)
{
}

void TraceEmissionBudget::consume_source_access()
{
  if (source_accesses_ >= limits_.emitted_source_accesses)
  {
    throw std::invalid_argument(
      "emitted source accesses exceeds " +
      std::to_string(limits_.emitted_source_accesses));
  }
  ++source_accesses_;
}

void TraceEmissionBudget::consume_line_reference()
{
  if (line_references_ >= limits_.emitted_line_references)
  {
    throw std::invalid_argument(
      "emitted line references exceeds " +
      std::to_string(limits_.emitted_line_references));
  }
  ++line_references_;
}

}  // namespace yarda
