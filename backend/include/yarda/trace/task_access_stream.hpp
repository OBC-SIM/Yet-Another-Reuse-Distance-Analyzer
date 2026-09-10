#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "yarda/trace/emission_budget.hpp"
#include "yarda/trace/resolved_access.hpp"
#include "yarda/trace/work_limits.hpp"

namespace yarda
{

/**
 * @brief Synchronous consumer of task boundaries and resolved source accesses.
 *
 * All callbacks are required and borrowed for the duration of streaming. Their
 * argument references are valid only during that callback; copy retained data.
 * A callback exception stops all delivery and propagates unchanged. Consumers
 * must discard partial state on failure, including previously completed tasks.
 */
struct TaskAccessSink
{
  /** @brief Begin a task, reporting its expanded static opaque-call count. */
  std::function<void(const std::string &, std::uint64_t)> begin_task;
  /** @brief Receive one resolved access in task-local ordinal order. */
  std::function<void(const std::string &, const ResolvedAccess &)> access;
  /** @brief End a successfully delivered task with its source coverage. */
  std::function<void(const std::string &, const TraceCoverage &)> end_task;
};

/** @brief Aggregate source coverage from a completely delivered module. */
struct TaskAccessStreamResult
{
  /** @brief Source-only coverage; emitted_line_references remains zero. */
  TraceCoverage coverage;
  /** @brief Sum of task-local expanded static opaque-call counts. */
  std::uint64_t excluded_opaque_call_sites = 0;
};

/**
 * @brief Deliver analyzed tasks without materializing their resolved traces.
 *
 * Task selection, inline expansion, opaque-call counting and resolution match
 * resolved_task_traces(). Empty tasks still receive begin/end callbacks.
 * Source ordinals restart per task. Opaque calls are reported, allowing a
 * hierarchy consumer to reject them at begin_task. Input DOM and structurally
 * expanded calls remain in memory; concrete loop traces are emitted one by one.
 *
 * @param raw Borrowed APE v2 LAT module, unchanged throughout the call.
 * @param objects Borrowed linked object model, unchanged throughout the call.
 * @param sink Borrowed required callbacks, configured before streaming.
 * @param budget Borrowed module budget, also usable by a budgeted line sink.
 * @param loop_limits Borrowed inclusive limits, copied for this invocation.
 * @return Aggregate source coverage and exclusions only on complete success.
 * @throws std::invalid_argument for invalid callbacks, malformed LAT, rejected
 * call expansion or exhausted structural, loop or emission limits.
 * @throws ResolutionError for unsupported or unresolved source accesses.
 * @note Any sink exception propagates unchanged. No callbacks follow failure;
 * an interrupted task receives no successful end notification.
 */
TaskAccessStreamResult stream_resolved_task_accesses(
  const nlohmann::json & raw, const ObjectAddressModel & objects,
  const TaskAccessSink & sink, TraceEmissionBudget & budget,
  const LoopWorkLimits & loop_limits);

/**
 * @brief Stream sources with default single and cumulative loop allowances.
 * @param raw Borrowed APE v2 LAT module, unchanged throughout the call.
 * @param objects Borrowed linked object model, unchanged throughout the call.
 * @param sink Borrowed required callbacks with the same lifetime contract.
 * @param budget Borrowed module emission budget, also shared with line mapping.
 * @return Aggregate source coverage and exclusions only on complete success.
 * @throws std::invalid_argument for invalid input, callbacks or exhausted
 * limits.
 * @throws ResolutionError for unsupported or unresolved source accesses.
 * @note Each loop allowance defaults to 1,000,000. Sink exceptions propagate
 * unchanged; callers must discard partial state from every task on failure.
 */
TaskAccessStreamResult stream_resolved_task_accesses(
  const nlohmann::json & raw, const ObjectAddressModel & objects,
  const TaskAccessSink & sink, TraceEmissionBudget & budget);

/**
 * @brief Stream sources with a fresh default emission budget.
 * @param raw Borrowed APE v2 LAT module.
 * @param objects Borrowed linked object model.
 * @param sink Borrowed required callbacks with the same lifetime contract.
 * @return Aggregate source coverage and exclusions on complete success.
 * @throws std::invalid_argument for invalid input, callbacks or exhausted
 * limits.
 * @throws ResolutionError for unsupported or unresolved source accesses.
 * @note Use the budget overload to share cumulative limits with line mapping.
 */
TaskAccessStreamResult stream_resolved_task_accesses(
  const nlohmann::json & raw, const ObjectAddressModel & objects,
  const TaskAccessSink & sink);

}  // namespace yarda
