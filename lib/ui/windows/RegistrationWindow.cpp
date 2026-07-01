#include "ui/windows/RegistrationWindow.h"

#include "logic/app/Data.h"
#include "ui/Helpers.h"

#include "registration/Config.h"
#include "registration/Artifacts.h"
#include "registration/Setup.h"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <uuid.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cfloat>
#include <cstddef>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{

constexpr std::array k_backends{
  registration::Backend::ANTs,
  registration::Backend::FireANTs,
  registration::Backend::Greedy};

constexpr int k_minResolutionLevels = 1;
constexpr int k_maxResolutionLevels = 4;
constexpr int k_minIterationsPerLevel = 1;
constexpr int k_minShrinkFactor = 1;
constexpr int k_maxIterationsPerLevel = 512;
constexpr int k_maxShrinkFactor = 16;
constexpr float k_maxSmoothingSigma = 8.0f;
constexpr std::array k_defaultIterationsByLevel{128, 64, 32, 16};
constexpr std::array k_defaultShrinkFactorsByLevel{8, 4, 2, 1};
constexpr std::array k_defaultSmoothingSigmasByLevel{3.0, 2.0, 1.0, 0.0};

std::vector<registration::SetupImageChoice> imageChoices(const AppData& appData)
{
  std::vector<registration::SetupImageChoice> choices;
  choices.reserve(appData.numImages());

  const std::optional<uuids::uuid> refUid = appData.refImageUid();
  const std::optional<uuids::uuid> activeUid = appData.activeImageUid();
  for (const uuids::uuid& imageUid : appData.imageUidsOrdered()) {
    const Image* image = appData.image(imageUid);
    if (!image) {
      continue;
    }

    registration::SetupImageChoice choice;
    choice.image.uid = uuids::to_string(imageUid);
    choice.image.fileName = image->header().fileName();
    choice.image.displayName = image->settings().displayName();
    choice.image.source = registration::DataSource::LoadedImage;
    const glm::uvec3 dims = image->header().pixelDimensions();
    choice.dimension =
      static_cast<int>(std::max(1u, static_cast<unsigned>((dims.x > 1u) + (dims.y > 1u) + (dims.z > 1u))));
    choice.isReference = refUid && *refUid == imageUid;
    choice.isActive = activeUid && *activeUid == imageUid;
    choices.push_back(std::move(choice));
  }

  return choices;
}

struct DataChoice
{
  registration::DataRef ref;
  std::string label;
};

std::vector<DataChoice> maskChoices(const AppData& appData)
{
  std::vector<DataChoice> choices;
  for (const uuids::uuid& imageUid : appData.imageUidsOrdered()) {
    const Image* image = appData.image(imageUid);
    if (!image) {
      continue;
    }

    registration::DataRef imageRef;
    imageRef.uid = uuids::to_string(imageUid);
    imageRef.fileName = image->header().fileName();
    imageRef.displayName = image->settings().displayName();
    imageRef.source = registration::DataSource::LoadedImage;
    choices.push_back(DataChoice{imageRef, "Image: " + imageRef.displayName});

    for (const uuids::uuid& segUid : appData.imageToSegUids(imageUid)) {
      const Image* seg = appData.seg(segUid);
      if (!seg) {
        continue;
      }
      registration::DataRef segRef;
      segRef.uid = uuids::to_string(segUid);
      segRef.fileName = seg->header().fileName();
      segRef.displayName = seg->settings().displayName();
      segRef.source = registration::DataSource::Segmentation;
      choices.push_back(
        DataChoice{segRef, "Seg: " + segRef.displayName + " (" + image->settings().displayName() + ")"});
    }
  }
  return choices;
}

std::optional<std::string> outputDirectoryWarning(const std::string& text)
{
  if (text.empty()) {
    return std::nullopt;
  }

  std::error_code error;
  const std::filesystem::path path{text};
  if (!std::filesystem::exists(path, error)) {
    return "Folder does not exist.";
  }
  if (error) {
    return "Folder cannot be checked: " + error.message();
  }
  if (!std::filesystem::is_directory(path, error)) {
    return "Path exists, but is not a folder.";
  }
  if (error) {
    return "Folder cannot be checked: " + error.message();
  }
  return std::nullopt;
}

int backendIndex(registration::Backend backend)
{
  for (int i = 0; i < static_cast<int>(k_backends.size()); ++i) {
    if (k_backends.at(static_cast<std::size_t>(i)) == backend) {
      return i;
    }
  }
  return 0;
}

ImVec2 halfMainViewportSize()
{
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (!viewport) {
    return ImVec2{640.0f, 480.0f};
  }
  return ImVec2{std::max(360.0f, 0.5f * viewport->WorkSize.x), std::max(320.0f, 0.5f * viewport->WorkSize.y)};
}

std::string imageChoiceLabel(const registration::SetupImageChoice& choice)
{
  std::string label = choice.image.displayName.empty() ? choice.image.uid : choice.image.displayName;
  if (choice.isReference && choice.isActive) {
    label += " (reference + active)";
  }
  else if (choice.isReference) {
    label += " (reference)";
  }
  else if (choice.isActive) {
    label += " (active)";
  }
  return label;
}

const registration::SetupImageChoice* findImageChoice(
  const std::vector<registration::SetupImageChoice>& choices,
  const std::string& uid)
{
  const auto it = std::find_if(choices.begin(), choices.end(), [&](const registration::SetupImageChoice& choice) {
    return choice.image.uid == uid;
  });
  return it == choices.end() ? nullptr : &*it;
}

void renderImageChoiceCombo(
  const char* label,
  const std::vector<registration::SetupImageChoice>& choices,
  const registration::DataRef& current,
  const std::function<void(const registration::SetupImageChoice&)>& onSelected)
{
  const registration::SetupImageChoice* currentChoice = findImageChoice(choices, current.uid);
  const std::string preview = currentChoice ? imageChoiceLabel(*currentChoice) : std::string{"None"};
  if (ImGui::BeginCombo(label, preview.c_str())) {
    for (const registration::SetupImageChoice& choice : choices) {
      const bool selected = choice.image.uid == current.uid;
      const std::string choiceLabel = imageChoiceLabel(choice);
      if (ImGui::Selectable(choiceLabel.c_str(), selected)) {
        onSelected(choice);
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
}

const DataChoice* findDataChoice(const std::vector<DataChoice>& choices, const registration::DataRef& current)
{
  if (current.uid.empty()) {
    return nullptr;
  }
  const auto it = std::find_if(choices.begin(), choices.end(), [&](const DataChoice& choice) {
    return choice.ref.uid == current.uid && choice.ref.source == current.source;
  });
  return it == choices.end() ? nullptr : &*it;
}

bool renderDataChoiceCombo(
  const char* label,
  const std::vector<DataChoice>& choices,
  registration::DataRef& current,
  bool allowNone = true)
{
  const DataChoice* currentChoice = findDataChoice(choices, current);
  const std::string preview = currentChoice ? currentChoice->label : std::string{"None"};
  bool changed = false;

  if (ImGui::BeginCombo(label, preview.c_str())) {
    if (allowNone && ImGui::Selectable("None", current.uid.empty())) {
      current = registration::DataRef{};
      changed = true;
    }
    for (const DataChoice& choice : choices) {
      const bool selected = currentChoice && choice.ref.uid == current.uid && choice.ref.source == current.source;
      if (ImGui::Selectable(choice.label.c_str(), selected)) {
        current = choice.ref;
        changed = true;
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  return changed;
}

void renderValidation(const registration::ValidationResult& validation)
{
  if (validation.messages.empty()) {
    ImGui::TextColored(ImVec4{0.35f, 0.85f, 0.45f, 1.0f}, "Ready to start registration.");
    return;
  }

  for (const registration::ValidationMessage& message : validation.messages) {
    const bool isError = message.severity == registration::ValidationSeverity::Error;
    const ImVec4 color = isError ? ImVec4{0.95f, 0.35f, 0.30f, 1.0f} : ImVec4{0.95f, 0.75f, 0.25f, 1.0f};
    ImGui::TextColored(color, "%s: %s", isError ? "Error" : "Warning", message.text.c_str());
  }
}

void renderHelpMarker(const std::string& tooltip)
{
  if (!tooltip.empty()) {
    ImGui::SameLine();
    helpMarker(tooltip.c_str());
  }
}

int parseInteger(std::string_view value, int fallback)
{
  const std::string text{value};
  char* end = nullptr;
  const long parsed = std::strtol(text.c_str(), &end, 10);
  return end && *end == '\0' ? static_cast<int>(parsed) : fallback;
}

double parseDouble(std::string_view value, double fallback);

std::vector<int> parseIntegerSchedule(std::string_view value, std::string_view fallback)
{
  auto parse = [](std::string_view text) {
    std::vector<int> values;
    std::string token;
    for (const char ch : text) {
      if (ch == 'x' || ch == 'X' || ch == ',' || ch == ';' || std::isspace(static_cast<unsigned char>(ch))) {
        if (!token.empty()) {
          values.push_back(std::max(k_minIterationsPerLevel, parseInteger(token, 0)));
          token.clear();
        }
        continue;
      }
      token.push_back(ch);
    }
    if (!token.empty()) {
      values.push_back(std::max(k_minIterationsPerLevel, parseInteger(token, 0)));
    }
    return values;
  };

  std::vector<int> values = parse(value);
  if (values.empty()) {
    values = parse(fallback);
  }
  if (values.empty()) {
    values.push_back(100);
  }
  if (values.size() > static_cast<std::size_t>(k_maxResolutionLevels)) {
    values.resize(static_cast<std::size_t>(k_maxResolutionLevels));
  }
  return values;
}

std::string formatIntegerSchedule(const std::vector<int>& values)
{
  std::ostringstream stream;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i > 0u) {
      stream << 'x';
    }
    stream << std::max(k_minIterationsPerLevel, values.at(i));
  }
  return stream.str();
}

std::vector<double> parseDoubleSchedule(std::string_view value, std::string_view fallback)
{
  auto parse = [](std::string_view text) {
    std::vector<double> values;
    std::string token;
    for (const char ch : text) {
      if (ch == 'x' || ch == 'X' || ch == ',' || ch == ';' || std::isspace(static_cast<unsigned char>(ch))) {
        if (!token.empty()) {
          values.push_back(parseDouble(token, 0.0));
          token.clear();
        }
        continue;
      }
      token.push_back(ch);
    }
    if (!token.empty()) {
      values.push_back(parseDouble(token, 0.0));
    }
    return values;
  };

  std::vector<double> values = parse(value);
  if (values.empty()) {
    values = parse(fallback);
  }
  if (values.empty()) {
    values.push_back(0.0);
  }
  if (values.size() > static_cast<std::size_t>(k_maxResolutionLevels)) {
    values.resize(static_cast<std::size_t>(k_maxResolutionLevels));
  }
  return values;
}

std::string formatDoubleSchedule(const std::vector<double>& values)
{
  std::ostringstream stream;
  stream << std::setprecision(6);
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i > 0u) {
      stream << 'x';
    }
    stream << values.at(i);
  }
  return stream.str();
}

std::vector<int> integerDefaultsForSchedule(std::string_view key, int levelCount)
{
  levelCount = std::clamp(levelCount, k_minResolutionLevels, k_maxResolutionLevels);
  if (key == "shrinkFactors" || key == "scales") {
    const auto begin = k_defaultShrinkFactorsByLevel.end() - levelCount;
    return {begin, k_defaultShrinkFactorsByLevel.end()};
  }
  return {
    k_defaultIterationsByLevel.begin(),
    k_defaultIterationsByLevel.begin() + static_cast<std::ptrdiff_t>(levelCount)};
}

std::vector<double> doubleDefaultsForSchedule(std::string_view key, int levelCount)
{
  levelCount = std::clamp(levelCount, k_minResolutionLevels, k_maxResolutionLevels);
  if (key == "smoothingSigmas") {
    const auto begin = k_defaultSmoothingSigmasByLevel.end() - levelCount;
    return {begin, k_defaultSmoothingSigmasByLevel.end()};
  }
  return std::vector<double>(static_cast<std::size_t>(levelCount), 0.0);
}

void resizeIntegerSchedule(std::vector<int>& values, int levelCount, std::string_view key)
{
  const std::vector<int> defaults = integerDefaultsForSchedule(key, levelCount);
  if (values.size() > defaults.size()) {
    if (key == "shrinkFactors" || key == "scales") {
      values.erase(values.begin(), values.end() - static_cast<std::ptrdiff_t>(defaults.size()));
    }
    else {
      values.resize(defaults.size());
    }
    return;
  }
  if (key == "shrinkFactors" || key == "scales") {
    values.insert(
      values.begin(),
      defaults.begin(),
      defaults.begin() + static_cast<std::ptrdiff_t>(defaults.size() - values.size()));
  }
  else {
    values.insert(values.end(), defaults.begin() + static_cast<std::ptrdiff_t>(values.size()), defaults.end());
  }
}

void resizeDoubleSchedule(std::vector<double>& values, int levelCount, std::string_view key)
{
  const std::vector<double> defaults = doubleDefaultsForSchedule(key, levelCount);
  if (values.size() > defaults.size()) {
    if (key == "smoothingSigmas") {
      values.erase(values.begin(), values.end() - static_cast<std::ptrdiff_t>(defaults.size()));
    }
    else {
      values.resize(defaults.size());
    }
    return;
  }
  if (key == "smoothingSigmas") {
    values.insert(
      values.begin(),
      defaults.begin(),
      defaults.begin() + static_cast<std::ptrdiff_t>(defaults.size() - values.size()));
  }
  else {
    values.insert(values.end(), defaults.begin() + static_cast<std::ptrdiff_t>(values.size()), defaults.end());
  }
}

int resolutionLevelCount(const registration::SetupState& state)
{
  const registration::ParameterValue* iterations = registration::findParameterValue(state, "iterations");
  if (!iterations) {
    return 3;
  }
  return static_cast<int>(parseIntegerSchedule(iterations->value, "128x64x32").size());
}

double parseDouble(std::string_view value, double fallback)
{
  const std::string text{value};
  char* end = nullptr;
  const double parsed = std::strtod(text.c_str(), &end);
  return end && *end == '\0' ? parsed : fallback;
}

std::array<float, 2> parseFloatPair(std::string_view value, std::string_view fallback)
{
  auto parse = [](std::string_view text) {
    std::array<float, 2> values{0.0f, 0.0f};
    std::size_t count = 0u;
    std::string token;
    for (const char ch : text) {
      if (ch == ',' || ch == 'x' || ch == 'X' || ch == ';' || std::isspace(static_cast<unsigned char>(ch))) {
        if (!token.empty() && count < values.size()) {
          values.at(count++) = static_cast<float>(parseDouble(token, 0.0));
          token.clear();
        }
        continue;
      }
      token.push_back(ch);
    }
    if (!token.empty() && count < values.size()) {
      values.at(count++) = static_cast<float>(parseDouble(token, 0.0));
    }
    return std::pair{values, count};
  };

  auto [values, count] = parse(value);
  if (count == values.size()) {
    return values;
  }

  auto [fallbackValues, fallbackCount] = parse(fallback);
  return fallbackCount == fallbackValues.size() ? fallbackValues : std::array<float, 2>{0.0f, 0.0f};
}

std::string formatFloatPair(const std::array<float, 2>& values)
{
  std::ostringstream stream;
  stream << std::setprecision(6) << values.at(0) << ',' << values.at(1);
  return stream.str();
}

std::string setupSignature(
  const std::vector<registration::SetupImageChoice>& choices,
  const std::filesystem::path& outputDirectory)
{
  std::ostringstream stream;
  stream << outputDirectory.string();
  for (const registration::SetupImageChoice& choice : choices) {
    stream << '|' << choice.image.uid << ':' << choice.image.fileName.string() << ':' << choice.image.displayName << ':'
           << choice.dimension;
  }
  return stream.str();
}

void preserveParameterValues(
  std::vector<registration::ParameterValue>& values,
  const std::vector<registration::ParameterValue>& oldValues)
{
  for (registration::ParameterValue& value : values) {
    const auto oldIt =
      std::find_if(oldValues.begin(), oldValues.end(), [&](const registration::ParameterValue& oldValue) {
        return oldValue.key == value.key;
      });
    if (oldIt != oldValues.end()) {
      value.value = oldIt->value;
    }
  }
}

registration::SetupState createSetupStatePreservingSelections(
  const std::vector<registration::SetupImageChoice>& choices,
  registration::Backend fallbackBackend,
  const std::filesystem::path& outputDirectory,
  const std::optional<registration::SetupState>& previousState)
{
  const registration::Backend backend = previousState ? previousState->job.backend : fallbackBackend;
  registration::SetupState state = registration::createSetupState(choices, backend, outputDirectory);
  if (!previousState) {
    return state;
  }

  const registration::JobSpec& previousJob = previousState->job;
  if (const registration::SetupImageChoice* fixed = findImageChoice(choices, previousJob.fixedImage.uid)) {
    registration::setFixedImage(state, *fixed);
  }
  if (const registration::SetupImageChoice* moving = findImageChoice(choices, previousJob.movingImage.uid)) {
    registration::setMovingImage(state, *moving);
  }

  if (registration::supportsTransformModel(state.capabilities, previousJob.transformModel)) {
    state.job.transformModel = previousJob.transformModel;
  }
  if (registration::supportsMetric(state.capabilities, previousJob.metric)) {
    state.job.metric = previousJob.metric;
  }

  state.job.iterationSchedule = previousJob.iterationSchedule;
  state.job.useImageCentersForInitialization = previousJob.useImageCentersForInitialization;
  state.job.useCurrentAffineTransformsForInitialization = previousJob.useCurrentAffineTransformsForInitialization;
  state.job.fixedMask = previousJob.fixedMask;
  state.job.movingMask = previousJob.movingMask;
  state.job.auxiliaryImagePairs = previousJob.auxiliaryImagePairs;
  state.job.landmarks = previousJob.landmarks;
  state.job.surfaces = previousJob.surfaces;
  state.job.outputs = previousJob.outputs;
  state.job.extraArguments = previousJob.extraArguments;
  preserveParameterValues(state.parameterValues, previousState->parameterValues);
  state.job.parameterValues = state.parameterValues;
  registration::refreshValidation(state);
  return state;
}

const char* statusText(registration::JobStatus status)
{
  switch (status) {
    case registration::JobStatus::Queued:
      return "Queued";
    case registration::JobStatus::PreparingInputs:
      return "Preparing";
    case registration::JobStatus::Running:
      return "Running";
    case registration::JobStatus::WritingOutputs:
      return "Writing";
    case registration::JobStatus::ImportingOutputs:
      return "Importing";
    case registration::JobStatus::Completed:
      return "Completed";
    case registration::JobStatus::Cancelled:
      return "Cancelled";
    case registration::JobStatus::Failed:
      return "Failed";
  }
  return "Unknown";
}

ImVec4 statusColor(registration::JobStatus status)
{
  switch (status) {
    case registration::JobStatus::Completed:
      return ImVec4{0.35f, 0.85f, 0.45f, 1.0f};
    case registration::JobStatus::Cancelled:
      return ImVec4{0.75f, 0.75f, 0.75f, 1.0f};
    case registration::JobStatus::Failed:
      return ImVec4{0.95f, 0.35f, 0.30f, 1.0f};
    case registration::JobStatus::Queued:
    case registration::JobStatus::PreparingInputs:
    case registration::JobStatus::Running:
    case registration::JobStatus::WritingOutputs:
    case registration::JobStatus::ImportingOutputs:
      return ImVec4{0.35f, 0.70f, 0.95f, 1.0f};
  }
  return ImVec4{0.75f, 0.75f, 0.75f, 1.0f};
}

std::string jobTitle(const registration::JobRecord& job)
{
  const std::string fixed = job.spec.fixedImage.displayName.empty() ? "fixed" : job.spec.fixedImage.displayName;
  const std::string moving = job.spec.movingImage.displayName.empty() ? "moving" : job.spec.movingImage.displayName;
  return moving + " to " + fixed;
}

std::string metricDisplayLabel(registration::Backend backend, registration::Metric metric)
{
  switch (metric) {
    case registration::Metric::SSD:
      return "Sum of squared differences (SSD)";
    case registration::Metric::MSE:
      return "Mean squared error (MSE)";
    case registration::Metric::MI:
      return "Mutual information (MI)";
    case registration::Metric::NMI:
      return "Normalized mutual information (NMI)";
    case registration::Metric::NCC:
      return "Normalized cross-correlation (NCC)";
    case registration::Metric::WNCC:
      return "Windowed normalized cross-correlation (WNCC)";
    case registration::Metric::CC:
      return "Cross-correlation (CC)";
    case registration::Metric::Mattes:
      return "Mattes mutual information (Mattes)";
    case registration::Metric::Demons:
      return "Demons image metric (Demons)";
    case registration::Metric::GC:
      return "Global correlation (GC)";
    case registration::Metric::FusedCC:
      return "Fused cross-correlation (FusedCC)";
    case registration::Metric::FusedMI:
      return "Fused mutual information (FusedMI)";
    case registration::Metric::MaskedCC:
      return "Masked cross-correlation (MaskedCC)";
    case registration::Metric::MaskedMSE:
      return "Masked mean squared error (MaskedMSE)";
    case registration::Metric::LabelGuided:
      return "Label-guided metric (LabelGuided)";
    case registration::Metric::PointSet:
      return "Point-set metric (PointSet)";
  }
  return std::string{registration::label(metric)};
}

std::string transformDisplayLabel(registration::Backend backend, registration::TransformModel model)
{
  if (backend == registration::Backend::Greedy) {
    switch (model) {
      case registration::TransformModel::Rigid:
        return "Rigid (6 DOF)";
      case registration::TransformModel::Similarity:
        return "Similarity (7 DOF, uniform scale)";
      case registration::TransformModel::Affine:
        return "Affine (12 DOF)";
      case registration::TransformModel::AffineDeformable:
        return "Affine + deformable transformation (12 DOF affine)";
      case registration::TransformModel::Deformable:
        return "Deformable transformation";
      case registration::TransformModel::RigidAffine:
      case registration::TransformModel::Translation:
      case registration::TransformModel::BSplineDisplacement:
      case registration::TransformModel::GaussianDisplacement:
      case registration::TransformModel::TimeVaryingVelocity:
        break;
    }
  }

  if (backend == registration::Backend::ANTs) {
    switch (model) {
      case registration::TransformModel::Translation:
        return "Translation";
      case registration::TransformModel::Rigid:
        return "Rigid";
      case registration::TransformModel::Similarity:
        return "Similarity";
      case registration::TransformModel::Affine:
        return "Affine";
      case registration::TransformModel::RigidAffine:
        return "Affine";
      case registration::TransformModel::Deformable:
        return "Symmetric normalization (SyN)";
      case registration::TransformModel::AffineDeformable:
        return "Affine + symmetric normalization (SyN)";
      case registration::TransformModel::BSplineDisplacement:
        return "B-spline symmetric normalization (BSplineSyN)";
      case registration::TransformModel::GaussianDisplacement:
        return "Gaussian displacement field";
      case registration::TransformModel::TimeVaryingVelocity:
        return "Time-varying velocity field";
    }
  }

  if (backend == registration::Backend::FireANTs) {
    switch (model) {
      case registration::TransformModel::Rigid:
        return "Rigid (Rigid)";
      case registration::TransformModel::Similarity:
        return "Similarity (Similarity)";
      case registration::TransformModel::Affine:
        return "Affine (Affine)";
      case registration::TransformModel::Deformable:
        return "Deformable transformation (Deformable)";
      case registration::TransformModel::AffineDeformable:
        return "Affine + deformable transformation (AffineDeformable)";
      case registration::TransformModel::RigidAffine:
      case registration::TransformModel::Translation:
      case registration::TransformModel::BSplineDisplacement:
      case registration::TransformModel::GaussianDisplacement:
      case registration::TransformModel::TimeVaryingVelocity:
        break;
    }
  }

  return std::string{registration::label(model)};
}

bool pathIsWithinDirectory(const std::filesystem::path& path, const std::filesystem::path& directory)
{
  std::error_code error;
  const std::filesystem::path normalizedPath = std::filesystem::weakly_canonical(path, error);
  if (error) {
    return false;
  }

  const std::filesystem::path normalizedDirectory = std::filesystem::weakly_canonical(directory, error);
  if (error) {
    return false;
  }

  auto pathIt = normalizedPath.begin();
  for (auto dirIt = normalizedDirectory.begin(); dirIt != normalizedDirectory.end(); ++dirIt, ++pathIt) {
    if (pathIt == normalizedPath.end() || *pathIt != *dirIt) {
      return false;
    }
  }
  return true;
}

bool isTemporaryPath(const std::filesystem::path& path)
{
  std::error_code error;
  const std::filesystem::path tempDirectory = std::filesystem::temp_directory_path(error);
  return !error && pathIsWithinDirectory(path, tempDirectory);
}

void addExistingTemporaryFile(std::set<std::filesystem::path>& files, const std::filesystem::path& path)
{
  if (path.empty() || !isTemporaryPath(path)) {
    return;
  }

  std::error_code error;
  if (std::filesystem::is_regular_file(path, error) && !error) {
    files.insert(path);
  }
}

std::vector<std::filesystem::path> collectTemporaryRegistrationFiles(const registration::JobStore& jobs)
{
  std::set<std::filesystem::path> files;
  for (const registration::JobRecord& job : jobs.jobs()) {
    if (job.spec.outputDirectory.empty() || !isTemporaryPath(job.spec.outputDirectory)) {
      continue;
    }

    addExistingTemporaryFile(files, registration::artifactPath(job.spec, registration::ArtifactRole::JobSpec));
    addExistingTemporaryFile(files, registration::artifactPath(job.spec, registration::ArtifactRole::ResultManifest));
    addExistingTemporaryFile(files, registration::artifactPath(job.spec, registration::ArtifactRole::AffineTransform));
    addExistingTemporaryFile(files, registration::artifactPath(job.spec, registration::ArtifactRole::InverseWarp));
    addExistingTemporaryFile(files, registration::artifactPath(job.spec, registration::ArtifactRole::ForwardWarp));
    addExistingTemporaryFile(files, registration::artifactPath(job.spec, registration::ArtifactRole::WarpedImage));
    addExistingTemporaryFile(files, job.spec.initialAffineTransform);

    for (const registration::InputArtifact& artifact : registration::buildInputArtifactPlan(job.spec)) {
      addExistingTemporaryFile(files, artifact.path);
    }
    if (job.manifest) {
      addExistingTemporaryFile(files, job.manifest->affineTransform);
      addExistingTemporaryFile(files, job.manifest->inverseWarp);
      addExistingTemporaryFile(files, job.manifest->forwardWarp);
      addExistingTemporaryFile(files, job.manifest->warpedImage);
      for (const std::filesystem::path& path : job.manifest->warpedSegmentations) {
        addExistingTemporaryFile(files, path);
      }
      for (const std::filesystem::path& path : job.manifest->transformedLandmarks) {
        addExistingTemporaryFile(files, path);
      }
      for (const std::filesystem::path& path : job.manifest->transformedSurfaces) {
        addExistingTemporaryFile(files, path);
      }
    }

    std::error_code error;
    if (!std::filesystem::is_directory(job.spec.outputDirectory, error) || error) {
      continue;
    }
    const std::string prefix = registration::outputPrefix(job.spec);
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(job.spec.outputDirectory, error))
    {
      if (error) {
        break;
      }
      const std::string filename = entry.path().filename().string();
      if (filename.starts_with(prefix) && entry.is_regular_file(error) && !error) {
        addExistingTemporaryFile(files, entry.path());
      }
    }
  }

  return {files.begin(), files.end()};
}

std::string timePointText(const std::optional<registration::JobRecord::Clock::time_point>& timePoint)
{
  if (!timePoint) {
    return {};
  }

  const std::time_t time = registration::JobRecord::Clock::to_time_t(*timePoint);
  std::tm localTime{};
  std::tm utcTime{};
#ifdef _WIN32
  localtime_s(&localTime, &time);
  gmtime_s(&utcTime, &time);
#else
  localtime_r(&time, &localTime);
  gmtime_r(&time, &utcTime);
#endif

  const std::time_t localEpoch = std::mktime(&localTime);
  const std::time_t utcAsLocalEpoch = std::mktime(&utcTime);
  const long offsetSeconds = static_cast<long>(std::difftime(localEpoch, utcAsLocalEpoch));
  const char sign = offsetSeconds < 0 ? '-' : '+';
  const long offsetMagnitude = std::labs(offsetSeconds);
  const long offsetHours = offsetMagnitude / 3600;
  const long offsetMinutes = (offsetMagnitude % 3600) / 60;

  std::ostringstream stream;
  stream << std::put_time(&localTime, "%y/%m/%d %H:%M:%S") << ' ' << sign << std::setw(2) << std::setfill('0')
         << offsetHours << std::setw(2) << offsetMinutes;
  return stream.str();
}

double sortTime(const std::optional<registration::JobRecord::Clock::time_point>& timePoint)
{
  if (!timePoint) {
    return 0.0;
  }
  return std::chrono::duration<double>(timePoint->time_since_epoch()).count();
}

double sortDuration(const registration::JobRecord& job)
{
  const std::optional<std::chrono::system_clock::duration> duration = registration::jobDuration(job);
  return duration ? std::chrono::duration<double>(*duration).count() : 0.0;
}

std::string durationText(const registration::JobRecord& job)
{
  const std::optional<std::chrono::system_clock::duration> duration = registration::jobDuration(job);
  return duration ? registration::formatDuration(*duration) : std::string{};
}

std::string jobMessageText(const registration::JobRecord& job)
{
  const std::string message = registration::latestMessage(job);
  if (!message.empty()) {
    return message;
  }
  if (!job.warnings.empty()) {
    return job.warnings.back();
  }

  switch (job.status) {
    case registration::JobStatus::Completed:
      return "Registration completed.";
    case registration::JobStatus::Cancelled:
      return "Registration cancelled.";
    case registration::JobStatus::Failed:
      return "Registration failed.";
    case registration::JobStatus::Queued:
    case registration::JobStatus::PreparingInputs:
    case registration::JobStatus::Running:
    case registration::JobStatus::WritingOutputs:
    case registration::JobStatus::ImportingOutputs:
      return "Waiting for backend execution.";
  }
  return {};
}

float progressFraction(const registration::JobRecord& job);

enum class JobTableColumn
{
  Order,
  Job,
  Actions,
  Backend,
  Transform,
  Metric,
  Status,
  Progress,
  Start,
  End,
  Duration,
  Message,
  Output
};

int compareJobTableColumn(
  const registration::JobRecord& left,
  const registration::JobRecord& right,
  JobTableColumn column)
{
  auto compareStrings = [](std::string lhs, std::string rhs) {
    return lhs < rhs ? -1 : (rhs < lhs ? 1 : 0);
  };
  auto compareNumbers = [](double lhs, double rhs) {
    return lhs < rhs ? -1 : (rhs < lhs ? 1 : 0);
  };

  switch (column) {
    case JobTableColumn::Order:
      return compareNumbers(static_cast<double>(left.order), static_cast<double>(right.order));
    case JobTableColumn::Job:
      return compareStrings(jobTitle(left), jobTitle(right));
    case JobTableColumn::Actions:
      return 0;
    case JobTableColumn::Backend:
      return compareStrings(
        std::string{registration::label(left.spec.backend)},
        std::string{registration::label(right.spec.backend)});
    case JobTableColumn::Transform:
      return compareStrings(
        std::string{registration::label(left.spec.transformModel)},
        std::string{registration::label(right.spec.transformModel)});
    case JobTableColumn::Metric:
      return compareStrings(
        std::string{registration::label(left.spec.metric)},
        std::string{registration::label(right.spec.metric)});
    case JobTableColumn::Status:
      return compareStrings(statusText(left.status), statusText(right.status));
    case JobTableColumn::Progress:
      return compareNumbers(progressFraction(left), progressFraction(right));
    case JobTableColumn::Start:
      return compareNumbers(sortTime(left.startedAt), sortTime(right.startedAt));
    case JobTableColumn::End:
      return compareNumbers(sortTime(left.endedAt), sortTime(right.endedAt));
    case JobTableColumn::Duration:
      return compareNumbers(sortDuration(left), sortDuration(right));
    case JobTableColumn::Message:
      return compareStrings(registration::latestMessage(left), registration::latestMessage(right));
    case JobTableColumn::Output:
      return compareStrings(left.spec.outputDirectory.string(), right.spec.outputDirectory.string());
  }
  return 0;
}

std::vector<const registration::JobRecord*> sortedJobs(
  const registration::JobStore& jobs,
  ImGuiTableSortSpecs* sortSpecs)
{
  std::vector<const registration::JobRecord*> sorted;
  sorted.reserve(jobs.jobs().size());
  for (const registration::JobRecord& job : jobs.jobs()) {
    sorted.push_back(&job);
  }

  if (!sortSpecs || sortSpecs->SpecsCount == 0) {
    return sorted;
  }

  std::stable_sort(
    sorted.begin(),
    sorted.end(),
    [sortSpecs](const registration::JobRecord* lhs, const registration::JobRecord* rhs) {
      for (int specIndex = 0; specIndex < sortSpecs->SpecsCount; ++specIndex) {
        const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[specIndex];
        const int result = compareJobTableColumn(*lhs, *rhs, static_cast<JobTableColumn>(spec.ColumnUserID));
        if (result == 0) {
          continue;
        }
        return spec.SortDirection == ImGuiSortDirection_Ascending ? result < 0 : result > 0;
      }
      return lhs->order < rhs->order;
    });
  return sorted;
}

float progressFraction(const registration::JobRecord& job)
{
  if (const std::optional<double> progress = registration::latestProgress(job)) {
    return static_cast<float>(std::clamp(*progress, 0.0, 1.0));
  }
  return registration::JobStatus::Completed == job.status ? 1.0f : 0.0f;
}

void renderJobTableProgressBar(const registration::JobRecord& job)
{
  const float progress = progressFraction(job);
  const bool complete = progress >= 1.0f;
  const bool activeProgress = progress > 0.0f && progress < 1.0f;

  int pushedColors = 0;
  if (complete) {
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4{0.18f, 0.48f, 0.22f, 1.0f});
    ++pushedColors;
  }
  else if (activeProgress) {
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4{0.82f, 0.62f, 0.12f, 1.0f});
    ++pushedColors;
  }

  ImGui::ProgressBar(progress, ImVec2{-FLT_MIN, 0.0f});
  if (pushedColors > 0) {
    ImGui::PopStyleColor(pushedColors);
  }
}

const char* outputStreamLabel(registration::OutputStream stream)
{
  return registration::OutputStream::Stderr == stream ? "stderr" : "stdout";
}

std::string jobLogText(const registration::JobRecord& job)
{
  std::ostringstream stream;
  stream << jobTitle(job) << '\n';
  stream << "Status: " << statusText(job.status) << "\n\n";

  if (!job.commands.empty()) {
    stream << "Commands:\n";
    for (const registration::CommandExecution& command : job.commands) {
      stream << "  " << command.displayString << '\n';
      stream << "    exit: " << command.result.exitCode << '\n';
      if (!command.result.failureMessage.empty()) {
        stream << "    error: " << command.result.failureMessage << '\n';
      }
    }
    stream << '\n';
  }

  if (!job.warnings.empty()) {
    stream << "Warnings:\n";
    for (const std::string& warning : job.warnings) {
      stream << "  " << warning << '\n';
    }
    stream << '\n';
  }

  if (!job.errorMessage.empty()) {
    stream << "Error:\n  " << job.errorMessage << "\n\n";
  }

  stream << "Output:\n";
  for (const registration::ProcessOutputLine& line : job.outputLines) {
    stream << '[' << outputStreamLabel(line.stream) << "] " << line.text << '\n';
  }
  return stream.str();
}

void renderRegistrationJobDetailsPopup(const registration::JobRecord* job)
{
  if (!job) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2{760.0f, 520.0f}, ImGuiCond_FirstUseEver);
  if (ImGui::BeginPopupModal("Image Registration Job Details", nullptr)) {
    ImGui::TextWrapped("%s", jobTitle(*job).c_str());
    ImGui::TextColored(statusColor(job->status), "%s", statusText(job->status));

    std::string logText = jobLogText(*job);
    const float footerHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;

    ImGui::Separator();
    ImGui::InputTextMultiline(
      "##RegistrationJobLog",
      &logText,
      ImVec2{ImGui::GetContentRegionAvail().x, -footerHeight},
      ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoUndoRedo);

    ImGui::Separator();
    if (ImGui::Button("Copy Log")) {
      ImGui::SetClipboardText(logText.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Close")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

bool sliderIntN(const char* label, int* values, int count, int minValue, int maxValue)
{
  switch (count) {
    case 1:
      return ImGui::SliderInt(label, values, minValue, maxValue);
    case 2:
      return ImGui::SliderInt2(label, values, minValue, maxValue);
    case 3:
      return ImGui::SliderInt3(label, values, minValue, maxValue);
    case 4:
      return ImGui::SliderInt4(label, values, minValue, maxValue);
    default:
      return false;
  }
}

bool sliderFloatN(const char* label, float* values, int count, float minValue, float maxValue, const char* format)
{
  switch (count) {
    case 1:
      return ImGui::SliderFloat(label, values, minValue, maxValue, format);
    case 2:
      return ImGui::SliderFloat2(label, values, minValue, maxValue, format);
    case 3:
      return ImGui::SliderFloat3(label, values, minValue, maxValue, format);
    case 4:
      return ImGui::SliderFloat4(label, values, minValue, maxValue, format);
    default:
      return false;
  }
}

void renderIterationsPerLevelWidget(registration::ParameterValue& value, const registration::ParameterSchema& parameter)
{
  std::vector<int> iterations = parseIntegerSchedule(value.value, parameter.defaultValue);
  int levelCount = static_cast<int>(iterations.size());
  if (ImGui::SliderInt("Resolution levels", &levelCount, k_minResolutionLevels, k_maxResolutionLevels)) {
    levelCount = std::clamp(levelCount, k_minResolutionLevels, k_maxResolutionLevels);
    resizeIntegerSchedule(iterations, levelCount, parameter.key);
    value.value = formatIntegerSchedule(iterations);
  }
  renderHelpMarker("Number of multi-resolution pyramid levels, ordered coarse to fine.");

  std::array<int, k_maxResolutionLevels> values{};
  std::copy(iterations.begin(), iterations.end(), values.begin());
  if (sliderIntN(
        parameter.label.c_str(),
        values.data(),
        static_cast<int>(iterations.size()),
        k_minIterationsPerLevel,
        k_maxIterationsPerLevel))
  {
    std::copy_n(values.begin(), iterations.size(), iterations.begin());
    value.value = formatIntegerSchedule(iterations);
  }
  renderHelpMarker(parameter.tooltip);
}

void renderIntegerScheduleWidget(
  registration::ParameterValue& value,
  const registration::ParameterSchema& parameter,
  int levelCount,
  int minimumValue,
  int maximumValue)
{
  std::vector<int> values = parseIntegerSchedule(value.value, parameter.defaultValue);
  const std::size_t originalSize = values.size();
  resizeIntegerSchedule(values, levelCount, parameter.key);
  if (values.size() != originalSize) {
    value.value = formatIntegerSchedule(values);
  }
  std::array<int, k_maxResolutionLevels> sliderValues{};
  std::copy(values.begin(), values.end(), sliderValues.begin());
  if (sliderIntN(
        parameter.label.c_str(),
        sliderValues.data(),
        static_cast<int>(values.size()),
        minimumValue,
        maximumValue))
  {
    std::copy_n(sliderValues.begin(), values.size(), values.begin());
    value.value = formatIntegerSchedule(values);
  }
  renderHelpMarker(parameter.tooltip);
}

void renderDoubleScheduleWidget(
  registration::ParameterValue& value,
  const registration::ParameterSchema& parameter,
  int levelCount,
  double minimumValue)
{
  std::vector<double> values = parseDoubleSchedule(value.value, parameter.defaultValue);
  const std::size_t originalSize = values.size();
  resizeDoubleSchedule(values, levelCount, parameter.key);
  if (values.size() != originalSize) {
    value.value = formatDoubleSchedule(values);
  }
  std::array<float, k_maxResolutionLevels> sliderValues{};
  std::transform(values.begin(), values.end(), sliderValues.begin(), [](double parsed) {
    return static_cast<float>(parsed);
  });
  if (sliderFloatN(
        parameter.label.c_str(),
        sliderValues.data(),
        static_cast<int>(values.size()),
        static_cast<float>(minimumValue),
        k_maxSmoothingSigma,
        "%.4g"))
  {
    std::transform(
      sliderValues.begin(),
      sliderValues.begin() + static_cast<std::ptrdiff_t>(values.size()),
      values.begin(),
      [](float parsed) { return static_cast<double>(parsed); });
    value.value = formatDoubleSchedule(values);
  }
  renderHelpMarker(parameter.tooltip);
}

void renderFloatPairWidget(registration::ParameterValue& value, const registration::ParameterSchema& parameter)
{
  std::array<float, 2> parsed = parseFloatPair(value.value, parameter.defaultValue);
  if (ImGui::InputFloat2(parameter.label.c_str(), parsed.data(), "%.4g")) {
    value.value = formatFloatPair(parsed);
  }
  renderHelpMarker(parameter.tooltip);
}

void renderParameterWidget(
  registration::SetupState& state,
  registration::ParameterValue& value,
  const registration::ParameterSchema& parameter)
{
  if (parameter.key == "iterations" && parameter.kind == registration::ParameterKind::IntegerVector) {
    renderIterationsPerLevelWidget(value, parameter);
    return;
  }
  if (
    (parameter.key == "shrinkFactors" || parameter.key == "scales") &&
    parameter.kind == registration::ParameterKind::IntegerVector)
  {
    renderIntegerScheduleWidget(value, parameter, resolutionLevelCount(state), k_minShrinkFactor, k_maxShrinkFactor);
    return;
  }
  if (parameter.key == "smoothingSigmas" && parameter.kind == registration::ParameterKind::FloatVector) {
    renderDoubleScheduleWidget(value, parameter, resolutionLevelCount(state), 0.0);
    return;
  }
  if (parameter.key == "smoothingSigma" && parameter.kind == registration::ParameterKind::FloatVector) {
    renderFloatPairWidget(value, parameter);
    return;
  }

  switch (parameter.kind) {
    case registration::ParameterKind::Boolean: {
      bool checked = value.value == "true" || value.value == "1" || value.value == "yes" || value.value == "on";
      if (ImGui::Checkbox(parameter.label.c_str(), &checked)) {
        value.value = checked ? "true" : "false";
      }
      renderHelpMarker(parameter.tooltip);
      break;
    }
    case registration::ParameterKind::Integer: {
      int parsed = parseInteger(value.value, parseInteger(parameter.defaultValue, 0));
      if (parameter.minValue || parameter.maxValue) {
        const int minValue = static_cast<int>(parameter.minValue.value_or(-2147483647.0));
        const int maxValue = static_cast<int>(parameter.maxValue.value_or(2147483647.0));
        if (ImGui::SliderInt(parameter.label.c_str(), &parsed, minValue, maxValue)) {
          value.value = std::to_string(parsed);
        }
      }
      else if (ImGui::InputInt(parameter.label.c_str(), &parsed)) {
        value.value = std::to_string(parsed);
      }
      renderHelpMarker(parameter.tooltip);
      break;
    }
    case registration::ParameterKind::Float: {
      double parsed = parseDouble(value.value, parseDouble(parameter.defaultValue, 0.0));
      if (parameter.minValue || parameter.maxValue) {
        const double minValue = parameter.minValue.value_or(0.0);
        const double maxValue = parameter.maxValue.value_or(1.0);
        if (ImGui::SliderScalar(parameter.label.c_str(), ImGuiDataType_Double, &parsed, &minValue, &maxValue, "%.4g")) {
          value.value = std::to_string(parsed);
        }
      }
      else if (ImGui::InputDouble(parameter.label.c_str(), &parsed)) {
        value.value = std::to_string(parsed);
      }
      renderHelpMarker(parameter.tooltip);
      break;
    }
    case registration::ParameterKind::Choice: {
      const char* preview = value.value.empty() ? parameter.defaultValue.c_str() : value.value.c_str();
      if (ImGui::BeginCombo(parameter.label.c_str(), preview)) {
        for (const std::string& choice : parameter.choices) {
          const bool selected = value.value == choice;
          if (ImGui::Selectable(choice.c_str(), selected)) {
            value.value = choice;
          }
          if (selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
      renderHelpMarker(parameter.tooltip);
      break;
    }
    case registration::ParameterKind::Text:
    case registration::ParameterKind::IntegerVector:
    case registration::ParameterKind::FloatVector:
    case registration::ParameterKind::FilePath:
    case registration::ParameterKind::DirectoryPath:
      ImGui::InputText(parameter.label.c_str(), &value.value);
      renderHelpMarker(parameter.tooltip);
      break;
  }
}

void renderTransformCombo(registration::SetupState& state)
{
  const std::string preview = transformDisplayLabel(state.job.backend, state.job.transformModel);
  if (ImGui::BeginCombo("Transform", preview.c_str())) {
    for (const registration::TransformModel model : state.capabilities.transformModels) {
      const bool selected = model == state.job.transformModel;
      const std::string label = transformDisplayLabel(state.job.backend, model);
      if (ImGui::Selectable(label.c_str(), selected)) {
        state.job.transformModel = model;
        registration::refreshValidation(state);
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  helpMarker("Transformation model requested from the selected registration backend.");
}

void renderMetricCombo(registration::SetupState& state)
{
  const std::string preview = metricDisplayLabel(state.job.backend, state.job.metric);
  if (ImGui::BeginCombo("Metric", preview.c_str())) {
    for (const registration::Metric metric : state.capabilities.metrics) {
      const bool selected = metric == state.job.metric;
      const std::string label = metricDisplayLabel(state.job.backend, metric);
      if (ImGui::Selectable(label.c_str(), selected)) {
        state.job.metric = metric;
        registration::refreshValidation(state);
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  helpMarker("Similarity metric optimized by the selected registration backend.");
}

void renderAuxiliaryImagePairs(
  registration::SetupState& state,
  const std::vector<registration::SetupImageChoice>& imageChoicesForPairs)
{
  const bool supported = registration::supportsFeature(state.capabilities, registration::Feature::AuxiliaryImagePairs);
  ImGui::BeginDisabled(!supported);

  if (ImGui::Button("Add auxiliary pair")) {
    registration::AuxiliaryImagePair pair;
    pair.fixed = state.job.fixedImage;
    pair.moving = state.job.movingImage;
    pair.metric = state.job.metric;
    pair.weight = 1.0;
    state.job.auxiliaryImagePairs.push_back(std::move(pair));
    registration::refreshValidation(state);
  }
  ImGui::SameLine();
  helpMarker(
    "Add another fixed/moving image pair as an additional registration constraint. "
    "Greedy and ANTs use these as additional metric terms.");

  if (!supported) {
    ImGui::TextDisabled("Auxiliary image pairs are not supported by this backend.");
    ImGui::EndDisabled();
    return;
  }

  for (std::size_t index = 0; index < state.job.auxiliaryImagePairs.size(); ++index) {
    registration::AuxiliaryImagePair& pair = state.job.auxiliaryImagePairs[index];
    ImGui::PushID(static_cast<int>(index));
    ImGui::Separator();
    ImGui::Text("Auxiliary pair %zu", index + 1u);

    renderImageChoiceCombo(
      "Fixed",
      imageChoicesForPairs,
      pair.fixed,
      [&](const registration::SetupImageChoice& choice) {
        pair.fixed = choice.image;
        registration::refreshValidation(state);
      });
    renderImageChoiceCombo(
      "Moving",
      imageChoicesForPairs,
      pair.moving,
      [&](const registration::SetupImageChoice& choice) {
        pair.moving = choice.image;
        registration::refreshValidation(state);
      });

    if (state.job.backend == registration::Backend::Greedy) {
      ImGui::TextDisabled("Metric: primary metric");
      ImGui::SameLine();
      helpMarker("Greedy applies the primary metric to all fixed/moving image pairs.");
    }
    else {
      if (ImGui::BeginCombo("Metric", metricDisplayLabel(state.job.backend, pair.metric).c_str())) {
        for (const registration::Metric metric : state.capabilities.metrics) {
          const bool selected = metric == pair.metric;
          const std::string label = metricDisplayLabel(state.job.backend, metric);
          if (ImGui::Selectable(label.c_str(), selected)) {
            pair.metric = metric;
            registration::refreshValidation(state);
          }
          if (selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
    }

    double weight = pair.weight;
    if (ImGui::InputDouble("Weight", &weight, 0.0, 0.0, "%.4g")) {
      pair.weight = std::max(0.0, weight);
      registration::refreshValidation(state);
    }
    ImGui::SameLine();
    helpMarker("Relative metric weight for this auxiliary pair.");

    if (ImGui::Button("Remove")) {
      state.job.auxiliaryImagePairs.erase(state.job.auxiliaryImagePairs.begin() + static_cast<std::ptrdiff_t>(index));
      registration::refreshValidation(state);
      ImGui::PopID();
      break;
    }
    ImGui::PopID();
  }

  ImGui::EndDisabled();
}

bool renderParameterByKey(registration::SetupState& state, const registration::ParameterSchema& parameter)
{
  registration::ParameterValue* value = registration::findParameterValue(state, parameter.key);
  if (!value) {
    return false;
  }
  renderParameterWidget(state, *value, parameter);
  return true;
}

void renderParameterVisibilityToggles(bool& showAdvanced, bool& showExpert)
{
  ImGui::Checkbox("Show advanced options", &showAdvanced);
  ImGui::SameLine();
  ImGui::Checkbox("Show expert options", &showExpert);
}

void renderVisibleParameters(registration::SetupState& state, bool& showAdvanced, bool& showExpert)
{
  const std::vector<registration::ParameterSchema> parameters = registration::visibleParameters(state);
  if (parameters.empty()) {
    ImGui::TextDisabled("No parameters visible.");
    return;
  }

  bool renderedToggles = false;
  for (const registration::ParameterSchema& parameter : parameters) {
    if (parameter.advanced || parameter.expert) {
      continue;
    }
    if (renderParameterByKey(state, parameter) && parameter.key == "smoothingUnits") {
      renderParameterVisibilityToggles(showAdvanced, showExpert);
      renderedToggles = true;
      state.showAdvancedParameters = showAdvanced;
      state.showExpertParameters = showExpert;
    }
  }

  if (!renderedToggles) {
    renderParameterVisibilityToggles(showAdvanced, showExpert);
    state.showAdvancedParameters = showAdvanced;
    state.showExpertParameters = showExpert;
  }

  const std::vector<registration::ParameterSchema> expandedParameters = registration::visibleParameters(state);
  for (const registration::ParameterSchema& parameter : expandedParameters) {
    if (!parameter.advanced && !parameter.expert) {
      continue;
    }
    renderParameterByKey(state, parameter);
  }
}

std::string commandPreviewText(
  const registration::SetupState& state,
  const registration::CommandGenerationOptions& commandOptions)
{
  const std::vector<std::string> commands = registration::commandPreviews(state, commandOptions);
  std::string text;
  for (const std::string& command : commands) {
    text += command;
    text.push_back('\n');
  }
  return text;
}

float maxTextLineWidth(const std::string& text)
{
  float width = 0.0f;
  std::size_t lineBegin = 0u;
  while (lineBegin <= text.size()) {
    const std::size_t lineEnd = text.find('\n', lineBegin);
    const std::string_view line{
      text.data() + lineBegin,
      (lineEnd == std::string::npos ? text.size() : lineEnd) - lineBegin};
    width = std::max(width, ImGui::CalcTextSize(line.data(), line.data() + line.size()).x);
    if (lineEnd == std::string::npos) {
      break;
    }
    lineBegin = lineEnd + 1u;
  }
  return width;
}

void renderCommandPreview(
  const registration::SetupState& state,
  const registration::CommandGenerationOptions& commandOptions,
  float height)
{
  std::string text = commandPreviewText(state, commandOptions);
  if (text.empty()) {
    ImGui::TextDisabled("No command preview available.");
    return;
  }

  const ImGuiStyle& style = ImGui::GetStyle();
  const float visibleWidth = ImGui::GetContentRegionAvail().x;
  const float contentWidth =
    std::max(visibleWidth, maxTextLineWidth(text) + 2.0f * style.FramePadding.x + style.ScrollbarSize);

  if (ImGui::BeginChild(
        "##RegistrationCommandPreviewScroll",
        ImVec2{visibleWidth, height},
        ImGuiChildFlags_None,
        ImGuiWindowFlags_HorizontalScrollbar))
  {
    ImGui::InputTextMultiline(
      "##RegistrationCommandPreview",
      &text,
      ImVec2{contentWidth, ImGui::GetContentRegionAvail().y},
      ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoUndoRedo);
  }
  ImGui::EndChild();
}

} // namespace

void renderRegistrationSetupWindow(AppData& appData)
{
  const ImVec2 defaultSize = halfMainViewportSize();
  ImGui::SetNextWindowSize(defaultSize, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2{360.0f, 320.0f}, ImVec2{FLT_MAX, FLT_MAX});
  setNextDockablePanelWindowClass();
  if (!ImGui::Begin("Image Registration Setup##RegistrationSetup", &appData.guiData().m_showRegistrationSetupWindow)) {
    ImGui::End();
    return;
  }

  static bool s_showAdvanced = false;
  registration::BackendConfig& config = appData.settings().registrationBackendConfig();
  static bool s_showExpert = config.showExpertOptionsByDefault;
  static std::filesystem::path s_lastDefaultOutputDirectory;
  static std::string s_outputDirectoryText;
  static std::optional<std::string> s_outputDirectoryWarning;
  if (s_lastDefaultOutputDirectory != config.defaultOutputDirectory) {
    s_outputDirectoryText = config.defaultOutputDirectory.string();
    s_lastDefaultOutputDirectory = config.defaultOutputDirectory;
    s_outputDirectoryWarning = outputDirectoryWarning(s_outputDirectoryText);
  }

  const std::filesystem::path outputDirectory =
    config.defaultOutputDirectory.empty() ? std::filesystem::temp_directory_path() : config.defaultOutputDirectory;
  const std::vector<registration::SetupImageChoice> choices = imageChoices(appData);
  const std::vector<DataChoice> masks = maskChoices(appData);
  static std::optional<registration::SetupState> s_state;
  static std::string s_setupSignature;
  const std::string signature = setupSignature(choices, outputDirectory);
  if (!s_state || s_setupSignature != signature) {
    s_state = createSetupStatePreservingSelections(choices, config.defaultBackend, outputDirectory, s_state);
    s_setupSignature = signature;
  }
  registration::SetupState& state = *s_state;
  if (state.job.backend != config.defaultBackend) {
    registration::setBackend(state, config.defaultBackend);
  }
  state.showAdvancedParameters = s_showAdvanced;
  state.showExpertParameters = s_showExpert;

  const ImGuiStyle& style = ImGui::GetStyle();
  const float footerHeight = (2.0f * ImGui::GetFrameHeightWithSpacing()) + style.ItemSpacing.y;
  if (ImGui::BeginChild("RegistrationSetupContent", ImVec2{0.0f, -footerHeight}, ImGuiChildFlags_None)) {
    const float controlWidth = std::max(260.0f, 0.78f * ImGui::GetContentRegionAvail().x);
    ImGui::PushItemWidth(controlWidth);
    int selectedBackend = backendIndex(config.defaultBackend);
    const std::string preview{registration::label(k_backends.at(static_cast<std::size_t>(selectedBackend)))};
    if (ImGui::BeginCombo("Backend", preview.c_str())) {
      for (int i = 0; i < static_cast<int>(k_backends.size()); ++i) {
        const registration::Backend backend = k_backends.at(static_cast<std::size_t>(i));
        const bool selected = i == selectedBackend;
        const std::string backendLabel{registration::label(backend)};
        if (ImGui::Selectable(backendLabel.c_str(), selected)) {
          config.defaultBackend = backend;
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    helpMarker("Registration backend used to estimate the image transformation.");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Inputs", ImGuiTreeNodeFlags_DefaultOpen)) {
      renderImageChoiceCombo(
        "Fixed image",
        choices,
        state.job.fixedImage,
        [&](const registration::SetupImageChoice& choice) { registration::setFixedImage(state, choice); });
      ImGui::SameLine();
      helpMarker("Image that defines the reference space for registration.");
      renderImageChoiceCombo(
        "Moving image",
        choices,
        state.job.movingImage,
        [&](const registration::SetupImageChoice& choice) { registration::setMovingImage(state, choice); });
      ImGui::SameLine();
      helpMarker("Image that will be transformed into the fixed/reference image space.");

      if (renderDataChoiceCombo("Fixed mask", masks, state.job.fixedMask)) {
        registration::refreshValidation(state);
      }
      ImGui::SameLine();
      helpMarker(
        "Optional mask in fixed/reference image space. The backend evaluates registration only within the selected "
        "fixed-region constraint when supported.");

      if (renderDataChoiceCombo("Moving mask", masks, state.job.movingMask)) {
        registration::refreshValidation(state);
      }
      ImGui::SameLine();
      helpMarker(
        "Optional mask in moving image space. The backend excludes moving-image samples outside the selected "
        "moving-region constraint when supported.");

      renderAuxiliaryImagePairs(state, choices);
      ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
      renderTransformCombo(state);
      renderMetricCombo(state);
      ImGui::Separator();
      renderVisibleParameters(state, s_showAdvanced, s_showExpert);
      ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Options", ImGuiTreeNodeFlags_DefaultOpen)) {
      const auto commitOutputDirectory = [&]() {
        config.defaultOutputDirectory =
          s_outputDirectoryText.empty() ? std::filesystem::path{} : std::filesystem::path{s_outputDirectoryText};
        s_lastDefaultOutputDirectory = config.defaultOutputDirectory;
        state.job.outputDirectory = config.defaultOutputDirectory.empty() ? std::filesystem::temp_directory_path()
                                                                          : config.defaultOutputDirectory;
        s_outputDirectoryWarning = outputDirectoryWarning(s_outputDirectoryText);
        registration::refreshValidation(state);
      };

      if (ImGui::InputText("Output directory", &s_outputDirectoryText, ImGuiInputTextFlags_EnterReturnsTrue)) {
        commitOutputDirectory();
      }
      if (ImGui::IsItemDeactivatedAfterEdit()) {
        commitOutputDirectory();
      }
      ImGui::SameLine();
      helpMarker(
        "Directory where registration outputs are written. Leave empty to use a per-job folder in the system "
        "temporary directory.");
      if (s_outputDirectoryWarning) {
        ImGui::TextDisabled("%s", s_outputDirectoryWarning->c_str());
      }
      if (config.defaultOutputDirectory.empty()) {
        ImGui::TextDisabled(
          "Using per-job folders under: %s",
          (std::filesystem::temp_directory_path() / "entropy-registration").string().c_str());
      }

      if (state.job.backend == registration::Backend::FireANTs && s_showExpert) {
        ImGui::SeparatorText("Developer");
        ImGui::PushItemWidth(controlWidth);
        ImGui::InputText("FireANTs bridge module", &config.fireAntsBridgeModule);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        helpMarker(
          "Python module launched with 'python -m'. This is normally Entropy's bundled FireANTs bridge and should "
          "only be changed when testing alternate bridge modules.");
      }

      if (ImGui::Checkbox(
            "Use current affine transforms as initialization",
            &state.job.useCurrentAffineTransformsForInitialization))
      {
        if (state.job.useCurrentAffineTransformsForInitialization) {
          state.job.useImageCentersForInitialization = false;
        }
        registration::refreshValidation(state);
      }
      ImGui::SameLine();
      helpMarker(
        "Initialize registration with the moving image's current initial and manual affine transforms. "
        "After import, the computed affine replaces those transforms as the new initial affine.");
      ImGui::Spacing();
      ImGui::TextUnformatted("Output data to load following registration:");

      if (ImGui::Checkbox("Moving image transformed to fixed image space", &state.job.outputs.loadWarpedImage)) {
        registration::refreshValidation(state);
      }
      ImGui::SameLine();
      helpMarker("Load the baked/resampled moving image produced by the backend as a new image.");

      if (ImGui::Checkbox("Affine transformation matrix for moving image", &state.job.outputs.loadAffineTransform)) {
        registration::refreshValidation(state);
      }
      ImGui::SameLine();
      helpMarker("Apply the computed affine transform to the original moving image.");

      if (ImGui::Checkbox(
            "Inverse warp field for transforming/resampling moving image to fixed image space",
            &state.job.outputs.loadInverseWarp))
      {
        registration::refreshValidation(state);
      }
      ImGui::SameLine();
      helpMarker(
        "Load and assign the inverse sampling warp. This field is sampled in fixed/reference space and points to "
        "moving-image sample locations.");

      if (ImGui::Checkbox(
            "Forward warp field for transforming moving image landmarks and annotations to fixed image space",
            &state.job.outputs.loadForwardWarp))
      {
        registration::refreshValidation(state);
      }
      ImGui::SameLine();
      helpMarker(
        "Load and assign the forward point warp. This field maps moving-space points into fixed/reference image "
        "space, which is the direction needed for landmarks and annotations.");
      ImGui::Separator();
    }

    const bool commandPreviewOpen = ImGui::CollapsingHeader("Command Preview", ImGuiTreeNodeFlags_DefaultOpen);
    if (commandPreviewOpen) {
      const float previewHeight = std::max(120.0f, ImGui::GetContentRegionAvail().y);
      renderCommandPreview(state, registration::commandOptions(config), previewHeight);
    }
    ImGui::PopItemWidth();
  }
  ImGui::EndChild();

  if (ImGui::BeginChild(
        "##RegistrationSetupFooter",
        ImVec2{0.0f, 0.0f},
        ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar))
  {
    ImGui::Separator();
    renderValidation(state.validation);
    ImGui::SetCursorPosY(std::max(0.0f, ImGui::GetCursorPosY() - style.ItemSpacing.y));

    const float buttonY = std::max(ImGui::GetCursorPosY(), ImGui::GetContentRegionMax().y - ImGui::GetFrameHeight());
    ImGui::SetCursorPosY(buttonY);
    ImGui::BeginDisabled(!state.validation.canLaunch());
    if (ImGui::Button("Start Registration")) {
      state.job.parameterValues = state.parameterValues;
      appData.registrationJobs().add(state.job);
      appData.guiData().m_showRegistrationJobsWindow = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
      ImGui::SetTooltip("%s", "Create and start a registration job.");
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    const std::string commandText = commandPreviewText(state, registration::commandOptions(config));
    ImGui::BeginDisabled(commandText.empty());
    if (ImGui::Button("Copy Command")) {
      ImGui::SetClipboardText(commandText.c_str());
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
      ImGui::SetTooltip(
        "%s",
        "Copy the raw backend command preview to the clipboard. These commands are intended for inspection and "
        "advanced debugging.");
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Close")) {
      appData.guiData().m_showRegistrationSetupWindow = false;
    }
  }
  ImGui::EndChild();

  ImGui::End();
}

void renderRegistrationJobsWindow(
  AppData& appData,
  const std::function<void(const std::string& jobId)>& importJobOutputs)
{
  setNextDockablePanelWindowClass();
  if (!ImGui::Begin("Image Registration Jobs##RegistrationJobs", &appData.guiData().m_showRegistrationJobsWindow)) {
    ImGui::End();
    return;
  }

  registration::JobStore& jobs = appData.registrationJobs();
  if (jobs.jobs().empty()) {
    ImGui::TextDisabled("No registration jobs have been started.");
    ImGui::TextWrapped("%s", "Started jobs will appear here with progress, logs, warnings, and import controls.");
    ImGui::End();
    return;
  }

  static std::string s_selectedLogJobId;
  static std::vector<std::filesystem::path> s_temporaryFilesToRemove;
  static std::string s_temporaryCleanupMessage;
  const registration::JobRecord* selectedLogJob = nullptr;
  bool openDetailsPopup = false;

  if (ImGui::Button("Remove temporary files...")) {
    s_temporaryFilesToRemove = collectTemporaryRegistrationFiles(jobs);
    s_temporaryCleanupMessage.clear();
    ImGui::OpenPopup("Remove Registration Temporary Files");
  }
  ImGui::SameLine();
  helpMarker("Remove registration files that Entropy generated in the system temporary directory.");
  if (!s_temporaryCleanupMessage.empty()) {
    ImGui::SameLine();
    ImGui::TextDisabled("%s", s_temporaryCleanupMessage.c_str());
  }

  constexpr ImGuiTableFlags tableFlags =
    ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
    ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
  if (ImGui::BeginTable("RegistrationJobsTable", 13, tableFlags, ImVec2{0.0f, 0.0f})) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const auto smallButtonWidth = [&](const char* label) {
      return ImGui::CalcTextSize(label).x + (2.0f * style.FramePadding.x);
    };
    const float actionsColumnWidth = smallButtonWidth("Cancel") + smallButtonWidth("Import") + smallButtonWidth("Log") +
                                     (2.0f * style.ItemSpacing.x) + (4.0f * style.CellPadding.x) + style.ScrollbarSize;

    ImGui::TableSetupColumn(
      "#",
      ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort,
      44.0f,
      static_cast<ImGuiID>(JobTableColumn::Order));
    ImGui::TableSetupColumn("Job", ImGuiTableColumnFlags_WidthFixed, 160.0f, static_cast<ImGuiID>(JobTableColumn::Job));
    ImGui::TableSetupColumn(
      "Actions",
      ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize,
      actionsColumnWidth,
      static_cast<ImGuiID>(JobTableColumn::Actions));
    ImGui::TableSetupColumn(
      "Backend",
      ImGuiTableColumnFlags_WidthFixed,
      82.0f,
      static_cast<ImGuiID>(JobTableColumn::Backend));
    ImGui::TableSetupColumn(
      "Transform",
      ImGuiTableColumnFlags_WidthFixed,
      128.0f,
      static_cast<ImGuiID>(JobTableColumn::Transform));
    ImGui::TableSetupColumn(
      "Metric",
      ImGuiTableColumnFlags_WidthFixed,
      90.0f,
      static_cast<ImGuiID>(JobTableColumn::Metric));
    ImGui::TableSetupColumn(
      "Status",
      ImGuiTableColumnFlags_WidthFixed,
      96.0f,
      static_cast<ImGuiID>(JobTableColumn::Status));
    ImGui::TableSetupColumn(
      "Progress",
      ImGuiTableColumnFlags_WidthFixed,
      110.0f,
      static_cast<ImGuiID>(JobTableColumn::Progress));
    ImGui::TableSetupColumn(
      "Start",
      ImGuiTableColumnFlags_WidthFixed,
      150.0f,
      static_cast<ImGuiID>(JobTableColumn::Start));
    ImGui::TableSetupColumn("End", ImGuiTableColumnFlags_WidthFixed, 150.0f, static_cast<ImGuiID>(JobTableColumn::End));
    ImGui::TableSetupColumn(
      "Duration",
      ImGuiTableColumnFlags_WidthFixed,
      88.0f,
      static_cast<ImGuiID>(JobTableColumn::Duration));
    ImGui::TableSetupColumn(
      "Message",
      ImGuiTableColumnFlags_WidthFixed,
      240.0f,
      static_cast<ImGuiID>(JobTableColumn::Message));
    ImGui::TableSetupColumn(
      "Output",
      ImGuiTableColumnFlags_WidthStretch,
      1.0f,
      static_cast<ImGuiID>(JobTableColumn::Output));
    ImGui::TableHeadersRow();

    const std::vector<const registration::JobRecord*> sorted = sortedJobs(jobs, ImGui::TableGetSortSpecs());
    for (const registration::JobRecord* jobPtr : sorted) {
      const registration::JobRecord& job = *jobPtr;
      ImGui::PushID(job.id.c_str());
      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%llu", static_cast<unsigned long long>(job.order));

      ImGui::TableSetColumnIndex(1);
      ImGui::TextWrapped("%s", jobTitle(job).c_str());

      ImGui::TableSetColumnIndex(2);
      const bool active = registration::isActiveJobStatus(job.status);
      ImGui::BeginDisabled(!active);
      if (ImGui::SmallButton("Cancel")) {
        jobs.appendProgress(
          job.id,
          registration::ProgressEvent{
            .kind = registration::ProgressEventKind::Cancelled,
            .message = "Registration was cancelled before backend execution"});
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      const bool canImport = job.manifest.has_value() && importJobOutputs;
      ImGui::BeginDisabled(!canImport);
      if (canImport) {
        const ImVec4 importColor = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
        ImGui::PushStyleColor(ImGuiCol_Button, importColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, importColor);
      }
      if (ImGui::SmallButton("Import")) {
        importJobOutputs(job.id);
      }
      if (canImport) {
        ImGui::PopStyleColor(2);
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::SmallButton("Log")) {
        s_selectedLogJobId = job.id;
        openDetailsPopup = true;
      }

      ImGui::TableSetColumnIndex(3);
      const std::string backendLabel{registration::label(job.spec.backend)};
      ImGui::TextUnformatted(backendLabel.c_str());

      ImGui::TableSetColumnIndex(4);
      const std::string transformLabel{registration::label(job.spec.transformModel)};
      ImGui::TextUnformatted(transformLabel.c_str());

      ImGui::TableSetColumnIndex(5);
      const std::string metricLabel{registration::label(job.spec.metric)};
      ImGui::TextUnformatted(metricLabel.c_str());

      ImGui::TableSetColumnIndex(6);
      ImGui::TextColored(statusColor(job.status), "%s", statusText(job.status));

      ImGui::TableSetColumnIndex(7);
      renderJobTableProgressBar(job);

      ImGui::TableSetColumnIndex(8);
      const std::string startText = timePointText(job.startedAt);
      if (!startText.empty()) {
        ImGui::TextUnformatted(startText.c_str());
      }
      else {
        ImGui::TextDisabled("-");
      }

      ImGui::TableSetColumnIndex(9);
      const std::string endText = timePointText(job.endedAt);
      if (!endText.empty()) {
        ImGui::TextUnformatted(endText.c_str());
      }
      else {
        ImGui::TextDisabled("-");
      }

      ImGui::TableSetColumnIndex(10);
      const std::string elapsedText = durationText(job);
      if (!elapsedText.empty()) {
        ImGui::TextUnformatted(elapsedText.c_str());
      }
      else {
        ImGui::TextDisabled("-");
      }

      ImGui::TableSetColumnIndex(11);
      const std::string message = jobMessageText(job);
      if (!message.empty()) {
        ImGui::TextWrapped("%s", message.c_str());
      }
      else {
        ImGui::TextDisabled("-");
      }

      ImGui::TableSetColumnIndex(12);
      const std::string outputDirectory = job.spec.outputDirectory.string();
      if (!outputDirectory.empty()) {
        std::string outputText = outputDirectory;
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##RegistrationOutputDirectory", &outputText, ImGuiInputTextFlags_ReadOnly);
      }
      else {
        ImGui::TextDisabled("-");
      }
      ImGui::PopID();
    }

    ImGui::EndTable();
  }

  if (ImGui::BeginPopupModal("Remove Registration Temporary Files", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    if (s_temporaryFilesToRemove.empty()) {
      ImGui::TextWrapped("%s", "No registration temporary files were found.");
      if (ImGui::Button("Close")) {
        ImGui::CloseCurrentPopup();
      }
    }
    else {
      ImGui::TextWrapped(
        "%s",
        "The following registration temporary files will be removed. Files outside the system temporary directory are "
        "not included.");
      if (ImGui::BeginChild("RegistrationTemporaryFiles", ImVec2{760.0f, 260.0f}, ImGuiChildFlags_Borders)) {
        for (const std::filesystem::path& path : s_temporaryFilesToRemove) {
          ImGui::TextUnformatted(path.string().c_str());
        }
      }
      ImGui::EndChild();
      ImGui::Separator();
      if (ImGui::Button("Remove files")) {
        std::size_t removed = 0;
        std::size_t failed = 0;
        for (const std::filesystem::path& path : s_temporaryFilesToRemove) {
          std::error_code error;
          if (std::filesystem::remove(path, error) && !error) {
            ++removed;
          }
          else if (error) {
            ++failed;
          }
        }
        s_temporaryCleanupMessage = failed == 0
                                      ? std::format("Removed {} temporary file(s).", removed)
                                      : std::format("Removed {} file(s); {} could not be removed.", removed, failed);
        s_temporaryFilesToRemove.clear();
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::EndPopup();
  }

  if (!s_selectedLogJobId.empty()) {
    selectedLogJob = jobs.find(s_selectedLogJobId);
  }
  if (openDetailsPopup) {
    ImGui::OpenPopup("Image Registration Job Details");
  }
  renderRegistrationJobDetailsPopup(selectedLogJob);
  ImGui::End();
}

void renderRegistrationProgressWindow(AppData& appData)
{
  const registration::JobStore& jobs = appData.registrationJobs();
  if (!jobs.hasActiveJobs()) {
    return;
  }

  const registration::JobRecord* activeJob = nullptr;
  for (const registration::JobRecord& job : jobs.jobs()) {
    if (registration::isActiveJobStatus(job.status)) {
      activeJob = &job;
      break;
    }
  }
  if (!activeJob) {
    return;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const ImVec2 padding = ImGui::GetStyle().WindowPadding;
  const ImVec2 pos{viewport->WorkPos.x + padding.x, viewport->WorkPos.y + viewport->WorkSize.y - padding.y};
  ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2{0.0f, 1.0f});
  ImGui::SetNextWindowSizeConstraints(ImVec2{280.0f, 0.0f}, ImVec2{520.0f, FLT_MAX});

  constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                     ImGuiWindowFlags_NoNav;
  if (ImGui::Begin("RegistrationProgress", nullptr, flags)) {
    ImGui::TextUnformatted("Registration job");
    ImGui::TextWrapped("%s", jobTitle(*activeJob).c_str());
    ImGui::TextColored(statusColor(activeJob->status), "%s", statusText(activeJob->status));
    ImGui::ProgressBar(progressFraction(*activeJob), ImVec2{ImGui::GetContentRegionAvail().x, 0.0f});
    const std::string message = jobMessageText(*activeJob);
    if (!message.empty()) {
      ImGui::TextWrapped("%s", message.c_str());
    }
    else {
      ImGui::TextDisabled("-");
    }
  }
  ImGui::End();
}
