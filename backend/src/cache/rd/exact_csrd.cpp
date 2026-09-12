#include "yarda/cache/exact_csrd.hpp"

#include <stdexcept>

#include "exact_csrd_arithmetic.hpp"
#include "recency_index.hpp"

namespace yarda
{

ExactCsrdAnalyzer::ExactCsrdAnalyzer(CacheGeometry geometry)
  : geometry_(geometry), set_count_(cache_set_count(geometry))
{
}

ExactCsrdAnalyzer::~ExactCsrdAnalyzer() = default;

CsrdObservation ExactCsrdAnalyzer::observe(const DecodedCacheAddress & address)
{
  if (address.block_number != address.address / geometry_.line_size ||
      address.set_index != address.block_number % set_count_ ||
      address.tag != address.block_number / set_count_ ||
      address.line_offset != address.address % geometry_.line_size)
  {
    throw std::invalid_argument(
      "exact CSRD received an inconsistent cache address");
  }
  const auto lookups = detail::csrd_checked_add(summary_.lookups, 1);
  auto state = sets_.find(address.set_index);
  if (state == sets_.end())
    state =
      sets_.emplace(address.set_index, std::make_unique<detail::RecencyIndex>())
        .first;
  const auto distance = state->second->observe(address.block_number);
  if (!distance)
  {
    const auto cold = detail::csrd_checked_add(summary_.cold_misses, 1);
    const auto unique = detail::csrd_checked_add(summary_.unique_lines, 1);
    summary_.cold_misses = cold;
    summary_.unique_lines = unique;
    summary_.lookups = lookups;
    return {LruAccessOutcome::ColdMiss, std::nullopt};
  }

  const bool hit = *distance < geometry_.associativity;
  auto & count = hit ? summary_.hits : summary_.replacement_misses;
  const auto next_count = detail::csrd_checked_add(count, 1);
  auto & frequency = summary_.histogram[*distance];
  frequency = detail::csrd_checked_add(frequency, 1);
  count = next_count;
  summary_.lookups = lookups;
  return {hit ? LruAccessOutcome::Hit : LruAccessOutcome::ReplacementMiss,
          distance};
}

const ExactCsrdSummary & ExactCsrdAnalyzer::summary() const noexcept
{
  return summary_;
}

}  // namespace yarda
