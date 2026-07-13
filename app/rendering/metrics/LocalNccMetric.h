#pragma once

namespace rendering::local_ncc
{

/** @brief Display transform applied to a local normalized cross-correlation value. */
enum class Presentation
{
  Dissimilarity, //!< Convert high correlation to low displayed mismatch.
  Correlation    //!< Display correlation directly after mapping [-1, 1] to [0, 1].
};

/**
 * @brief Convert an NCC value to the scalar metric displayed by the shader.
 * @param ncc Normalized cross-correlation value.
 * @param presentation Display transform.
 * @param negativeCorrelationAsMismatch Whether negative correlation is treated as maximum mismatch.
 * @return Metric value in `[0, 1]`.
 */
float metricValue(float ncc, Presentation presentation, bool negativeCorrelationAsMismatch);

/**
 * @brief Compute the minimum valid paired samples required for a patch.
 * @param patchRadius Patch radius in samples.
 * @param minValidFraction Fraction of the full patch that must overlap both images.
 * @return Minimum valid sample count.
 */
int requiredSampleCount(int patchRadius, float minValidFraction);

} // namespace rendering::local_ncc
