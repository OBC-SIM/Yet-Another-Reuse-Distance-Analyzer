#include "recency_index.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <utility>

#include "exact_csrd_arithmetic.hpp"

namespace yarda::detail
{

RecencyIndex::RecencyIndex(std::uint64_t capacity_floor,
                           std::uint64_t slot_limit)
  : floor_(capacity_floor), slot_limit_(slot_limit), capacity_(capacity_floor)
{
  if (floor_ == 0 || floor_ > slot_limit_)
  {
    throw std::invalid_argument(
      "recency capacity floor is outside slot bounds");
  }
  tree_.resize(
    csrd_storage_size(csrd_checked_add(capacity_, 1), tree_.max_size()), 0);
}

std::optional<std::uint64_t> RecencyIndex::observe(std::uint64_t key)
{
  if (next_slot_ > capacity_) compact();
  const auto following_slot = csrd_checked_add(next_slot_, 1);
  // Compaction changes slot identities, so lookup must follow compaction.
  const auto previous = slots_.find(key);
  std::optional<std::uint64_t> distance;
  if (previous == slots_.end())
  {
    const auto next_count = csrd_checked_add(active_count_, 1);
    slots_.emplace(key, next_slot_);
    active_count_ = next_count;
  }
  else
  {
    distance = active_count_ - prefix_sum(previous->second);
    update(previous->second, false);
    previous->second = next_slot_;
  }
  update(next_slot_, true);
  next_slot_ = following_slot;
  return distance;
}

std::uint64_t RecencyIndex::prefix_sum(std::uint64_t slot) const
{
  std::uint64_t total = 0;
  while (slot > 0)
  {
    total += tree_[static_cast<std::size_t>(slot)];
    slot -= slot & (~slot + 1);
  }
  return total;
}

void RecencyIndex::update(std::uint64_t slot, bool insert)
{
  while (slot <= capacity_)
  {
    auto & count = tree_[static_cast<std::size_t>(slot)];
    if (insert)
    {
      // Every partial sum is bounded by the checked active count.
      assert(count < active_count_);
      ++count;
    }
    else
    {
      assert(count > 0);
      --count;
    }
    const auto step = slot & (~slot + 1);
    if (step > capacity_ - slot) break;
    slot += step;
  }
}

void RecencyIndex::compact()
{
  const auto capacity = std::max(
    floor_, csrd_checked_add(active_count_, active_count_, slot_limit_));
  const auto next_slot = csrd_checked_add(active_count_, 1);
  std::vector<std::uint64_t> rebuilt(
    csrd_storage_size(csrd_checked_add(capacity, 1), tree_.max_size()), 0);
  std::vector<std::pair<std::uint64_t, std::uint64_t>> ordered;
  ordered.reserve(csrd_storage_size(active_count_, ordered.max_size()));
  for (const auto & [key, slot] : slots_) ordered.emplace_back(slot, key);
  std::sort(ordered.begin(), ordered.end());

  // After packing, slots 1..V contain ones; later slots contain zeros.
  for (std::uint64_t slot = 1; slot <= capacity; ++slot)
  {
    const auto lower = slot - (slot & (~slot + 1));
    if (lower < active_count_)
      rebuilt[static_cast<std::size_t>(slot)] =
        std::min(slot, active_count_) - lower;
  }
  std::uint64_t slot = 1;
  for (const auto & entry : ordered) slots_.at(entry.second) = slot++;
  tree_.swap(rebuilt);
  capacity_ = capacity;
  next_slot_ = next_slot;
}

}  // namespace yarda::detail
