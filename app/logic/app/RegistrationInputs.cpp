#include "logic/app/RegistrationInputs.h"
#include "logic/app/Data.h"
#include "image/ImageWriter.h"
#include "registration/AffineTransformIO.h"
#include "registration/Artifacts.h"

#include <spdlog/spdlog.h>
#include <spdlog/fmt/std.h>

#include <algorithm>

namespace fs = std::filesystem;

bool registration_inputs::materialize(AppData& data, registration::JobSpec& job)
{
  std::error_code directoryError;
  fs::create_directories(job.outputDirectory.parent_path(), directoryError);

  if (directoryError || !fs::create_directory(job.outputDirectory, directoryError)) {
    spdlog::error(
      "Cannot exclusively create registration workspace {}: {}",
      job.outputDirectory,
      directoryError.message());
    return false;
  }

  auto parseUid = [](const registration::DataRef& ref) -> std::optional<uuids::uuid> {
    if (ref.uid.empty()) {
      return std::nullopt;
    }
    return uuids::uuid::from_string(ref.uid);
  };

  auto exportRef = [&](registration::DataRef& ref, const std::filesystem::path& path) -> bool {
    if (
      ref.source != registration::DataSource::LoadedImage && ref.source != registration::DataSource::Segmentation &&
      !ref.fileName.empty())
    {
      return true;
    }

    const std::optional<uuids::uuid> uid = parseUid(ref);
    if (!uid) {
      spdlog::error("Cannot export registration input '{}'; it has no valid UID", ref.displayName);
      return false;
    }

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
      spdlog::error("Cannot create registration input directory {}: {}", path.parent_path(), error.message());
      return false;
    }

    Image* image = nullptr;
    switch (ref.source) {
      case registration::DataSource::LoadedImage:
        image = data.image(*uid);
        break;
      case registration::DataSource::Segmentation:
        image = data.seg(*uid);
        break;
      case registration::DataSource::None:
      case registration::DataSource::LandmarkGroup:
      case registration::DataSource::AnnotationSet:
      case registration::DataSource::Surface:
      case registration::DataSource::ExternalFile:
        break;
    }

    if (!image) {
      spdlog::error(
        "Cannot export registration input '{}' with UID {}; unsupported source type or object is missing",
        ref.displayName,
        ref.uid);
      return false;
    }

    if (job.dimension != (image->header().pixelDimensions().z == 1u ? 2 : 3)) {
      spdlog::error("Registration input '{}' has a different spatial dimension than the job", ref.displayName);
      return false;
    }

    const auto result = image_io::writeImage(
      *image,
      path,
      {.component = ref.component, .timePoint = ref.timePoint, .writeTwoDimensional = job.dimension == 2});

    if (!result) {
      spdlog::error("Cannot export registration input '{}' to {}", ref.displayName, path);
      return false;
    }

    ref.fileName = path;
    job.ownedInputFiles.push_back(path);
    spdlog::info("Exported registration input '{}' to {}", ref.displayName, path);
    return true;
  };

  auto exportArtifact = [&](const registration::InputArtifact& artifact) -> bool {
    switch (artifact.role) {
      case registration::ArtifactRole::FixedMask:
        return exportRef(job.fixedMask, artifact.path);
      case registration::ArtifactRole::MovingMask:
        return exportRef(job.movingMask, artifact.path);
      case registration::ArtifactRole::AuxiliaryFixedImage:
      case registration::ArtifactRole::AuxiliaryMovingImage: {
        for (std::size_t index = 0; index < job.auxiliaryImagePairs.size(); ++index) {
          auto& pair = job.auxiliaryImagePairs[index];
          if (artifact.path != registration::artifactPath(job, artifact.role, index)) continue;
          if (artifact.role == registration::ArtifactRole::AuxiliaryFixedImage) {
            return exportRef(pair.fixed, artifact.path);
          }
          if (artifact.role == registration::ArtifactRole::AuxiliaryMovingImage) {
            return exportRef(pair.moving, artifact.path);
          }
        }
        break;
      }
      case registration::ArtifactRole::FixedLandmarks:
      case registration::ArtifactRole::MovingLandmarks:
      case registration::ArtifactRole::Surface:
        spdlog::error(
          "Registration input export for {} is not implemented yet; provide a file-backed input for now",
          registration::label(artifact.role));
        return false;
      case registration::ArtifactRole::JobSpec:
      case registration::ArtifactRole::ResultManifest:
      case registration::ArtifactRole::AffineTransform:
      case registration::ArtifactRole::InverseWarp:
      case registration::ArtifactRole::ForwardWarp:
      case registration::ArtifactRole::WarpedImage:
      case registration::ArtifactRole::WarpedSegmentation:
      case registration::ArtifactRole::TransformedLandmarks:
      case registration::ArtifactRole::TransformedSurface:
        break;
    }

    spdlog::error("Unable to match registration input artifact {}", registration::label(artifact.role));
    return false;
  };

  if (job.useCurrentAffineTransformsForInitialization) {
    const std::optional<uuids::uuid> movingUid = parseUid(job.movingImage);
    const Image* movingImage = movingUid ? data.image(*movingUid) : nullptr;
    if (!movingImage) {
      spdlog::error("Cannot export current affine initialization; moving image is missing");
      return false;
    }

    std::error_code error;
    std::filesystem::create_directories(job.outputDirectory, error);
    if (error) {
      spdlog::error("Cannot create registration output directory {}: {}", job.outputDirectory, error.message());
      return false;
    }

    const glm::dmat4 currentDisplayAffine{movingImage->transformations().worldDef_T_subject()};
    const glm::dmat4 currentSamplingAffine = glm::inverse(currentDisplayAffine);
    job.initialAffineTransform = registration::initialAffineInputPath(job);
    const bool saved =
      job.backend == registration::Backend::Greedy
        ? registration::writeGreedyAffineTransform(job.initialAffineTransform, currentSamplingAffine)
        : registration::writeItkAffineTransform(job.initialAffineTransform, currentSamplingAffine, job.dimension);
    if (!saved) {
      spdlog::error("Cannot save current affine initialization to {}", job.initialAffineTransform);
      job.initialAffineTransform.clear();
      return false;
    }
    job.useImageCentersForInitialization = false;
    job.ownedInputFiles.push_back(job.initialAffineTransform);
    spdlog::info("Exported registration initial affine transform to {}", job.initialAffineTransform);
  }

  if (
    !exportRef(job.fixedImage, job.outputDirectory / "fixed_input.nii.gz") ||
    !exportRef(job.movingImage, job.outputDirectory / "moving_input.nii.gz"))
    return false;

  // Loaded objects are snapshots of current pixels/header geometry, never aliases of old disk files.
  const auto clearLoadedPath = [](registration::DataRef& ref) {
    if (ref.source == registration::DataSource::LoadedImage || ref.source == registration::DataSource::Segmentation)
      ref.fileName.clear();
  };

  clearLoadedPath(job.fixedMask);
  clearLoadedPath(job.movingMask);
  for (auto& pair : job.auxiliaryImagePairs) {
    clearLoadedPath(pair.fixed);
    clearLoadedPath(pair.moving);
  }

  const std::vector<registration::InputArtifact> inputArtifacts = registration::buildInputArtifactPlan(job);
  return std::ranges::all_of(inputArtifacts, [&exportArtifact](const registration::InputArtifact& artifact) {
    return !artifact.exportRequired || exportArtifact(artifact);
  });
}
