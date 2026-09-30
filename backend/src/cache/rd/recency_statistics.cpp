#include "recency_index.hpp"

#include <chrono>
#include <stdexcept>

#include "exact_csrd_arithmetic.hpp"

namespace yarda::detail
{

void RecencyIndex::compact()
{
  if (!measure_statistics_)
  {
    compact_storage();
    return;
  }
  const auto start = std::chrono::steady_clock::now();
  compact_storage();
  const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::steady_clock::now() - start).count();
  if (elapsed < 0) throw std::logic_error("compaction clock moved backwards");
  compaction_count_ = csrd_checked_add(compaction_count_, 1);
  compaction_time_ns_ = csrd_checked_add(compaction_time_ns_, elapsed);
}

CsrdStatistics RecencyIndex::statistics() const
{
  if (!measure_statistics_)
    throw std::logic_error("recency statistics were not enabled");
  return {1, active_count_, capacity_, tree_.capacity(), slots_.bucket_count(),
          0, compaction_count_, compaction_time_ns_,
          maximum_compaction_scratch_bytes_};
}

} // namespace yarda::detail
