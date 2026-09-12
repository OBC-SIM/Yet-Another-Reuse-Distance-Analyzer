#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

namespace yarda::detail
{

struct RecencyIndexTestAccess;

/**
 * @brief Rank opaque keys by their last observation, retaining every key.
 *
 * Keys have no cache-set or residency semantics. Compaction preserves their
 * relative order and keeps allocated storage proportional to distinct keys.
 */
class RecencyIndex
{
public:
  /**
   * @brief Create an empty index with bounded slot storage.
   * @param capacity_floor Minimum capacity; positive, also usable in tests.
   * @param slot_limit Inclusive capacity ceiling for checked arithmetic tests.
   * @throws std::invalid_argument for a zero floor or floor above the ceiling.
   * @throws std::overflow_error if storage including the sentinel cannot fit.
   */
  explicit RecencyIndex(
    std::uint64_t capacity_floor = 16,
    std::uint64_t slot_limit = std::numeric_limits<std::uint64_t>::max());

  /**
   * @brief Return a key's distinct newer-key count and make it most recent.
   * @param key Opaque identity, copied into owned history on its first visit.
   * @return Exact rank before the touch, absent for a previously unseen key.
   * @throws std::overflow_error if capacity or slot arithmetic exceeds bounds.
   * @note Discard the instance after a failed observation.
   */
  std::optional<std::uint64_t> observe(std::uint64_t key);

private:
  friend struct RecencyIndexTestAccess;
  std::uint64_t prefix_sum(std::uint64_t slot) const;
  void update(std::uint64_t slot, bool insert);
  void compact();

  std::unordered_map<std::uint64_t, std::uint64_t> slots_;
  std::vector<std::uint64_t> tree_;
  std::uint64_t floor_ = 0;
  std::uint64_t slot_limit_ = 0;
  std::uint64_t capacity_ = 0;
  std::uint64_t next_slot_ = 1;
  std::uint64_t active_count_ = 0;
};

}  // namespace yarda::detail
