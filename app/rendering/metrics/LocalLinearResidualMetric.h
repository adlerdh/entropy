#pragma once

#include <span>

namespace entropy::rendering::local_linear_residual
{

/** @brief Least-squares fit for `moving ~= gain * fixed + bias`. */
struct Fit
{
  float gain = 0.0f;     //!< Multiplicative term.
  float bias = 0.0f;     //!< Additive term.
  float residual = 0.0f; //!< Root-mean-square residual.
  bool valid = false;    //!< True when the fit has enough variance and samples.
};

/**
 * @brief Fit a local linear relationship between paired intensity samples.
 * @param fixed Reference image samples.
 * @param moving Comparison image samples.
 * @param varianceEpsilon Minimum reference variance required for a stable gain.
 * @return Linear fit and RMS residual.
 */
Fit fit(std::span<const float> fixed, std::span<const float> moving, float varianceEpsilon);

} // namespace entropy::rendering::local_linear_residual
