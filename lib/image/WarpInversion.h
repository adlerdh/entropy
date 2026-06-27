#pragma once

#include "common/Expected.h"
#include "image/Image.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

/**
 * @brief Direction of the warp field that will be computed.
 */
enum class ComputedWarpDirection
{
  Inverse, //!< Compute an inverse warp from an existing forward warp.
  Forward  //!< Compute a forward warp from an existing inverse warp.
};

/**
 * @brief Options controlling iterative displacement-field inversion.
 */
struct WarpInversionOptions
{
  uint32_t maxIterations = 100;         //!< Maximum number of fixed-point iterations.
  double meanErrorTolerance = 1.0e-4;   //!< ITK mean error stopping threshold.
  double maxErrorTolerance = 1.0e-2;    //!< ITK max error stopping threshold.
  bool enforceBoundaryCondition = true; //!< Force zero displacement at field boundaries.
};

/**
 * @brief Quality metrics measured after a warp inversion.
 */
struct WarpInversionReport
{
  double itkMeanErrorNorm = 0.0;     //!< ITK-reported mean error norm.
  double itkMaxErrorNorm = 0.0;      //!< ITK-reported max error norm.
  double meanResidualMm = 0.0;       //!< Mean inverse-consistency residual in millimeters.
  double maxResidualMm = 0.0;        //!< Max inverse-consistency residual in millimeters.
  double meanResidualVoxels = 0.0;   //!< Mean residual normalized by output voxel spacing.
  double maxResidualVoxels = 0.0;    //!< Max residual normalized by output voxel spacing.
  uint64_t samples = 0;              //!< Number of output field samples checked.
  uint64_t outsideOppositeField = 0; //!< Number of samples outside the opposite field.
  double elapsedSeconds = 0.0;       //!< Wall-clock inversion time.
};

/**
 * @brief Output from displacement-field inversion.
 */
struct WarpInversionResult
{
  Image image;                     //!< Computed 3-component float displacement field.
  WarpInversionReport report;      //!< Inversion quality metrics.
  ComputedWarpDirection direction; //!< Direction of the computed field.
};

/**
 * @brief Build a display name for a computed matching warp.
 * @param sourceWarp Existing warp that is being inverted.
 * @param direction Direction of the computed matching warp.
 * @return Human-readable display name for the computed field.
 */
std::string computedWarpDisplayName(const Image& sourceWarp, ComputedWarpDirection direction);

/**
 * @brief Compute the matching forward or inverse displacement field.
 * @param sourceWarp Existing 3-component displacement field.
 * @param outputDomain Image whose geometry defines the computed field domain.
 * @param direction Direction of the computed field.
 * @param options Iteration and stopping options.
 * @param progress Optional progress callback in range [0, 1].
 * @param cancel Optional cancellation flag checked during ITK progress events.
 * @return Computed warp and quality report, or an error message.
 */
entropy_expected::expected<WarpInversionResult, std::string> computeMatchingWarp(
  const Image& sourceWarp,
  const Image& outputDomain,
  ComputedWarpDirection direction,
  const WarpInversionOptions& options = {},
  const std::function<void(double)>& progress = {},
  const std::atomic_bool* cancel = nullptr);
