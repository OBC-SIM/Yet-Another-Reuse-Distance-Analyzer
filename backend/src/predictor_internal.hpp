#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "yarda/profile.hpp"

namespace yarda::detail
{

struct LoopPrediction
{
  ReuseProfile profile;
  std::vector<std::string> trace;
};

LoopPrediction predict_3d_loop(const nlohmann::json & node,
                               const std::vector<std::uint64_t> & target);

}  // namespace yarda::detail
