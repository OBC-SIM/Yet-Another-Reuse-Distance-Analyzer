#include "access_resolver.hpp"

#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

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

bool has_runtime_dependent_index(const std::vector<std::string> & indices)
{
  for (const auto & index : indices)
  {
    if (!parse_exact_integer(index))
    {
      return true;
    }
  }
  return false;
}

}  // namespace

ResolvedAccess resolve_access(const nlohmann::json & node,
                              const std::vector<std::string> & indices,
                              const std::string & task_id,
                              std::uint64_t source_access_ordinal,
                              const ObjectAddressModel & objects,
                              const AccessLayoutResolver & layouts,
                              TraceCoverage & coverage)
{
  const auto object_id =
    node.contains("object") && node.at("object").is_string()
      ? node.at("object").get<std::string>()
      : std::string{};
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
           object_id, "load/store operation metadata is unavailable", coverage);
  }
  AccessOperation resolved_operation = AccessOperation::Unknown;
  if (*operation == "load")
  {
    resolved_operation = AccessOperation::Load;
  }
  else if (*operation == "store")
  {
    resolved_operation = AccessOperation::Store;
  }
  else
  {
    reject(ResolutionCategory::Unsupported, task_id, source_access_ordinal,
           object_id, "unsupported memory operation: " + *operation, coverage);
  }
  if (has_runtime_dependent_index(indices))
  {
    reject(ResolutionCategory::Unsupported, task_id, source_access_ordinal,
           object_id, "runtime-dependent index cannot be resolved", coverage);
  }
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

  std::optional<ByteAccess> access;
  try
  {
    access = layouts.resolve(node, indices);
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
  const auto object = objects.objects.find(object_id);
  if (object == objects.objects.end())
  {
    reject(ResolutionCategory::Unresolved, task_id, source_access_ordinal,
           object_id, "ELF object symbol is unavailable", coverage);
  }
  if (offset >= object->second.size || size > object->second.size - offset)
  {
    reject(ResolutionCategory::Unresolved, task_id, source_access_ordinal,
           object_id, "access exceeds ELF object extent", coverage);
  }
  if (object->second.base > std::numeric_limits<std::uint64_t>::max() - offset)
  {
    reject(ResolutionCategory::Unresolved, task_id, source_access_ordinal,
           object_id, "ELF object address overflows", coverage);
  }
  const auto address = object->second.base + offset;
  if (size - 1 > std::numeric_limits<std::uint64_t>::max() - address)
  {
    reject(ResolutionCategory::Unresolved, task_id, source_access_ordinal,
           object_id, "linked access range overflows", coverage);
  }

  ++coverage.resolved_accesses;
  return ResolvedAccess{object_id,
                        offset,
                        size,
                        address,
                        objects.basis,
                        resolved_operation,
                        source_access_ordinal};
}

}  // namespace yarda::detail
