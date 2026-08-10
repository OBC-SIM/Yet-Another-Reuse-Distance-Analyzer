#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

#include "yarda/reuse/profile.hpp"

namespace yarda
{

/**
 * @brief Predicted profile associated with one reported block.
 */
struct NamedProfile
{
  std::string name;
  ReuseProfile profile;
};

/**
 * @brief Whole-program and block-level Dilation prediction result.
 */
struct PredictionResult
{
  ReuseProfile program;
  std::vector<NamedProfile> blocks;
};

/**
 * @brief Predict element-level RDH for all analyzed functions in a LAT module.
 *
 * Stable RD families use the Dilation equation. Unstable families are
 * replaced with their exact C++ profile to avoid dropping references.
 *
 * @param raw Legacy or APE v2 LAT module.
 * @return Program and block profiles.
 * @throws std::invalid_argument for loop nests deeper than three.
 */
PredictionResult predict_module(const nlohmann::json & raw);

}  // namespace yarda
