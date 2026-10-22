#pragma once

#include <nlohmann/json.hpp>

namespace yarda::detail
{

// Access width belongs to the node; object metadata only supplies a fallback.
// A compatible actual object replaces Array extents during call binding.
inline void apply_object_metadata(nlohmann::json & node,
                                  const nlohmann::json & metadata,
                                  bool replace_shape = false)
{
  if (node.value("type", "") == "Array" && metadata.contains("shape") &&
      (replace_shape || !node.contains("shape")))
  {
    node["shape"] = metadata.at("shape");
  }
  if (!node.contains("elem_size") && metadata.contains("elem_size"))
  {
    node["elem_size"] = metadata.at("elem_size");
  }
}

}  // namespace yarda::detail
