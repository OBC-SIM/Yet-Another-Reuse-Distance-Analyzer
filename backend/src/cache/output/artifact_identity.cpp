#include "yarda/cache/artifact_identity.hpp"

#include <array>
#include <fstream>
#include <stdexcept>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA256.h>

#include "artifact_contract.hpp"

namespace yarda
{
namespace
{

std::string finish_hash(llvm::SHA256 & hash)
{
  constexpr char digits[] = "0123456789abcdef";
  const auto bytes = hash.final();
  std::string result;
  result.reserve(64);
  for (const auto byte : bytes)
  {
    const auto value = static_cast<unsigned char>(byte);
    result.push_back(digits[value >> 4]);
    result.push_back(digits[value & 15]);
  }
  return result;
}

void validate_options(const nlohmann::json & value)
{
  if (value.is_object() || value.is_array())
  {
    for (const auto & child : value)
      validate_options(child);
    return;
  }
  if (!value.is_number_integer() && !value.is_boolean() && !value.is_string())
    throw std::invalid_argument(
        "semantic option leaves must be integers, booleans or strings");
}

} // namespace

std::string sha256_file_bytes(const std::string & path)
{
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("cannot open hash input: " + path);
  llvm::SHA256 hash;
  std::array<char, 65536> buffer;
  while (input)
  {
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    hash.update(llvm::StringRef(buffer.data(),
                                static_cast<std::size_t>(input.gcount())));
  }
  if (!input.eof() || input.bad())
    throw std::runtime_error("failed to read hash input: " + path);
  return finish_hash(hash);
}

std::string hierarchy_analysis_id(const AnalysisIdentityInput & input)
{
  if (input.tool_version.empty() || input.analysis_core_id != 0)
    throw std::invalid_argument(
        "hierarchy identity requires version and core 0");
  detail::require_sha256(input.map_sha256);
  detail::require_sha256(input.elf_sha256);
  detail::require_sha256(input.cache_config_sha256);
  if (!input.semantic_analysis_options.is_object())
    throw std::invalid_argument("semantic analysis options must be an object");
  validate_options(input.semantic_analysis_options);
  const nlohmann::json preimage = {
      {"schema_version", detail::kResultSchemaVersion},
      {"analysis_mode", detail::kAnalysisMode},
      {"model_id", detail::kModelId},
      {"csrd_mode", detail::kCsrdMode},
      {"address_basis", detail::kAddressBasis},
      {"tool_version", input.tool_version},
      {"analysis_core_id", input.analysis_core_id},
      {"map_sha256", input.map_sha256},
      {"elf_sha256", input.elf_sha256},
      {"cache_config_sha256", input.cache_config_sha256},
      {"semantic_analysis_options", input.semantic_analysis_options},
  };
  try
  {
    llvm::SHA256 hash;
    hash.update(preimage.dump());
    return finish_hash(hash);
  }
  catch (const nlohmann::json::exception & error)
  {
    throw std::invalid_argument("invalid hierarchy identity JSON: " +
                                std::string(error.what()));
  }
}

} // namespace yarda
