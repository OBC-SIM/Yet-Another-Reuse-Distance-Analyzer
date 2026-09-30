#pragma once

#include <nlohmann/json.hpp>

namespace yarda
{

/**
 * @brief Aggregate IR instructions without expanding memory or loop traces.
 * @param raw Borrowed MAP document with version 1 ir_instructions metadata.
 * @return Counts per opcode/function and totals for one invocation of each
 * analyzed root (or each non-inline definition when no root is annotated).
 * Counts exclude debug/lifetime intrinsics and callee bodies; PHIs and
 * terminators count as IR instructions, and each remaining call counts once.
 * @throws std::invalid_argument for missing, unsupported or malformed counts.
 * @throws std::overflow_error when a count does not fit uint64_t.
 */
nlohmann::json count_ir_instructions(const nlohmann::json & raw);

}  // namespace yarda
