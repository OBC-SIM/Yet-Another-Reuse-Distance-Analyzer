#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "yarda/profile.hpp"

namespace yarda
{

using SignedHistogram = std::map<std::size_t, std::int64_t>;

/**
 * @brief Inputs required by the 2D or 3D Dilation equation.
 */
struct DilationContext
{
  std::vector<std::uint64_t> bounds;
  ReuseProfile base;
  std::unordered_map<std::string, SignedHistogram> coefficients;
};

/**
 * @brief Evaluate a 2D or 3D Dilation equation.
 *
 * @param depth Loop depth, either 2 or 3.
 * @param context Target bounds, stable base profile, and coefficients.
 * @return Predicted stable reuse-distance frequencies.
 * @throws std::invalid_argument for unsupported depth or incomplete context.
 */
ReuseProfile dilate(std::size_t depth, const DilationContext & context);

}  // namespace yarda
