#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#include "yarda/trace/trace_coverage.hpp"

namespace yarda
{

/** @brief Machine-readable class of a strict access-resolution failure. */
enum class ResolutionCategory
{
  /** The access is well identified but outside the supported analysis model. */
  Unsupported,
  /** Required operation, layout, object, or linked-range data is unavailable.
   */
  Unresolved,
};

/** @brief Categorized strict-resolution error with source provenance. */
class ResolutionError : public std::invalid_argument
{
public:
  /**
   * @brief Construct a categorized access-resolution failure.
   *
   * @param category Stable machine-readable failure category.
   * @param task_id Analyzed root containing the rejected access.
   * @param source_access_ordinal Zero-based source position in its trace scope.
   * @param object_id Canonical object identity, or an empty string if absent.
   * @param coverage Coverage snapshot including the rejected access.
   * @param message Human-readable failure description.
   */
  ResolutionError(ResolutionCategory category, std::string task_id,
                  std::uint64_t source_access_ordinal, std::string object_id,
                  TraceCoverage coverage, std::string message)
    : std::invalid_argument(std::move(message))
    , category_(category)
    , task_id_(std::move(task_id))
    , source_access_ordinal_(source_access_ordinal)
    , object_id_(std::move(object_id))
    , coverage_(coverage)
  {
  }

  /**
   * @brief Return the stable failure category.
   *
   * @return Unsupported or unresolved failure classification.
   */
  [[nodiscard]] ResolutionCategory category() const noexcept
  {
    return category_;
  }

  /**
   * @brief Return the analyzed root containing the rejected access.
   *
   * @return Borrowed task identifier valid for this error's lifetime.
   */
  [[nodiscard]] const std::string & task_id() const noexcept
  {
    return task_id_;
  }

  /**
   * @brief Return the rejected access's source emission position.
   *
   * @return Module-wide ordinal for block APIs or task-local ordinal for task
   * APIs.
   */
  [[nodiscard]] std::uint64_t source_access_ordinal() const noexcept
  {
    return source_access_ordinal_;
  }

  /**
   * @brief Return the rejected access's canonical object identity.
   *
   * @return Borrowed object ID, empty when the LAT access has none.
   */
  [[nodiscard]] const std::string & object_id() const noexcept
  {
    return object_id_;
  }

  /**
   * @brief Return coverage accumulated through the rejected access.
   *
   * For task APIs the snapshot aggregates all selected tasks visited through
   * the failure, even though `source_access_ordinal()` is task-local.
   *
   * @return Borrowed coverage snapshot valid for this error's lifetime.
   */
  [[nodiscard]] const TraceCoverage & coverage() const noexcept
  {
    return coverage_;
  }

private:
  ResolutionCategory category_;
  std::string task_id_;
  std::uint64_t source_access_ordinal_;
  std::string object_id_;
  TraceCoverage coverage_;
};

}  // namespace yarda
