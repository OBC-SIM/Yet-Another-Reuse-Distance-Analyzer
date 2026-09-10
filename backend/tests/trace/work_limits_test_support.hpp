#pragma once

#include <functional>
#include <stdexcept>

#include "task_access_stream_test_support.hpp"

namespace yarda::test::work
{

using namespace ::yarda::test::stream;

/** @brief Build one analyzed loop without duplicating LAT fixture boilerplate.
 */
inline Json loop_module(std::int64_t bound, Json body, std::int64_t start = 0,
                        std::int64_t step = 1)
{
  return module(Json::array({function(
    "kernel", Json::array({loop(bound, std::move(body), "i", start, step)}))}));
}

/** @brief Require the budget diagnostic, distinguishing competing failures. */
inline void expect_limit_error(const std::function<void()> & execute,
                               const std::string & expected)
{
  try
  {
    execute();
    FAIL() << "expected limit failure: " << expected;
  }
  catch (const std::invalid_argument & error)
  {
    EXPECT_EQ(std::string(error.what()), expected);
  }
}

}  // namespace yarda::test::work
