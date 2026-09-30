#pragma once

#include <cstdint>

namespace yarda
{

/**
 * @brief Snapshot measured history storage separately from cache semantics.
 *
 * Counts describe one live analyzer at a task boundary. Historical entries and
 * touched sets never shrink during a task, so their final counts are also their
 * peaks. Fenwick elements and hash buckets are actual container capacities,
 * not estimates of allocator bytes. Scratch is the maximum simultaneous byte
 * capacity of one compaction's rebuilt tree and sorting vector, additional to
 * retained storage. It excludes allocator overhead. This is not process RSS.
 */
struct CsrdStatistics
{
  std::uint64_t touched_sets = 0;
  std::uint64_t active_history_entries = 0;
  std::uint64_t fenwick_slots = 0;
  std::uint64_t allocated_fenwick_elements = 0;
  std::uint64_t hash_buckets = 0;
  std::uint64_t histogram_keys = 0;
  std::uint64_t compaction_count = 0;
  std::uint64_t compaction_time_ns = 0;
  std::uint64_t maximum_compaction_scratch_bytes = 0;
};

} // namespace yarda
