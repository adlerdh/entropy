#include "registration/Artifacts.h"

#include <iomanip>
#include <sstream>

namespace registration
{
namespace
{

std::string indexedSuffix(const char* prefix, std::size_t index, const char* extension)
{
  std::ostringstream stream;
  stream << prefix << '_' << std::setw(2) << std::setfill('0') << (index + 1) << extension;
  return stream.str();
}

void addInput(
  std::vector<InputArtifact>& artifacts,
  ArtifactRole role,
  const DataRef& source,
  const std::filesystem::path& plannedPath)
{
  if (source.source == DataSource::None && source.fileName.empty() && source.uid.empty()) {
    return;
  }

  InputArtifact artifact;
  artifact.role = role;
  artifact.source = source;
  artifact.path = source.fileName.empty() ? plannedPath : source.fileName;
  artifact.exportRequired = source.fileName.empty();
  artifacts.push_back(std::move(artifact));
}

} // namespace

std::string_view label(ArtifactRole role)
{
  switch (role) {
    case ArtifactRole::JobSpec:
      return "Job spec";
    case ArtifactRole::ResultManifest:
      return "Result manifest";
    case ArtifactRole::AffineTransform:
      return "Affine transform";
    case ArtifactRole::InverseWarp:
      return "Inverse warp";
    case ArtifactRole::ForwardWarp:
      return "Forward warp";
    case ArtifactRole::WarpedImage:
      return "Warped image";
    case ArtifactRole::WarpedSegmentation:
      return "Warped segmentation";
    case ArtifactRole::TransformedLandmarks:
      return "Transformed landmarks";
    case ArtifactRole::TransformedSurface:
      return "Transformed surface";
    case ArtifactRole::FixedMask:
      return "Fixed mask";
    case ArtifactRole::MovingMask:
      return "Moving mask";
    case ArtifactRole::AuxiliaryFixedImage:
      return "Auxiliary fixed image";
    case ArtifactRole::AuxiliaryMovingImage:
      return "Auxiliary moving image";
    case ArtifactRole::FixedLandmarks:
      return "Fixed landmarks";
    case ArtifactRole::MovingLandmarks:
      return "Moving landmarks";
    case ArtifactRole::Surface:
      return "Surface";
  }
  return "Artifact";
}

std::string outputPrefix(const JobSpec& job)
{
  return job.outputPrefix.empty() ? "registration" : job.outputPrefix;
}

std::filesystem::path artifactPath(const JobSpec& job, ArtifactRole role, std::size_t index)
{
  const std::string prefix = outputPrefix(job);

  switch (role) {
    case ArtifactRole::JobSpec:
      return job.outputDirectory / (prefix + "_job.json");
    case ArtifactRole::ResultManifest:
      return job.outputDirectory / (prefix + "_result.json");
    case ArtifactRole::AffineTransform:
      return job.outputDirectory / (prefix + "_affine.mat");
    case ArtifactRole::InverseWarp:
      return job.outputDirectory / (prefix + "_inverse_warp.nii.gz");
    case ArtifactRole::ForwardWarp:
      return job.outputDirectory / (prefix + "_forward_warp.nii.gz");
    case ArtifactRole::WarpedImage:
      return job.outputDirectory / (prefix + "_warped.nii.gz");
    case ArtifactRole::WarpedSegmentation:
      return job.outputDirectory / (prefix + indexedSuffix("_warped_segmentation", index, ".nii.gz"));
    case ArtifactRole::TransformedLandmarks:
      return job.outputDirectory / (prefix + indexedSuffix("_transformed_landmarks", index, ".json"));
    case ArtifactRole::TransformedSurface:
      return job.outputDirectory / (prefix + indexedSuffix("_transformed_surface", index, ".vtk"));
    case ArtifactRole::FixedMask:
      return job.outputDirectory / (prefix + "_fixed_mask.nii.gz");
    case ArtifactRole::MovingMask:
      return job.outputDirectory / (prefix + "_moving_mask.nii.gz");
    case ArtifactRole::AuxiliaryFixedImage:
      return job.outputDirectory / (prefix + indexedSuffix("_aux_fixed", index, ".nii.gz"));
    case ArtifactRole::AuxiliaryMovingImage:
      return job.outputDirectory / (prefix + indexedSuffix("_aux_moving", index, ".nii.gz"));
    case ArtifactRole::FixedLandmarks:
      return job.outputDirectory / (prefix + "_fixed_landmarks.json");
    case ArtifactRole::MovingLandmarks:
      return job.outputDirectory / (prefix + "_moving_landmarks.json");
    case ArtifactRole::Surface:
      return job.outputDirectory / (prefix + indexedSuffix("_surface", index, ".vtk"));
  }
  return job.outputDirectory / (prefix + "_artifact");
}

ResultManifest buildExpectedResultManifest(const JobSpec& job)
{
  ResultManifest manifest;
  manifest.backend = job.backend;
  manifest.fixedImageUid = job.fixedImage.uid;
  manifest.movingImageUid = job.movingImage.uid;
  manifest.warpConvention = "inverse_warp_samples_moving_from_fixed_space";

  if (job.outputs.loadWarpedImage) {
    manifest.warpedImage = artifactPath(job, ArtifactRole::WarpedImage);
  }
  if (job.outputs.loadInverseWarp || job.outputs.applyWarpToMovingImage) {
    manifest.inverseWarp = artifactPath(job, ArtifactRole::InverseWarp);
  }
  if (job.outputs.loadForwardWarp || job.outputs.transformLandmarksAndAnnotations || job.outputs.transformSurfaces) {
    manifest.forwardWarp = artifactPath(job, ArtifactRole::ForwardWarp);
  }
  if (job.outputs.loadWarpedSegmentation) {
    manifest.warpedSegmentations.push_back(artifactPath(job, ArtifactRole::WarpedSegmentation));
  }
  if (job.outputs.transformLandmarksAndAnnotations) {
    manifest.transformedLandmarks.push_back(artifactPath(job, ArtifactRole::TransformedLandmarks));
  }
  if (job.outputs.transformSurfaces) {
    manifest.transformedSurfaces.push_back(artifactPath(job, ArtifactRole::TransformedSurface));
  }

  manifest.affineTransform = artifactPath(job, ArtifactRole::AffineTransform);
  return manifest;
}

std::vector<InputArtifact> buildInputArtifactPlan(const JobSpec& job)
{
  std::vector<InputArtifact> artifacts;
  artifacts.reserve(2);

  addInput(artifacts, ArtifactRole::FixedMask, job.fixedMask, artifactPath(job, ArtifactRole::FixedMask));
  addInput(artifacts, ArtifactRole::MovingMask, job.movingMask, artifactPath(job, ArtifactRole::MovingMask));

  for (std::size_t index = 0; index < job.auxiliaryImagePairs.size(); ++index) {
    const AuxiliaryImagePair& pair = job.auxiliaryImagePairs[index];
    addInput(
      artifacts,
      ArtifactRole::AuxiliaryFixedImage,
      pair.fixed,
      artifactPath(job, ArtifactRole::AuxiliaryFixedImage, index));
    addInput(
      artifacts,
      ArtifactRole::AuxiliaryMovingImage,
      pair.moving,
      artifactPath(job, ArtifactRole::AuxiliaryMovingImage, index));
  }

  if (job.landmarks.enabled) {
    addInput(
      artifacts,
      ArtifactRole::FixedLandmarks,
      job.landmarks.fixedLandmarks,
      artifactPath(job, ArtifactRole::FixedLandmarks));
    addInput(
      artifacts,
      ArtifactRole::MovingLandmarks,
      job.landmarks.movingLandmarks,
      artifactPath(job, ArtifactRole::MovingLandmarks));
  }

  for (std::size_t index = 0; index < job.surfaces.size(); ++index) {
    addInput(artifacts, ArtifactRole::Surface, job.surfaces[index], artifactPath(job, ArtifactRole::Surface, index));
  }

  return artifacts;
}

} // namespace registration
