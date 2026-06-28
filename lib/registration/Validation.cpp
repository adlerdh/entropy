#include "registration/Validation.h"

#include "registration/Capabilities.h"

#include <algorithm>

namespace registration
{
namespace
{

bool hasPathOrUid(const DataRef& ref)
{
  return !ref.uid.empty() || !ref.fileName.empty();
}

void addMessage(ValidationResult& result, ValidationSeverity severity, std::string field, std::string text)
{
  result.messages.push_back(ValidationMessage{severity, std::move(field), std::move(text)});
}

void requireFeature(
  ValidationResult& result,
  const BackendCapabilities& capabilities,
  Feature feature,
  const char* field,
  const char* text)
{
  if (!supportsFeature(capabilities, feature)) {
    addMessage(result, ValidationSeverity::Error, field, text);
  }
}

} // namespace

bool ValidationResult::canLaunch() const
{
  return std::none_of(messages.begin(), messages.end(), [](const ValidationMessage& message) {
    return message.severity == ValidationSeverity::Error;
  });
}

ValidationResult validateJob(const JobSpec& job, const BackendCapabilities& capabilities)
{
  ValidationResult result;

  if (!hasPathOrUid(job.fixedImage)) {
    addMessage(result, ValidationSeverity::Error, "fixedImage", "Choose a fixed/reference image.");
  }
  if (!hasPathOrUid(job.movingImage)) {
    addMessage(result, ValidationSeverity::Error, "movingImage", "Choose a moving image.");
  }
  if (!job.fixedImage.uid.empty() && job.fixedImage.uid == job.movingImage.uid) {
    addMessage(result, ValidationSeverity::Error, "movingImage", "The moving image must differ from the fixed image.");
  }
  if (!supportsTransformModel(capabilities, job.transformModel)) {
    addMessage(
      result,
      ValidationSeverity::Error,
      "transformModel",
      "The selected backend does not support this transform model.");
  }
  if (!supportsMetric(capabilities, job.metric)) {
    addMessage(result, ValidationSeverity::Error, "metric", "The selected backend does not support this metric.");
  }

  if (hasPathOrUid(job.fixedMask)) {
    requireFeature(
      result,
      capabilities,
      Feature::FixedMask,
      "fixedMask",
      "The selected backend does not support fixed masks.");
  }
  if (hasPathOrUid(job.movingMask)) {
    requireFeature(
      result,
      capabilities,
      Feature::MovingMask,
      "movingMask",
      "The selected backend does not support moving masks.");
  }
  if (!job.auxiliaryImagePairs.empty()) {
    requireFeature(
      result,
      capabilities,
      Feature::AuxiliaryImagePairs,
      "auxiliaryImagePairs",
      "The selected backend does not support auxiliary image pairs.");
  }
  if (job.landmarks.enabled) {
    if (job.landmarks.matchedPairs == 0) {
      addMessage(result, ValidationSeverity::Error, "landmarks", "No matched landmark pairs are available.");
    }
    if (!supportsFeature(capabilities, Feature::LandmarkTransform)) {
      addMessage(
        result,
        ValidationSeverity::Error,
        "landmarks",
        "The selected backend does not support landmark output.");
    }
    if (!supportsFeature(capabilities, Feature::LandmarkDrivenRegistration)) {
      addMessage(
        result,
        ValidationSeverity::Warning,
        "landmarks",
        "Landmarks will be transformed/evaluated after registration, but will not drive optimization for this "
        "backend.");
    }
  }
  if (!job.surfaces.empty()) {
    requireFeature(
      result,
      capabilities,
      Feature::SurfaceTransform,
      "surfaces",
      "The selected backend does not support transformed surface outputs.");
  }
  if (job.outputs.loadInverseWarp) {
    requireFeature(
      result,
      capabilities,
      Feature::InverseWarpOutput,
      "outputs.inverseWarp",
      "The selected backend does not support inverse/sampling warp output.");
  }
  if (job.outputs.loadForwardWarp) {
    requireFeature(
      result,
      capabilities,
      Feature::ForwardWarpOutput,
      "outputs.forwardWarp",
      "The selected backend does not support forward warp output.");
  }
  if (job.outputs.loadWarpedSegmentation) {
    requireFeature(
      result,
      capabilities,
      Feature::WarpedSegmentationOutput,
      "outputs.warpedSegmentation",
      "The selected backend does not support warped segmentation output.");
    if (!hasPathOrUid(job.movingMask)) {
      addMessage(
        result,
        ValidationSeverity::Error,
        "outputs.warpedSegmentation",
        "Choose a moving segmentation before requesting warped segmentation output.");
    }
  }
  if (job.outputs.transformSurfaces && job.surfaces.empty()) {
    addMessage(
      result,
      ValidationSeverity::Warning,
      "outputs.transformSurfaces",
      "No surfaces are selected for transformation.");
  }

  if (job.outputPrefix.empty()) {
    addMessage(
      result,
      ValidationSeverity::Warning,
      "outputPrefix",
      "Entropy will generate an output prefix for this job.");
  }

  return result;
}

} // namespace registration
