#include "yarda/cache/yaml_config_parser.hpp"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

namespace
{

class TemporaryYaml
{
public:
  explicit TemporaryYaml(const std::string & content)
  {
    static std::atomic<unsigned> sequence{0};
    path_ = std::filesystem::temp_directory_path() /
            ("yarda-cache-config-" + std::to_string(sequence++) + ".yaml");
    std::ofstream output(path_);
    output << content;
  }

  ~TemporaryYaml()
  {
    std::error_code error;
    std::filesystem::remove(path_, error);
  }

  const std::filesystem::path & path() const { return path_; }

private:
  std::filesystem::path path_;
};

TEST(YamlConfigParserTest, ParsesVersionedHierarchyAndPolicies)
{
  const auto config =
    yarda::parse_cache_config(YARDA_CACHE_CONFIG_EXAMPLE_PATH);

  ASSERT_EQ(config.schema_version, 1U);
  ASSERT_EQ(config.num_cores, 1U);
  ASSERT_EQ(config.core_mappings.size(), 1U);
  ASSERT_EQ(config.caches.size(), 2U);
  EXPECT_EQ(config.core_mappings.front().l1, "L1D0");
  EXPECT_EQ(config.caches.front().size_bytes, 32U * 1024U);
  EXPECT_EQ(config.caches.front().private_to, 0U);
  EXPECT_EQ(config.caches.front().replacement, yarda::Replacement::LRU);
  EXPECT_EQ(config.caches.back().replacement, yarda::Replacement::FIFO);
  EXPECT_EQ(config.caches.back().write_policy,
            yarda::WritePolicy::WriteThrough);
  EXPECT_FALSE(config.caches.back().write_allocate);
  EXPECT_FALSE(config.caches.back().private_to.has_value());
  EXPECT_EQ(config.memory.delay_cycles, 120U);
}

TEST(YamlConfigParserTest, ParsesNumericByteQuantities)
{
  TemporaryYaml yaml(R"(
schema_version: 1
cores: {count: 1, mapping: [{id: 0, l1: L1}]}
caches:
  - {name: L1, role: L1, private_to: 0, size_bytes: 32768,
     line_size: 64, associativity: 8, next: Memory}
memory: {name: Memory, delay_cycles: 100}
)");

  const auto config = yarda::parse_cache_config(yaml.path());

  EXPECT_EQ(config.caches.front().size_bytes, 32768U);
  EXPECT_EQ(config.caches.front().line_size, 64U);
  EXPECT_EQ(config.caches.front().write_policy, yarda::WritePolicy::WriteBack);
  EXPECT_TRUE(config.caches.front().write_allocate);
}

TEST(YamlConfigParserTest, RejectsUnknownKeys)
{
  TemporaryYaml yaml(R"(
schema_version: 1
cores: {count: 1, mapping: [{id: 0, l1: L1}]}
caches:
  - {name: L1, role: L1, private_to: 0, size_bytes: 32 KiB,
     line_size: 64 B, associativity: 8, next: Memory, typo: true}
memory: {name: Memory, delay_cycles: 100}
)");

  EXPECT_THROW(yarda::parse_cache_config(yaml.path()), std::invalid_argument);
}

TEST(YamlConfigParserTest, RejectsCustomLikeInheritance)
{
  TemporaryYaml yaml(R"(
schema_version: 1
cores: {count: 1, mapping: [{id: 0, l1: L1}]}
caches:
  - {name: L1, role: L1, private_to: 0, size_bytes: 32 KiB,
     line_size: 64 B, associativity: 8, next: Memory, like: Base}
memory: {name: Memory, delay_cycles: 100}
)");

  EXPECT_THROW(yarda::parse_cache_config(yaml.path()), std::invalid_argument);
}

TEST(YamlConfigParserTest, RejectsUnsupportedSchemaVersion)
{
  TemporaryYaml yaml(R"(
schema_version: 2
cores: {count: 1, mapping: [{id: 0, l1: L1}]}
caches:
  - {name: L1, role: L1, private_to: 0, size_bytes: 32 KiB,
     line_size: 64 B, associativity: 8, next: Memory}
memory: {name: Memory, delay_cycles: 100}
)");

  EXPECT_THROW(yarda::parse_cache_config(yaml.path()), std::invalid_argument);
}

TEST(YamlConfigParserTest, ParsesMruReplacement)
{
  TemporaryYaml yaml(R"(
schema_version: 1
cores: {count: 1, mapping: [{id: 0, l1: L1}]}
caches:
  - {name: L1, role: L1, private_to: 0, size_bytes: 32 KiB,
     line_size: 64 B, associativity: 8, replacement: MRU, next: Memory}
memory: {name: Memory, delay_cycles: 100}
)");

  const auto config = yarda::parse_cache_config(yaml.path());

  EXPECT_EQ(config.caches.front().replacement, yarda::Replacement::MRU);
}

TEST(YamlConfigParserTest, RejectsUnknownByteUnit)
{
  TemporaryYaml yaml(R"(
schema_version: 1
cores: {count: 1, mapping: [{id: 0, l1: L1}]}
caches:
  - {name: L1, role: L1, private_to: 0, size_bytes: 32 XB,
     line_size: 64 B, associativity: 8, next: Memory}
memory: {name: Memory, delay_cycles: 100}
)");

  EXPECT_THROW(yarda::parse_cache_config(yaml.path()), std::invalid_argument);
}

TEST(YamlConfigParserTest, ReportsMissingFileAsRuntimeError)
{
  EXPECT_THROW(yarda::parse_cache_config("/tmp/"
                                         "yarda-cache-config-does-not-exist."
                                         "yaml"),
               std::runtime_error);
}

TEST(YamlConfigParserTest, ReportsMalformedYamlAsRuntimeError)
{
  TemporaryYaml yaml("caches: [unterminated");

  EXPECT_THROW(yarda::parse_cache_config(yaml.path()), std::runtime_error);
}

}  // namespace
