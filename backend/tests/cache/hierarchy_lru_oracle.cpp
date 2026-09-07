#include "hierarchy_lru_oracle.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace yarda
{
namespace test
{
namespace
{

void increment(std::uint64_t & value)
{
  if (value == std::numeric_limits<std::uint64_t>::max())
  {
    throw std::overflow_error("hierarchy LRU oracle counter overflows");
  }
  ++value;
}

std::size_t checked_ways(std::uint64_t associativity)
{
  if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t))
  {
    if (associativity > std::numeric_limits<std::size_t>::max())
    {
      throw std::overflow_error(
        "hierarchy LRU oracle associativity exceeds size_t");
    }
  }
  return static_cast<std::size_t>(associativity);
}

struct SetState
{
  std::vector<std::uint64_t> mru_to_lru;
  std::unordered_set<std::uint64_t> ever_seen;
};

class ExplicitLruLevel
{
public:
  explicit ExplicitLruLevel(const CacheGeometry & geometry)
    : geometry_(geometry)
    , set_count_(cache_set_count(geometry))
    , ways_(checked_ways(geometry.associativity))
  {
  }

  LruAccessOutcome lookup(const DecodedCacheAddress & address)
  {
    validate(address);
    increment(counts_.lookups);
    auto & state = sets_[address.set_index];
    const auto resident = std::find(
      state.mru_to_lru.begin(), state.mru_to_lru.end(), address.block_number);
    if (resident != state.mru_to_lru.end())
    {
      const auto block = *resident;
      state.mru_to_lru.erase(resident);
      state.mru_to_lru.insert(state.mru_to_lru.begin(), block);
      increment(counts_.hits);
      return LruAccessOutcome::Hit;
    }

    increment(counts_.misses);
    if (state.ever_seen.insert(address.block_number).second)
    {
      increment(counts_.cold_misses);
      return LruAccessOutcome::ColdMiss;
    }
    increment(counts_.replacement_misses);
    return LruAccessOutcome::ReplacementMiss;
  }

  void fill(const DecodedCacheAddress & address)
  {
    auto & resident = sets_[address.set_index].mru_to_lru;
    assert(std::find(resident.begin(), resident.end(), address.block_number) ==
           resident.end());
    if (resident.size() == ways_)
    {
      resident.pop_back();
    }
    resident.insert(resident.begin(), address.block_number);
  }

  const OracleLevelCounts & counts() const noexcept { return counts_; }

private:
  void validate(const DecodedCacheAddress & address) const
  {
    if (address.block_number != address.address / geometry_.line_size ||
        address.set_index != address.block_number % set_count_ ||
        address.tag != address.block_number / set_count_ ||
        address.line_offset != address.address % geometry_.line_size)
    {
      throw std::invalid_argument(
        "hierarchy LRU oracle received an inconsistent cache mapping");
    }
  }

  CacheGeometry geometry_;
  std::uint64_t set_count_ = 0;
  std::size_t ways_ = 0;
  std::unordered_map<std::uint64_t, SetState> sets_;
  OracleLevelCounts counts_;
};

bool is_hit(LruAccessOutcome outcome)
{
  return outcome == LruAccessOutcome::Hit;
}

}  // namespace

OracleTaskHierarchy analyze_with_explicit_lru(
  const MappedTaskTrace & l1_trace, const CacheGeometry & l1_geometry,
  const CacheGeometry & llc_geometry)
{
  if (l1_geometry.line_size != llc_geometry.line_size)
  {
    throw std::invalid_argument(
      "hierarchy LRU oracle requires equal cache-line sizes");
  }

  ExplicitLruLevel l1(l1_geometry);
  ExplicitLruLevel llc(llc_geometry);
  OracleTaskHierarchy result;
  result.task_id = l1_trace.task_id;
  result.events.reserve(l1_trace.accesses.size());

  for (const auto & mapping : l1_trace.accesses)
  {
    OracleHierarchyEvent event;
    event.l1_outcome = l1.lookup(mapping.decoded);
    if (is_hit(event.l1_outcome))
    {
      event.first_service = OracleFirstServiceLevel::L1;
      increment(result.ehc_l1);
      result.events.push_back(event);
      continue;
    }

    const auto llc_address =
      decode_cache_address(mapping.decoded.address, llc_geometry);
    event.llc_outcome = llc.lookup(llc_address);
    if (is_hit(*event.llc_outcome))
    {
      event.first_service = OracleFirstServiceLevel::LLC;
      increment(result.ehc_llc);
    }
    else
    {
      llc.fill(llc_address);
      event.first_service = OracleFirstServiceLevel::Memory;
      increment(result.all_cache_misses);
    }
    l1.fill(mapping.decoded);
    result.events.push_back(event);
  }

  result.l1 = l1.counts();
  result.llc = llc.counts();
  return result;
}

}  // namespace test
}  // namespace yarda
