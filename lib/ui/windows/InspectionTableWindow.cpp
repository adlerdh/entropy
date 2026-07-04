#include "ui/windows/InspectionWindow.h"

#include "ui/Helpers.h"
#include "ui/headers/HeaderCommon.h"
#include "logic/app/Data.h"

#include "image/Image.h"
#include "image/ImageDerivedData.h"

#include <IconsForkAwesome.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/color_space.hpp>
#include <glm/gtx/component_wise.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <unordered_map>
#include <vector>

namespace
{
using uuid = uuids::uuid;

const ImVec4 whiteText(1, 1, 1, 1);
const ImVec4 blackText(0, 0, 0, 1);

std::size_t visibleImageCount(const AppData& appData)
{
  const std::size_t numImages = appData.numImages();
  return std::min(numImages, appData.guiData().m_visibleImageCountDuringLoad.value_or(numImages));
}

enum class InspectorColumn : int
{
  Image = 0,
  Value,
  InterpolatedValue,
  Percentile,
  Real,
  Imaginary,
  Phase,
  Minimum,
  Mean,
  Maximum,
  Magnitude,
  JacobianDeterminant,
  LogJacobianDeterminant,
  CurlMagnitude,
  Divergence,
  Label,
  Region,
  Voxel,
  Subject,
  SampleVoxel,
  SampleSubject,
  TimeFrame,
  TimeValue,
  Count
};

constexpr std::size_t k_inspectorColumnCount = static_cast<std::size_t>(InspectorColumn::Count);
static_assert(
  std::tuple_size_v<std::remove_reference_t<decltype(std::declval<GuiData&>().m_inspectionColumnVisible)> > ==
  k_inspectorColumnCount);

constexpr std::array<const char*, k_inspectorColumnCount> k_inspectorColumnNames{
  "Image",      "Value", "Value (interp.)", "Percentile", "Real",          "Imaginary",         "Phase",
  "Minimum",    "Mean",  "Maximum",         "Magnitude",  "Jacobian Det.", "Log-Jacobian Det.", "Curl Mag.",
  "Divergence", "Label", "Region",          "Voxel",      "Subject (mm)",  "Sampled voxel",     "Sampled subject (mm)",
  "Time frame", "Time"};

constexpr std::array<float, k_inspectorColumnCount> k_inspectorColumnMinWidths{
  120.0f, 64.0f, 72.0f, 78.0f, 64.0f, 82.0f, 72.0f,  78.0f,  64.0f,  78.0f, 88.0f, 108.0f,
  132.0f, 90.0f, 94.0f, 48.0f, 72.0f, 96.0f, 148.0f, 125.0f, 160.0f, 82.0f, 64.0f};

constexpr std::array<float, k_inspectorColumnCount> k_inspectorColumnDefaultWidths{
  150.0f, 82.0f,  122.0f, 100.0f, 82.0f,  112.0f, 96.0f,  96.0f,  82.0f,  96.0f, 108.0f, 122.0f,
  150.0f, 104.0f, 112.0f, 62.0f,  140.0f, 125.0f, 210.0f, 150.0f, 210.0f, 96.0f, 82.0f};

constexpr std::array<float, k_inspectorColumnCount> k_inspectorColumnMaxWidths{
  640.0f, 110.0f, 130.0f, 115.0f, 110.0f, 130.0f, 120.0f, 115.0f, 110.0f, 115.0f, 125.0f, 140.0f,
  170.0f, 120.0f, 130.0f, 72.0f,  180.0f, 150.0f, 230.0f, 170.0f, 230.0f, 120.0f, 110.0f};

constexpr std::array<bool, k_inspectorColumnCount> k_inspectorColumnCanHide{
  false, true, true, true, true, true, true, true, true, true, true, true,
  true,  true, true, true, true, true, true, true, true, true, true};

constexpr int columnIndex(InspectorColumn column)
{
  return static_cast<int>(column);
}

std::optional<double> activeComponentPercentile(const Image& image, const std::vector<double>& imageValuesNN)
{
  const uint32_t activeComponent = image.settings().activeComponent();
  if (activeComponent >= imageValuesNN.size()) {
    return std::nullopt;
  }

  const double value = imageValuesNN.at(activeComponent);
  const QuantileOfValue quantile = isComponentFloatingPoint(image.header().memoryComponentType())
                                     ? image.valueToQuantile(activeComponent, value)
                                     : image.valueToQuantile(activeComponent, static_cast<int64_t>(value));

  const double percentile = 50.0 * (quantile.lowerQuantile + quantile.upperQuantile);
  return std::clamp(percentile, 0.0, 100.0);
}

bool hasMultiComponentImage(const AppData& appData)
{
  const std::size_t numVisibleImages = visibleImageCount(appData);
  for (std::size_t imageIndex = 0; imageIndex < numVisibleImages; ++imageIndex) {
    const auto imageUid = appData.imageUid(imageIndex);
    const Image* image = imageUid ? appData.image(*imageUid) : nullptr;
    if (image && image->header().numComponentsPerPixel() > 1) {
      return true;
    }
  }

  return false;
}

bool hasComplexValuedImage(const AppData& appData)
{
  const std::size_t numVisibleImages = visibleImageCount(appData);
  for (std::size_t imageIndex = 0; imageIndex < numVisibleImages; ++imageIndex) {
    const auto imageUid = appData.imageUid(imageIndex);
    const Image* image = imageUid ? appData.image(*imageUid) : nullptr;
    if (image && isComplexValuedImage(*image)) {
      return true;
    }
  }

  return false;
}

bool hasVectorFieldImage(const AppData& appData)
{
  const std::size_t numVisibleImages = visibleImageCount(appData);
  for (std::size_t imageIndex = 0; imageIndex < numVisibleImages; ++imageIndex) {
    const auto imageUid = appData.imageUid(imageIndex);
    const Image* image = imageUid ? appData.image(*imageUid) : nullptr;
    if (image && isVectorFieldImage(*image)) {
      return true;
    }
  }

  return false;
}

bool hasTimeSeriesImage(const AppData& appData)
{
  const std::size_t numVisibleImages = visibleImageCount(appData);
  for (std::size_t imageIndex = 0; imageIndex < numVisibleImages; ++imageIndex) {
    const auto imageUid = appData.imageUid(imageIndex);
    const Image* image = imageUid ? appData.image(*imageUid) : nullptr;
    if (image && image->isTimeSeries()) {
      return true;
    }
  }

  return false;
}

bool hasWarpedImage(const AppData& appData)
{
  const std::size_t numVisibleImages = visibleImageCount(appData);
  for (std::size_t imageIndex = 0; imageIndex < numVisibleImages; ++imageIndex) {
    const auto imageUid = appData.imageUid(imageIndex);
    if (!imageUid) {
      continue;
    }
    const Image* image = appData.image(*imageUid);
    const std::optional<uuid> defUid = appData.imageToActiveInverseWarpUid(*imageUid);
    if (
      image && image->settings().warpEnabled() && image->settings().warpStrength() > 0.0f && defUid &&
      appData.warpField(*defUid))
    {
      return true;
    }
  }

  return false;
}

std::optional<uint32_t> timeFrameValue(const Image& image)
{
  if (!image.isTimeSeries()) {
    return std::nullopt;
  }

  return image.timeAxis().clamp(image.settings().activeTimePoint());
}

std::optional<double> timeValue(const Image& image)
{
  if (const std::optional<uint32_t> timePoint = timeFrameValue(image)) {
    return image.timeAxis().value(*timePoint);
  }

  return std::nullopt;
}

std::string timeValueColumnName(const AppData& appData)
{
  std::optional<std::string> units;
  const std::size_t numVisibleImages = visibleImageCount(appData);
  for (std::size_t imageIndex = 0; imageIndex < numVisibleImages; ++imageIndex) {
    const auto imageUid = appData.imageUid(imageIndex);
    const Image* image = imageUid ? appData.image(*imageUid) : nullptr;
    if (!image || !image->isTimeSeries()) {
      continue;
    }

    const std::string& imageUnits = image->timeAxis().units();
    if (!units) {
      units = imageUnits;
      continue;
    }
    if (*units != imageUnits) {
      return "Time";
    }
  }

  if (units && !units->empty()) {
    return "Time (" + *units + ")";
  }
  return "Time";
}

bool usesGlobalImageVisibilityControl(const Image& image)
{
  return image.header().numComponentsPerPixel() > 1;
}

bool imageVisibleInAllViews(const Image& image)
{
  return usesGlobalImageVisibilityControl(image) ? image.settings().globalVisibility() : image.settings().visibility();
}

void setImageVisibleInAllViews(Image& image, bool visible)
{
  if (usesGlobalImageVisibilityControl(image)) {
    image.settings().setGlobalVisibility(visible);
  }
  else {
    image.settings().setVisibility(visible);
  }
}

bool isComponentProjectionColumn(InspectorColumn column)
{
  switch (column) {
    case InspectorColumn::Minimum:
    case InspectorColumn::Mean:
    case InspectorColumn::Maximum:
    case InspectorColumn::Magnitude:
      return true;
    case InspectorColumn::Image:
    case InspectorColumn::TimeFrame:
    case InspectorColumn::TimeValue:
    case InspectorColumn::Value:
    case InspectorColumn::InterpolatedValue:
    case InspectorColumn::Percentile:
    case InspectorColumn::Real:
    case InspectorColumn::Imaginary:
    case InspectorColumn::Phase:
    case InspectorColumn::JacobianDeterminant:
    case InspectorColumn::LogJacobianDeterminant:
    case InspectorColumn::CurlMagnitude:
    case InspectorColumn::Divergence:
    case InspectorColumn::Label:
    case InspectorColumn::Region:
    case InspectorColumn::Voxel:
    case InspectorColumn::Subject:
    case InspectorColumn::SampleVoxel:
    case InspectorColumn::SampleSubject:
    case InspectorColumn::Count:
      return false;
  }

  return false;
}

bool isVectorDerivativeColumn(InspectorColumn column)
{
  switch (column) {
    case InspectorColumn::JacobianDeterminant:
    case InspectorColumn::LogJacobianDeterminant:
    case InspectorColumn::CurlMagnitude:
    case InspectorColumn::Divergence:
      return true;
    case InspectorColumn::Image:
    case InspectorColumn::TimeFrame:
    case InspectorColumn::TimeValue:
    case InspectorColumn::Value:
    case InspectorColumn::InterpolatedValue:
    case InspectorColumn::Percentile:
    case InspectorColumn::Real:
    case InspectorColumn::Imaginary:
    case InspectorColumn::Phase:
    case InspectorColumn::Minimum:
    case InspectorColumn::Mean:
    case InspectorColumn::Maximum:
    case InspectorColumn::Magnitude:
    case InspectorColumn::Label:
    case InspectorColumn::Region:
    case InspectorColumn::Voxel:
    case InspectorColumn::Subject:
    case InspectorColumn::SampleVoxel:
    case InspectorColumn::SampleSubject:
    case InspectorColumn::Count:
      return false;
  }

  return false;
}

bool isComplexColumn(InspectorColumn column)
{
  switch (column) {
    case InspectorColumn::Real:
    case InspectorColumn::Imaginary:
    case InspectorColumn::Phase:
      return true;
    case InspectorColumn::Image:
    case InspectorColumn::TimeFrame:
    case InspectorColumn::TimeValue:
    case InspectorColumn::Value:
    case InspectorColumn::InterpolatedValue:
    case InspectorColumn::Percentile:
    case InspectorColumn::Minimum:
    case InspectorColumn::Mean:
    case InspectorColumn::Maximum:
    case InspectorColumn::Magnitude:
    case InspectorColumn::JacobianDeterminant:
    case InspectorColumn::LogJacobianDeterminant:
    case InspectorColumn::CurlMagnitude:
    case InspectorColumn::Divergence:
    case InspectorColumn::Label:
    case InspectorColumn::Region:
    case InspectorColumn::Voxel:
    case InspectorColumn::Subject:
    case InspectorColumn::SampleVoxel:
    case InspectorColumn::SampleSubject:
    case InspectorColumn::Count:
      return false;
  }

  return false;
}

bool isWarpedCoordinateColumn(InspectorColumn column)
{
  switch (column) {
    case InspectorColumn::SampleVoxel:
    case InspectorColumn::SampleSubject:
      return true;
    case InspectorColumn::Image:
    case InspectorColumn::TimeFrame:
    case InspectorColumn::TimeValue:
    case InspectorColumn::Value:
    case InspectorColumn::InterpolatedValue:
    case InspectorColumn::Percentile:
    case InspectorColumn::Real:
    case InspectorColumn::Imaginary:
    case InspectorColumn::Phase:
    case InspectorColumn::Minimum:
    case InspectorColumn::Mean:
    case InspectorColumn::Maximum:
    case InspectorColumn::Magnitude:
    case InspectorColumn::JacobianDeterminant:
    case InspectorColumn::LogJacobianDeterminant:
    case InspectorColumn::CurlMagnitude:
    case InspectorColumn::Divergence:
    case InspectorColumn::Label:
    case InspectorColumn::Region:
    case InspectorColumn::Voxel:
    case InspectorColumn::Subject:
    case InspectorColumn::Count:
      return false;
  }

  return false;
}

struct WarpedSamplePosition
{
  glm::dvec3 voxel;
  glm::dvec3 subject;
};

std::optional<WarpedSamplePosition>
warpedSamplePosition(const AppData& appData, const uuid& imageUid, const Image& image, const glm::vec3& subjectPos)
{
  if (!image.settings().warpEnabled() || image.settings().warpStrength() <= 0.0f) {
    return std::nullopt;
  }

  const std::optional<uuid> defUid = appData.imageToActiveInverseWarpUid(imageUid);
  const Image* def = defUid ? appData.warpField(*defUid) : nullptr;
  if (!def || def->header().numComponentsPerPixel() < 3) {
    return std::nullopt;
  }

  const glm::dvec4 worldPosH =
    glm::dmat4{image.transformations().worldDef_T_subject()} * glm::dvec4{glm::dvec3{subjectPos}, 1.0};
  const glm::dvec3 worldPos = glm::dvec3{worldPosH} / worldPosH.w;
  const glm::dvec4 defVoxelH = glm::dmat4{def->transformations().pixel_T_worldDef()} * glm::dvec4{worldPos, 1.0};
  const glm::dvec3 defVoxel = glm::dvec3{defVoxelH} / defVoxelH.w;
  const uint32_t timePoint = def->timeAxis().clamp(def->settings().activeTimePoint());

  const auto dx = def->valueLinear<double>(0, defVoxel.x, defVoxel.y, defVoxel.z, timePoint);
  const auto dy = def->valueLinear<double>(1, defVoxel.x, defVoxel.y, defVoxel.z, timePoint);
  const auto dz = def->valueLinear<double>(2, defVoxel.x, defVoxel.y, defVoxel.z, timePoint);
  if (!dx || !dy || !dz) {
    return std::nullopt;
  }

  const glm::dvec3 sampleWorld =
    worldPos + static_cast<double>(image.settings().warpStrength()) * glm::dvec3{*dx, *dy, *dz};
  const glm::dvec4 sampleVoxelH = glm::dmat4{image.transformations().pixel_T_worldDef()} * glm::dvec4{sampleWorld, 1.0};
  const glm::dvec4 sampleSubjectH =
    glm::dmat4{image.transformations().subject_T_worldDef()} * glm::dvec4{sampleWorld, 1.0};

  return WarpedSamplePosition{glm::dvec3{sampleVoxelH} / sampleVoxelH.w, glm::dvec3{sampleSubjectH} / sampleSubjectH.w};
}

std::optional<double> complexColumnValue(InspectorColumn column, const Image& image, const std::vector<double>& values)
{
  if (!isComplexValuedImage(image) || values.size() < 2 || !isComplexColumn(column)) {
    return std::nullopt;
  }

  switch (column) {
    case InspectorColumn::Real:
      return values.at(0);
    case InspectorColumn::Imaginary:
      return values.at(1);
    case InspectorColumn::Phase:
      return complexPhaseValue(
        values.at(0),
        values.at(1),
        image.settings().complexPhaseRange(),
        image.settings().complexPhaseUnit());
    case InspectorColumn::Image:
    case InspectorColumn::TimeFrame:
    case InspectorColumn::TimeValue:
    case InspectorColumn::Value:
    case InspectorColumn::InterpolatedValue:
    case InspectorColumn::Percentile:
    case InspectorColumn::Minimum:
    case InspectorColumn::Mean:
    case InspectorColumn::Maximum:
    case InspectorColumn::Magnitude:
    case InspectorColumn::JacobianDeterminant:
    case InspectorColumn::LogJacobianDeterminant:
    case InspectorColumn::CurlMagnitude:
    case InspectorColumn::Divergence:
    case InspectorColumn::Label:
    case InspectorColumn::Region:
    case InspectorColumn::Voxel:
    case InspectorColumn::Subject:
    case InspectorColumn::SampleVoxel:
    case InspectorColumn::SampleSubject:
    case InspectorColumn::Count:
      break;
  }

  return std::nullopt;
}

std::optional<double> componentProjectionValue(InspectorColumn column, const std::vector<double>& componentValues)
{
  if (componentValues.size() < 2 || !isComponentProjectionColumn(column)) {
    return std::nullopt;
  }

  double minimum = std::numeric_limits<double>::max();
  double maximum = std::numeric_limits<double>::lowest();
  double sum = 0.0;
  double sumSquares = 0.0;
  std::size_t finiteCount = 0;

  for (const double value : componentValues) {
    if (!std::isfinite(value)) {
      continue;
    }

    minimum = std::min(minimum, value);
    maximum = std::max(maximum, value);
    sum += value;
    sumSquares += value * value;
    ++finiteCount;
  }

  if (0 == finiteCount) {
    return std::nullopt;
  }

  switch (column) {
    case InspectorColumn::Minimum:
      return minimum;
    case InspectorColumn::Mean:
      return sum / static_cast<double>(finiteCount);
    case InspectorColumn::Maximum:
      return maximum;
    case InspectorColumn::Magnitude:
      return std::sqrt(sumSquares);
    case InspectorColumn::Image:
    case InspectorColumn::TimeFrame:
    case InspectorColumn::TimeValue:
    case InspectorColumn::Value:
    case InspectorColumn::InterpolatedValue:
    case InspectorColumn::Percentile:
    case InspectorColumn::Real:
    case InspectorColumn::Imaginary:
    case InspectorColumn::Phase:
    case InspectorColumn::JacobianDeterminant:
    case InspectorColumn::LogJacobianDeterminant:
    case InspectorColumn::CurlMagnitude:
    case InspectorColumn::Divergence:
    case InspectorColumn::Label:
    case InspectorColumn::Region:
    case InspectorColumn::Voxel:
    case InspectorColumn::Subject:
    case InspectorColumn::SampleVoxel:
    case InspectorColumn::SampleSubject:
    case InspectorColumn::Count:
      break;
  }

  return std::nullopt;
}

std::optional<double>
vectorDerivativeColumnValue(InspectorColumn column, const Image& image, const std::optional<glm::ivec3>& voxelPos)
{
  if (!voxelPos || !isVectorFieldImage(image) || !isVectorDerivativeColumn(column)) {
    return std::nullopt;
  }
  if (voxelPos->x < 0 || voxelPos->y < 0 || voxelPos->z < 0) {
    return std::nullopt;
  }

  const glm::uvec3 voxel{
    static_cast<uint32_t>(voxelPos->x),
    static_cast<uint32_t>(voxelPos->y),
    static_cast<uint32_t>(voxelPos->z)};
  const uint32_t timePoint = image.timeAxis().clamp(image.settings().activeTimePoint());

  switch (column) {
    case InspectorColumn::JacobianDeterminant:
      return vectorDerivativeProjectionValue(
        image,
        ComponentProjectionMode::VectorJacobianDeterminant,
        voxel,
        timePoint);
    case InspectorColumn::LogJacobianDeterminant:
      return vectorDerivativeProjectionValue(
        image,
        ComponentProjectionMode::VectorLogJacobianDeterminant,
        voxel,
        timePoint);
    case InspectorColumn::CurlMagnitude:
      return vectorDerivativeProjectionValue(image, ComponentProjectionMode::VectorCurlMagnitude, voxel, timePoint);
    case InspectorColumn::Divergence:
      return vectorDerivativeProjectionValue(image, ComponentProjectionMode::VectorDivergence, voxel, timePoint);
    case InspectorColumn::Image:
    case InspectorColumn::TimeFrame:
    case InspectorColumn::TimeValue:
    case InspectorColumn::Value:
    case InspectorColumn::InterpolatedValue:
    case InspectorColumn::Percentile:
    case InspectorColumn::Real:
    case InspectorColumn::Imaginary:
    case InspectorColumn::Phase:
    case InspectorColumn::Minimum:
    case InspectorColumn::Mean:
    case InspectorColumn::Maximum:
    case InspectorColumn::Magnitude:
    case InspectorColumn::Label:
    case InspectorColumn::Region:
    case InspectorColumn::Voxel:
    case InspectorColumn::Subject:
    case InspectorColumn::SampleVoxel:
    case InspectorColumn::SampleSubject:
    case InspectorColumn::Count:
      break;
  }

  return std::nullopt;
}

float inspectionColumnWidthForText(const char* text)
{
  const ImGuiStyle& style = ImGui::GetStyle();
  return ImGui::CalcTextSize(text).x + 2.0f * (style.CellPadding.x + style.FramePadding.x);
}

float imageColumnWidthForCellText(const char* displayName, const char* roleSuffix)
{
  const ImGuiStyle& style = ImGui::GetStyle();
  const float visibilityButtonWidth = ImGui::GetFrameHeight();
  const float nameWidth = ImGui::CalcTextSize(displayName).x + 2.0f * style.FramePadding.x;
  const float suffixWidth =
    roleSuffix && roleSuffix[0] != '\0' ? ImGui::CalcTextSize(roleSuffix).x + style.ItemSpacing.x : 0.0f;
  return visibilityButtonWidth + style.ItemSpacing.x + nameWidth + suffixWidth + 2.0f * style.CellPadding.x;
}

void submitAutosizeWidthMarker(float width)
{
  if (width <= 0.0f) {
    return;
  }

  const ImVec2 cursorPos = ImGui::GetCursorPos();
  ImGui::Dummy(ImVec2{width, 0.0f});
  ImGui::SetCursorPos(cursorPos);
}

float clampInspectionColumnWidth(InspectorColumn column, float width)
{
  const std::size_t index = static_cast<std::size_t>(column);
  return std::clamp(width, k_inspectorColumnMinWidths.at(index), k_inspectorColumnMaxWidths.at(index));
}

const char* inspectionColumnTooltip(InspectorColumn column)
{
  switch (column) {
    case InspectorColumn::TimeFrame:
      return "Displayed time frame index";
    case InspectorColumn::TimeValue:
      return "Physical time value for the displayed frame";
    case InspectorColumn::Value:
      return "Nearest image voxel value";
    case InspectorColumn::InterpolatedValue:
      return "Linearly interpolated image voxel value";
    case InspectorColumn::Percentile:
      return "Image voxel value percentile";
    case InspectorColumn::Real:
      return "Real component value at the nearest image voxel";
    case InspectorColumn::Imaginary:
      return "Imaginary component value at the nearest image voxel";
    case InspectorColumn::Phase:
      return "Complex phase at the nearest image voxel";
    case InspectorColumn::Minimum:
      return "Minimum component value at the nearest image voxel";
    case InspectorColumn::Mean:
      return "Mean component value at the nearest image voxel";
    case InspectorColumn::Maximum:
      return "Maximum component value at the nearest image voxel";
    case InspectorColumn::Magnitude:
      return "Magnitude of the component vector at the nearest image voxel";
    case InspectorColumn::JacobianDeterminant:
      return "Jacobian determinant of the vector field at the nearest image voxel";
    case InspectorColumn::LogJacobianDeterminant:
      return "Natural log of the Jacobian determinant at the nearest image voxel";
    case InspectorColumn::CurlMagnitude:
      return "Curl magnitude of the vector field at the nearest image voxel";
    case InspectorColumn::Divergence:
      return "Divergence of the vector field at the nearest image voxel";
    case InspectorColumn::Label:
      return "Segmentation label value";
    case InspectorColumn::Region:
      return "Segmentation label region name";
    case InspectorColumn::Voxel:
      return "Voxel index (i: column, j: row, k: slice)";
    case InspectorColumn::Subject:
      return "Subject-space LPS coordinates in millimeters (x: R->L, y: A->P, z: I->S)";
    case InspectorColumn::SampleVoxel:
      return "Moving-image voxel coordinate sampled after applying the inverse warp";
    case InspectorColumn::SampleSubject:
      return "Moving-image subject coordinate sampled after applying the inverse warp";
    case InspectorColumn::Image:
    case InspectorColumn::Count:
      break;
  }

  return nullptr;
}
} // namespace

void renderInspectionWindowWithTable(
  AppData& appData,
  const std::function<std::pair<std::string, std::string>(std::size_t index)>& getImageDisplayAndFileName,
  const std::function<std::optional<glm::vec3>(std::size_t imageIndex)>& getSubjectPos,
  const std::function<std::optional<glm::ivec3>(std::size_t imageIndex)>& getVoxelPos,
  const std::function<void(std::size_t imageIndex, const glm::vec3& subjectPos)> setSubjectPos,
  const std::function<void(std::size_t imageIndex, const glm::ivec3& voxelPos)> setVoxelPos,
  const std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)>& getImageValuesNN,
  const std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)>& getImageValuesLinear,
  const std::function<std::optional<int64_t>(std::size_t imageIndex)>& getSegLabel,
  const std::function<ParcellationLabelTable*(std::size_t tableIndex)>& getLabelTable,
  const std::function<void(const uuids::uuid& imageUid)>& updateImageUniforms)
{
  static bool s_firstRun = true; // Is this the first run?

  static constexpr float k_pad = 10.0f;
  static int corner = -1;

  //    bool selectionButtonShown = false;

  static const ImVec2 k_windowPadding(0.0f, 0.0f);
  static const ImVec2 k_itemInnerSpacing(1.0f, 1.0f);
  static const ImVec2 k_cellPadding(0.0f, 0.0f);
  static const float k_windowRounding(0.0f);

  //    static const ImVec4 blueColor( 0.0f, 0.5f, 1.0f, 1.0f );

  static const ImGuiTableFlags k_tableFlags =
    ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders |
    ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;

  static const ImGuiWindowFlags k_windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoFocusOnAppearing;

  static bool s_showTitleBar = false;
  static bool s_autoSizeColumnsRequested = false;
  static std::array<bool, k_inspectorColumnCount> s_autoSizeColumnRequested{};
  static bool s_hadTimeSeriesColumnAvailable = false;
  static bool s_hadWarpedCoordinateColumnAvailable = false;
  const bool canShowTimeFrameColumn = hasTimeSeriesImage(appData);
  const bool canShowComponentProjectionColumns = hasMultiComponentImage(appData);
  const bool canShowComplexColumns = hasComplexValuedImage(appData);
  const bool canShowVectorDerivativeColumns = hasVectorFieldImage(appData);
  const bool canShowWarpedCoordinateColumns = hasWarpedImage(appData);
  if (!canShowWarpedCoordinateColumns) {
    appData.guiData().m_inspectionColumnVisible.at(columnIndex(InspectorColumn::SampleVoxel)) = false;
    appData.guiData().m_inspectionColumnVisible.at(columnIndex(InspectorColumn::SampleSubject)) = false;
  }

  if (canShowTimeFrameColumn && !s_hadTimeSeriesColumnAvailable) {
    appData.guiData().m_inspectionColumnVisible.at(columnIndex(InspectorColumn::TimeFrame)) = true;
    appData.guiData().m_inspectionColumnVisible.at(columnIndex(InspectorColumn::TimeValue)) = true;
    s_autoSizeColumnRequested.at(columnIndex(InspectorColumn::TimeFrame)) = true;
    s_autoSizeColumnRequested.at(columnIndex(InspectorColumn::TimeValue)) = true;
  }
  s_hadTimeSeriesColumnAvailable = canShowTimeFrameColumn;
  if (canShowWarpedCoordinateColumns && !s_hadWarpedCoordinateColumnAvailable) {
    appData.guiData().m_inspectionColumnVisible.at(columnIndex(InspectorColumn::SampleVoxel)) = true;
    appData.guiData().m_inspectionColumnVisible.at(columnIndex(InspectorColumn::SampleSubject)) = true;
    s_autoSizeColumnRequested.at(columnIndex(InspectorColumn::SampleVoxel)) = true;
    s_autoSizeColumnRequested.at(columnIndex(InspectorColumn::SampleSubject)) = true;
  }
  s_hadWarpedCoordinateColumnAvailable = canShowWarpedCoordinateColumns;
  const std::string timeValueHeader = timeValueColumnName(appData);

  // For which images to show coordinates?
  static std::unordered_map<uuid, bool> s_showSubject;

  if (s_firstRun) {
    // Show all images by default:
    const std::size_t numVisibleImages = visibleImageCount(appData);
    for (std::size_t imageIndex = 0; imageIndex < numVisibleImages; ++imageIndex) {
      if (const auto imageUid = appData.imageUid(imageIndex)) {
        s_showSubject.insert({*imageUid, true});
      }
    }

    s_firstRun = false;
  }

  auto renderColumnVisibilityItems = [&appData,
                                      &timeValueHeader,
                                      canShowTimeFrameColumn,
                                      canShowComponentProjectionColumns,
                                      canShowComplexColumns,
                                      canShowVectorDerivativeColumns,
                                      canShowWarpedCoordinateColumns]() {
    if (ImGui::MenuItem("Auto-size columns")) {
      s_autoSizeColumnsRequested = true;
    }
    ImGui::Separator();

    for (std::size_t column = 0; column < k_inspectorColumnNames.size(); ++column) {
      const auto inspectorColumn = static_cast<InspectorColumn>(column);
      if (
        (InspectorColumn::TimeFrame == inspectorColumn || InspectorColumn::TimeValue == inspectorColumn) &&
        !canShowTimeFrameColumn)
      {
        appData.guiData().m_inspectionColumnVisible.at(column) = false;
        continue;
      }
      if (isComponentProjectionColumn(inspectorColumn) && !canShowComponentProjectionColumns) {
        appData.guiData().m_inspectionColumnVisible.at(column) = false;
        continue;
      }
      if (isComplexColumn(inspectorColumn) && !canShowComplexColumns) {
        appData.guiData().m_inspectionColumnVisible.at(column) = false;
        continue;
      }
      if (isVectorDerivativeColumn(inspectorColumn) && !canShowVectorDerivativeColumns) {
        appData.guiData().m_inspectionColumnVisible.at(column) = false;
        continue;
      }
      if (isWarpedCoordinateColumn(inspectorColumn) && !canShowWarpedCoordinateColumns) {
        appData.guiData().m_inspectionColumnVisible.at(column) = false;
        continue;
      }

      bool visible = appData.guiData().m_inspectionColumnVisible.at(column);
      if (!k_inspectorColumnCanHide.at(column)) {
        ImGui::BeginDisabled();
      }
      const char* columnName = k_inspectorColumnNames.at(column);
      if (InspectorColumn::TimeValue == inspectorColumn) {
        columnName = timeValueHeader.c_str();
      }
      if (ImGui::MenuItem(columnName, nullptr, visible) && k_inspectorColumnCanHide.at(column)) {
        const bool newVisible = !visible;
        appData.guiData().m_inspectionColumnVisible.at(column) = newVisible;
        if (newVisible) {
          s_autoSizeColumnRequested.at(column) = true;
        }
      }
      if (!k_inspectorColumnCanHide.at(column)) {
        ImGui::EndDisabled();
      }
    }
  };

  auto renderColumnVisibilityMenu = [&renderColumnVisibilityItems]() {
    if (ImGui::BeginMenu("Columns")) {
      renderColumnVisibilityItems();
      ImGui::EndMenu();
    }
  };

  auto renderImagesItems = [&appData, &getImageDisplayAndFileName]() {
    const std::size_t numVisibleImages = visibleImageCount(appData);
    for (std::size_t imageIndex = 0; imageIndex < numVisibleImages; ++imageIndex) {
      const auto imageUid = appData.imageUid(imageIndex);
      if (!imageUid) continue;

      auto it = s_showSubject.find(*imageUid);
      if (std::end(s_showSubject) == it) {
        s_showSubject.insert({*imageUid, true});
      }

      bool& visible = s_showSubject[*imageUid];
      const auto names = getImageDisplayAndFileName(imageIndex);

      if (ImGui::MenuItem(names.first.c_str(), nullptr, visible)) {
        visible = !visible;
      }

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", names.second.c_str());
      }
    }
  };

  auto renderImagesMenu = [&renderImagesItems]() {
    if (ImGui::BeginMenu("Images...")) {
      renderImagesItems();
      ImGui::EndMenu();
    }
  };

  auto renderWindowItems = [&appData]() {
    if (ImGui::BeginMenu("Position")) {
      if (ImGui::MenuItem("Custom", nullptr, corner == -1)) corner = -1;
      if (ImGui::MenuItem("Top-left", nullptr, corner == 0)) corner = 0;
      if (ImGui::MenuItem("Top-right", nullptr, corner == 1)) corner = 1;
      if (ImGui::MenuItem("Bottom-left", nullptr, corner == 2)) corner = 2;
      if (ImGui::MenuItem("Bottom-right", nullptr, corner == 3)) corner = 3;
      ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Show title bar", nullptr, s_showTitleBar)) {
      s_showTitleBar = !s_showTitleBar;
    }

    ImGui::Separator();
    if (appData.guiData().m_showInspectionWindow && ImGui::MenuItem("Close")) {
      appData.guiData().m_showInspectionWindow = false;
    }
  };

  auto renderWindowMenu = [&renderWindowItems]() {
    if (ImGui::BeginMenu("Window")) {
      renderWindowItems();
      ImGui::EndMenu();
    }
  };

  auto contextMenu = [&renderImagesMenu, &renderColumnVisibilityMenu, &renderWindowMenu]() {
    renderImagesMenu();
    renderColumnVisibilityMenu();
    renderWindowMenu();
  };

  auto dockedMenu = [&renderImagesMenu, &renderColumnVisibilityMenu]() {
    renderImagesMenu();
    renderColumnVisibilityMenu();
  };

  auto columnWidths = [&]() {
    std::array<float, k_inspectorColumnCount> widths{};
    for (std::size_t column = 0; column < widths.size(); ++column) {
      widths[column] = std::max(
        k_inspectorColumnDefaultWidths.at(column),
        inspectionColumnWidthForText(k_inspectorColumnNames.at(column)));
    }

    auto expandWidth = [&widths](InspectorColumn column, const char* text) {
      const std::size_t index = static_cast<std::size_t>(column);
      widths[index] = std::max(widths[index], inspectionColumnWidthForText(text));
    };

    expandWidth(InspectorColumn::TimeFrame, "000");
    expandWidth(InspectorColumn::TimeValue, "-0000.000");
    expandWidth(InspectorColumn::Value, "-0000.000");
    expandWidth(InspectorColumn::InterpolatedValue, "-0000.000");
    expandWidth(InspectorColumn::Percentile, "100.00");
    expandWidth(InspectorColumn::Label, "00000");
    expandWidth(InspectorColumn::Voxel, "0000, 0000, 0000");
    expandWidth(InspectorColumn::Subject, "-000.000, -000.000, -000.000");
    expandWidth(InspectorColumn::SampleVoxel, "-0000.000, -0000.000, -0000.000");
    expandWidth(InspectorColumn::SampleSubject, "-000.000, -000.000, -000.000");

    for (std::size_t imageIndex = 0; imageIndex < appData.numImages(); ++imageIndex) {
      const auto imageUid = appData.imageUid(imageIndex);
      if (!imageUid) continue;

      auto it = s_showSubject.find(*imageUid);
      if (std::end(s_showSubject) != it && !it->second) continue;

      const Image* image = appData.image(*imageUid);
      if (!image) continue;

      const bool isRef = appData.refImageUid() && *appData.refImageUid() == *imageUid;
      const bool isActiveImage = appData.activeImageUid() && *appData.activeImageUid() == *imageUid;
      const std::string roleSuffix =
        entropy::ui::headers::imageRoleSuffixShortReference(isRef, isActiveImage, appData.numImages());
      const std::size_t imageColumnIndex = columnIndex(InspectorColumn::Image);
      widths[imageColumnIndex] = std::max(
        widths[imageColumnIndex],
        imageColumnWidthForCellText(image->settings().displayName().c_str(), roleSuffix.c_str()));

      if (image->isTimeSeries()) {
        expandWidth(InspectorColumn::TimeFrame, "000");
        expandWidth(InspectorColumn::TimeValue, "-0000.000");
      }

      if (image->header().numComponentsPerPixel() > 1) {
        expandWidth(InspectorColumn::Value, "-0000.000, -0000.000, -0000.000");
        expandWidth(InspectorColumn::InterpolatedValue, "-0000.000, -0000.000, -0000.000");
        expandWidth(InspectorColumn::Minimum, "-0000.000");
        expandWidth(InspectorColumn::Mean, "-0000.000");
        expandWidth(InspectorColumn::Maximum, "-0000.000");
        expandWidth(InspectorColumn::Magnitude, "-0000.000");
      }
      if (isVectorFieldImage(*image)) {
        expandWidth(InspectorColumn::JacobianDeterminant, "-0000.000");
        expandWidth(InspectorColumn::LogJacobianDeterminant, "-0000.000");
        expandWidth(InspectorColumn::CurlMagnitude, "-0000.000");
        expandWidth(InspectorColumn::Divergence, "-0000.000");
      }
      if (isComplexValuedImage(*image)) {
        expandWidth(InspectorColumn::Real, "-0000.000");
        expandWidth(InspectorColumn::Imaginary, "-0000.000");
        expandWidth(InspectorColumn::Phase, "-000.000");
      }

      if (const auto segLabel = getSegLabel(imageIndex)) {
        const auto labelText = std::to_string(*segLabel);
        expandWidth(InspectorColumn::Label, labelText.c_str());

        const auto segUid = appData.imageToActiveSegUid(*imageUid);
        const Image* seg = segUid ? appData.seg(*segUid) : nullptr;
        ParcellationLabelTable* table = seg ? getLabelTable(seg->settings().labelTableIndex()) : nullptr;
        if (table) {
          const std::string labelName = table->getName(static_cast<uint64_t>(*segLabel));
          expandWidth(InspectorColumn::Region, labelName.c_str());
        }
      }
    }

    for (std::size_t column = 0; column < widths.size(); ++column) {
      widths[column] = clampInspectionColumnWidth(static_cast<InspectorColumn>(column), widths[column]);
    }

    return widths;
  };

  const bool useOverlayPresentation = corner != -1;
  ImGuiWindowFlags windowFlags = k_windowFlags;

  if (corner != -1) {
    windowFlags |= ImGuiWindowFlags_NoMove;

    ImGuiIO& io = ImGui::GetIO();

    const ImVec2 windowPos(
      (corner & 1) ? io.DisplaySize.x - k_pad : k_pad,
      (corner & 2) ? io.DisplaySize.y - k_pad : k_pad);

    const ImVec2 windowPosPivot((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPosPivot);
  }

  if (corner != -1 && !s_showTitleBar) {
    windowFlags |= ImGuiWindowFlags_NoDecoration;
  }

  const ImVec4* colors = ImGui::GetStyle().Colors;
  ImVec4 menuBarBgColor = colors[ImGuiCol_MenuBarBg];
  menuBarBgColor.w /= 2.0f;

  if (useOverlayPresentation) {
    ImGui::SetNextWindowBgAlpha(0.0f);
  }

  const std::array<float, k_inspectorColumnCount> inspectorColumnWidths = columnWidths();

  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, k_cellPadding);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImGui::GetStyle().FramePadding);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, k_itemInnerSpacing);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(
    ImGuiStyleVar_WindowPadding,
    useOverlayPresentation ? k_windowPadding : ImGui::GetStyle().WindowPadding);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, k_windowRounding);

  setNextWindowSizeConstraintsToMainViewport(480.0f, 120.0f);
  ImGui::SetNextWindowSize(ImVec2{720.0f, 180.0f}, ImGuiCond_FirstUseEver);
  setNextDockablePanelWindowClass();
  if (ImGui::Begin("Voxel Inspector##InspectionWindow", &(appData.guiData().m_showInspectionWindow), windowFlags)) {
    if (useOverlayPresentation) {
      ImGui::PushStyleColor(ImGuiCol_MenuBarBg, menuBarBgColor);
    }
    if (ImGui::BeginMenuBar()) {
      if (useOverlayPresentation) {
        contextMenu();
      }
      else {
        dockedMenu();
      }
      ImGui::EndMenuBar();
    }
    if (useOverlayPresentation) {
      ImGui::PopStyleColor(1); // ImGuiCol_MenuBarBg
    }

    if (ImGui::BeginTable("Image Information", static_cast<int>(k_inspectorColumnCount), k_tableFlags)) {
      ImGui::TableSetupScrollFreeze(1, 1);

      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::Image)),
        ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthFixed,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Image)));

      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::Value)),
        ImGuiTableColumnFlags_WidthFixed,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Value)));
      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::InterpolatedValue)),
        ImGuiTableColumnFlags_WidthFixed,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::InterpolatedValue)));
      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::Percentile)),
        ImGuiTableColumnFlags_WidthFixed,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Percentile)));

      const ImGuiTableColumnFlags componentProjectionColumnFlags =
        ImGuiTableColumnFlags_WidthFixed |
        (canShowComponentProjectionColumns ? ImGuiTableColumnFlags_None : ImGuiTableColumnFlags_Disabled);
      const ImGuiTableColumnFlags complexColumnFlags =
        ImGuiTableColumnFlags_WidthFixed |
        (canShowComplexColumns ? ImGuiTableColumnFlags_None : ImGuiTableColumnFlags_Disabled);
      const ImGuiTableColumnFlags vectorDerivativeColumnFlags =
        ImGuiTableColumnFlags_WidthFixed |
        (canShowVectorDerivativeColumns ? ImGuiTableColumnFlags_None : ImGuiTableColumnFlags_Disabled);
      const ImGuiTableColumnFlags warpedCoordinateColumnFlags =
        ImGuiTableColumnFlags_WidthFixed |
        (canShowWarpedCoordinateColumns ? ImGuiTableColumnFlags_None : ImGuiTableColumnFlags_Disabled);

      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::Real)),
        complexColumnFlags,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Real)));
      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::Imaginary)),
        complexColumnFlags,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Imaginary)));
      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::Magnitude)),
        componentProjectionColumnFlags,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Magnitude)));
      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::Phase)),
        complexColumnFlags,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Phase)));

      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::Minimum)),
        componentProjectionColumnFlags,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Minimum)));
      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::Mean)),
        componentProjectionColumnFlags,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Mean)));
      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::Maximum)),
        componentProjectionColumnFlags,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Maximum)));
      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::JacobianDeterminant)),
        vectorDerivativeColumnFlags,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::JacobianDeterminant)));
      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::LogJacobianDeterminant)),
        vectorDerivativeColumnFlags,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::LogJacobianDeterminant)));
      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::CurlMagnitude)),
        vectorDerivativeColumnFlags,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::CurlMagnitude)));
      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::Divergence)),
        vectorDerivativeColumnFlags,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Divergence)));
      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::Label)),
        ImGuiTableColumnFlags_WidthFixed,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Label)));
      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::Region)),
        ImGuiTableColumnFlags_WidthFixed,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Region)));

      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::Voxel)),
        ImGuiTableColumnFlags_WidthFixed,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Voxel)));
      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::Subject)),
        ImGuiTableColumnFlags_WidthFixed,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Subject)));
      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::SampleVoxel)),
        warpedCoordinateColumnFlags,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::SampleVoxel)));
      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::SampleSubject)),
        warpedCoordinateColumnFlags,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::SampleSubject)));

      const ImGuiTableColumnFlags timeFrameColumnFlags =
        ImGuiTableColumnFlags_WidthFixed |
        (canShowTimeFrameColumn ? ImGuiTableColumnFlags_None : ImGuiTableColumnFlags_Disabled);
      ImGui::TableSetupColumn(
        k_inspectorColumnNames.at(columnIndex(InspectorColumn::TimeFrame)),
        timeFrameColumnFlags,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::TimeFrame)));
      ImGui::TableSetupColumn(
        timeValueHeader.c_str(),
        timeFrameColumnFlags,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::TimeValue)));

      for (std::size_t column = 0; column < k_inspectorColumnNames.size(); ++column) {
        ImGui::TableSetColumnEnabled(
          static_cast<int>(column),
          appData.guiData().m_inspectionColumnVisible.at(column) || !k_inspectorColumnCanHide.at(column));
      }

      ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
      if (s_autoSizeColumnsRequested) {
        for (std::size_t column = 0; column < inspectorColumnWidths.size(); ++column) {
          if (0 != (ImGui::TableGetColumnFlags(static_cast<int>(column)) & ImGuiTableColumnFlags_IsEnabled)) {
            ImGui::TableSetColumnWidth(static_cast<int>(column), inspectorColumnWidths.at(column));
          }
        }
        s_autoSizeColumnRequested.fill(false);
        s_autoSizeColumnsRequested = false;
      }
      else {
        for (std::size_t column = 0; column < s_autoSizeColumnRequested.size(); ++column) {
          if (!s_autoSizeColumnRequested.at(column)) continue;
          if (0 == (ImGui::TableGetColumnFlags(static_cast<int>(column)) & ImGuiTableColumnFlags_IsEnabled)) continue;

          ImGui::TableSetColumnWidth(static_cast<int>(column), inspectorColumnWidths.at(column));
          s_autoSizeColumnRequested.at(column) = false;
        }
      }

      for (std::size_t column = 0; column < k_inspectorColumnNames.size(); ++column) {
        ImGui::TableSetColumnIndex(static_cast<int>(column));
        const char* columnName = k_inspectorColumnNames.at(column);
        if (InspectorColumn::TimeValue == static_cast<InspectorColumn>(column)) {
          columnName = timeValueHeader.c_str();
        }
        ImGui::TableHeader(columnName);

        const char* tooltip = inspectionColumnTooltip(static_cast<InspectorColumn>(column));
        if (tooltip && ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", tooltip);
        }
      }

      const std::size_t numVisibleImages = visibleImageCount(appData);
      for (std::size_t imageIndex = 0; imageIndex < numVisibleImages; ++imageIndex) {
        const auto imageUid = appData.imageUid(imageIndex);
        Image* image = (imageUid ? appData.image(*imageUid) : nullptr);
        if (!image) continue;

        auto it = s_showSubject.find(*imageUid);
        if (std::end(s_showSubject) == it) {
          s_showSubject.insert({*imageUid, true});
        }

        if (!s_showSubject[*imageUid]) continue;

        ImGui::PushID(static_cast<int>(imageIndex)); /** PushID: imageIndex **/

        const auto segUid = appData.imageToActiveSegUid(*imageUid);
        const Image* seg = (segUid ? appData.seg(*segUid) : nullptr);

        ParcellationLabelTable* table = (seg ? getLabelTable(seg->settings().labelTableIndex()) : nullptr);

        // Get all image component values
        static constexpr bool k_getOnlyActiveComponent = false;
        std::vector<double> imageValuesNN = getImageValuesNN(imageIndex, k_getOnlyActiveComponent);
        std::vector<double> imageValuesLinear = getImageValuesLinear(imageIndex, k_getOnlyActiveComponent);

        const std::optional<int64_t> segLabel = getSegLabel(imageIndex);

        const std::optional<glm::ivec3> voxelPos = getVoxelPos(imageIndex);
        const std::optional<glm::vec3> subjectPos = getSubjectPos(imageIndex);

        auto markColumnAutosizeWidth = [&](InspectorColumn column) {
          submitAutosizeWidthMarker(inspectorColumnWidths.at(columnIndex(column)));
        };

        ImGui::TableNextColumn(); // "Image"
        markColumnAutosizeWidth(InspectorColumn::Image);

        glm::vec3 darkerBorderColorHsv = glm::hsvColor(image->settings().borderColor());
        darkerBorderColorHsv[2] = std::max(0.5f * darkerBorderColorHsv[2], 0.0f);
        const glm::vec3 darkerBorderColorRgb = glm::rgbColor(darkerBorderColorHsv);

        const ImVec4 inputTextBgColor(darkerBorderColorRgb.r, darkerBorderColorRgb.g, darkerBorderColorRgb.b, 1.0f);
        const ImVec4 inputTextFgColor = (glm::luminosity(darkerBorderColorRgb) < 0.75f) ? whiteText : blackText;

        const bool isRef = appData.refImageUid() && *appData.refImageUid() == *imageUid;
        const bool isActiveImage = appData.activeImageUid() && *appData.activeImageUid() == *imageUid;
        const std::string roleSuffix =
          entropy::ui::headers::imageRoleSuffixShortReference(isRef, isActiveImage, appData.numImages());
        const float suffixWidth =
          roleSuffix.empty() ? 0.0f : ImGui::CalcTextSize(roleSuffix.c_str()).x + ImGui::GetStyle().ItemSpacing.x;

        const bool imageVisible = imageVisibleInAllViews(*image);
        const std::string visibilityButton =
          std::string(imageVisible ? ICON_FK_EYE : ICON_FK_EYE_SLASH) + "##imageVisibility";
        const ImVec2 visibilityButtonSize{ImGui::GetFrameHeight(), ImGui::GetFrameHeight()};
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.0f, 0.0f, 0.0f, 0.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
        if (ImGui::Button(visibilityButton.c_str(), visibilityButtonSize)) {
          setImageVisibleInAllViews(*image, !imageVisible);
          if (updateImageUniforms) {
            updateImageUniforms(*imageUid);
          }
        }
        ImGui::PopStyleColor(3); // ImGuiCol_Button, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", imageVisible ? "Hide image in all views" : "Show image in all views");
        }

        ImGui::SameLine();
        const float nameWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - suffixWidth);

        ImGui::PushStyleColor(ImGuiCol_FrameBg, inputTextBgColor);
        ImGui::PushStyleColor(ImGuiCol_Text, inputTextFgColor);
        ImGui::PushItemWidth(nameWidth);
        bool nameHovered = false;
        {
          std::string displayName = image->settings().displayName();
          if (ImGui::InputText("##displayName", &displayName)) {
            image->settings().setDisplayName(displayName);
          }
          nameHovered = ImGui::IsItemHovered();
        }
        ImGui::PopItemWidth();
        ImGui::PopStyleColor(2); // ImGuiCol_FrameBg, ImGuiCol_Text

        if (!roleSuffix.empty()) {
          ImGui::SameLine();
          ImGui::TextUnformatted(roleSuffix.c_str());
          nameHovered = nameHovered || ImGui::IsItemHovered();
        }

        if (nameHovered) {
          ImGui::SetTooltip("%s", image->header().fileName().c_str());
        }

        ImGui::TableNextColumn(); // "Value (NN)"
        markColumnAutosizeWidth(InspectorColumn::Value);

        if (!imageValuesNN.empty()) {
          if (isComponentFloatingPoint(image->header().memoryComponentType())) {
            if (image->header().numComponentsPerPixel() > 1) {
              ImGui::PushItemWidth(-1);
              ImGui::InputScalarN(
                "##imageValuesNN",
                ImGuiDataType_Double,
                imageValuesNN.data(),
                static_cast<int>(imageValuesNN.size()),
                nullptr,
                nullptr,
                appData.guiData().m_imageValuePrecisionFormat.c_str(),
                ImGuiInputTextFlags_ReadOnly);
              ImGui::PopItemWidth();

              if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Active component: %d", image->settings().activeComponent());
              }
            }
            else {
              double a = imageValuesNN[0];

              ImGui::PushItemWidth(-1);
              ImGui::InputScalar(
                "##imageValuesNN",
                ImGuiDataType_Double,
                &a,
                nullptr,
                nullptr,
                appData.guiData().m_imageValuePrecisionFormat.c_str(),
                ImGuiInputTextFlags_ReadOnly);
              ImGui::PopItemWidth();
            }
          }
          else {
            if (image->header().numComponentsPerPixel() > 1) {
              std::vector<int64_t> imageValuesNNInt;

              for (std::size_t i = 0; i < imageValuesNN.size(); ++i) {
                imageValuesNNInt.push_back(static_cast<int64_t>(imageValuesNN[i]));
              }

              ImGui::PushItemWidth(-1);
              ImGui::InputScalarN(
                "##imageValuesNN",
                ImGuiDataType_S64,
                imageValuesNNInt.data(),
                static_cast<int>(imageValuesNNInt.size()),
                nullptr,
                nullptr,
                "%ld",
                ImGuiInputTextFlags_ReadOnly);
              ImGui::PopItemWidth();

              if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Active component: %d", image->settings().activeComponent());
              }
            }
            else {
              int64_t a = static_cast<int64_t>(imageValuesNN[0]);

              ImGui::PushItemWidth(-1);
              ImGui::InputScalar(
                "##imageValuesNN",
                ImGuiDataType_S64,
                &a,
                nullptr,
                nullptr,
                "%ld",
                ImGuiInputTextFlags_ReadOnly);
              ImGui::PopItemWidth();
            }
          }
        }
        else {
          ImGui::Text("<N/A>");
        }

        ImGui::TableNextColumn(); // "Value (Linear)"
        markColumnAutosizeWidth(InspectorColumn::InterpolatedValue);

        if (!imageValuesLinear.empty()) {
          // Display linearly interpolated image values using doubles for both floating point and
          // integer images
          // if ( isComponentFloatingPoint( image->header().memoryComponentType() ) )
          {
            if (image->header().numComponentsPerPixel() > 1) {
              ImGui::PushItemWidth(-1);
              ImGui::InputScalarN(
                "##imageValuesLinear",
                ImGuiDataType_Double,
                imageValuesLinear.data(),
                static_cast<int>(imageValuesLinear.size()),
                nullptr,
                nullptr,
                appData.guiData().m_imageValuePrecisionFormat.c_str(),
                ImGuiInputTextFlags_ReadOnly);
              ImGui::PopItemWidth();

              if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Active component: %d", image->settings().activeComponent());
              }
            }
            else {
              double a = imageValuesLinear[0];

              ImGui::PushItemWidth(-1);
              ImGui::InputScalar(
                "##imageValuesLinear",
                ImGuiDataType_Double,
                &a,
                nullptr,
                nullptr,
                appData.guiData().m_imageValuePrecisionFormat.c_str(),
                ImGuiInputTextFlags_ReadOnly);
              ImGui::PopItemWidth();
            }
          }
          /*
                    else
                    {
                        if ( image->header().numComponentsPerPixel() > 1 )
                        {
                            std::vector< int64_t > imageValuesLinearInt;

                            for ( size_t i = 0; i < imageValuesLinear.size(); ++i )
                            {
                                imageValuesLinearInt.push_back( static_cast<int64_t>(
             imageValuesLinear[i] ) );
                            }

                            ImGui::PushItemWidth( -1 );
                            ImGui::InputScalarN( "##imageValuesLinear", ImGuiDataType_S64,
                                                imageValuesLinearInt.data(),
             imageValuesLinearInt.size(), nullptr, nullptr, "%ld", ImGuiInputTextFlags_ReadOnly );
                            ImGui::PopItemWidth();

                            if ( ImGui::IsItemHovered() )
                            {
                                ImGui::SetTooltip( "Active component: %d",
             image->settings().activeComponent() );
                            }
                        }
                        else
                        {
                            int64_t a = static_cast<int64_t>( imageValuesLinear[0] );

                            ImGui::PushItemWidth( -1 );
                            ImGui::InputScalar( "##imageValuesLinear", ImGuiDataType_S64, &a,
             nullptr, nullptr, "%ld", ImGuiInputTextFlags_ReadOnly ); ImGui::PopItemWidth();
                        }
                    }
                    */
        }
        else {
          ImGui::Text("<N/A>");
        }

        ImGui::TableNextColumn(); // "Percentile"
        markColumnAutosizeWidth(InspectorColumn::Percentile);

        if (const std::optional<double> percentile = activeComponentPercentile(*image, imageValuesNN)) {
          double a = *percentile;
          ImGui::PushItemWidth(-1);
          ImGui::InputScalar(
            "##imagePercentile",
            ImGuiDataType_Double,
            &a,
            nullptr,
            nullptr,
            appData.guiData().m_percentilePrecisionFormat.c_str(),
            ImGuiInputTextFlags_ReadOnly);
          ImGui::PopItemWidth();

          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Active component: %d", image->settings().activeComponent());
          }
        }
        else {
          ImGui::Text("<N/A>");
        }

        auto renderComplexColumn = [&](InspectorColumn column, const char* itemId) {
          ImGui::TableNextColumn();
          markColumnAutosizeWidth(column);
          if (const std::optional<double> value = complexColumnValue(column, *image, imageValuesNN)) {
            double displayValue = *value;
            ImGui::PushItemWidth(-1);
            ImGui::InputScalar(
              itemId,
              ImGuiDataType_Double,
              &displayValue,
              nullptr,
              nullptr,
              appData.guiData().m_imageValuePrecisionFormat.c_str(),
              ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();
          }
          else {
            ImGui::Text("<N/A>");
          }
        };

        auto renderComponentProjectionColumn = [&](InspectorColumn column, const char* itemId) {
          ImGui::TableNextColumn();
          markColumnAutosizeWidth(column);
          if (const std::optional<double> value = componentProjectionValue(column, imageValuesNN)) {
            double displayValue = *value;
            ImGui::PushItemWidth(-1);
            ImGui::InputScalar(
              itemId,
              ImGuiDataType_Double,
              &displayValue,
              nullptr,
              nullptr,
              appData.guiData().m_imageValuePrecisionFormat.c_str(),
              ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();
          }
          else {
            ImGui::Text("<N/A>");
          }
        };

        renderComplexColumn(InspectorColumn::Real, "##complexReal");
        renderComplexColumn(InspectorColumn::Imaginary, "##complexImaginary");
        renderComponentProjectionColumn(InspectorColumn::Magnitude, "##componentMagnitude");
        renderComplexColumn(InspectorColumn::Phase, "##complexPhase");
        renderComponentProjectionColumn(InspectorColumn::Minimum, "##componentMinimum");
        renderComponentProjectionColumn(InspectorColumn::Mean, "##componentMean");
        renderComponentProjectionColumn(InspectorColumn::Maximum, "##componentMaximum");

        auto renderVectorDerivativeColumn = [&](InspectorColumn column, const char* itemId) {
          ImGui::TableNextColumn();
          markColumnAutosizeWidth(column);
          if (const std::optional<double> value = vectorDerivativeColumnValue(column, *image, voxelPos)) {
            double displayValue = *value;
            ImGui::PushItemWidth(-1);
            ImGui::InputScalar(
              itemId,
              ImGuiDataType_Double,
              &displayValue,
              nullptr,
              nullptr,
              appData.guiData().m_imageValuePrecisionFormat.c_str(),
              ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();
          }
          else {
            ImGui::Text("<N/A>");
          }
        };

        renderVectorDerivativeColumn(InspectorColumn::JacobianDeterminant, "##vectorJacobianDeterminant");
        renderVectorDerivativeColumn(InspectorColumn::LogJacobianDeterminant, "##vectorLogJacobianDeterminant");
        renderVectorDerivativeColumn(InspectorColumn::CurlMagnitude, "##vectorCurlMagnitude");
        renderVectorDerivativeColumn(InspectorColumn::Divergence, "##vectorDivergence");

        if (segLabel) {
          ImGui::TableNextColumn(); // "Label"
          markColumnAutosizeWidth(InspectorColumn::Label);

          // Segmentation labels are unsigned, so we can cast:
          uint64_t l = static_cast<uint64_t>(*segLabel);
          ImGui::PushItemWidth(-1);
          ImGui::InputScalar("##segLabel", ImGuiDataType_U64, &l, nullptr, nullptr, "%ld");
          ImGui::PopItemWidth();

          if (table) {
            std::string labelName = table->getName(l);

            if (ImGui::IsItemHovered()) {
              // Tooltip for the segmentation label
              ImGui::SetTooltip("%s", labelName.c_str());
            }

            ImGui::TableNextColumn(); // "Region"
            markColumnAutosizeWidth(InspectorColumn::Region);

            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##labelName", &labelName)) {
              table->setName(l, labelName);
            }
            ImGui::PopItemWidth();
          }
          else {
            ImGui::TableNextColumn(); // "Region"
            markColumnAutosizeWidth(InspectorColumn::Region);
            ImGui::Text("<N/A>");
          }
        }
        else {
          ImGui::TableNextColumn(); // "Label"
          markColumnAutosizeWidth(InspectorColumn::Label);
          ImGui::Text("<N/A>");

          ImGui::TableNextColumn(); // "Region"
          markColumnAutosizeWidth(InspectorColumn::Region);
          ImGui::Text("<N/A>");
        }

        if (voxelPos) {
          static const glm::ivec3 k_zero{0};
          static const glm::ivec3 k_minDim{0};

          ImGui::TableNextColumn(); // "Voxel"
          markColumnAutosizeWidth(InspectorColumn::Voxel);

          const glm::ivec3 k_maxDim = static_cast<glm::ivec3>(image->header().pixelDimensions()) - glm::ivec3{1, 1, 1};

          glm::ivec3 a = *voxelPos;
          ImGui::PushItemWidth(-1);
          if (ImGui::DragScalarN(
                "##voxelPos",
                ImGuiDataType_S32,
                glm::value_ptr(a),
                3,
                1.0f,
                glm::value_ptr(k_minDim),
                glm::value_ptr(k_maxDim),
                "%d"))
          {
            if (
              glm::all(glm::greaterThanEqual(a, k_zero)) &&
              glm::all(glm::lessThan(a, glm::ivec3{image->header().pixelDimensions()})))
            {
              setVoxelPos(imageIndex, a);
            }
          }
          ImGui::PopItemWidth();
        }
        else {
          ImGui::TableNextColumn(); // "Voxel"
          markColumnAutosizeWidth(InspectorColumn::Voxel);
          ImGui::Text("<N/A>");
        }

        if (subjectPos) {
          ImGui::TableNextColumn(); // "Subject LPS"
          markColumnAutosizeWidth(InspectorColumn::Subject);

          // Step size is the  minimum voxel spacing
          const float stepSize = glm::compMin(image->header().spacing());
          glm::vec3 a = *subjectPos;

          ImGui::PushItemWidth(-1);
          if (ImGui::DragScalarN(
                "##physicalPos",
                ImGuiDataType_Float,
                glm::value_ptr(a),
                3,
                stepSize,
                nullptr,
                nullptr,
                appData.guiData().m_coordsPrecisionFormat.c_str()))
          {
            setSubjectPos(imageIndex, a);
          }
          ImGui::PopItemWidth();
        }
        else {
          ImGui::TableNextColumn(); // "Subject LPS"
          markColumnAutosizeWidth(InspectorColumn::Subject);
          ImGui::Text("<N/A>");
        }

        const std::optional<WarpedSamplePosition> warpedSample =
          subjectPos ? warpedSamplePosition(appData, *imageUid, *image, *subjectPos) : std::nullopt;

        auto renderWarpedCoordinateColumn = [&](InspectorColumn column, const char* itemId) {
          ImGui::TableNextColumn();
          markColumnAutosizeWidth(column);
          if (warpedSample) {
            glm::dvec3 value = (InspectorColumn::SampleVoxel == column) ? warpedSample->voxel : warpedSample->subject;
            ImGui::PushItemWidth(-1);
            ImGui::InputScalarN(
              itemId,
              ImGuiDataType_Double,
              glm::value_ptr(value),
              3,
              nullptr,
              nullptr,
              appData.guiData().m_coordsPrecisionFormat.c_str(),
              ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();
          }
          else {
            ImGui::Text("<N/A>");
          }
        };

        renderWarpedCoordinateColumn(InspectorColumn::SampleVoxel, "##sampleVoxel");
        renderWarpedCoordinateColumn(InspectorColumn::SampleSubject, "##sampleSubject");

        ImGui::TableNextColumn(); // "Time frame"
        markColumnAutosizeWidth(InspectorColumn::TimeFrame);
        if (const std::optional<uint32_t> timeFrame = timeFrameValue(*image)) {
          static constexpr uint32_t k_minTimeFrame = 0;
          const uint32_t maxTimeFrame = image->timeAxis().numTimePoints() - 1u;
          uint32_t value = *timeFrame;

          ImGui::PushItemWidth(-1);
          if (ImGui::DragScalar("##timeFrame", ImGuiDataType_U32, &value, 1.0f, &k_minTimeFrame, &maxTimeFrame, "%u")) {
            const uint32_t requestedTimeFrame = image->timeAxis().clamp(value);
            if (requestedTimeFrame != image->settings().activeTimePoint()) {
              image->settings().setActiveTimePoint(requestedTimeFrame);
              if (updateImageUniforms) {
                updateImageUniforms(*imageUid);
              }
            }
          }
          ImGui::PopItemWidth();
        }
        else {
          ImGui::TextUnformatted("N/A");
        }

        ImGui::TableNextColumn(); // "Time"
        markColumnAutosizeWidth(InspectorColumn::TimeValue);
        if (const std::optional<double> time = timeValue(*image)) {
          double value = *time;
          ImGui::PushItemWidth(-1);
          ImGui::InputScalar(
            "##timeValue",
            ImGuiDataType_Double,
            &value,
            nullptr,
            nullptr,
            appData.guiData().m_timeValuePrecisionFormat.c_str(),
            ImGuiInputTextFlags_ReadOnly);
          ImGui::PopItemWidth();
        }
        else {
          ImGui::TextUnformatted("N/A");
        }

        ImGui::PopID(); /** PopID: imageIndex **/
      }

      for (std::size_t column = 0; column < k_inspectorColumnNames.size(); ++column) {
        if (!k_inspectorColumnCanHide.at(column)) {
          continue;
        }

        const bool columnEnabled =
          0 != (ImGui::TableGetColumnFlags(static_cast<int>(column)) & ImGuiTableColumnFlags_IsEnabled);
        bool& storedColumnVisible = appData.guiData().m_inspectionColumnVisible.at(column);
        if (storedColumnVisible != columnEnabled) {
          storedColumnVisible = columnEnabled;
          if (columnEnabled) {
            s_autoSizeColumnRequested.at(column) = true;
          }
        }
      }

      ImGui::EndTable();
    }

    if (ImGui::BeginPopupContextWindow()) {
      // Show context menu on right-button click:
      contextMenu();
      ImGui::EndPopup();
    }
    else if (ImGui::BeginPopup("selectionPopup")) {
      // Show context menu if the user has clicked the popup button:
      contextMenu();
      ImGui::EndPopup();
    }
  }

  ImGui::End();

  // ImGuiStyleVar_CellPadding
  // ImGuiStyleVar_FramePadding
  // ImGuiStyleVar_ItemInnerSpacing
  // ImGuiStyleVar_ScrollbarSize
  // ImGuiStyleVar_WindowBorderSize
  // ImGuiStyleVar_WindowPadding
  // ImGuiStyleVar_WindowRounding
  ImGui::PopStyleVar(6);
}
