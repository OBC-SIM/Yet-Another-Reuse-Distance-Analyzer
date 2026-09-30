#pragma once

namespace yarda
{

/** @brief Memory operation carried by one source access. */
enum class AccessOperation
{
  /** Legacy input did not carry an explicit load/store operation. */
  Unknown,
  /** Source access reads from its linked storage object. */
  Load,
  /** Source access writes to its linked storage object. */
  Store,
};

}  // namespace yarda
