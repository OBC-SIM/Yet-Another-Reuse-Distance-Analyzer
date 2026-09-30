#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace yarda::detail::structured
{

/** @brief Current ABI type while walking one ordered structured access. */
struct TypeState
{
  std::string kind;
  std::vector<std::int64_t> shape;
  std::string element_type;
  std::int64_t element_size;
};

/**
 * @brief Reject a structured layout with its existing diagnostic prefix.
 * @param object_id Borrowed storage identity.
 * @param reason Borrowed failure description.
 * @return Never returns; throws std::invalid_argument.
 */
[[noreturn]] void reject(const std::string & object_id,
                         const std::string & reason);

/**
 * @brief Read an ABI integer without silently accepting other JSON types.
 * @param value Borrowed metadata object.
 * @param key Non-null borrowed field name.
 * @param object_id Borrowed diagnostic identity.
 * @return Signed metadata integer, or throws for missing/invalid fields.
 */
std::int64_t required_integer(const nlohmann::json & value, const char * key,
                              const std::string & object_id);

/**
 * @brief Validate and own one object or field type's layout metadata.
 * @param metadata Borrowed object or field metadata.
 * @param object_id Borrowed diagnostic identity.
 * @return Type state with positive element size and dimensions.
 */
TypeState type_state(const nlohmann::json & metadata,
                     const std::string & object_id);

/**
 * @brief Compute a type extent with the original checked multiplication order.
 * @param state Borrowed validated type state.
 * @param object_id Borrowed diagnostic identity.
 * @return Byte extent, or throws on overflow.
 */
std::int64_t type_extent(const TypeState & state,
                         const std::string & object_id);

/**
 * @brief Require matching ABI structure size and field name/index identity.
 * @param structures Borrowed immutable structure metadata.
 * @param state Borrowed current leaf type.
 * @param segment Borrowed field path segment.
 * @param object_id Borrowed diagnostic identity.
 * @return Field borrowed from structures; never retained by a prepared plan.
 */
const nlohmann::json & find_field(const nlohmann::json & structures,
                                  const TypeState & state,
                                  const nlohmann::json & segment,
                                  const std::string & object_id);

}  // namespace yarda::detail::structured
