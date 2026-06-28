#include "registration/ImportPlan.h"

namespace registration
{
namespace
{

bool hasPath(const std::filesystem::path& path)
{
  return !path.empty();
}

std::string nameOrUid(const DataRef& image)
{
  if (!image.displayName.empty()) {
    return image.displayName;
  }
  if (!image.uid.empty()) {
    return image.uid;
  }
  return "image";
}

void addMissingWarning(ImportPlan& plan, const char* artifactName)
{
  plan.warnings.push_back(std::string("Requested output is missing from result manifest: ") + artifactName);
}

void addStep(
  ImportPlan& plan,
  ImportAction action,
  std::filesystem::path path,
  std::string displayName,
  std::string targetImageUid)
{
  ImportStep step;
  step.action = action;
  step.path = std::move(path);
  step.displayName = std::move(displayName);
  step.targetImageUid = std::move(targetImageUid);
  plan.steps.push_back(std::move(step));
}

} // namespace

std::string warpedImageName(const DataRef& fixedImage, const DataRef& movingImage)
{
  return nameOrUid(movingImage) + " registered to " + nameOrUid(fixedImage);
}

std::string inverseWarpName(const DataRef& fixedImage, const DataRef& movingImage)
{
  return nameOrUid(movingImage) + " inverse warp to " + nameOrUid(fixedImage);
}

std::string forwardWarpName(const DataRef& fixedImage, const DataRef& movingImage)
{
  return nameOrUid(movingImage) + " forward warp from " + nameOrUid(fixedImage);
}

ImportPlan buildImportPlan(const JobSpec& job, const ResultManifest& manifest)
{
  ImportPlan plan;
  plan.warnings = manifest.warnings;

  if (job.outputs.loadInverseWarp) {
    if (hasPath(manifest.inverseWarp)) {
      addStep(
        plan,
        ImportAction::LoadInverseWarp,
        manifest.inverseWarp,
        inverseWarpName(job.fixedImage, job.movingImage),
        job.movingImage.uid);
    }
    else {
      addMissingWarning(plan, "inverse warp");
    }
  }

  if (job.outputs.loadForwardWarp) {
    if (hasPath(manifest.forwardWarp)) {
      addStep(
        plan,
        ImportAction::LoadForwardWarp,
        manifest.forwardWarp,
        forwardWarpName(job.fixedImage, job.movingImage),
        job.movingImage.uid);
    }
    else {
      addMissingWarning(plan, "forward warp");
    }
  }

  if (job.outputs.applyWarpToMovingImage && (hasPath(manifest.inverseWarp) || hasPath(manifest.forwardWarp))) {
    addStep(plan, ImportAction::AssignWarpsToMovingImage, {}, {}, job.movingImage.uid);
  }

  if (job.outputs.loadWarpedImage) {
    if (hasPath(manifest.warpedImage)) {
      addStep(
        plan,
        ImportAction::LoadWarpedImage,
        manifest.warpedImage,
        warpedImageName(job.fixedImage, job.movingImage),
        job.movingImage.uid);
    }
    else {
      addMissingWarning(plan, "warped image");
    }
  }

  if (job.outputs.loadWarpedSegmentation) {
    if (manifest.warpedSegmentations.empty()) {
      addMissingWarning(plan, "warped segmentation");
    }
    for (const std::filesystem::path& path : manifest.warpedSegmentations) {
      addStep(plan, ImportAction::LoadWarpedSegmentation, path, "Warped segmentation", job.movingImage.uid);
    }
  }

  if (job.outputs.transformLandmarksAndAnnotations) {
    for (const std::filesystem::path& path : manifest.transformedLandmarks) {
      addStep(plan, ImportAction::TransformLandmarksAndAnnotations, path, "Transformed landmarks", job.movingImage.uid);
    }
  }

  if (job.outputs.transformSurfaces) {
    if (manifest.transformedSurfaces.empty()) {
      addMissingWarning(plan, "transformed surface");
    }
    for (const std::filesystem::path& path : manifest.transformedSurfaces) {
      addStep(plan, ImportAction::LoadTransformedSurface, path, "Transformed surface", job.movingImage.uid);
    }
  }

  if (job.outputs.makeWarpedImageActive && hasPath(manifest.warpedImage)) {
    addStep(plan, ImportAction::MakeWarpedImageActive, {}, {}, job.movingImage.uid);
  }

  return plan;
}

} // namespace registration
