#include "access_resolver.hpp"

#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "yarda/trace/resolution_error.hpp"

namespace yarda::detail
{
namespace
{

using Json = nlohmann::json;

[[noreturn]] void reject(ResolutionCategory category,
                         const std::string & task_id,
                         std::uint64_t source_access_ordinal,
                         const std::string & object_id,
                         const std::string & reason, TraceCoverage & coverage)
{
  ++coverage.rejected_accesses;
  const auto label = object_id.empty() ? std::string("<unknown>") : object_id;
  const auto category_name =
    category == ResolutionCategory::Unsupported ? "unsupported" : "unresolved";
  throw ResolutionError(
    category, task_id, source_access_ordinal, object_id, coverage,
    category_name + std::string(" task access [task=") + task_id +
      ", ordinal=" + std::to_string(source_access_ordinal) +
      ", object=" + label + "]: " + reason);
}

std::optional<std::string> operation_name(const Json & node)
{
  if (!node.contains("op") || !node.at("op").is_string())
  {
    return std::nullopt;
  }
  return node.at("op").get<std::string>();
}

}  // namespace

ResolvedAccess resolve_access(const nlohmann::json & node,
                              PreparedAccess & prepared, bool exact_indices,
                              const std::string & task_id,
                              std::uint64_t source_access_ordinal,
                              const ObjectAddressModel & objects,
                              const AccessLayoutResolver & layouts,
                              TraceCoverage & coverage)
{
  PreparedAccessPlan candidate;
  auto & plan = prepared.plan ? *prepared.plan : candidate;
  if (!prepared.plan)
  {
    plan.object_id = node.contains("object") && node.at("object").is_string()
                       ? node.at("object").get<std::string>()
                       : std::string{};
    const auto & object_id = plan.object_id;
    if (object_id.rfind("global::", 0) != 0)
    {
      reject(ResolutionCategory::Unsupported, task_id, source_access_ordinal,
             object_id, "non-global storage is outside ELF task analysis",
             coverage);
    }
    if (node.value("type", "") == "Scalar" && node.contains("indices") &&
        (!node.at("indices").is_array() || !node.at("indices").empty()))
    {
      reject(ResolutionCategory::Unresolved, task_id, source_access_ordinal,
             object_id, "scalar access contains invalid indices", coverage);
    }
    const auto operation = operation_name(node);
    if (!operation)
    {
      reject(ResolutionCategory::Unresolved, task_id, source_access_ordinal,
             object_id, "load/store operation metadata is unavailable",
             coverage);
    }
    if (*operation == "load")
    {
      plan.operation = AccessOperation::Load;
    }
    else if (*operation == "store")
    {
      plan.operation = AccessOperation::Store;
    }
    else
    {
      reject(ResolutionCategory::Unsupported, task_id, source_access_ordinal,
             object_id, "unsupported memory operation: " + *operation,
             coverage);
    }
  }
  const auto & object_id = plan.object_id;
  if (!exact_indices)
  {
    reject(ResolutionCategory::Unsupported, task_id, source_access_ordinal,
           object_id, "runtime-dependent index cannot be resolved", coverage);
  }
  if (!prepared.plan)
  {
    const auto object_kind = layouts.object_kind(object_id);
    if (!object_kind)
    {
      reject(ResolutionCategory::Unresolved, task_id, source_access_ordinal,
             object_id, "object metadata is unavailable", coverage);
    }
    if (object_kind->empty())
    {
      reject(ResolutionCategory::Unresolved, task_id, source_access_ordinal,
             object_id, "object kind metadata is invalid", coverage);
    }
    if (*object_kind == "pointer")
    {
      reject(ResolutionCategory::Unsupported, task_id, source_access_ordinal,
             object_id, "pointer-backed storage is outside ELF task analysis",
             coverage);
    }
    if (*object_kind != "array" && *object_kind != "scalar" &&
        *object_kind != "struct")
    {
      reject(ResolutionCategory::Unsupported, task_id, source_access_ordinal,
             object_id, "object kind is not recognized: " + *object_kind,
             coverage);
    }
  }

  std::optional<ByteAccess> access;
  try
  {
    access = prepared.plan
               ? plan.layout.resolve(prepared.numeric_values, object_id)
               : layouts.prepare(node, prepared.numeric_values, plan.layout);
  }
  catch (const ResolutionError &)
  {
    throw;
  }
  catch (const std::invalid_argument & error)
  {
    reject(ResolutionCategory::Unresolved, task_id, source_access_ordinal,
           object_id, error.what(), coverage);
  }
  catch (const nlohmann::json::exception & error)
  {
    reject(ResolutionCategory::Unresolved, task_id, source_access_ordinal,
           object_id, error.what(), coverage);
  }
  if (!access)
  {
    reject(ResolutionCategory::Unresolved, task_id, source_access_ordinal,
           object_id, "access layout is unavailable", coverage);
  }
  if (access->offset < 0 || access->size <= 0)
  {
    reject(ResolutionCategory::Unresolved, task_id, source_access_ordinal,
           object_id, "access byte range is invalid", coverage);
  }

  const auto offset = static_cast<std::uint64_t>(access->offset);
  const auto size = static_cast<std::uint64_t>(access->size);
  if (!prepared.plan)
  {
    const auto object = objects.objects.find(object_id);
    if (object == objects.objects.end())
    {
      reject(ResolutionCategory::Unresolved, task_id, source_access_ordinal,
             object_id, "ELF object symbol is unavailable", coverage);
    }
    plan.object = object->second;
    plan.basis = objects.basis;
  }
  if (offset >= plan.object.size || size > plan.object.size - offset)
  {
    reject(ResolutionCategory::Unresolved, task_id, source_access_ordinal,
           object_id, "access exceeds ELF object extent", coverage);
  }
  if (plan.object.base > std::numeric_limits<std::uint64_t>::max() - offset)
  {
    reject(ResolutionCategory::Unresolved, task_id, source_access_ordinal,
           object_id, "ELF object address overflows", coverage);
  }
  const auto address = plan.object.base + offset;
  if (size - 1 > std::numeric_limits<std::uint64_t>::max() - address)
  {
    reject(ResolutionCategory::Unresolved, task_id, source_access_ordinal,
           object_id, "linked access range overflows", coverage);
  }

  ++coverage.resolved_accesses;
  ResolvedAccess result{object_id,
                        offset,
                        size,
                        address,
                        plan.basis,
                        plan.operation,
                        source_access_ordinal};
  if (!prepared.plan) prepared.plan = std::move(candidate);
  return result;
}

}  // namespace yarda::detail
