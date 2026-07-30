#include "yarda/dilation.hpp"

#include <algorithm>
#include <stdexcept>

namespace yarda
{
namespace
{

std::int64_t value(const DilationContext & context, const std::string & name,
                   std::size_t distance)
{
  const auto coefficient = context.coefficients.find(name);
  if (coefficient == context.coefficients.end())
  {
    throw std::invalid_argument("missing Dilation coefficient: " + name);
  }
  const auto frequency = coefficient->second.find(distance);
  return frequency == coefficient->second.end() ? 0 : frequency->second;
}

}  // namespace

ReuseProfile dilate(std::size_t depth, const DilationContext & context)
{
  if (context.bounds.size() != depth)
  {
    throw std::invalid_argument("Dilation bound count does not match depth");
  }
  if (depth != 2 && depth != 3)
  {
    throw std::invalid_argument("Dilation supports only 2D and 3D loops");
  }

  ReuseProfile predicted;
  const auto di = static_cast<std::int64_t>(context.bounds[0]) - 2;
  const auto dj = static_cast<std::int64_t>(context.bounds[1]) - 2;
  for (const auto & [distance, base_frequency] : context.base.histogram)
  {
    auto frequency = static_cast<std::int64_t>(base_frequency);
    if (depth == 2)
    {
      frequency += di * value(context, "Incr_J", distance);
      frequency += dj * value(context, "Incr_K", distance);
      frequency += di * dj * value(context, "Coff_JK", distance);
    }
    else
    {
      const auto dk = static_cast<std::int64_t>(context.bounds[2]) - 2;
      frequency += di * value(context, "Incr_I", distance);
      frequency += dj * value(context, "Incr_J", distance);
      frequency += dk * value(context, "Incr_K", distance);
      frequency += di * dj * value(context, "Coff_IJ", distance);
      frequency += di * dk * value(context, "Coff_IK", distance);
      frequency += dj * dk * value(context, "Coff_JK", distance);
      frequency += di * dj * dk * value(context, "Coff_IJK", distance);
    }
    predicted.histogram[distance] =
      static_cast<std::uint64_t>(std::max<std::int64_t>(0, frequency));
  }
  return predicted;
}

}  // namespace yarda
