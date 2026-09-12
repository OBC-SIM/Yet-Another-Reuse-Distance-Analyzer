#include "yarda/cache/cache_config.hpp"

#include <gtest/gtest.h>
#include <stdexcept>

namespace
{

yarda::HierarchyConfig valid_config()
{
  yarda::HierarchyConfig config;
  config.schema_version = 1;
  config.num_cores = 1;
  config.core_mappings = {{0, "L1D0"}};
  config.caches = {
    {"L1D0", "L1", 0, 32 * 1024, 64, 8, yarda::Replacement::LRU,
     yarda::WritePolicy::WriteBack, true, 1, "L2"},
    {"L2", "LLC", std::nullopt, 2 * 1024 * 1024, 64, 16,
     yarda::Replacement::FIFO, yarda::WritePolicy::WriteThrough, false, 12,
     "Memory"},
  };
  config.memory = {"Memory", 120};
  return config;
}

TEST(CacheConfigTest, ConvertsCapacityToAddressGeometry)
{
  const auto config = valid_config();

  const auto geometry = yarda::make_cache_geometry(config.caches.front());

  EXPECT_EQ(geometry.line_size, 64U);
  EXPECT_EQ(geometry.line_count, 512U);
  EXPECT_EQ(geometry.associativity, 8U);
}

TEST(CacheConfigTest, ResolvesEntryCacheForCore)
{
  const auto config = valid_config();

  EXPECT_EQ(yarda::entry_cache_config(config, 0).name, "L1D0");
}

TEST(CacheConfigTest, AcceptsValidHierarchy)
{
  EXPECT_NO_THROW(yarda::validate_cache_config(valid_config()));
}

TEST(CacheConfigTest, RejectsDuplicateCacheNames)
{
  auto config = valid_config();
  config.caches.back().name = "L1D0";

  EXPECT_THROW(yarda::validate_cache_config(config), std::invalid_argument);
}

TEST(CacheConfigTest, RejectsBrokenNextReference)
{
  auto config = valid_config();
  config.caches.front().next = "missing";

  EXPECT_THROW(yarda::validate_cache_config(config), std::invalid_argument);
}

TEST(CacheConfigTest, RejectsCacheCycle)
{
  auto config = valid_config();
  config.caches.back().next = "L1D0";

  EXPECT_THROW(yarda::validate_cache_config(config), std::invalid_argument);
}

TEST(CacheConfigTest, RejectsInvalidPrivateCore)
{
  auto config = valid_config();
  config.caches.front().private_to = 2;

  EXPECT_THROW(yarda::validate_cache_config(config), std::invalid_argument);
}

TEST(CacheConfigTest, RejectsDuplicateCoreMappings)
{
  auto config = valid_config();
  config.num_cores = 2;
  config.core_mappings.push_back({0, "L1D0"});

  EXPECT_THROW(yarda::validate_cache_config(config), std::invalid_argument);
}

TEST(CacheConfigTest, RejectsNonL1EntryCache)
{
  auto config = valid_config();
  config.core_mappings.front().l1 = "L2";

  EXPECT_THROW(yarda::validate_cache_config(config), std::invalid_argument);
}

TEST(CacheConfigTest, RejectsUnalignedCapacity)
{
  auto config = valid_config();
  config.caches.front().size_bytes += 1;

  EXPECT_THROW(yarda::validate_cache_config(config), std::invalid_argument);
}

}  // namespace
