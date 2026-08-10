#pragma once

#include <string>
#include <vector>

#include "yarda/reuse/profile.hpp"

namespace yarda
{

/**
 * @brief Merge block profiles while accounting for cross-block first reuses.
 */
class BlockMerger
{
public:
  /**
   * @brief Add one block profile and its representative trace.
   *
   * @param block_profile Reuses and cold references within the block.
   * @param block_trace Representative block trace in execution order.
   * @return Current whole-program profile.
   */
  const ReuseProfile & merge(const ReuseProfile & block_profile,
                             const std::vector<std::string> & block_trace);

  /**
   * @brief Return the current merged profile.
   */
  [[nodiscard]] const ReuseProfile & profile() const;

private:
  ReuseProfile profile_;
  std::vector<std::string> lru_stack_;
};

}  // namespace yarda
