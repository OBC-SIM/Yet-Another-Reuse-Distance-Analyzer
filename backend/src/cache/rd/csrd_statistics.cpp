#include "yarda/cache/exact_csrd.hpp"

#include <algorithm>
#include <stdexcept>

#include "exact_csrd_arithmetic.hpp"
#include "recency_index.hpp"

namespace yarda
{

CsrdStatistics ExactCsrdAnalyzer::statistics() const
{
  if (!measure_statistics_)
    throw std::logic_error("CSRD statistics were not enabled");
  CsrdStatistics result;
  result.touched_sets = sets_.size();
  result.histogram_keys = summary_.histogram.size();
  for (const auto & entry : sets_)
  {
    const auto state = entry.second->statistics();
    for (const auto member : {&CsrdStatistics::active_history_entries,
                             &CsrdStatistics::fenwick_slots,
                             &CsrdStatistics::allocated_fenwick_elements,
                             &CsrdStatistics::hash_buckets,
                             &CsrdStatistics::compaction_count,
                             &CsrdStatistics::compaction_time_ns})
      result.*member = detail::csrd_checked_add(result.*member, state.*member);
    result.maximum_compaction_scratch_bytes = std::max(
      result.maximum_compaction_scratch_bytes,
      state.maximum_compaction_scratch_bytes);
  }
  return result;
}

} // namespace yarda
