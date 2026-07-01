#pragma once

#include "registration/Types.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace registration
{

/**
 * @brief File role used by a registration backend or post-registration import.
 */
enum class ArtifactRole
{
  JobSpec,              //!< Backend bridge job specification.
  ResultManifest,       //!< Backend bridge result manifest.
  AffineTransform,      //!< Affine transform produced by registration.
  InverseWarp,          //!< Fixed-to-moving sampling warp.
  ForwardWarp,          //!< Moving-to-fixed point warp.
  WarpedImage,          //!< Warped moving image.
  WarpedSegmentation,   //!< Warped moving segmentation.
  TransformedLandmarks, //!< Transformed landmarks or point annotations.
  TransformedSurface,   //!< Transformed surface mesh.
  FixedMask,            //!< Exported fixed mask input.
  MovingMask,           //!< Exported moving mask input.
  AuxiliaryFixedImage,  //!< Exported fixed auxiliary image input.
  AuxiliaryMovingImage, //!< Exported moving auxiliary image input.
  FixedLandmarks,       //!< Exported fixed landmarks input.
  MovingLandmarks,      //!< Exported moving landmarks input.
  Surface               //!< Exported surface input.
};

/**
 * @brief Input artifact that must exist before a backend can run.
 */
struct InputArtifact
{
  ArtifactRole role = ArtifactRole::JobSpec; //!< Input role.
  DataRef source;                            //!< Entropy object that supplies the input.
  std::filesystem::path path;                //!< File path passed to the backend.
  bool exportRequired = false;               //!< True when Entropy must write the file first.
};

/**
 * @brief Human-readable label for an artifact role.
 * @param role Artifact role.
 * @return Stable role label for logs, JSON, and tests.
 */
std::string_view label(ArtifactRole role);

/**
 * @brief Return the normalized output prefix for a job.
 * @param job Registration job.
 * @return Job output prefix, or "registration" when none was supplied.
 */
std::string outputPrefix(const JobSpec& job);

/**
 * @brief Build the standard path for an output artifact.
 * @param job Registration job.
 * @param role Artifact role.
 * @param index Zero-based index used for repeated artifact roles.
 * @return File path in the job output directory.
 */
std::filesystem::path artifactPath(const JobSpec& job, ArtifactRole role, std::size_t index = 0);

/**
 * @brief Build the backend-readable path used for Entropy-exported affine initialization.
 * @param job Registration job.
 * @return Greedy matrix path for Greedy jobs, or ITK transform path for ITK-based backends.
 */
std::filesystem::path initialAffineInputPath(const JobSpec& job);

/**
 * @brief Build the ANTs output prefix path passed to --output.
 * @param job Registration job.
 * @return ANTs output prefix in the job output directory.
 */
std::filesystem::path antsOutputPrefixPath(const JobSpec& job);

/**
 * @brief Build the affine transform path written by ANTs.
 * @param job Registration job.
 * @return ANTs affine transform path.
 */
std::filesystem::path antsAffineTransformPath(const JobSpec& job);

/**
 * @brief Build the inverse/sampling warp path written by ANTs.
 * @param job Registration job.
 * @return ANTs fixed-to-moving sampling warp path.
 */
std::filesystem::path antsWarpPath(const JobSpec& job);

/**
 * @brief Build the forward point-warp path written by ANTs.
 * @param job Registration job.
 * @return ANTs moving-to-fixed inverse-warp output path.
 */
std::filesystem::path antsInverseWarpPath(const JobSpec& job);

/**
 * @brief Build the manifest Entropy expects a backend to produce for a successful job.
 * @param job Registration job.
 * @return Result manifest populated with requested output artifact paths.
 */
ResultManifest buildExpectedResultManifest(const JobSpec& job);

/**
 * @brief Build the input artifact plan for a job.
 * @param job Registration job.
 * @return Inputs that are already file-backed or must be exported by the app layer.
 */
std::vector<InputArtifact> buildInputArtifactPlan(const JobSpec& job);

} // namespace registration
