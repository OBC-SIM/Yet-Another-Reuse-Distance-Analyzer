#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace yarda::detail
{

struct PreparedLayout;

/** @brief Exact byte range selected by one LAT access node. */
struct ByteAccess
{
  std::int64_t offset;
  std::int64_t size;
};

bool has_field_path(const nlohmann::json & node);

std::optional<std::int64_t> parse_exact_integer(const std::string & value);

/** @brief Resolve ordered LAT access paths against ABI layout metadata. */
class AccessLayoutResolver
{
public:
  AccessLayoutResolver();
  explicit AccessLayoutResolver(const nlohmann::json & raw);

  std::optional<ByteAccess>
  resolve(const nlohmann::json & node,
          const std::vector<std::string> & indices) const;

  std::optional<std::string> object_kind(const std::string & object_id) const;

  /**
   * @brief Resolve the first numeric access while recording immutable layout.
   * @param node Borrowed immutable access after inline object binding.
   * @param indices Borrowed current numeric row.
   * @param plan Output scratch plan; usable only if this call succeeds.
   * @return Byte range, or no value when a legacy layout cannot be resolved.
   * @throws std::invalid_argument or JSON exceptions at existing layout checks.
   */
  std::optional<ByteAccess> prepare(const nlohmann::json & node,
                                    const std::vector<std::int64_t> & indices,
                                    PreparedLayout & plan) const;

private:
  nlohmann::json objects_;
  nlohmann::json structs_;
};

}  // namespace yarda::detail
