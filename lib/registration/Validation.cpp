#include "registration/Validation.h"

#include "registration/Capabilities.h"

#include <algorithm>
#include <string>

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

std::string parameterValueOr(const JobSpec& job, const std::string& key, const std::string& fallback)
{
  const auto it =
    std::find_if(job.parameterValues.begin(), job.parameterValues.end(), [&](const ParameterValue& value) {
      return value.key == key;
    });
  return it == job.parameterValues.end() ? fallback : it->value;
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
  if (
    job.backend == Backend::FireANTs && (hasPathOrUid(job.fixedMask) || hasPathOrUid(job.movingMask)) &&
    job.metric != Metric::CC && job.metric != Metric::MSE && job.metric != Metric::SSD)
  {
    addMessage(
      result,
      ValidationSeverity::Error,
      "metric",
      "FireANTs masked registration is documented for CC and MSE metrics. Choose CC or MSE when using masks.");
  }
  if (
    job.backend == Backend::FireANTs &&
    parameterValueOr(job, "deformableOptimizer", "Use optimizer setting") == "Levenberg-Marquardt" &&
    parameterValueOr(job, "deformationModel", "compositive") == "geodesic")
  {
    addMessage(
      result,
      ValidationSeverity::Error,
      "deformableOptimizer",
      "FireANTs Levenberg-Marquardt optimizer is only available for compositive deformable warps, not geodesic "
      "deformation.");
  }
  if (job.backend == Backend::FireANTs && !job.initialAffineTransform.empty()) {
    addMessage(
      result,
      ValidationSeverity::Error,
      "initialAffineTransform",
      "FireANTs does not currently support Entropy affine-transform files as initialization input.");
  }
  if (
    job.backend == Backend::FireANTs && includesDeformableTransform(job.transformModel) &&
    includesAffineTransform(job.transformModel) && job.outputs.loadAffineTransform)
  {
    addMessage(
      result,
      ValidationSeverity::Warning,
      "outputs.affineTransform",
      "FireANTs CLI currently writes only the final deformable transform for affine+deformable jobs. "
      "Entropy will import the composite warp, but a separate affine transform may not be available.");
  }
  if (
    job.backend == Backend::Greedy && hasPathOrUid(job.movingMask) && !includesDeformableTransform(job.transformModel))
  {
    addMessage(
      result,
      ValidationSeverity::Warning,
      "movingMask",
      "Greedy moving masks are applied during deformable registration only and will be ignored for affine-only "
      "registration.");
  }
  if (!job.auxiliaryImagePairs.empty()) {
    requireFeature(
      result,
      capabilities,
      Feature::AuxiliaryImagePairs,
      "auxiliaryImagePairs",
      "The selected backend does not support auxiliary image pairs.");
    for (std::size_t index = 0; index < job.auxiliaryImagePairs.size(); ++index) {
      const AuxiliaryImagePair& pair = job.auxiliaryImagePairs[index];
      if (!hasPathOrUid(pair.fixed) || !hasPathOrUid(pair.moving)) {
        addMessage(
          result,
          ValidationSeverity::Error,
          "auxiliaryImagePairs",
          "Auxiliary image pair " + std::to_string(index + 1u) + " must have both fixed and moving images.");
      }
      if (pair.weight <= 0.0) {
        addMessage(
          result,
          ValidationSeverity::Error,
          "auxiliaryImagePairs",
          "Auxiliary image pair " + std::to_string(index + 1u) + " must have a positive weight.");
      }
    }
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
