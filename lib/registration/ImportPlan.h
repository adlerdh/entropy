#pragma once

#include "registration/Types.h"

#include <filesystem>
#include <string>
#include <vector>

namespace registration
{

/**
 * @brief Import/update action to perform after a registration backend completes.
 */
enum class ImportAction
{
  ApplyAffineTransform,             //!< Apply the affine transform to the moving image
  LoadInverseWarp,                  //!< Load the fixed-to-moving sampling warp
  LoadForwardWarp,                  //!< Load the moving-to-fixed point warp
  AssignWarpsToMovingImage,         //!< Assign imported warps to the moving image
  LoadWarpedImage,                  //!< Load the warped moving image
  LoadWarpedSegmentation,           //!< Load a warped segmentation
  TransformLandmarksAndAnnotations, //!< Import transformed point annotations
  LoadTransformedSurface,           //!< Load a transformed surface
  MakeWarpedImageActive             //!< Make the warped image active
};

/**
 * @brief One ordered import/update step produced from a result manifest.
 */
struct ImportStep
{
  ImportAction action = ImportAction::LoadWarpedImage; //!< Action to perform
  std::filesystem::path path;                          //!< Artifact path, when applicable
  std::string displayName;                             //!< Suggested Entropy object name
  std::string targetImageUid;                          //!< Image UID affected by the step
};

/**
 * @brief Ordered set of post-registration import/update steps.
 */
struct ImportPlan
{
  std::vector<ImportStep> steps;     //!< Steps in safe execution order
  std::vector<std::string> warnings; //!< Non-fatal import planning warnings
};

/**
 * @brief Return a display name for the warped moving image.
 * @param fixedImage Fixed/reference image.
 * @param movingImage Moving image.
 * @return User-facing object name.
 */
std::string warpedImageName(const DataRef& fixedImage, const DataRef& movingImage);

/**
 * @brief Return a display name for the inverse/sampling warp.
 * @param fixedImage Fixed/reference image.
 * @param movingImage Moving image.
 * @return User-facing warp name.
 */
std::string inverseWarpName(const DataRef& fixedImage, const DataRef& movingImage);

/**
 * @brief Return a display name for the forward point warp.
 * @param fixedImage Fixed/reference image.
 * @param movingImage Moving image.
 * @return User-facing warp name.
 */
std::string forwardWarpName(const DataRef& fixedImage, const DataRef& movingImage);

/**
 * @brief Build the ordered import plan for a completed registration job.
 * @param job Original job specification.
 * @param manifest Backend result manifest.
 * @return Ordered import/update plan and warnings.
 */
ImportPlan buildImportPlan(const JobSpec& job, const ResultManifest& manifest);

} // namespace registration
