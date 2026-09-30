#pragma once

#include <nlohmann/json.hpp>

namespace yarda::cli
{
/**
 * @brief Capture IR analysis failure without discarding a combined cache
 * result.
 *
 * @param raw Borrowed MAP document.
 * @return Exact IR counts, or status "error" with a reason and no totals.
 * @note Only IR input/count errors are captured; other failures propagate.
 */
nlohmann::json optional_ir_report(const nlohmann::json & raw);
}  // namespace yarda::cli
