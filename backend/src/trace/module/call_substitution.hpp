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
 * @param objects Borrowed metadata for formal and actual storage objects.
 * @return Rebound node; invalid affine composition remains deferred text.
 * @throws std::invalid_argument for incompatible Array element sizes, row
 * dimensions, or structured root layouts. Validation is eager even for
 * nodes inside zero-trip loops; diagnostics identify formal and actual objects.
 */
nlohmann::json substitute_call_node(nlohmann::json node,
                                    const CallMapping & names,
                                    const CallMapping & object_ids,
                                    const nlohmann::json & objects);

}  // namespace yarda::detail
