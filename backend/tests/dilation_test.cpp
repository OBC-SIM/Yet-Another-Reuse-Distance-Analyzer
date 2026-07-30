#include "yarda/dilation.hpp"

#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <map>

namespace
{

TEST(DilationTest, PredictsTwoDimensionalFrequency)
{
  yarda::DilationContext context;
  context.bounds = {4, 5};
  context.base.histogram = {{7, 10}};
  context.coefficients = {
    {"Incr_J", {{7, 2}}},
    {"Incr_K", {{7, 3}}},
    {"Coff_JK", {{7, 1}}},
  };

  const auto profile = yarda::dilate(2, context);

  EXPECT_EQ(profile.histogram, (std::map<std::size_t, std::uint64_t>{{7, 29}}));
}

TEST(DilationTest, PredictsThreeDimensionalFrequency)
{
  yarda::DilationContext context;
  context.bounds = {3, 3, 3};
  context.base.histogram = {{1, 8}};
  context.coefficients = {
    {"Incr_I", {{1, 1}}},   {"Incr_J", {{1, 1}}},  {"Incr_K", {{1, 1}}},
    {"Coff_IJ", {{1, 1}}},  {"Coff_IK", {{1, 1}}}, {"Coff_JK", {{1, 1}}},
    {"Coff_IJK", {{1, 1}}},
  };

  const auto profile = yarda::dilate(3, context);

  EXPECT_EQ(profile.histogram.at(1), 15);
}

TEST(DilationTest, RejectsUnsupportedDepth)
{
  EXPECT_THROW(yarda::dilate(4, {}), std::invalid_argument);
}

}  // namespace
