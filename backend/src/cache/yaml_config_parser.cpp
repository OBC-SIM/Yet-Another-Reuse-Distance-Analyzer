#include "yarda/cache/yaml_config_parser.hpp"

#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <yaml-cpp/yaml.h>

namespace yarda
{
namespace
{

using Keys = std::initializer_list<const char *>;

void require_map(const YAML::Node & node, const std::string & path)
{
  if (!node || !node.IsMap())
  {
    throw std::invalid_argument(path + " must be a mapping");
  }
}

void require_sequence(const YAML::Node & node, const std::string & path)
{
  if (!node || !node.IsSequence())
  {
    throw std::invalid_argument(path + " must be a sequence");
  }
}

void reject_unknown(const YAML::Node & node, Keys allowed,
                    const std::string & path)
{
  require_map(node, path);
  std::unordered_set<std::string> names;
  for (const auto * key : allowed)
  {
    names.emplace(key);
  }
  for (const auto & field : node)
  {
    std::string key;
    try
    {
      key = field.first.as<std::string>();
    }
    catch (const YAML::Exception &)
    {
      throw std::invalid_argument(path + " contains a non-string key");
    }
    if (names.count(key) == 0)
    {
      throw std::invalid_argument(path + " contains unknown key: " + key);
    }
  }
}

const YAML::Node required(const YAML::Node & node, const char * key,
                          const std::string & path)
{
  const auto value = node[key];
  if (!value)
  {
    throw std::invalid_argument(path + "." + key + " is required");
  }
  return value;
}

template <typename Value>
Value scalar(const YAML::Node & node, const std::string & path)
{
  if (!node || !node.IsScalar())
  {
    throw std::invalid_argument(path + " must be a scalar");
  }
  try
  {
    return node.as<Value>();
  }
  catch (const YAML::Exception & error)
  {
    throw std::invalid_argument(path + " has invalid value: " + error.what());
  }
}

std::string trim(const std::string & input)
{
  std::size_t first = 0;
  while (first < input.size() &&
         std::isspace(static_cast<unsigned char>(input[first])))
  {
    ++first;
  }
  std::size_t last = input.size();
  while (last > first &&
         std::isspace(static_cast<unsigned char>(input[last - 1])))
  {
    --last;
  }
  return input.substr(first, last - first);
}

std::uint64_t unit_multiplier(const std::string & unit,
                              const std::string & path)
{
  if (unit.empty() || unit == "B") return 1;
  if (unit == "KB") return 1000ULL;
  if (unit == "MB") return 1000ULL * 1000ULL;
  if (unit == "GB") return 1000ULL * 1000ULL * 1000ULL;
  if (unit == "KiB") return 1024ULL;
  if (unit == "MiB") return 1024ULL * 1024ULL;
  if (unit == "GiB") return 1024ULL * 1024ULL * 1024ULL;
  throw std::invalid_argument(path + " has unknown byte unit: " + unit);
}

std::uint64_t byte_quantity(const YAML::Node & node, const std::string & path)
{
  if (!node || !node.IsScalar())
  {
    throw std::invalid_argument(path + " must be a byte quantity");
  }
  try
  {
    return node.as<std::uint64_t>();
  }
  catch (const YAML::BadConversion &)
  {
  }

  const auto text = trim(scalar<std::string>(node, path));
  std::size_t position = 0;
  while (position < text.size() &&
         std::isdigit(static_cast<unsigned char>(text[position])))
  {
    ++position;
  }
  if (position == 0)
  {
    throw std::invalid_argument(path + " is missing a byte count");
  }

  std::uint64_t value = 0;
  try
  {
    value = std::stoull(text.substr(0, position));
  }
  catch (const std::exception &)
  {
    throw std::invalid_argument(path + " byte count is out of range");
  }
  const auto multiplier = unit_multiplier(trim(text.substr(position)), path);
  if (value > std::numeric_limits<std::uint64_t>::max() / multiplier)
  {
    throw std::invalid_argument(path + " byte quantity overflows");
  }
  return value * multiplier;
}

std::uint32_t uint32_value(const YAML::Node & node, const std::string & path)
{
  const auto value = scalar<std::uint64_t>(node, path);
  if (value > std::numeric_limits<std::uint32_t>::max())
  {
    throw std::invalid_argument(path + " exceeds uint32 range");
  }
  return static_cast<std::uint32_t>(value);
}

Replacement replacement(const YAML::Node & node, const std::string & path)
{
  const auto value = scalar<std::string>(node, path);
  if (value == "LRU") return Replacement::LRU;
  if (value == "FIFO") return Replacement::FIFO;
  if (value == "MRU") return Replacement::MRU;
  throw std::invalid_argument(path + " has unknown policy: " + value);
}

WritePolicy write_policy(const YAML::Node & node, const std::string & path)
{
  const auto value = scalar<std::string>(node, path);
  if (value == "write-back") return WritePolicy::WriteBack;
  if (value == "write-through") return WritePolicy::WriteThrough;
  throw std::invalid_argument(path + " has unknown policy: " + value);
}

CoreMapping parse_core(const YAML::Node & node, std::size_t index)
{
  const auto path = "cores.mapping[" + std::to_string(index) + "]";
  reject_unknown(node, {"id", "l1"}, path);
  return {
    uint32_value(required(node, "id", path), path + ".id"),
    scalar<std::string>(required(node, "l1", path), path + ".l1"),
  };
}

CacheConfig parse_cache(const YAML::Node & node, std::size_t index)
{
  const auto path = "caches[" + std::to_string(index) + "]";
  reject_unknown(node,
                 {"name", "role", "private_to", "size_bytes", "line_size",
                  "associativity", "replacement", "write_policy",
                  "write_allocate", "delay_cycles", "next"},
                 path);
  CacheConfig cache;
  cache.name = scalar<std::string>(required(node, "name", path), path + ".nam"
                                                                        "e");
  cache.role = scalar<std::string>(required(node, "role", path), path + ".rol"
                                                                        "e");
  if (node["private_to"])
    cache.private_to = uint32_value(node["private_to"], path + ".private_to");
  cache.size_bytes =
    byte_quantity(required(node, "size_bytes", path), path + ".size_bytes");
  if (node["line_size"])
    cache.line_size = byte_quantity(node["line_size"], path + ".line_size");
  if (node["associativity"])
    cache.associativity =
      scalar<std::uint64_t>(node["associativity"], path + ".associativity");
  if (node["replacement"])
    cache.replacement = replacement(node["replacement"], path + ".replacement");
  if (node["write_policy"])
    cache.write_policy =
      write_policy(node["write_policy"], path + ".write_policy");
  if (node["write_allocate"])
    cache.write_allocate =
      scalar<bool>(node["write_allocate"], path + ".write_allocate");
  if (node["delay_cycles"])
    cache.delay_cycles =
      scalar<std::uint64_t>(node["delay_cycles"], path + ".delay_cycles");
  cache.next = scalar<std::string>(required(node, "next", path), path + ".nex"
                                                                        "t");
  return cache;
}

HierarchyConfig parse_root(const YAML::Node & root)
{
  reject_unknown(root, {"schema_version", "cores", "caches", "memory"}, "root");
  HierarchyConfig config;
  config.schema_version =
    uint32_value(required(root, "schema_version", "root"), "schema_version");

  const auto cores = required(root, "cores", "root");
  reject_unknown(cores, {"count", "mapping"}, "cores");
  config.num_cores =
    uint32_value(required(cores, "count", "cores"), "cores.count");
  const auto mappings = required(cores, "mapping", "cores");
  require_sequence(mappings, "cores.mapping");
  for (std::size_t index = 0; index < mappings.size(); ++index)
    config.core_mappings.push_back(parse_core(mappings[index], index));

  const auto caches = required(root, "caches", "root");
  require_sequence(caches, "caches");
  for (std::size_t index = 0; index < caches.size(); ++index)
    config.caches.push_back(parse_cache(caches[index], index));

  const auto memory = required(root, "memory", "root");
  reject_unknown(memory, {"name", "delay_cycles"}, "memory");
  if (memory["name"])
    config.memory.name = scalar<std::string>(memory["name"], "memory.name");
  if (memory["delay_cycles"])
    config.memory.delay_cycles =
      scalar<std::uint64_t>(memory["delay_cycles"], "memory.delay_cycles");
  return config;
}

}  // namespace

HierarchyConfig parse_cache_config(const std::filesystem::path & path)
{
  YAML::Node root;
  try
  {
    root = YAML::LoadFile(path.string());
  }
  catch (const YAML::Exception & error)
  {
    throw std::runtime_error("cannot parse cache YAML '" + path.string() +
                             "': " + error.what());
  }
  auto config = parse_root(root);
  validate_cache_config(config);
  return config;
}

}  // namespace yarda
