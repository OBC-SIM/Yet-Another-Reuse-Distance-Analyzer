#include "yarda/cache/line_mapping_json.hpp"

#include <string>

namespace yarda
{
namespace
{

std::string address_basis_name(AddressBasis basis)
{
  return basis == AddressBasis::ImageRelative ? "image-relative" : "absolute";
}

}  // namespace

nlohmann::json cache_line_mapping_json(const CacheLineMappingTable & mappings)
{
  auto result = nlohmann::json::array();
  for (const auto & [key, mapping] : mappings)
  {
    static_cast<void>(key);
    result.push_back({
      {"object", mapping.object_id},
      {"object_offset", mapping.object_byte_offset},
      {"address_basis", address_basis_name(mapping.address_basis)},
      {"address", mapping.decoded.address},
      {"block_number", mapping.decoded.block_number},
      {"tag", mapping.decoded.tag},
      {"index", mapping.decoded.set_index},
      {"offset", mapping.decoded.line_offset},
    });
  }
  return result;
}

}  // namespace yarda
