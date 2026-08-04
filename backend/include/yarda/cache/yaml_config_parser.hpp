#pragma once

#include <filesystem>

#include "yarda/cache/cache_config.hpp"

namespace yarda
{

/**
 * @brief Parse and validate a versioned cache hierarchy YAML file.
 *
 * Numeric byte quantities are interpreted as bytes. String quantities accept
 * `B`, decimal `KB`/`MB`/`GB`, and binary `KiB`/`MiB`/`GiB` suffixes.
 *
 * @param path YAML file to read without retaining the path or file handle.
 * @return Fully parsed and semantically validated hierarchy.
 * @throws std::runtime_error if the file is unreadable or malformed YAML.
 * @throws std::invalid_argument if fields or topology violate schema v1.
 */
HierarchyConfig parse_cache_config(const std::filesystem::path & path);

}  // namespace yarda
