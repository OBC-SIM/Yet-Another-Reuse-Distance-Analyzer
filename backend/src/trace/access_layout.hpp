#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace yarda::detail
{

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

private:
  nlohmann::json objects_;
  nlohmann::json structs_;
};

}  // namespace yarda::detail
