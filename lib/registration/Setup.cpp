#include "registration/Setup.h"

#include "registration/Artifacts.h"

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

bool parameterAppliesToJob(const ParameterSchema& parameter, const JobSpec& job)
{
  if (job.backend == Backend::Greedy && parameter.key == "wnccRadius") {
    return job.metric == Metric::NCC || job.metric == Metric::WNCC;
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

void normalizeJobForCapabilities(JobSpec& job, const BackendCapabilities& capabilities)
{
  job.backend = capabilities.backend;
  if (!supportsTransformModel(capabilities, job.transformModel)) {
    job.transformModel = defaultTransformModel(capabilities);
  }
  if (!supportsMetric(capabilities, job.metric)) {
    job.metric = defaultMetric(capabilities);
  }
}

std::filesystem::path previewInitialAffinePath(const JobSpec& job)
{
  std::filesystem::path outputDirectory = job.outputDirectory;
  if (outputDirectory == std::filesystem::temp_directory_path()) {
    outputDirectory /= "entropy-registration";
    outputDirectory /= "<job-id>";
  }
  return outputDirectory / (outputPrefix(job) + "_initial_affine.mat");
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
  if (state.job.useCurrentAffineTransformsForInitialization) {
    state.job.useImageCentersForInitialization = false;
  }

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
  const std::vector<ParameterValue> oldValues = state.parameterValues;
  state.capabilities = capabilitiesForBackend(backend);
  normalizeJobForCapabilities(state.job, state.capabilities);
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
