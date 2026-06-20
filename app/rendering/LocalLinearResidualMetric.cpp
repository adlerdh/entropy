#include "rendering/LocalLinearResidualMetric.h"

#include <algorithm>
#include <cmath>

namespace entropy::rendering::local_linear_residual
{

Fit fit(std::span<const float> fixed, std::span<const float> moving, float varianceEpsilon)
{
  if (fixed.size() != moving.size() || fixed.empty()) {
    return {};
  }

  varianceEpsilon = std::max(varianceEpsilon, 0.0f);

  float sumFixed = 0.0f;
  float sumMoving = 0.0f;
  for (std::size_t i = 0; i < fixed.size(); ++i) {
    sumFixed += fixed[i];
    sumMoving += moving[i];
  }

  const float count = static_cast<float>(fixed.size());
  const float meanFixed = sumFixed / count;
  const float meanMoving = sumMoving / count;

  float fixedVarianceSum = 0.0f;
  float covarianceSum = 0.0f;
  for (std::size_t i = 0; i < fixed.size(); ++i) {
    const float centeredFixed = fixed[i] - meanFixed;
    fixedVarianceSum += centeredFixed * centeredFixed;
    covarianceSum += centeredFixed * (moving[i] - meanMoving);
  }

  if (fixedVarianceSum <= varianceEpsilon) {
    return {};
  }

  Fit result;
  result.gain = covarianceSum / fixedVarianceSum;
  result.bias = meanMoving - result.gain * meanFixed;
  result.valid = true;

  float residualSum = 0.0f;
  for (std::size_t i = 0; i < fixed.size(); ++i) {
    const float predicted = result.gain * fixed[i] + result.bias;
    const float residual = moving[i] - predicted;
    residualSum += residual * residual;
  }
  result.residual = std::sqrt(residualSum / count);

  return result;
}

} // namespace entropy::rendering::local_linear_residual
