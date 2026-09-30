#include "ir_report.hpp"

#include <stdexcept>

#include "yarda/trace/instruction_counts.hpp"

namespace yarda::cli
{
nlohmann::json optional_ir_report(const nlohmann::json & raw)
{
  std::string reason;
  try
  {
    return count_ir_instructions(raw);
  }
  catch (const std::invalid_argument & error)
  {
    reason = error.what();
  }
  catch (const std::overflow_error & error)
  {
    reason = error.what();
  }
  catch (const nlohmann::json::exception & error)
  {
    reason = error.what();
  }
  return {{"schema_version", 2},
          {"analysis", "ir-instructions"},
          {"status", "error"},
          {"reason", std::move(reason)}};
}
}  // namespace yarda::cli
