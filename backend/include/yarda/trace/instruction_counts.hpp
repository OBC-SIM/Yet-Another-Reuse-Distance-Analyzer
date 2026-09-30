#pragma once

#include <nlohmann/json.hpp>

namespace yarda
{

/**
 * @brief Aggregate IR instructions without expanding memory or loop traces.
 *
 * @param raw Borrowed MAP document with version 2 ir_instructions metadata.
 * Version 1 is accepted only for functions without calls.
 * @return Counts per opcode/function and totals for one invocation of each
 * analyzed root (or each non-inline definition when no root is annotated).
 * Counts exclude PHIs and debug/lifetime intrinsics, and include inline callee
 * bodies recursively, weighted by call-site executions. Other callees contribute
 * only the call instruction. Static counts expand each inline call site once.
 * Per-function blocks and self counts remain exclusive of callees.
 * @throws std::invalid_argument for missing/unsupported counts or inline cycles.
 * @throws nlohmann::json::exception for malformed metadata.
 * @throws std::overflow_error when a count does not fit uint64_t.
 */
nlohmann::json count_ir_instructions(const nlohmann::json & raw);

}  // namespace yarda
