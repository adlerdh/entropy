#include "registration/Setup.h"

#include "registration/Artifacts.h"
#include "registration/Capabilities.h"

#include <algorithm>
#include <cctype>

namespace registration
{
namespace
{

const SetupImageChoice* firstReferenceImage(const std::vector<SetupImageChoice>& images)
{
  const auto it =
    std::find_if(images.begin(), images.end(), [](const SetupImageChoice& choice) { return choice.isReference; });
  return it == images.end() ? nullptr : &*it;
}

const SetupImageChoice* firstActiveMovingImage(const std::vector<SetupImageChoice>& images, const std::string& fixedUid)
{
  const auto it = std::find_if(images.begin(), images.end(), [&](const SetupImageChoice& choice) {
    return choice.isActive && choice.image.uid != fixedUid;
  });
  return it == images.end() ? nullptr : &*it;
}

const SetupImageChoice* firstMovingImage(const std::vector<SetupImageChoice>& images, const std::string& fixedUid)
{
  const auto it = std::find_if(images.begin(), images.end(), [&](const SetupImageChoice& choice) {
    return choice.image.uid != fixedUid;
  });
  return it == images.end() ? nullptr : &*it;
}

TransformModel defaultTransformModel(const BackendCapabilities& capabilities)
{
  if (supportsTransformModel(capabilities, TransformModel::AffineDeformable)) {
    return TransformModel::AffineDeformable;
  }
  return capabilities.transformModels.empty() ? TransformModel::Affine : capabilities.transformModels.front();
}

Metric defaultMetric(const BackendCapabilities& capabilities)
{
  if (
    (capabilities.backend == Backend::ANTs || capabilities.backend == Backend::FireANTs) &&
    supportsMetric(capabilities, Metric::MI))
  {
    return Metric::MI;
  }
  if (supportsMetric(capabilities, Metric::WNCC)) {
    return Metric::WNCC;
  }
  if (supportsMetric(capabilities, Metric::CC)) {
    return Metric::CC;
  }
  return capabilities.metrics.empty() ? Metric::NCC : capabilities.metrics.front();
}

std::string safeName(std::string value)
{
  for (char& ch : value) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (!std::isalnum(uch)) {
      ch = '_';
    }
  }

  value.erase(
    std::unique(value.begin(), value.end(), [](char a, char b) { return a == '_' && b == '_'; }),
    value.end());
  while (!value.empty() && value.front() == '_') {
    value.erase(value.begin());
  }
  while (!value.empty() && value.back() == '_') {
    value.pop_back();
  }
  return value.empty() ? "registration" : value;
}

std::string defaultOutputPrefix(const DataRef& fixedImage, const DataRef& movingImage)
{
  if (fixedImage.displayName.empty() || movingImage.displayName.empty()) {
    return "registration";
  }
  return safeName(movingImage.displayName + "_to_" + fixedImage.displayName);
}

std::vector<ParameterValue> defaultParameterValues(const BackendCapabilities& capabilities)
{
  std::vector<ParameterValue> values;
  values.reserve(capabilities.parameters.size());
  for (const ParameterSchema& parameter : capabilities.parameters) {
    values.push_back(ParameterValue{parameter.key, parameter.defaultValue});
  }
  return values;
}

std::string parameterValueOr(const JobSpec& job, const std::string& key, const std::string& fallback)
{
  const auto it =
    std::find_if(job.parameterValues.begin(), job.parameterValues.end(), [&](const ParameterValue& value) {
      return value.key == key;
    });
  return it == job.parameterValues.end() ? fallback : it->value;
}

bool includesRigidStage(TransformModel model)
{
  return model == TransformModel::Rigid || model == TransformModel::RigidAffine ||
         model == TransformModel::RigidAffineDeformable;
}

bool parameterAppliesToJob(const ParameterSchema& parameter, const JobSpec& job)
{
  if (job.backend == Backend::Greedy && parameter.key == "wnccRadius") {
    return job.metric == Metric::NCC || job.metric == Metric::WNCC;
  }
  if (job.backend == Backend::FireANTs) {
    if (parameter.key == "ccKernelSize" || parameter.key == "ccKernelType") {
      return job.metric == Metric::CC;
    }
    if (parameter.key == "miKernel" || parameter.key == "miBins") {
      return job.metric == Metric::MI;
    }
    if (parameter.key == "rigidScaling") {
      return includesRigidStage(job.transformModel);
    }
    if (parameter.key == "centerOfFrameInitialization" || parameter.key == "aroundCenter") {
      return includesAffineTransform(job.transformModel);
    }
    if (parameter.key == "blurImages") {
      return includesAffineTransform(job.transformModel) || includesDeformableTransform(job.transformModel);
    }
    if (
      parameter.key == "momentOrder" || parameter.key == "momentPerformScaling" ||
      parameter.key == "momentOrientation" || parameter.key == "momentScale" || parameter.key == "momentLossType")
    {
      return parameterValueOr(job, "initialMovingTransform", "None") != "None";
    }
    if (
      parameter.key == "deformableTransform" || parameter.key == "deformationModel" ||
      parameter.key == "lossReduction" || parameter.key == "smoothWarpSigma" || parameter.key == "smoothGradSigma")
    {
      return includesDeformableTransform(job.transformModel);
    }
    if (
      parameter.key == "levenbergLambdaInit" || parameter.key == "levenbergLambdaIncrease" ||
      parameter.key == "levenbergLambdaDecrease")
    {
      return includesDeformableTransform(job.transformModel) &&
             parameterValueOr(job, "deformableOptimizer", "Use optimizer setting") == "Levenberg-Marquardt";
    }
    if (parameter.key == "integratorN") {
      return includesDeformableTransform(job.transformModel) &&
             parameterValueOr(job, "deformationModel", "compositive") == "geodesic";
    }
    if (parameter.key == "greedyFreeform") {
      return includesDeformableTransform(job.transformModel) &&
             parameterValueOr(job, "deformableTransform", "Greedy") == "Greedy";
    }
  }
  if (job.backend == Backend::ANTs) {
    if (parameter.key == "synUpdateFieldVariance" || parameter.key == "synTotalFieldVariance") {
      return job.transformModel == TransformModel::Deformable || job.transformModel == TransformModel::AffineDeformable;
    }
    if (
      parameter.key == "bsplineSyNUpdateMeshSize" || parameter.key == "bsplineSyNTotalMeshSize" ||
      parameter.key == "bsplineSyNSplineOrder")
    {
      return job.transformModel == TransformModel::BSplineDisplacement;
    }
    if (parameter.key == "gaussianDisplacementUpdateVariance" || parameter.key == "gaussianDisplacementTotalVariance") {
      return job.transformModel == TransformModel::GaussianDisplacement;
    }
    if (
      parameter.key == "timeVaryingVelocityTimeIndices" || parameter.key == "timeVaryingVelocityUpdateVariance" ||
      parameter.key == "timeVaryingVelocityUpdateTimeVariance" || parameter.key == "timeVaryingVelocityTotalVariance")
    {
      return job.transformModel == TransformModel::TimeVaryingVelocity;
    }
  }
  return true;
}

void preserveParameterValues(std::vector<ParameterValue>& values, const std::vector<ParameterValue>& oldValues)
{
  for (ParameterValue& value : values) {
    const auto it = std::find_if(oldValues.begin(), oldValues.end(), [&](const ParameterValue& oldValue) {
      return oldValue.key == value.key;
    });
    if (it != oldValues.end()) {
      value.value = it->value;
    }
  }
}

void normalizeOutputsForCapabilities(JobSpec& job, const BackendCapabilities& capabilities)
{
  if (!supportsFeature(capabilities, Feature::InverseWarpOutput)) {
    job.outputs.loadInverseWarp = false;
    job.outputs.applyWarpToMovingImage = false;
  }
  if (!supportsFeature(capabilities, Feature::ForwardWarpOutput)) {
    job.outputs.loadForwardWarp = false;
  }
  if (!supportsFeature(capabilities, Feature::WarpedSegmentationOutput)) {
    job.outputs.loadWarpedSegmentation = false;
  }
  if (!supportsFeature(capabilities, Feature::LandmarkTransform)) {
    job.outputs.transformLandmarksAndAnnotations = false;
  }
  if (!supportsFeature(capabilities, Feature::SurfaceTransform)) {
    job.outputs.transformSurfaces = false;
  }
}

void normalizeInitializationForCapabilities(JobSpec& job, const BackendCapabilities& capabilities)
{
  if (capabilities.backend == Backend::FireANTs) {
    job.useCurrentAffineTransformsForInitialization = false;
    job.initialAffineTransform.clear();
    job.useImageCentersForInitialization = false;
    return;
  }

  if (job.useCurrentAffineTransformsForInitialization) {
    job.useImageCentersForInitialization = false;
  }
}

void normalizeJobForCapabilities(JobSpec& job, const BackendCapabilities& capabilities)
{
  job.backend = capabilities.backend;
  if (!supportsTransformModel(capabilities, job.transformModel)) {
    job.transformModel = defaultTransformModel(capabilities);
  }
  if (!supportsMetric(capabilities, job.metric)) {
    job.metric = defaultMetric(capabilities);
  }
  normalizeOutputsForCapabilities(job, capabilities);
  normalizeInitializationForCapabilities(job, capabilities);
}

std::filesystem::path previewInitialAffinePath(const JobSpec& job)
{
  JobSpec previewJob = job;
  std::filesystem::path outputDirectory = previewJob.outputDirectory;
  if (outputDirectory == std::filesystem::temp_directory_path()) {
    outputDirectory /= "entropy-registration";
    outputDirectory /= "<job-id>";
  }
  previewJob.outputDirectory = outputDirectory;
  return initialAffineInputPath(previewJob);
}

void materializePreviewOnlyInputs(JobSpec& job)
{
  if (
    job.useCurrentAffineTransformsForInitialization && includesAffineTransform(job.transformModel) &&
    job.initialAffineTransform.empty())
  {
    job.initialAffineTransform = previewInitialAffinePath(job);
  }
}

} // namespace

SetupState createSetupState(
  const std::vector<SetupImageChoice>& images,
  Backend backend,
  const std::filesystem::path& outputDirectory)
{
  SetupState state;
  state.capabilities = capabilitiesForBackend(backend);
  state.job.backend = backend;
  state.job.outputDirectory = outputDirectory;
  state.job.transformModel = defaultTransformModel(state.capabilities);
  state.job.metric = defaultMetric(state.capabilities);
  normalizeOutputsForCapabilities(state.job, state.capabilities);
  normalizeInitializationForCapabilities(state.job, state.capabilities);

  const SetupImageChoice* fixed = firstReferenceImage(images);
  if (!fixed && !images.empty()) {
    fixed = &images.front();
  }
  if (fixed) {
    state.job.fixedImage = fixed->image;
    state.job.dimension = fixed->dimension;
  }

  const SetupImageChoice* moving = fixed ? firstActiveMovingImage(images, fixed->image.uid) : nullptr;
  if (!moving && fixed) {
    moving = firstMovingImage(images, fixed->image.uid);
  }
  if (moving) {
    state.job.movingImage = moving->image;
  }

  state.job.outputPrefix = defaultOutputPrefix(state.job.fixedImage, state.job.movingImage);
  state.parameterValues = defaultParameterValues(state.capabilities);
  state.job.parameterValues = state.parameterValues;
  refreshValidation(state);
  return state;
}

void setBackend(SetupState& state, Backend backend)
{
  const Backend oldBackend = state.job.backend;
  const std::vector<ParameterValue> oldValues = state.parameterValues;
  state.capabilities = capabilitiesForBackend(backend);
  normalizeJobForCapabilities(state.job, state.capabilities);
  if (oldBackend != backend) {
    state.job.metric = defaultMetric(state.capabilities);
  }
  state.parameterValues = defaultParameterValues(state.capabilities);
  preserveParameterValues(state.parameterValues, oldValues);
  state.job.parameterValues = state.parameterValues;
  refreshValidation(state);
}

void setFixedImage(SetupState& state, const SetupImageChoice& image)
{
  state.job.fixedImage = image.image;
  state.job.dimension = image.dimension;
  state.job.outputPrefix = defaultOutputPrefix(state.job.fixedImage, state.job.movingImage);
  refreshValidation(state);
}

void setMovingImage(SetupState& state, const SetupImageChoice& image)
{
  state.job.movingImage = image.image;
  state.job.outputPrefix = defaultOutputPrefix(state.job.fixedImage, state.job.movingImage);
  refreshValidation(state);
}

void refreshValidation(SetupState& state)
{
  state.validation = validateJob(state.job, state.capabilities);
}

std::vector<ParameterSchema> visibleParameters(const SetupState& state)
{
  std::vector<ParameterSchema> parameters;
  for (const ParameterSchema& parameter : state.capabilities.parameters) {
    if (!parameterAppliesToJob(parameter, state.job)) {
      continue;
    }
    if (parameter.expert && !state.showExpertParameters) {
      continue;
    }
    if (parameter.advanced && !state.showAdvancedParameters && !state.showExpertParameters) {
      continue;
    }
    parameters.push_back(parameter);
  }
  return parameters;
}

ParameterValue* findParameterValue(SetupState& state, std::string_view key)
{
  const auto it =
    std::find_if(state.parameterValues.begin(), state.parameterValues.end(), [&](const ParameterValue& value) {
      return value.key == key;
    });
  return it == state.parameterValues.end() ? nullptr : &*it;
}

const ParameterValue* findParameterValue(const SetupState& state, std::string_view key)
{
  const auto it =
    std::find_if(state.parameterValues.begin(), state.parameterValues.end(), [&](const ParameterValue& value) {
      return value.key == key;
    });
  return it == state.parameterValues.end() ? nullptr : &*it;
}

std::vector<std::string> commandPreviews(const SetupState& state, const CommandGenerationOptions& commandOptions)
{
  if (!state.validation.canLaunch()) {
    return {};
  }

  JobSpec job = state.job;
  job.parameterValues = state.parameterValues;
  materializePreviewOnlyInputs(job);
  const std::vector<CommandSpec> commands = generateCommands(job, commandOptions);
  std::vector<std::string> previews;
  previews.reserve(commands.size());
  for (const CommandSpec& command : commands) {
    previews.push_back(displayCommand(command));
  }
  return previews;
}

std::vector<std::string> commandPreviews(const SetupState& state)
{
  return commandPreviews(state, CommandGenerationOptions{});
}

} // namespace registration
