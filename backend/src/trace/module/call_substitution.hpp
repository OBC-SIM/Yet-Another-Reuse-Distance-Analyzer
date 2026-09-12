#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>

namespace yarda::detail
{

using CallMapping = std::unordered_map<std::string, std::string>;

/**
 * @brief Apply inline actual bindings to a copied node and its nested body.
 * @param node Owned node; array indices and path values use affine composition.
 * @param names Borrowed simultaneous formal-to-actual bindings.
 * @param object_ids Borrowed canonical formal-to-actual object identities.
 * @param objects Borrowed metadata for actual storage objects.
 * @return Rebound node; invalid affine composition remains deferred text.
 */
nlohmann::json substitute_call_node(nlohmann::json node,
                                    const CallMapping & names,
                                    const CallMapping & object_ids,
                                    const nlohmann::json & objects);

}  // namespace yarda::detail
