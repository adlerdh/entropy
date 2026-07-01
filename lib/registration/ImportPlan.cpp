#include "registration/ImportPlan.h"

#include "registration/Artifacts.h"

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

ResultManifest manifestWithExpectedAntsArtifacts(const JobSpec& job, ResultManifest manifest)
{
  if (job.backend != Backend::ANTs || !includesDeformableTransform(job.transformModel)) {
    return manifest;
  }

  const ResultManifest expected = buildExpectedResultManifest(job);
  if (manifest.inverseWarp.empty()) {
    manifest.inverseWarp = expected.inverseWarp;
  }
  if (manifest.forwardWarp.empty()) {
    manifest.forwardWarp = expected.forwardWarp;
  }
  if (manifest.warpedImage.empty()) {
    manifest.warpedImage = expected.warpedImage;
  }
  if (manifest.affineTransform.empty()) {
    manifest.affineTransform = expected.affineTransform;
  }
  return manifest;
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
  const ResultManifest resolvedManifest = manifestWithExpectedAntsArtifacts(job, manifest);
  plan.warnings = resolvedManifest.warnings;
  const bool hasDeformableOutput = includesDeformableTransform(job.transformModel);

  if (job.outputs.loadAffineTransform && hasPath(resolvedManifest.affineTransform)) {
    addStep(
      plan,
      ImportAction::ApplyAffineTransform,
      resolvedManifest.affineTransform,
      "Affine transform",
      job.movingImage.uid);
  }

  if (job.outputs.loadWarpedImage) {
    if (hasPath(resolvedManifest.warpedImage)) {
      addStep(
        plan,
        ImportAction::LoadWarpedImage,
        resolvedManifest.warpedImage,
        warpedImageName(job.fixedImage, job.movingImage),
        job.movingImage.uid);
    }
    else {
      addMissingWarning(plan, "warped image");
    }
  }

  if (hasDeformableOutput && job.outputs.loadInverseWarp) {
    if (hasPath(resolvedManifest.inverseWarp)) {
      addStep(
        plan,
        ImportAction::LoadInverseWarp,
        resolvedManifest.inverseWarp,
        inverseWarpName(job.fixedImage, job.movingImage),
        job.movingImage.uid);
    }
    else {
      addMissingWarning(plan, "inverse warp");
    }
  }

  if (hasDeformableOutput && job.outputs.loadForwardWarp) {
    if (hasPath(resolvedManifest.forwardWarp)) {
      addStep(
        plan,
        ImportAction::LoadForwardWarp,
        resolvedManifest.forwardWarp,
        forwardWarpName(job.fixedImage, job.movingImage),
        job.movingImage.uid);
    }
    else {
      addMissingWarning(plan, "forward warp");
    }
  }

  if (
    hasDeformableOutput && job.outputs.applyWarpToMovingImage &&
    (hasPath(resolvedManifest.inverseWarp) || hasPath(resolvedManifest.forwardWarp)))
  {
    addStep(plan, ImportAction::AssignWarpsToMovingImage, {}, {}, job.movingImage.uid);
  }

  if (job.outputs.loadWarpedSegmentation) {
    if (resolvedManifest.warpedSegmentations.empty()) {
      addMissingWarning(plan, "warped segmentation");
    }
    for (const std::filesystem::path& path : resolvedManifest.warpedSegmentations) {
      addStep(plan, ImportAction::LoadWarpedSegmentation, path, "Warped segmentation", job.movingImage.uid);
    }
  }

  if (job.outputs.transformLandmarksAndAnnotations) {
    for (const std::filesystem::path& path : resolvedManifest.transformedLandmarks) {
      addStep(plan, ImportAction::TransformLandmarksAndAnnotations, path, "Transformed landmarks", job.movingImage.uid);
    }
  }

  if (job.outputs.transformSurfaces) {
    if (resolvedManifest.transformedSurfaces.empty()) {
      addMissingWarning(plan, "transformed surface");
    }
    for (const std::filesystem::path& path : resolvedManifest.transformedSurfaces) {
      addStep(plan, ImportAction::LoadTransformedSurface, path, "Transformed surface", job.movingImage.uid);
    }
  }

  if (job.outputs.makeWarpedImageActive && hasPath(resolvedManifest.warpedImage)) {
    addStep(plan, ImportAction::MakeWarpedImageActive, {}, {}, job.movingImage.uid);
  }

  return plan;
}

} // namespace registration
