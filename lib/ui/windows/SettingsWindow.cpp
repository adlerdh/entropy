#include "ui/windows/SettingsWindow.h"

#include "ui/Helpers.h"
#include "ui/ImGuiCustomControls.h"
#include "ui/NativeFileDialogs.h"
#include "ui/Style.h"
#include "ui/dialogs/NativeMessageDialogs.h"
#include "ui/widgets/Widgets.h"
#include "ui/settings/SettingsModel.h"

#include "common/LoggingSettings.h"
#include "logic/app/AppPaths.h"
#include "logic/app/Data.h"
#include "registration/Config.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ui_settings = entropy::ui::settings;

namespace
{

static constexpr bool k_recenterCrosshairs = true;
static constexpr bool k_realignCrosshairs = true;
static constexpr bool k_doNotRecenterOnCurrentCrosshairsPosition = false;
static constexpr float k_viewOptionControlWidth = 180.0f;

void requestResetInterfaceSettings(const SettingsPersistenceCallbacks& persistenceCallbacks);
void renderResetInterfaceSettingsPopup(const SettingsPersistenceCallbacks& persistenceCallbacks);
static constexpr bool k_doNotResetObliqueOrientation = false;
static constexpr bool k_resetZoom = true;

static constexpr float k_windowMin = 0.0f;
static constexpr float k_windowMax = 1.0f;

static constexpr ImGuiColorEditFlags k_colorEditFlags =
  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_DisplayRGB |
  ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_InputRGB;

static constexpr ImGuiColorEditFlags k_colorAlphaEditFlags =
  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_DisplayRGB |
  ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf |
  ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_InputRGB;

constexpr std::array k_registrationBackends{
  registration::Backend::ANTs,
  registration::Backend::FireANTs,
  registration::Backend::Greedy};

void disabledTextWrapped(const char* text)
{
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextWrapped("%s", text);
  ImGui::PopStyleColor();
}

/**
 * @brief Return an absolute, canonical display path when possible.
 * @param[in] path Path to normalize for display.
 * @return Weakly canonical absolute path, or an absolute fallback if canonicalization fails.
 */
std::filesystem::path canonicalDisplayPath(const std::filesystem::path& path)
{
  std::error_code ec;
  std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(path, ec);
  if (!ec) {
    return canonicalPath;
  }

  std::filesystem::path absolutePath = std::filesystem::absolute(path, ec);
  return ec ? path : absolutePath;
}

float visibleLabelWidth(const char* label)
{
  const std::string_view text{label};
  const std::size_t idSeparator = text.find("##");
  const std::string_view visibleText = std::string_view::npos == idSeparator ? text : text.substr(0, idSeparator);
  return ImGui::CalcTextSize(visibleText.data(), visibleText.data() + visibleText.size()).x;
}

float fillWidthForLabeledControl(const char* label, float extraTrailingWidth = 0.0f)
{
  const float labelWidth = visibleLabelWidth(label);
  const float spacing = labelWidth > 0.0f ? ImGui::GetStyle().ItemSpacing.x : 0.0f;
  return std::max(1.0f, ImGui::GetContentRegionAvail().x - labelWidth - spacing - extraTrailingWidth);
}

float settingsControlWidth()
{
  return ImGui::CalcItemWidth();
}

float fillWidthForLabelColumn(float labelWidth)
{
  const float spacing = labelWidth > 0.0f ? ImGui::GetStyle().ItemSpacing.x : 0.0f;
  return std::max(1.0f, ImGui::GetContentRegionAvail().x - labelWidth - spacing);
}

void finishSettingsSection(bool sectionOpen)
{
  if (!sectionOpen) {
    return;
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
}

/**
 * @brief Render a copyable, read-only path field.
 * @param[in] label Field label.
 * @param[in] path Path text shown in the field.
 * @param[in] itemWidth Width of the text field, or a negative value to fill the row after its own label.
 */
void renderReadOnlyPathField(const char* label, const std::filesystem::path& path, float itemWidth = -1.0f)
{
  std::string text = canonicalDisplayPath(path).string();
  std::vector<char> buffer(text.begin(), text.end());
  buffer.push_back('\0');

  ImGui::PushItemWidth(itemWidth >= 0.0f ? itemWidth : fillWidthForLabeledControl(label));
  ImGui::InputText(label, buffer.data(), buffer.size(), ImGuiInputTextFlags_ReadOnly);
  ImGui::PopItemWidth();
}

void renderPathSetting(const char* label, std::filesystem::path& path, const char* tooltip)
{
  std::string value = path.string();
  ImGui::PushItemWidth(fillWidthForLabeledControl(label));
  if (ImGui::InputText(label, &value)) {
    path = value;
  }
  ImGui::PopItemWidth();
  ImGui::SameLine();
  helpMarker(tooltip);
}

void renderTextSetting(const char* label, std::string& value, const char* tooltip)
{
  ImGui::PushItemWidth(fillWidthForLabeledControl(label));
  ImGui::InputText(label, &value);
  ImGui::PopItemWidth();
  ImGui::SameLine();
  helpMarker(tooltip);
}

void renderPathSettingFixedWidth(
  const char* label,
  std::filesystem::path& path,
  const char* tooltip,
  bool browseForFile = false)
{
  std::string value = path.string();
  ImGui::PushItemWidth(settingsControlWidth());
  if (ImGui::InputText(label, &value)) {
    path = value;
  }
  ImGui::PopItemWidth();
  ImGui::SameLine();
  ImGui::PushID(label);
  if (browseForFile && ImGui::Button("...")) {
    if (const std::optional<std::filesystem::path> selected = native_dialog::openFile({}, path)) {
      path = *selected;
    }
  }
  if (browseForFile) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
      ImGui::SetTooltip("%s", "Select executable");
    }
    ImGui::SameLine();
  }
  ImGui::PopID();
  helpMarker(tooltip);
}

void renderTextSettingFixedWidth(const char* label, std::string& value, const char* tooltip)
{
  ImGui::PushItemWidth(settingsControlWidth());
  ImGui::InputText(label, &value);
  ImGui::PopItemWidth();
  ImGui::SameLine();
  helpMarker(tooltip);
}

/**
 * @brief Render the ImGui settings file controls.
 * @param persistenceCallbacks File-backed user settings callbacks.
 */
void renderUiSettingsFileSection(const SettingsPersistenceCallbacks& persistenceCallbacks)
{
  disabledTextWrapped(
    "This file stores UI state such as window positions, docking layout, panel sizes, table columns, and similar "
    "interface settings.");
  renderReadOnlyPathField(
    "UI settings file",
    app_paths::userDataDirectory() / "entropy_ui.ini",
    ImGui::CalcItemWidth());

  if (ImGui::Button("Reset UI Settings")) {
    requestResetInterfaceSettings(persistenceCallbacks);
  }
  ImGui::SameLine();
  helpMarker("Clear saved ImGui interface state and regenerate the default layout.");
}

/**
 * @brief Render diagnostic settings that affect runtime logging.
 */
void renderDiagnosticsSettings()
{
  const auto currentLogLevel = entropy::logging::defaultLoggerSinkLevel();
  const auto currentLogLevelLabel = entropy::logging::logLevelLabel(currentLogLevel);

  if (ImGui::BeginCombo("Log verbosity", currentLogLevelLabel.data())) {
    for (const entropy::logging::LogLevelChoice& choice : entropy::logging::allLogLevelChoices()) {
      if (!entropy::logging::isLogLevelChoiceAvailable(choice)) {
        continue;
      }

      const bool selected = choice.level == currentLogLevel;
      if (ImGui::Selectable(choice.label.data(), selected)) {
        entropy::logging::setDefaultLoggerSinkLevel(choice.level);
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  helpMarker("Set console and application log file verbosity immediately");

  const float logFieldWidth = ImGui::CalcItemWidth();
  renderReadOnlyPathField("Application log", app_paths::logDirectory() / "entropy.txt", logFieldWidth);
  renderReadOnlyPathField("ImGui log", app_paths::logDirectory() / "entropy_ui.log", logFieldWidth);
}

GuiData::LayoutTabPlacement guiLayoutTabPlacement(UiLayoutTabPlacement placement)
{
  return UiLayoutTabPlacement::Bottom == placement ? GuiData::LayoutTabPlacement::Bottom
                                                   : GuiData::LayoutTabPlacement::Top;
}

void renderMetricSettingsPanel(
  RenderData::MetricParams& metricParams,
  bool& showColormapWindow,
  const char* name,
  const std::function<void(void)>& updateMetricUniforms,
  const std::function<std::size_t(void)>& getNumImageColorMaps,
  const std::function<const ImageColorMap*(std::size_t cmapIndex)>& getImageColorMap,
  bool showWindowAsSignedCorrelation = false)
{
  // Metric windowing range slider:
  const float slope = metricParams.m_slopeIntercept[0];
  const float xcept = metricParams.m_slopeIntercept[1];

  const float windowWidth = glm::clamp(1.0f / slope, 0.0f, 1.0f);
  const float windowCenter = glm::clamp((0.5f - xcept) / slope, 0.0f, 1.0f);

  float windowLow = std::max(windowCenter - 0.5f * windowWidth, 0.0f);
  float windowHigh = std::min(windowCenter + 0.5f * windowWidth, 1.0f);

  float displayWindowLow = showWindowAsSignedCorrelation ? 2.0f * windowLow - 1.0f : windowLow;
  float displayWindowHigh = showWindowAsSignedCorrelation ? 2.0f * windowHigh - 1.0f : windowHigh;

  if (ImGui::DragFloatRange2(
        "Window",
        &displayWindowLow,
        &displayWindowHigh,
        showWindowAsSignedCorrelation ? 0.02f : 0.01f,
        showWindowAsSignedCorrelation ? -1.0f : k_windowMin,
        showWindowAsSignedCorrelation ? 1.0f : k_windowMax,
        showWindowAsSignedCorrelation ? "Min NCC: %.2f" : "Min: %.2f",
        showWindowAsSignedCorrelation ? "Max NCC: %.2f" : "Max: %.2f",
        ImGuiSliderFlags_AlwaysClamp))
  {
    windowLow = showWindowAsSignedCorrelation ? 0.5f * (displayWindowLow + 1.0f) : displayWindowLow;
    windowHigh = showWindowAsSignedCorrelation ? 0.5f * (displayWindowHigh + 1.0f) : displayWindowHigh;

    if ((windowHigh - windowLow) < 0.01f) {
      if (windowLow >= 0.99f) {
        windowLow = windowHigh - 0.01f;
      }
      else {
        windowHigh = windowLow + 0.01f;
      }
    }

    const float newWidth = windowHigh - windowLow;
    const float newCenter = 0.5f * (windowLow + windowHigh);

    const float newSlope = 1.0f / newWidth;
    const float newXcept = 0.5f - newCenter * newSlope;

    metricParams.m_slopeIntercept = glm::vec2{newSlope, newXcept};
    updateMetricUniforms();
  }
  ImGui::SameLine();
  helpMarker(
    showWindowAsSignedCorrelation ? "Minimum and maximum NCC values. Internally these are mapped to the colormap range."
                                  : "Minimum and maximum of the metric window range");

  /*
  // Metric masking:
  bool doMasking = metricParams.m_doMasking;
  if (ImGui::Checkbox("Masking", &doMasking))
  {
    metricParams.m_doMasking = doMasking;
    updateMetricUniforms();
  }
  ImGui::SameLine();
  helpMarker("Only compute the metric within masked regions");
  */

  // Metric colormap dialog:
  bool* showImageColormapWindow = &showColormapWindow;
  *showImageColormapWindow |= ImGui::Button("Colormap");

  bool invertedCmap = metricParams.m_invertCmap;

  ImGui::SameLine();
  if (ImGui::Checkbox("Inverted", &invertedCmap)) {
    metricParams.m_invertCmap = invertedCmap;
    updateMetricUniforms();
  }
  ImGui::SameLine();
  helpMarker("Select/invert the metric colormap");

  auto getColormapIndex = [&metricParams]() {
    return metricParams.m_colorMapIndex;
  };
  auto setColormapIndex = [&metricParams](std::size_t cmapIndex) {
    metricParams.m_colorMapIndex = cmapIndex;
  };

  auto getImageColorMapInverted = [&metricParams]() {
    return metricParams.m_invertCmap;
  };

  auto getImageColorMapContinuous = [&metricParams]() {
    return metricParams.m_cmapContinuous;
  };

  auto getImageColorMapLevels = [&metricParams]() {
    return metricParams.m_cmapQuantizationLevels;
  };

  renderPaletteWindow(
    std::string("Select colormap for metric image").c_str(),
    showImageColormapWindow,
    getNumImageColorMaps,
    getImageColorMap,
    getColormapIndex,
    setColormapIndex,
    getImageColorMapInverted,
    getImageColorMapContinuous,
    getImageColorMapLevels,
    glm::vec3{0.0f, 1.0f, 1.0f},
    updateMetricUniforms);

  // Colormap preview:
  const float contentWidth = ImGui::GetContentRegionAvail().x;
  const float height = (ImGui::GetFontSize());

  char label[128];
  const ImageColorMap* cmap = getImageColorMap(getColormapIndex());
  snprintf(label, 128, "%s##cmap_%s", cmap->name().c_str(), name);

  const bool doQuantize =
    (!metricParams.m_cmapContinuous && (ImageColorMap::InterpolationMode::Linear == cmap->interpolationMode()));

  ImGui::paletteButton(
    label,
    cmap->data_RGBA_asVector(),
    metricParams.m_invertCmap,
    doQuantize,
    metricParams.m_cmapQuantizationLevels,
    glm::vec3{0.0f, 1.0f, 1.0f},
    ImVec2(contentWidth, height));

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", cmap->description().c_str());
  }
}

bool renderLocalNccSettings(
  AppData& appData,
  RenderData& renderData,
  const std::function<void(void)>& updateMetricUniforms,
  const std::function<std::size_t(void)>& getNumImageColorMaps,
  const std::function<const ImageColorMap*(std::size_t cmapIndex)>& getImageColorMap)
{
  if (!ImGui::CollapsingHeader("Local Normalized Cross-Correlation Metric", ImGuiTreeNodeFlags_DefaultOpen)) {
    return false;
  }

  disabledTextWrapped(
    "Local NCC compares the pattern of intensities inside a small patch, so it can show agreement even when image "
    "intensities differ by scale or offset. Higher correlation means better local agreement; dissimilarity converts "
    "poor agreement into brighter mismatch values.");
  ImGui::Spacing();

  const RenderData::LocalNccPresentation presentation = renderData.m_localNccPresentation;
  if (ImGui::RadioButton("Dissimilarity", RenderData::LocalNccPresentation::Dissimilarity == presentation)) {
    renderData.m_localNccPresentation = RenderData::LocalNccPresentation::Dissimilarity;
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("Correlation", RenderData::LocalNccPresentation::Correlation == presentation)) {
    renderData.m_localNccPresentation = RenderData::LocalNccPresentation::Correlation;
  }
  ImGui::SameLine();
  helpMarker("Dissimilarity highlights disagreement; correlation shows local NCC directly.");

  const bool showWindowAsSignedCorrelation =
    RenderData::LocalNccPresentation::Correlation == renderData.m_localNccPresentation;
  renderMetricSettingsPanel(
    renderData.m_localNccParams,
    appData.guiData().m_showLocalNccColormapWindow,
    "localncc",
    updateMetricUniforms,
    getNumImageColorMaps,
    getImageColorMap,
    showWindowAsSignedCorrelation);

  ImGui::Spacing();

  if (RenderData::LocalNccPresentation::Dissimilarity == renderData.m_localNccPresentation) {
    bool ignoreNegativeCorrelation = renderData.m_localNccIgnoreNegativeCorrelation;
    if (ImGui::Checkbox("Treat negative correlation as mismatch", &ignoreNegativeCorrelation)) {
      renderData.m_localNccIgnoreNegativeCorrelation = ignoreNegativeCorrelation;
    }
    ImGui::SameLine();
    helpMarker("Negative NCC values are treated as complete mismatch.");
  }

  constexpr std::array<std::pair<int, const char*>, 5> k_patchSizes{
    {{1, "3 x 3"}, {2, "5 x 5"}, {3, "7 x 7"}, {4, "9 x 9"}, {5, "11 x 11"}}};
  int patchIndex = 2;
  for (std::size_t i = 0; i < k_patchSizes.size(); ++i) {
    if (renderData.m_localNccPatchRadius == k_patchSizes[i].first) {
      patchIndex = static_cast<int>(i);
      break;
    }
  }
  if (ImGui::BeginCombo("Patch size", k_patchSizes[static_cast<std::size_t>(patchIndex)].second)) {
    for (std::size_t i = 0; i < k_patchSizes.size(); ++i) {
      const bool selected = static_cast<int>(i) == patchIndex;
      if (ImGui::Selectable(k_patchSizes[i].second, selected)) {
        renderData.m_localNccPatchRadius = k_patchSizes[i].first;
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  helpMarker("Number of view-plane samples used in each local NCC patch.");

  float sampleSpacing = renderData.m_localNccSampleSpacing;
  if (mySliderF32("Sample spacing", &sampleSpacing, 0.5f, 4.0f, "%.2f x")) {
    renderData.m_localNccSampleSpacing = sampleSpacing;
  }
  ImGui::SameLine();
  helpMarker("Patch sample spacing in reference-image voxel steps along the view-plane axes.");

  float minValidFraction = renderData.m_localNccMinValidFraction;
  if (mySliderF32("Minimum valid fraction", &minValidFraction, 0.1f, 1.0f, "%.2f")) {
    renderData.m_localNccMinValidFraction = minValidFraction;
  }
  ImGui::SameLine();
  helpMarker("Minimum fraction of patch samples that must lie inside both images.");

  float varianceEpsilon = renderData.m_localNccVarianceEpsilon;
  if (ImGui::InputFloat("Variance epsilon", &varianceEpsilon, 0.0f, 0.0f, "%.1e")) {
    renderData.m_localNccVarianceEpsilon = std::max(varianceEpsilon, 0.0f);
  }
  ImGui::SameLine();
  helpMarker("Patches with lower local variance in either image are treated as invalid.");

  const RenderData::LocalNccInvalidStyle invalidStyle = renderData.m_localNccInvalidStyle;
  if (ImGui::RadioButton("Invalid transparent", RenderData::LocalNccInvalidStyle::Transparent == invalidStyle)) {
    renderData.m_localNccInvalidStyle = RenderData::LocalNccInvalidStyle::Transparent;
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("Invalid gray", RenderData::LocalNccInvalidStyle::Gray == invalidStyle)) {
    renderData.m_localNccInvalidStyle = RenderData::LocalNccInvalidStyle::Gray;
  }
  ImGui::SameLine();
  helpMarker("How low-variance or insufficient-overlap patches are drawn.");

  return true;
}

bool renderLocalLinearResidualSettings(
  AppData& appData,
  RenderData& renderData,
  const std::function<void(void)>& updateMetricUniforms,
  const std::function<std::size_t(void)>& getNumImageColorMaps,
  const std::function<const ImageColorMap*(std::size_t cmapIndex)>& getImageColorMap)
{
  if (!ImGui::CollapsingHeader("Local Linear Residual Metric", ImGuiTreeNodeFlags_DefaultOpen)) {
    return false;
  }

  disabledTextWrapped(
    "Local linear residual fits 'moving = a * fixed +b' inside each local patch and displays the remaining residual "
    "error. Low values mean the images match after local gain and bias correction; the fitted a and b coefficients "
    "are used internally and are not displayed by this residual map.");
  ImGui::Spacing();

  renderMetricSettingsPanel(
    renderData.m_localLinearResidualParams,
    appData.guiData().m_showLocalLinearResidualColormapWindow,
    "localLinearResidual",
    updateMetricUniforms,
    getNumImageColorMaps,
    getImageColorMap);

  ImGui::Spacing();

  constexpr std::array<std::pair<int, const char*>, 5> k_patchSizes{
    {{1, "3 x 3"}, {2, "5 x 5"}, {3, "7 x 7"}, {4, "9 x 9"}, {5, "11 x 11"}}};
  int patchIndex = 2;
  for (std::size_t i = 0; i < k_patchSizes.size(); ++i) {
    if (renderData.m_localLinearResidualPatchRadius == k_patchSizes[i].first) {
      patchIndex = static_cast<int>(i);
      break;
    }
  }
  if (ImGui::BeginCombo("Patch size", k_patchSizes[static_cast<std::size_t>(patchIndex)].second)) {
    for (std::size_t i = 0; i < k_patchSizes.size(); ++i) {
      const bool selected = static_cast<int>(i) == patchIndex;
      if (ImGui::Selectable(k_patchSizes[i].second, selected)) {
        renderData.m_localLinearResidualPatchRadius = k_patchSizes[i].first;
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  helpMarker("Number of view-plane samples used to fit the local linear intensity model.");

  float sampleSpacing = renderData.m_localLinearResidualSampleSpacing;
  if (mySliderF32("Sample spacing", &sampleSpacing, 0.5f, 4.0f, "%.2f x")) {
    renderData.m_localLinearResidualSampleSpacing = sampleSpacing;
  }
  ImGui::SameLine();
  helpMarker("Patch sample spacing in reference-image voxel steps along the view-plane axes.");

  float minValidFraction = renderData.m_localLinearResidualMinValidFraction;
  if (mySliderF32("Minimum valid fraction", &minValidFraction, 0.1f, 1.0f, "%.2f")) {
    renderData.m_localLinearResidualMinValidFraction = minValidFraction;
  }
  ImGui::SameLine();
  helpMarker("Minimum fraction of patch samples that must lie inside both images.");

  float varianceEpsilon = renderData.m_localLinearResidualVarianceEpsilon;
  if (ImGui::InputFloat("Variance epsilon", &varianceEpsilon, 0.0f, 0.0f, "%.1e")) {
    renderData.m_localLinearResidualVarianceEpsilon = std::max(varianceEpsilon, 0.0f);
  }
  ImGui::SameLine();
  helpMarker("Patches with lower reference-image variance cannot produce a stable local gain.");

  const RenderData::LocalNccInvalidStyle invalidStyle = renderData.m_localLinearResidualInvalidStyle;
  if (ImGui::RadioButton("Invalid transparent", RenderData::LocalNccInvalidStyle::Transparent == invalidStyle)) {
    renderData.m_localLinearResidualInvalidStyle = RenderData::LocalNccInvalidStyle::Transparent;
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("Invalid gray", RenderData::LocalNccInvalidStyle::Gray == invalidStyle)) {
    renderData.m_localLinearResidualInvalidStyle = RenderData::LocalNccInvalidStyle::Gray;
  }
  ImGui::SameLine();
  helpMarker("How low-variance or insufficient-overlap patches are drawn.");

  return true;
}

/**
 * @brief Render annotation and landmark display settings inside the Views page.
 */
void renderAnnotationViewSettings(RenderData& renderData)
{
  ImGui::PushID("annotations"); /*** PushID annotations ***/

  bool annotOnTop = renderData.m_globalAnnotationParams.renderOnTopOfAllImagePlanes;
  if (ImGui::Checkbox("Annotations on top", &annotOnTop)) {
    renderData.m_globalAnnotationParams.renderOnTopOfAllImagePlanes = annotOnTop;
  }
  ImGui::SameLine();
  helpMarker("Render annotations on top of all image layers");

  bool lmOnTop = renderData.m_globalLandmarkParams.renderOnTopOfAllImagePlanes;
  if (ImGui::Checkbox("Landmarks on top", &lmOnTop)) {
    renderData.m_globalLandmarkParams.renderOnTopOfAllImagePlanes = lmOnTop;
  }
  ImGui::SameLine();
  helpMarker("Render landmarks on top of all image layers");

  bool hideVertices = renderData.m_globalAnnotationParams.hidePolygonVertices;
  if (ImGui::Checkbox("Hide all annotation vertices", &hideVertices)) {
    renderData.m_globalAnnotationParams.hidePolygonVertices = hideVertices;
  }
  ImGui::SameLine();
  helpMarker("Hide all annotation vertices");

  ImGui::PopID(); /*** PopID annotations ***/
}

/**
 * @brief Render the Views settings page contents.
 */
void renderViewsTab(AppData& appData, RenderData& renderData, const AllViewsRecenterType& recenterAllViews)
{
  ImGui::ColorEdit3("Background color", glm::value_ptr(renderData.m_2dBackgroundColor), k_colorEditFlags);

  // Show image-view intersection border
  bool showImageBorders = renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections;
  if (ImGui::Checkbox("Show image borders", &showImageBorders)) {
    renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections = showImageBorders;
  }
  ImGui::SameLine();
  helpMarker("Show borders of image intersections with views");
  if (!renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections) {
    renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersectionsInLightboxViews = false;
  }

  /// @note strokeWidth seems to not work with NanoVG across all platforms
  /*
          if ( renderData.m_globalSliceIntersectionParams.renderImageViewIntersections )
          {
              constexpr float k_minWidth = 1.0f;
              constexpr float k_maxWidth = 5.0f;

              ImGui::SliderScalar( "Border width", ImGuiDataType_Float,
                                   &renderData.m_globalSliceIntersectionParams.strokeWidth,
                                   &k_minWidth, &k_maxWidth, "%.1f" );

              ImGui::SameLine(); helpMarker( "Border width" );
          }
          */

  // Anatomical coordinate directions (including crosshairs) rotation locking
  bool lockDirectionsToReference = appData.settings().lockAnatomicalCoordinateAxesWithReferenceImage();
  if (ImGui::Checkbox("Lock anatomical directions to reference image", &(lockDirectionsToReference))) {
    appData.settings().setLockAnatomicalCoordinateAxesWithReferenceImage(lockDirectionsToReference);
  }
  ImGui::SameLine();
  helpMarker("Lock anatomical directions and crosshairs to reference image orientation");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Crosshairs
  const bool crosshairsOpen = ImGui::CollapsingHeader("Crosshairs", ImGuiTreeNodeFlags_DefaultOpen);
  if (crosshairsOpen) {
    if (ImGui::Checkbox("Show crosshairs in 2D", &renderData.m_showCrosshairs)) {
      if (!renderData.m_showCrosshairs) {
        renderData.m_showCrosshairsInLightboxViews = false;
      }
    }
    ImGui::SameLine();
    helpMarker("Show crosshairs in anatomical views");

    ImGui::ColorEdit4("Crosshairs line color", glm::value_ptr(renderData.m_crosshairsColor), k_colorAlphaEditFlags);

    ImGui::Dummy(ImVec2(0.0f, 1.0f));

    ImGui::Text("Snap crosshairs:");
    if (ImGui::RadioButton(
          "To reference image voxels",
          CrosshairsSnapping::ReferenceImage == renderData.m_snapCrosshairs))
    {
      renderData.m_snapCrosshairs = CrosshairsSnapping::ReferenceImage;
    }
    ImGui::SameLine();
    helpMarker("Snap crosshairs to reference image voxel centers");

    if (ImGui::RadioButton("To active image voxels", CrosshairsSnapping::ActiveImage == renderData.m_snapCrosshairs)) {
      renderData.m_snapCrosshairs = CrosshairsSnapping::ActiveImage;
    }
    ImGui::SameLine();
    helpMarker("Snap crosshairs to active image voxel centers");

    if (ImGui::RadioButton("Disable (no snapping)", CrosshairsSnapping::Disabled == renderData.m_snapCrosshairs)) {
      renderData.m_snapCrosshairs = CrosshairsSnapping::Disabled;
    }
    ImGui::SameLine();
    helpMarker("Do not snap crosshairs to image voxels");
  }
  finishSettingsSection(crosshairsOpen);

  // View centering:
  const bool viewRecenteringOpen = ImGui::CollapsingHeader("Recentering", ImGuiTreeNodeFlags_DefaultOpen);
  if (viewRecenteringOpen) {
    ImGui::Text("Center views and crosshairs on:");

    ImGui::SameLine();
    helpMarker("Default view and crosshairs centering behavior");

    if (ImGui::RadioButton("Reference image", ImageSelection::ReferenceImage == appData.state().recenteringMode())) {
      appData.state().setRecenteringMode(ImageSelection::ReferenceImage);

      recenterAllViews(
        k_recenterCrosshairs,
        k_realignCrosshairs,
        k_doNotRecenterOnCurrentCrosshairsPosition,
        k_doNotResetObliqueOrientation,
        k_resetZoom);
    }
    ImGui::SameLine();
    helpMarker("Recenter views and crosshairs on the reference image");

    if (ImGui::RadioButton("Active image", ImageSelection::ActiveImage == appData.state().recenteringMode())) {
      appData.state().setRecenteringMode(ImageSelection::ActiveImage);

      recenterAllViews(
        k_recenterCrosshairs,
        k_realignCrosshairs,
        k_doNotRecenterOnCurrentCrosshairsPosition,
        k_doNotResetObliqueOrientation,
        k_resetZoom);
    }
    ImGui::SameLine();
    helpMarker("Recenter views and crosshairs on the active image");

    if (ImGui::RadioButton(
          "Reference and active images",
          ImageSelection::ReferenceAndActiveImages == appData.state().recenteringMode()))
    {
      appData.state().setRecenteringMode(ImageSelection::ReferenceAndActiveImages);

      recenterAllViews(
        k_recenterCrosshairs,
        k_realignCrosshairs,
        k_doNotRecenterOnCurrentCrosshairsPosition,
        k_doNotResetObliqueOrientation,
        k_resetZoom);
    }
    ImGui::SameLine();
    helpMarker("Recenter views and crosshairs on the reference and active images");

    /// @todo These don't work yet
    /*
              if ( ImGui::RadioButton( "All visible images",
       ImageSelection::VisibleImagesInView == appData.state().recenteringMode() ) )
              {
                  appData.state().setRecenteringMode( ImageSelection::VisibleImagesInView );
                  recenterAllViews( k_recenterCrosshairs,
       k_doNotRecenterOnCurrentCrosshairsPosition );
              }
              ImGui::SameLine(); helpMarker( "Recenter views and crosshairs on the visible
       images in each view" );

              if ( ImGui::RadioButton( "Fixed image", ImageSelection::FixedImageInView ==
       appData.state().recenteringMode() ) )
              {
                  appData.state().setRecenteringMode( ImageSelection::FixedImageInView );
                  recenterAllViews( k_recenterCrosshairs,
       k_doNotRecenterOnCurrentCrosshairsPosition );
              }
              ImGui::SameLine(); helpMarker( "Recenter views on the fixed image in each view"
       );

              if ( ImGui::RadioButton( "Moving image", ImageSelection::MovingImageInView ==
       appData.state().recenteringMode() ) )
              {
                  appData.state().setRecenteringMode( ImageSelection::MovingImageInView );
                  recenterAllViews( k_recenterCrosshairs,
       k_doNotRecenterOnCurrentCrosshairsPosition );
              }
              ImGui::SameLine(); helpMarker( "Recenter views on the moving image in each view"
       );

              if ( ImGui::RadioButton( "Fixed and moving images",
       ImageSelection::FixedAndMovingImagesInView == appData.state().recenteringMode() ) )
              {
                  appData.state().setRecenteringMode(
       ImageSelection::FixedAndMovingImagesInView ); recenterAllViews( k_recenterCrosshairs,
       k_doNotRecenterOnCurrentCrosshairsPosition );
              }
              ImGui::SameLine(); helpMarker( "Recenter views on the fixed and moving images in
       each view" );
              */

    if (ImGui::RadioButton("All loaded images", ImageSelection::AllLoadedImages == appData.state().recenteringMode())) {
      appData.state().setRecenteringMode(ImageSelection::AllLoadedImages);

      recenterAllViews(
        k_recenterCrosshairs,
        k_realignCrosshairs,
        k_doNotRecenterOnCurrentCrosshairsPosition,
        k_doNotResetObliqueOrientation,
        k_resetZoom);
    }
    ImGui::SameLine();
    helpMarker("Recenter views and crosshairs on all loaded images");
  }
  finishSettingsSection(viewRecenteringOpen);

  // Anatomical labels:
  const bool anatomicalLabelsOpen = ImGui::CollapsingHeader("Anatomical Labels", ImGuiTreeNodeFlags_DefaultOpen);
  if (anatomicalLabelsOpen) {
    bool showAnatomicalLabels = renderData.m_showAnatomicalLabels;
    if (ImGui::Checkbox("Show anatomical labels", &showAnatomicalLabels)) {
      renderData.m_showAnatomicalLabels = showAnatomicalLabels;
      renderData.m_showAnatomicalLabelsInLightboxViews = showAnatomicalLabels;
    }
    ImGui::SameLine();
    helpMarker("Show anatomical direction labels in anatomical views");

    ImGui::ColorEdit4("Label text color", glm::value_ptr(renderData.m_anatomicalLabelColor), k_colorAlphaEditFlags);
    float labelScale = renderData.m_anatomicalLabelScale;
    ImGui::PushItemWidth(k_viewOptionControlWidth);
    if (mySliderF32("Scale", &labelScale, 0.5f, 2.0f, "%.1fx")) {
      renderData.m_anatomicalLabelScale = std::clamp(labelScale, 0.5f, 2.0f);
    }
    ImGui::PopItemWidth();
    ImGui::Dummy(ImVec2(0.0f, 1.0f));

    static constexpr bool kOrientChangeRecenterCrosshairs = false;
    static constexpr bool kOrientChangeRealignCrosshairs = false;
    static constexpr bool kOrientChangeRecenterOnXhairs = true;
    static constexpr bool kOrientChangeResetObliqueOrientation = false;
    static constexpr bool kOrientChangeResetZoom = false;

    auto setViewConvention = [&](ViewConvention convention) {
      appData.windowData().setViewOrientationConvention(convention);
      recenterAllViews(
        kOrientChangeRecenterCrosshairs,
        kOrientChangeRealignCrosshairs,
        kOrientChangeRecenterOnXhairs,
        kOrientChangeResetObliqueOrientation,
        kOrientChangeResetZoom);
    };

    ImGui::Text("Left/right orientation convention:");
    if (ImGui::RadioButton(
          "Radiological",
          ViewConvention::Radiological == appData.windowData().getViewOrientationConvention()))
    {
      setViewConvention(ViewConvention::Radiological);
    }
    ImGui::SameLine();
    helpMarker("Anatomical left is on view right; anatomical right is on view left");
    ImGui::SameLine();

    if (ImGui::RadioButton(
          "Neurological",
          ViewConvention::Neurological == appData.windowData().getViewOrientationConvention()))
    {
      setViewConvention(ViewConvention::Neurological);
    }
    ImGui::SameLine();
    helpMarker("Anatomical left is on view left; anatomical right is on view right");

    ImGui::Dummy(ImVec2(0.0f, 1.0f));

    ImGui::Text("Anatomical directions:");

    if (ImGui::RadioButton("Human", AnatomicalLabelType::Human == renderData.m_anatomicalLabelType)) {
      renderData.m_anatomicalLabelType = AnatomicalLabelType::Human;
    }
    ImGui::SameLine();
    helpMarker("Left, Right, Posterior, Anterior, Superior, Inferior");

    ImGui::SameLine();
    if (ImGui::RadioButton("Rodent", AnatomicalLabelType::Rodent == renderData.m_anatomicalLabelType)) {
      renderData.m_anatomicalLabelType = AnatomicalLabelType::Rodent;
    }
    ImGui::SameLine();
    helpMarker("Left, Right, Dorsal, Ventral, Caudal, Rostral");

    ImGui::SameLine();
    if (ImGui::RadioButton("Cartesian", AnatomicalLabelType::Cartesian == renderData.m_anatomicalLabelType)) {
      renderData.m_anatomicalLabelType = AnatomicalLabelType::Cartesian;
    }
    ImGui::SameLine();
    helpMarker("+x, -x, +y, -y, +z, -z");
  }
  finishSettingsSection(anatomicalLabelsOpen);

  const bool scaleBarsOpen = ImGui::CollapsingHeader("Scale Bars", ImGuiTreeNodeFlags_DefaultOpen);
  if (scaleBarsOpen) {
    bool showScaleBars = renderData.m_showScaleBars;
    if (ImGui::Checkbox("Show scale bars", &showScaleBars)) {
      renderData.m_showScaleBars = showScaleBars;
    }
    ImGui::SameLine();
    helpMarker("Show physical scale bars on anatomical views");

    if (showScaleBars) {
      ImGui::ColorEdit4("Scale bars color", glm::value_ptr(renderData.m_scaleBarColor), k_colorAlphaEditFlags);

      auto setScaleBarPosition = [&](ScaleBarPosition position) {
        renderData.m_scaleBarPosition = position;
        renderData.m_scaleBarOrientation =
          ui_settings::normalizedScaleBarOrientation(position, renderData.m_scaleBarOrientation);
      };

      ImGui::Spacing();
      ImGui::PushItemWidth(k_viewOptionControlWidth);
      if (ImGui::BeginCombo("Position", ui_settings::scaleBarPositionName(renderData.m_scaleBarPosition))) {
        for (const auto position : ui_settings::orderedScaleBarPositions()) {
          const bool selected = position == renderData.m_scaleBarPosition;
          if (ImGui::Selectable(ui_settings::scaleBarPositionName(position), selected)) {
            setScaleBarPosition(position);
          }
          if (selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
      ImGui::PopItemWidth();

      auto drawScaleBarPositionButton = [&](const ui_settings::ScaleBarPositionButton& button) {
        static constexpr ImVec2 k_buttonSize{32.0f, 24.0f};
        const bool selected = button.position == renderData.m_scaleBarPosition;
        if (selected) {
          ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::Button(button.label, k_buttonSize)) {
          setScaleBarPosition(button.position);
        }
        if (selected) {
          ImGui::PopStyleColor();
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", ui_settings::scaleBarPositionName(button.position));
        }
      };

      const auto& scaleBarPositionButtons = ui_settings::scaleBarPositionButtons();
      for (int i = 0; i < 3; ++i) {
        drawScaleBarPositionButton(scaleBarPositionButtons[static_cast<std::size_t>(i)]);
        if (i < 2) {
          ImGui::SameLine();
        }
      }
      drawScaleBarPositionButton(scaleBarPositionButtons[3]);
      ImGui::SameLine();
      ImGui::BeginDisabled();
      ImGui::Button("##scaleBarPositionCenter", ImVec2{32.0f, 24.0f});
      ImGui::EndDisabled();
      ImGui::SameLine();
      drawScaleBarPositionButton(scaleBarPositionButtons[4]);
      for (int i = 5; i < 8; ++i) {
        drawScaleBarPositionButton(scaleBarPositionButtons[static_cast<std::size_t>(i)]);
        if (i < 7) {
          ImGui::SameLine();
        }
      }

      renderData.m_scaleBarOrientation =
        ui_settings::normalizedScaleBarOrientation(renderData.m_scaleBarPosition, renderData.m_scaleBarOrientation);

      if (ui_settings::canChooseScaleBarOrientation(renderData.m_scaleBarPosition)) {
        ImGui::Spacing();
        ImGui::Text("Orientation:");
        if (ImGui::RadioButton(
              "Horizontal##scaleBarOrientation",
              ScaleBarOrientation::Horizontal == renderData.m_scaleBarOrientation))
        {
          renderData.m_scaleBarOrientation = ScaleBarOrientation::Horizontal;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton(
              "Vertical##scaleBarOrientation",
              ScaleBarOrientation::Vertical == renderData.m_scaleBarOrientation))
        {
          renderData.m_scaleBarOrientation = ScaleBarOrientation::Vertical;
        }
        ImGui::SameLine();
        helpMarker("Draw scale bars horizontally or vertically");
      }

      ImGui::Spacing();
      int targetLengthPercent = ui_settings::targetLengthPercentFromFraction(renderData.m_scaleBarTargetFraction);
      ImGui::PushItemWidth(k_viewOptionControlWidth);
      if (ImGui::SliderInt("Length", &targetLengthPercent, 5, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp)) {
        renderData.m_scaleBarTargetFraction = ui_settings::targetLengthFractionFromPercent(targetLengthPercent);
      }
      ImGui::PopItemWidth();
      ImGui::SameLine();
      helpMarker(
        "Approximate fraction of the view occupied by the scale bar before rounding to a clean physical length");

      int scaleBarMarginPx = ui_settings::marginPixelsFromFloat(renderData.m_scaleBarMarginPx);
      ImGui::PushItemWidth(k_viewOptionControlWidth);
      if (ImGui::SliderInt("Margin", &scaleBarMarginPx, 12, 96, "%d px", ImGuiSliderFlags_AlwaysClamp)) {
        renderData.m_scaleBarMarginPx = ui_settings::marginFloatFromPixels(scaleBarMarginPx);
      }
      ImGui::PopItemWidth();
      ImGui::SameLine();
      helpMarker("Offset scale bars away from view edges and corners");

      ImGui::Spacing();
      ImGui::Text("Ticks:");
      if (ImGui::RadioButton(
            "Automatic extra ticks##scaleBarTicks",
            ScaleBarTicks::Automatic == renderData.m_scaleBarTicks))
      {
        renderData.m_scaleBarTicks = ScaleBarTicks::Automatic;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("Endpoints only##scaleBarTicks", ScaleBarTicks::Endpoints == renderData.m_scaleBarTicks)) {
        renderData.m_scaleBarTicks = ScaleBarTicks::Endpoints;
      }
      ImGui::SameLine();
      helpMarker("Add evenly spaced extra ticks when the scale bar is long enough for readable subdivisions");
    }
  }
  finishSettingsSection(scaleBarsOpen);

  const bool annotationsOpen = ImGui::CollapsingHeader("Annotations", ImGuiTreeNodeFlags_DefaultOpen);
  if (annotationsOpen) {
    renderAnnotationViewSettings(renderData);
  }
  finishSettingsSection(annotationsOpen);

  if (ImGui::CollapsingHeader("Lightbox Views", ImGuiTreeNodeFlags_DefaultOpen)) {
    const bool showImageBorders = renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections;
    if (!showImageBorders) {
      renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersectionsInLightboxViews = false;
    }
    ImGui::BeginDisabled(!showImageBorders);
    bool showImageBordersInLightboxViews =
      showImageBorders &&
      renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersectionsInLightboxViews;
    if (ImGui::Checkbox("Show image borders##lightboxViews", &showImageBordersInLightboxViews)) {
      renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersectionsInLightboxViews =
        showImageBordersInLightboxViews;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    helpMarker("Show image intersection borders in each tile of lightbox layouts");

    const bool showCrosshairs = renderData.m_showCrosshairs;
    if (!showCrosshairs) {
      renderData.m_showCrosshairsInLightboxViews = false;
    }
    ImGui::BeginDisabled(!showCrosshairs);
    bool showCrosshairsInLightboxViews = showCrosshairs && renderData.m_showCrosshairsInLightboxViews;
    if (ImGui::Checkbox("Show crosshairs##lightboxViews", &showCrosshairsInLightboxViews)) {
      renderData.m_showCrosshairsInLightboxViews = showCrosshairsInLightboxViews;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    helpMarker("Show crosshairs in each tile of lightbox layouts");

    const bool showAnatomicalLabels = renderData.m_showAnatomicalLabels;
    if (!showAnatomicalLabels) {
      renderData.m_showAnatomicalLabelsInLightboxViews = false;
    }
    ImGui::BeginDisabled(!showAnatomicalLabels);
    bool showAnatomicalLabelsInLightboxViews = showAnatomicalLabels && renderData.m_showAnatomicalLabelsInLightboxViews;
    if (ImGui::Checkbox("Show anatomical labels##lightboxViews", &showAnatomicalLabelsInLightboxViews)) {
      renderData.m_showAnatomicalLabelsInLightboxViews = showAnatomicalLabelsInLightboxViews;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    helpMarker("Show anatomical direction labels in each tile of lightbox layouts");

    const bool showScaleBars = renderData.m_showScaleBars;
    ImGui::BeginDisabled(!showScaleBars);
    bool showScaleBarsInLightboxViews = renderData.m_showScaleBarsInLightboxViews;
    if (ImGui::Checkbox("Show scale bars##lightboxViews", &showScaleBarsInLightboxViews)) {
      renderData.m_showScaleBarsInLightboxViews = showScaleBarsInLightboxViews;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    helpMarker("Show scale bars in each tile of lightbox layouts");

    bool showOffsetLabels = renderData.m_showLightboxOffsetLabels;
    if (ImGui::Checkbox("Show slice offset labels", &showOffsetLabels)) {
      renderData.m_showLightboxOffsetLabels = showOffsetLabels;
    }
    ImGui::SameLine();
    helpMarker("Show each lightbox tile's physical offset from the crosshairs slice");

    if (showOffsetLabels) {
      ImGui::ColorEdit4(
        "Offset label color",
        glm::value_ptr(renderData.m_lightboxOffsetLabelColor),
        k_colorAlphaEditFlags);
    }
  }
}

/**
 * @brief Render the Interface settings page contents.
 */
void renderInterfaceTab(
  AppData& appData,
  const std::function<void(std::optional<float> scale)>& setUiScaleOverride,
  const std::function<void()>& requestFontReload,
  const std::function<void(UiColorPreset preset)>& applyUiColorPreset,
  const std::function<void(UiDensityPreset preset)>& applyUiDensityPreset,
  const std::function<void(float opacity)>& applyUiWindowBgOpacity,
  const std::function<void()>& readjustViewport,
  const SettingsPersistenceCallbacks& persistenceCallbacks)
{
  bool showLayoutTabs = appData.settings().showLayoutTabs();
  if (ImGui::Checkbox("Show layout tab bar", &showLayoutTabs)) {
    appData.settings().setShowLayoutTabs(showLayoutTabs);
    appData.guiData().m_showLayoutTabs = showLayoutTabs;
    if (readjustViewport) {
      readjustViewport();
    }
  }

  if (showLayoutTabs) {
    const bool layoutTabsTop = UiLayoutTabPlacement::Top == appData.settings().layoutTabPlacement();
    ImGui::Text("Position:");
    if (ImGui::RadioButton("Top##layoutTabBarPosition", layoutTabsTop)) {
      appData.settings().setLayoutTabPlacement(UiLayoutTabPlacement::Top);
      appData.guiData().m_layoutTabPlacement = guiLayoutTabPlacement(appData.settings().layoutTabPlacement());
      if (readjustViewport) {
        readjustViewport();
      }
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Bottom##layoutTabBarPosition", !layoutTabsTop)) {
      appData.settings().setLayoutTabPlacement(UiLayoutTabPlacement::Bottom);
      appData.guiData().m_layoutTabPlacement = guiLayoutTabPlacement(appData.settings().layoutTabPlacement());
      if (readjustViewport) {
        readjustViewport();
      }
    }
  }

  ImGui::Spacing();
  bool showGlobalTimeControls = appData.settings().showGlobalTimeControls();
  if (ImGui::Checkbox("Show global time controls", &showGlobalTimeControls)) {
    appData.settings().setShowGlobalTimeControls(showGlobalTimeControls);
  }
  ImGui::SameLine();
  helpMarker("Show the floating time-series playback controls shared by loaded time-series images.");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  const bool lookAndFeelOpen = ImGui::CollapsingHeader("Look and Feel", ImGuiTreeNodeFlags_DefaultOpen);
  if (lookAndFeelOpen) {
    const auto currentScale = appData.settings().uiScaleOverride();
    const auto& uiScaleChoices = ui_settings::uiScaleChoices();
    auto currentChoice = std::find_if(
      uiScaleChoices.begin(),
      uiScaleChoices.end(),
      [&currentScale](const ui_settings::ScaleChoice& choice) { return choice.scale == currentScale; });
    if (currentChoice == uiScaleChoices.end()) {
      currentChoice = uiScaleChoices.begin();
    }

    if (ImGui::BeginCombo("UI scale", currentChoice->label)) {
      for (const ui_settings::ScaleChoice& choice : uiScaleChoices) {
        const bool selected = choice.scale == currentScale;
        if (ImGui::Selectable(choice.label, selected)) {
          appData.settings().setUiScaleOverride(choice.scale);
          if (setUiScaleOverride) {
            setUiScaleOverride(appData.settings().uiScaleOverride());
          }
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    helpMarker(
      "Auto uses the platform UI scale. On macOS this is 100% so Retina backing scale keeps rendering sharp "
      "without enlarging the interface.");

    const UiFontFamily currentFamily = appData.settings().uiFontFamily();
    const auto& uiFontChoices = ui_settings::visibleFontChoices();
    auto currentFontChoice =
      std::find_if(uiFontChoices.begin(), uiFontChoices.end(), [currentFamily](const ui_settings::FontChoice& choice) {
        return choice.family == currentFamily;
      });
    if (currentFontChoice == uiFontChoices.end()) {
      currentFontChoice = uiFontChoices.begin();
    }

    if (ImGui::BeginCombo("UI font", currentFontChoice->label)) {
      for (const ui_settings::FontChoice& choice : uiFontChoices) {
        const bool selected = choice.family == currentFamily;
        if (ImGui::Selectable(choice.label, selected)) {
          appData.settings().setUiFontFamily(choice.family);
          if (requestFontReload) {
            requestFontReload();
          }
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }

    const UiColorPreset currentColorPreset = appData.settings().uiColorPreset();
    if (ImGui::BeginCombo("UI color scheme", uiColorPresetName(currentColorPreset))) {
      for (const UiColorPreset preset : ui_settings::uiColorPresets()) {
        const bool selected = preset == currentColorPreset;
        if (ImGui::Selectable(uiColorPresetName(preset), selected)) {
          appData.settings().setUiColorPreset(preset);
          if (applyUiColorPreset) {
            applyUiColorPreset(preset);
          }
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }

    const UiDensityPreset currentDensityPreset = appData.settings().uiDensityPreset();
    if (ImGui::BeginCombo("UI density", uiDensityPresetName(currentDensityPreset))) {
      for (const UiDensityPreset preset : ui_settings::uiDensityPresets()) {
        const bool selected = preset == currentDensityPreset;
        if (ImGui::Selectable(uiDensityPresetName(preset), selected)) {
          appData.settings().setUiDensityPreset(preset);
          if (applyUiDensityPreset) {
            applyUiDensityPreset(preset);
          }
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }

    float windowBgOpacity = appData.settings().uiWindowBgOpacity();
    if (ImGui::SliderFloat("Window background opacity", &windowBgOpacity, 0.2f, 1.0f, "%.2f")) {
      appData.settings().setUiWindowBgOpacity(windowBgOpacity);
      if (applyUiWindowBgOpacity) {
        applyUiWindowBgOpacity(appData.settings().uiWindowBgOpacity());
      }
    }
  }
  finishSettingsSection(lookAndFeelOpen);
}

/**
 * @brief Render the System settings page contents.
 */
void renderSystemTab(AppData& appData)
{
  const bool updatesOpen = ImGui::CollapsingHeader("Updates", ImGuiTreeNodeFlags_DefaultOpen);
  if (updatesOpen) {
    bool automaticChecks = appData.settings().automaticUpdateChecksEnabled();
    if (ImGui::Checkbox("Automatically check for updates", &automaticChecks)) {
      appData.settings().setAutomaticUpdateChecksEnabled(automaticChecks);
    }
    ImGui::SameLine();
    helpMarker(
      "When enabled, Entropy checks GitHub for a newer release once per app session and shows a notification only "
      "when an update is available.");
  }
  finishSettingsSection(updatesOpen);

  const bool diagnosticsOpen = ImGui::CollapsingHeader("Diagnostics", ImGuiTreeNodeFlags_DefaultOpen);
  if (diagnosticsOpen) {
    renderDiagnosticsSettings();
    ImGui::Spacing();
  }
  finishSettingsSection(diagnosticsOpen);

  if (ImGui::CollapsingHeader("Developer Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox("Show ImGui demo window", &(appData.guiData().m_showImGuiDemoWindow));
    ImGui::Checkbox("Show ImPlot demo window", &(appData.guiData().m_showImPlotDemoWindow));
  }
}

/**
 * @brief Render intensity projection default settings.
 */
void renderIntensityProjectionDefaults(RenderData& renderData);

/**
 * @brief Render the image settings page contents.
 */
void renderImagesTab(AppData& appData)
{
  const bool imageDisplayDefaultsOpen =
    ImGui::CollapsingHeader("Image Display Defaults", ImGuiTreeNodeFlags_DefaultOpen);
  if (imageDisplayDefaultsOpen) {
    ImGui::Checkbox(
      "Floating-point linear image interpolation",
      &appData.renderData().m_imageGrayFloatingPointInterpolation);
    ImGui::SameLine();
    helpMarker("Use floating-point instead of 8-bit fixed-point linear interpolation for grayscale images");
  }
  finishSettingsSection(imageDisplayDefaultsOpen);

  renderIntensityProjectionDefaults(appData.renderData());
}

struct RegistrationBackendInfo
{
  const char* header;
  const char* fullName;
  const char* summary;
  const char* primaryAuthors;
  const char* publication;
  const char* copyright;
  const char* license;
  const char* supportedVersions;
  const char* projectUrl;
  const char* documentationUrl;
};

constexpr std::array k_registrationBackendInfo{
  RegistrationBackendInfo{
    "ANTs",
    "Advanced Normalization Tools",
    "State-of-the-art command-line medical image registration and segmentation toolkit built on ITK.",
    "ANTsX/ConsortiumOfANTS contributors, including Brian Avants, Nick Tustison, Philip Cook, Jeffrey Duda, et al.",
    "See ANTsX publication and citation guidance in the upstream repository",
    "Copyright 2009-2023 ConsortiumOfANTS",
    "Apache License 2.0",
    "Entropy targets antsRegistration and antsApplyTransforms command-line tools from ANTs releases.",
    "https://github.com/ANTsX/ANTs",
    "https://github.com/ANTsX/ANTs/wiki"},
  RegistrationBackendInfo{
    "FireANTs",
    "FireANTs: Adaptive Riemannian Optimization for Multi-Scale Diffeomorphic Registration",
    "GPU-oriented Python registration package for fast Riemannian diffeomorphic registration",
    "Rohit Jena, Pratik Chaudhari, and James C. Gee",
    "Jena, Chaudhari, and Gee, Nature Communications, 2024",
    "Notices in the upstream package identify Rohit Jena as copyright holder",
    "FireANTs License version 1.0",
    "Entropy targets the FireANTs Python CLI through Entropy's bridge module.",
    "https://github.com/rohitrango/FireANTs",
    "https://fireants.readthedocs.io/en/latest/"},
  RegistrationBackendInfo{
    "Greedy",
    "Greedy diffeomorphic registration",
    "Fast command-line medical image registration with affine and deformable registration modes",
    "Paul A. Yushkevich (primary author)",
    "Commonly cited as the Greedy registration tool; see project documentation for citation guidance",
    "See the upstream Greedy source distribution for copyright details",
    "GNU General Public License version 3",
    "Entropy targets the Greedy command-line interface exposed by the installed greedy executable.",
    "https://github.com/pyushkevich/greedy",
    "https://sites.google.com/view/greedyreg/about"}};

void renderRegistrationBackendInfoRow(const char* label, const char* value)
{
  ImGui::TextDisabled("%s", label);
  ImGui::SameLine();
  ImGui::TextWrapped("%s", value);
}

void renderRegistrationBackendInfo(const RegistrationBackendInfo& info)
{
  ImGui::SeparatorText(info.header);
  ImGui::TextWrapped("%s", info.summary);
  renderRegistrationBackendInfoRow("Full name:", info.fullName);
  renderRegistrationBackendInfoRow("Authors:", info.primaryAuthors);
  renderRegistrationBackendInfoRow("Publication:", info.publication);
  renderRegistrationBackendInfoRow("Copyright:", info.copyright);
  renderRegistrationBackendInfoRow("License:", info.license);
  renderRegistrationBackendInfoRow("Supported versions:", info.supportedVersions);

  ImGui::TextDisabled("Project:");
  ImGui::SameLine();
  ImGui::TextLinkOpenURL(info.projectUrl, info.projectUrl);
  ImGui::TextDisabled("Documentation:");
  ImGui::SameLine();
  ImGui::TextLinkOpenURL(info.documentationUrl, info.documentationUrl);
}

/**
 * @brief Render registration backend settings.
 */
void renderRegistrationTab(AppData& appData)
{
  registration::BackendConfig& config = appData.settings().registrationBackendConfig();

  const bool backendDefaultsOpen = ImGui::CollapsingHeader("Backend Defaults", ImGuiTreeNodeFlags_DefaultOpen);
  if (backendDefaultsOpen) {
    const std::string preview{registration::label(config.defaultBackend)};
    ImGui::PushItemWidth(settingsControlWidth());
    if (ImGui::BeginCombo("Default registration backend", preview.c_str())) {
      for (const registration::Backend backend : k_registrationBackends) {
        const bool selected = backend == config.defaultBackend;
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
    ImGui::PopItemWidth();
    ImGui::SameLine();
    helpMarker("Backend preselected for new registration jobs.");

    ImGui::Spacing();
    disabledTextWrapped(
      "Backend registration executable fields can be command names when the tools are on the system PATH, or full "
      "paths to the executable files.");

    renderPathSettingFixedWidth(
      "Greedy executable",
      config.greedyExecutable,
      "Command or executable path used to launch Greedy.",
      true);
    renderPathSettingFixedWidth(
      "ANTs registration executable",
      config.antsRegistrationExecutable,
      "Command or executable path used to launch antsRegistration.",
      true);
    renderPathSettingFixedWidth(
      "ANTs apply transforms executable",
      config.antsApplyTransformsExecutable,
      "Command or executable path used to launch antsApplyTransforms for warped outputs.",
      true);
    renderPathSettingFixedWidth(
      "ANTs convert transform executable",
      config.antsConvertTransformFileExecutable,
      "Command or executable path used to convert ANTs affine output into Entropy's importable matrix artifact.",
      true);
    renderPathSettingFixedWidth(
      "FireANTs Python executable",
      config.fireAntsPythonExecutable,
      "Python executable used to run the FireANTs bridge module.",
      true);
  }
  finishSettingsSection(backendDefaultsOpen);

  const bool executionOpen = ImGui::CollapsingHeader("Execution", ImGuiTreeNodeFlags_DefaultOpen);
  if (executionOpen) {
    std::string outputDirectory = config.defaultOutputDirectory.string();
    ImGui::PushItemWidth(settingsControlWidth());
    if (ImGui::InputText("Output directory", &outputDirectory)) {
      config.defaultOutputDirectory =
        outputDirectory.empty() ? std::filesystem::path{} : std::filesystem::path{outputDirectory};
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("...##RegistrationOutputDirectory")) {
      if (
        const std::optional<std::filesystem::path> selected = native_dialog::pickFolder(config.defaultOutputDirectory))
      {
        config.defaultOutputDirectory = *selected;
      }
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
      ImGui::SetTooltip("%s", "Select output directory");
    }
    ImGui::SameLine();
    helpMarker(
      "Optional directory for registration outputs. Leave empty to use per-job folders in the system temporary "
      "directory.");

    int maxConcurrentJobs = config.maxConcurrentJobs;
    ImGui::PushItemWidth(settingsControlWidth());
    if (ImGui::InputInt("Max concurrent registration jobs", &maxConcurrentJobs)) {
      config.maxConcurrentJobs = std::max(1, maxConcurrentJobs);
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    helpMarker("Maximum number of registration jobs Entropy should run at the same time.");

    int cpuThreads = config.defaultCpuThreadCount;
    ImGui::PushItemWidth(settingsControlWidth());
    if (ImGui::InputInt("Default CPU threads", &cpuThreads)) {
      config.defaultCpuThreadCount = std::max(0, cpuThreads);
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    helpMarker("Default CPU thread count. Zero lets the backend choose.");

    renderTextSettingFixedWidth("FireANTs device", config.defaultFireAntsDevice, "PyTorch device passed to FireANTs.");

    ImGui::Checkbox("Keep temporary files", &config.keepTemporaryFiles);
    ImGui::SameLine();
    helpMarker("Keep exported intermediate files for debugging backend failures.");
  }
  finishSettingsSection(executionOpen);

  const bool backendInfoOpen = ImGui::CollapsingHeader("Backend Information");
  if (backendInfoOpen) {
    ImGui::TextWrapped(
      "Entropy launches external registration tools and imports their outputs. These summaries identify the upstream "
      "projects that Entropy can call; use the links for authoritative licensing, citation, and version details.");
    for (const RegistrationBackendInfo& info : k_registrationBackendInfo) {
      renderRegistrationBackendInfo(info);
    }
  }
}

/**
 * @brief Render the Synchronization settings page contents.
 */
void renderSynchronizeTab(AppData& appData)
{
  auto renderSyncRow = [](
                         const char* label,
                         const char* sendId,
                         const char* receiveId,
                         bool sendValue,
                         bool receiveValue,
                         const std::function<void(bool)>& setSend,
                         const std::function<void(bool)>& setReceive,
                         const char* tooltip) {
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);

    ImGui::TableSetColumnIndex(1);
    if (ImGui::Checkbox(sendId, &sendValue)) {
      setSend(sendValue);
    }

    ImGui::TableSetColumnIndex(2);
    if (ImGui::Checkbox(receiveId, &receiveValue)) {
      setReceive(receiveValue);
    }

    ImGui::TableSetColumnIndex(3);
    helpMarker(tooltip);
  };

  auto renderSyncOptions = [&renderSyncRow](
                             const char* tableId,
                             const char* cursorSendId,
                             const char* cursorReceiveId,
                             const char* zoomSendId,
                             const char* zoomReceiveId,
                             const char* panSendId,
                             const char* panReceiveId,
                             bool sendCursor,
                             bool receiveCursor,
                             bool sendZoom,
                             bool receiveZoom,
                             bool sendPan,
                             bool receivePan,
                             const std::function<void(bool)>& setSendCursor,
                             const std::function<void(bool)>& setReceiveCursor,
                             const std::function<void(bool)>& setSendZoom,
                             const std::function<void(bool)>& setReceiveZoom,
                             const std::function<void(bool)>& setSendPan,
                             const std::function<void(bool)>& setReceivePan,
                             const char* cursorTooltip,
                             const char* zoomTooltip,
                             const char* panTooltip) {
    ImGui::Spacing();
    if (ImGui::BeginTable(tableId, 4, ImGuiTableFlags_SizingFixedFit)) {
      renderSyncRow(
        "Cursor:",
        cursorSendId,
        cursorReceiveId,
        sendCursor,
        receiveCursor,
        setSendCursor,
        setReceiveCursor,
        cursorTooltip);

      renderSyncRow(
        "Zoom:",
        zoomSendId,
        zoomReceiveId,
        sendZoom,
        receiveZoom,
        setSendZoom,
        setReceiveZoom,
        zoomTooltip);

      renderSyncRow("Pan:", panSendId, panReceiveId, sendPan, receivePan, setSendPan, setReceivePan, panTooltip);

      ImGui::EndTable();
    }
  };

  bool entropySyncEnabled = appData.settings().entropyInstanceSyncEnabled();
  if (ImGui::Checkbox("Synchronize crosshairs position between Entropy instances", &entropySyncEnabled)) {
    appData.settings().setEntropyInstanceSyncEnabled(entropySyncEnabled);
  }
  ImGui::SameLine();
  helpMarker(
    "Synchronize cursor position between running Entropy instances that have the same project or same ordered image "
    "list loaded");

  bool synchronizeTimeSeries = appData.settings().synchronizeTimeSeries();
  if (ImGui::Checkbox("Synchronize time points across images", &synchronizeTimeSeries)) {
    appData.settings().setSynchronizeTimeSeries(synchronizeTimeSeries);
  }
  ImGui::SameLine();
  helpMarker(
    "When enabled, changing one time-series image frame changes other loaded time-series images to the same frame "
    "index "
    "when available.");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  const bool itkSnapOpen = ImGui::CollapsingHeader("ITK-SNAP", ImGuiTreeNodeFlags_DefaultOpen);
  if (itkSnapOpen) {
    bool snapSyncEnabled = appData.settings().cursorSyncEnabled();
    if (ImGui::Checkbox("Synchronize with ITK-SNAP", &snapSyncEnabled)) {
      appData.settings().setCursorSyncEnabled(snapSyncEnabled);
    }
    ImGui::SameLine();
    helpMarker("Synchronize with ITK-SNAP through shared memory");

    if (snapSyncEnabled) {
      renderSyncOptions(
        "##snapSyncOptions",
        "Send##cursorSync",
        "Receive##cursorSync",
        "Send##zoomSync",
        "Receive##zoomSync",
        "Send##panSync",
        "Receive##panSync",
        appData.settings().sendCursorSync(),
        appData.settings().receiveCursorSync(),
        appData.settings().sendZoomSync(),
        appData.settings().receiveZoomSync(),
        appData.settings().sendPanSync(),
        appData.settings().receivePanSync(),
        [&appData](bool value) { appData.settings().setSendCursorSync(value); },
        [&appData](bool value) { appData.settings().setReceiveCursorSync(value); },
        [&appData](bool value) { appData.settings().setSendZoomSync(value); },
        [&appData](bool value) { appData.settings().setReceiveZoomSync(value); },
        [&appData](bool value) { appData.settings().setSendPanSync(value); },
        [&appData](bool value) { appData.settings().setReceivePanSync(value); },
        "Send Entropy crosshairs movement to ITK-SNAP and receive ITK-SNAP cursor movement in Entropy. "
        "ITK-SNAP cursor messages use NIFTI/RAS coordinates; Entropy converts between internal LPS and "
        "ITK-SNAP RAS at the IPC boundary.",
        "Send Entropy view zoom to ITK-SNAP and receive ITK-SNAP view zoom in Entropy.",
        "Send Entropy view pan to ITK-SNAP and receive ITK-SNAP view pan in Entropy.");
    }
  }
}

/**
 * @brief Render the Segmentation settings page contents.
 */
void renderSegmentationTab(AppData& appData, RenderData& renderData)
{
  const bool displayOpen = ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen);
  if (displayOpen) {
    // Modulate opacity of segmentation with opacity of image:
    ImGui::Checkbox("Modulate segmentation with image opacity", &renderData.m_modulateSegOpacityWithImageOpacity);
    ImGui::SameLine();
    helpMarker("Modulate opacity of segmentation with opacity of image");

    ImGui::Dummy(ImVec2(0.0f, 1.0f));

    ImGui::Text("Boundary outline:");
    if (ImGui::RadioButton("Outline view pixels", SegmentationOutlineStyle::ViewPixel == renderData.m_segOutlineStyle))
    {
      renderData.m_segOutlineStyle = SegmentationOutlineStyle::ViewPixel;
    }
    ImGui::SameLine();
    helpMarker("Outline the outer view pixels of the image segmentation regions");

    if (ImGui::RadioButton(
          "Outline image voxels",
          SegmentationOutlineStyle::ImageVoxel == renderData.m_segOutlineStyle))
    {
      renderData.m_segOutlineStyle = SegmentationOutlineStyle::ImageVoxel;
    }
    ImGui::SameLine();
    helpMarker("Outline the outer voxels of the image segmentation regions");

    if (ImGui::RadioButton("Disable (no outline)", SegmentationOutlineStyle::Disabled == renderData.m_segOutlineStyle))
    {
      renderData.m_segOutlineStyle = SegmentationOutlineStyle::Disabled;
    }
    ImGui::SameLine();
    helpMarker("Disable segmentation outlining");

    if (SegmentationOutlineStyle::Disabled != renderData.m_segOutlineStyle) {
      ImGui::Spacing();
      ImGui::Dummy(ImVec2(0.0f, 1.0f));

      // Modulate opacity of interior of segmentation:
      mySliderF32("Interior opacity", &(renderData.m_segInteriorOpacity), 0.0f, 1.0f);
      ImGui::SameLine();
      helpMarker("Modulate opacity of interior of segmentation");
    }

    ImGui::Spacing();
    ImGui::Dummy(ImVec2(0.0f, 1.0f));

    float interpCutoff = renderData.m_segInterpCutoff;
    if (mySliderF32("Erosion factor", &interpCutoff, 0.5f, 1.0f)) {
      renderData.m_segInterpCutoff = interpCutoff;
    }
    ImGui::SameLine();
    helpMarker("Cutoff used to erode segmentation boundaries in linear or cubic interpolation mode");
  }
  finishSettingsSection(displayOpen);

  const bool brushOpen = ImGui::CollapsingHeader("Brush", ImGuiTreeNodeFlags_DefaultOpen);
  if (brushOpen) {
    AppSettings& settings = appData.settings();

    bool useRound = settings.useRoundBrush();
    if (ImGui::RadioButton("Round", useRound)) {
      settings.setUseRoundBrush(true);
    }
    ImGui::SameLine();
    helpMarker("Paint with a circular brush footprint");
    ImGui::SameLine();
    if (ImGui::RadioButton("Square", !useRound)) {
      settings.setUseRoundBrush(false);
    }
    ImGui::SameLine();
    helpMarker("Paint with a square brush footprint");

    bool use3d = settings.use3dBrush();
    if (ImGui::RadioButton("2D", !use3d)) {
      settings.setUse3dBrush(false);
    }
    ImGui::SameLine();
    helpMarker("Paint only on the current view slice");
    ImGui::SameLine();
    if (ImGui::RadioButton("3D", use3d)) {
      settings.setUse3dBrush(true);
    }
    ImGui::SameLine();
    helpMarker("Paint through neighboring slices in image space");

    bool useIso = settings.useIsotropicBrush();
    if (ImGui::Checkbox("Isotropic brush", &useIso)) {
      settings.setUseIsotropicBrush(useIso);
    }
    ImGui::SameLine();
    helpMarker("Use equal physical brush radius in all image directions");

    bool replaceBgWithFg = settings.replaceBackgroundWithForeground();
    if (ImGui::Checkbox("Replace background with foreground", &replaceBgWithFg)) {
      settings.setReplaceBackgroundWithForeground(replaceBgWithFg);
    }
    ImGui::SameLine();
    helpMarker("Use the foreground label as the replacement when painting over background voxels");

    bool xhairsMove = settings.crosshairsMoveWithBrush();
    if (ImGui::Checkbox("Crosshairs move with brush", &xhairsMove)) {
      settings.setCrosshairsMoveWithBrush(xhairsMove);
    }
    ImGui::SameLine();
    helpMarker("Move the crosshairs to the brush position while painting");
  }
  finishSettingsSection(brushOpen);

  if (ImGui::CollapsingHeader("Brush Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
    AppSettings& settings = appData.settings();
    BrushPreviewMode previewMode = settings.brushPreviewMode();

    if (ImGui::RadioButton("Hover##brushPreview", BrushPreviewMode::Hover == previewMode)) {
      settings.setBrushPreviewMode(BrushPreviewMode::Hover);
    }
    ImGui::SameLine();
    helpMarker("Preview the affected voxels while hovering before painting");
    ImGui::SameLine();
    if (ImGui::RadioButton("Off##brushPreview", BrushPreviewMode::Disabled == previewMode)) {
      settings.setBrushPreviewMode(BrushPreviewMode::Disabled);
    }
    ImGui::SameLine();
    helpMarker("Disable brush preview rendering");

    if (BrushPreviewMode::Disabled != settings.brushPreviewMode()) {
      BrushPreviewVoxels previewVoxels = settings.brushPreviewVoxels();
      if (ImGui::RadioButton("Changed voxels##brushPreviewVoxels", BrushPreviewVoxels::Changed == previewVoxels)) {
        settings.setBrushPreviewVoxels(BrushPreviewVoxels::Changed);
      }
      ImGui::SameLine();
      helpMarker("Preview only voxels whose label would change");
      ImGui::SameLine();
      if (ImGui::RadioButton("All voxels##brushPreviewVoxels", BrushPreviewVoxels::All == previewVoxels)) {
        settings.setBrushPreviewVoxels(BrushPreviewVoxels::All);
      }
      ImGui::SameLine();
      helpMarker("Preview every voxel inside the brush footprint");

      SegmentationOutlineStyle previewOutlineStyle = settings.brushPreviewOutlineStyle();
      if (ImGui::RadioButton(
            "Pixel outline##brushPreviewOutlineStyle",
            SegmentationOutlineStyle::ViewPixel == previewOutlineStyle))
      {
        settings.setBrushPreviewOutlineStyle(SegmentationOutlineStyle::ViewPixel);
      }
      ImGui::SameLine();
      helpMarker("Draw preview boundaries in screen pixels");
      ImGui::SameLine();
      if (ImGui::RadioButton(
            "Voxel outline##brushPreviewOutlineStyle",
            SegmentationOutlineStyle::ImageVoxel == previewOutlineStyle))
      {
        settings.setBrushPreviewOutlineStyle(SegmentationOutlineStyle::ImageVoxel);
      }
      ImGui::SameLine();
      helpMarker("Draw preview boundaries around image voxels");

      BrushPreviewStyle previewStyle = settings.brushPreviewStyle();
      if (ImGui::RadioButton("Outline##brushPreviewStyle", BrushPreviewStyle::Outline == previewStyle)) {
        settings.setBrushPreviewStyle(BrushPreviewStyle::Outline);
      }
      ImGui::SameLine();
      helpMarker("Draw only the preview outline");
      ImGui::SameLine();
      if (ImGui::RadioButton("Outline + fill##brushPreviewStyle", BrushPreviewStyle::OutlineAndFill == previewStyle)) {
        settings.setBrushPreviewStyle(BrushPreviewStyle::OutlineAndFill);
      }
      ImGui::SameLine();
      helpMarker("Draw the preview outline and translucent fill");

      if (BrushPreviewStyle::OutlineAndFill == settings.brushPreviewStyle()) {
        float previewFillOpacityPercent = 100.0f * settings.brushPreviewFillOpacity();
        if (mySliderF32("Fill opacity##brushPreviewFillOpacity", &previewFillOpacityPercent, 0.0f, 100.0f, "%.0f%%")) {
          settings.setBrushPreviewFillOpacity(previewFillOpacityPercent / 100.0f);
        }
        ImGui::SameLine();
        helpMarker("Opacity of the filled brush preview");
      }

      bool previewWhilePainting = settings.brushPreviewWhilePainting();
      if (ImGui::Checkbox("Show while painting##brushPreviewWhilePainting", &previewWhilePainting)) {
        settings.setBrushPreviewWhilePainting(previewWhilePainting);
      }
      ImGui::SameLine();
      helpMarker("Keep showing the preview while the brush is actively painting");
    }
  }
}

/**
 * @brief Render the Metrics settings section contents.
 */
void renderMetricsTab(
  AppData& appData,
  RenderData& renderData,
  const std::function<void(void)>& updateMetricUniforms,
  const std::function<std::size_t(void)>& getNumImageColorMaps,
  const std::function<const ImageColorMap*(std::size_t cmapIndex)>& getImageColorMap)
{
  ImGui::PushID("metrics"); /*** PushID metrics ***/

  ImGui::PushID("diff");
  const bool differenceOpen = ImGui::CollapsingHeader("Difference Metrics", ImGuiTreeNodeFlags_DefaultOpen);
  if (differenceOpen) {
    disabledTextWrapped(
      "Difference compares the displayed intensity values directly at each location. Low values mean the images have "
      "similar intensities; high values mean they differ, so this metric works best when both images use comparable "
      "intensity scales.");
    ImGui::Spacing();

    // Difference type:
    if (ImGui::RadioButton("Absolute", false == renderData.m_useSquare)) {
      renderData.m_useSquare = false;
    }

    ImGui::SameLine();
    if (ImGui::RadioButton("Squared difference", true == renderData.m_useSquare)) {
      renderData.m_useSquare = true;
    }
    ImGui::SameLine();
    helpMarker("Compute absolute or squared difference");

    renderMetricSettingsPanel(
      renderData.m_squaredDifferenceParams,
      appData.guiData().m_showDifferenceColormapWindow,
      "sqdiff",
      updateMetricUniforms,
      getNumImageColorMaps,
      getImageColorMap);
  }
  ImGui::PopID(); // "diff"

  finishSettingsSection(differenceOpen);

  ImGui::PushID("localncc");
  const bool localNccOpen =
    renderLocalNccSettings(appData, renderData, updateMetricUniforms, getNumImageColorMaps, getImageColorMap);
  ImGui::PopID();

  finishSettingsSection(localNccOpen);

  ImGui::PushID("localLinearResidual");
  const bool localLinearResidualOpen = renderLocalLinearResidualSettings(
    appData,
    renderData,
    updateMetricUniforms,
    getNumImageColorMaps,
    getImageColorMap);
  ImGui::PopID();

  ImGui::PopID(); /*** PopID metrics ***/
}

/**
 * @brief Render the Comparison modes settings section contents.
 */
bool renderComparisonModesTab(RenderData& renderData)
{
  ImGui::PushID("comparison"); /*** PushID metrics ***/

  if (!ImGui::CollapsingHeader("Comparison Modes", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::PopID(); /*** PopID comparison ***/
    return false;
  }

  //                if ( ImGui::TreeNode( "Comparison comparison" ) )
  //                {
  // Overlap style:
  ImGui::Text("Overlap color scheme:");

  if (ImGui::RadioButton("Red, green, yellow", false == renderData.m_overlayMagentaCyan)) {
    renderData.m_overlayMagentaCyan = false;
  }
  if (ImGui::RadioButton("Cyan, magenta, white", true == renderData.m_overlayMagentaCyan)) {
    renderData.m_overlayMagentaCyan = true;
  }

  ImGui::SameLine();
  helpMarker("Color style for 'overlay' views");
  ImGui::Spacing();

  // Quadrants style:
  ImGui::Text("Quadrants:");

  const glm::ivec2 Q = renderData.m_quadrants;

  if (ImGui::RadioButton("X", true == (Q.x && !Q.y))) {
    renderData.m_quadrants = glm::ivec2{true, false};
  }

  ImGui::SameLine();
  if (ImGui::RadioButton("Y", true == (!Q.x && Q.y))) {
    renderData.m_quadrants = glm::ivec2{false, true};
  }

  ImGui::SameLine();
  if (ImGui::RadioButton("X and Y comparison", true == (Q.x && Q.y))) {
    renderData.m_quadrants = glm::ivec2{true, true};
  }

  ImGui::SameLine();
  helpMarker("Comparison directions in 'quadrant' views");
  ImGui::Spacing();

  // Checkerboard squares
  ImGui::Text("Checkerboard:");

  int numSquares = renderData.m_numCheckerboardSquares;
  if (ImGui::InputInt("Number of checkers", &numSquares)) {
    if (2 <= numSquares && numSquares <= 2048) {
      renderData.m_numCheckerboardSquares = numSquares;
    }
  }
  ImGui::SameLine();
  helpMarker("Number of squares in Checkerboard mode");
  ImGui::Spacing();

  // Flashlight
  ImGui::Text("Flashlight:");

  // Flashlight radius
  const float radius = renderData.m_flashlightRadius;
  int radiusPercent = static_cast<int>(100 * radius);
  constexpr int k_minRadius = 1;
  constexpr int k_maxRadius = 100;

  if (ImGui::SliderScalar("Circle size", ImGuiDataType_S32, &radiusPercent, &k_minRadius, &k_maxRadius, "%d")) {
    renderData.m_flashlightRadius = static_cast<float>(radiusPercent) / 100.0f;
  }
  ImGui::SameLine();
  helpMarker("Circle size (as a percentage of the view size) for Flashlight rendering");

  ImGui::Spacing();
  if (ImGui::RadioButton("Overlay moving image atop fixed image", true == renderData.m_flashlightOverlays)) {
    renderData.m_flashlightOverlays = true;
  }

  if (ImGui::RadioButton("Replace fixed image with moving image", false == renderData.m_flashlightOverlays)) {
    renderData.m_flashlightOverlays = false;
  }
  ImGui::SameLine();
  helpMarker("Mode for Flashlight rendering: overlay or replacement");

  //                    ImGui::Separator();
  //                    ImGui::TreePop();
  //                }

  ImGui::PopID(); /*** PopID comparison ***/
  return true;
}

/**
 * @brief Render intensity projection default settings.
 */
void renderIntensityProjectionDefaults(RenderData& renderData)
{
  if (!ImGui::CollapsingHeader("Intensity Projection Defaults", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  disabledTextWrapped(
    "These values are used when intensity projection is enabled from a view overlay. Per-view projection modes are "
    "still selected from the view overlay.");

  ImGui::Spacing();

  bool doMaxExtent = renderData.m_doMaxExtentIntensityProjection;
  if (ImGui::Checkbox("Use maximum image extent", &doMaxExtent)) {
    renderData.m_doMaxExtentIntensityProjection = doMaxExtent;
  }
  ImGui::SameLine();
  helpMarker("Compute maximum, minimum, mean, and x-ray projections over the full image extent");

  const float projectionControlWidth = ImGui::CalcItemWidth();

  if (!renderData.m_doMaxExtentIntensityProjection) {
    float thickness = renderData.m_intensityProjectionSlabThickness;
    ImGui::PushItemWidth(projectionControlWidth);
    if (ImGui::InputFloat("Slab thickness", &thickness, 0.1f, 1.0f, "%0.2f mm")) {
      if (thickness >= 0.0f) {
        renderData.m_intensityProjectionSlabThickness = thickness;
      }
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    helpMarker("Default slab thickness for maximum, minimum, mean, and x-ray projections");
  }

  ImGui::Spacing();
  ImGui::Text("X-ray projection:");

  float energy = renderData.m_xrayEnergyKeV;
  ImGui::PushItemWidth(projectionControlWidth);
  if (ImGui::DragFloat(
        "Energy",
        &energy,
        10.0f,
        1.0f,
        20.0e3f,
        "%0.3f KeV",
        ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic))
  {
    renderData.setXrayEnergy(energy);
  }

  float window = renderData.m_xrayIntensityWindow;
  if (mySliderF32("Width", &window, 1.0e-3f, 1.0f, "%0.3f")) {
    renderData.m_xrayIntensityWindow = window;
  }

  float level = renderData.m_xrayIntensityLevel;
  if (mySliderF32("Level", &level, 0.0f, 1.0f, "%0.3f")) {
    renderData.m_xrayIntensityLevel = level;
  }
  ImGui::PopItemWidth();
}

/**
 * @brief Render the Raycasting settings section contents.
 */
void renderRaycastingTab(RenderData& renderData)
{
  ImGui::PushID("raycasting"); /*** PushID raycasting ***/

  /// @todo if these are added to the uniforms, then we'll have update uniforms when they
  /// change

  static constexpr float k_factorStep = 0.1f;
  static constexpr float k_minFactor = 0.5f;
  static constexpr float k_maxFactor = 2.0f;

  renderData.m_adaptiveRaycastSamplingEnabled = false;
  renderData.m_raycastSamplingFactor = std::clamp(renderData.m_raycastSamplingFactor, k_minFactor, k_maxFactor);
  if (ImGui::DragFloat(
        "Raycast sampling rate",
        &(renderData.m_raycastSamplingFactor),
        k_factorStep,
        k_minFactor,
        k_maxFactor,
        "%0.1f vox",
        ImGuiSliderFlags_AlwaysClamp))
  {
    // Update uniforms if m_raycastSamplingFactor gets added to uniforms
  }
  ImGui::SameLine();
  helpMarker("Sampling rate as a fraction of the voxel size along the ray path");

  ImGui::Spacing();
  ImGui::ColorEdit4("Raycast background color", glm::value_ptr(renderData.m_3dBackgroundColor), k_colorAlphaEditFlags);
  ImGui::SameLine();
  helpMarker("Color used for raycast pixels that do not hit visible image content");

  // Should the no-hit zone of raycast views be transparent, so that the view background is
  // visible?
  ImGui::Checkbox("Transparent background", &renderData.m_3dTransparentIfNoHit);
  ImGui::SameLine();
  helpMarker("Background of view is transparent outside of image volume");

  ImGui::Checkbox("Show image box", &renderData.m_raycastBackgroundEdgeBrighteningEnabled);
  ImGui::SameLine();
  helpMarker("Render a subtle outline of the raycast image box in 3D views");

  // Should the front and back faces be rendered in 3D raycasting?
  ImGui::Spacing();
  ImGui::Checkbox("Render front faces", &renderData.m_renderFrontFaces);
  ImGui::SameLine();
  helpMarker("Render front faces in raycasting");

  ImGui::Checkbox("Render back faces", &renderData.m_renderBackFaces);
  ImGui::SameLine();
  helpMarker("Render back faces in raycasting");

  ImGui::Checkbox("Reverse POV camera rotation", &renderData.m_reverseThreeDRotateAboutEye);
  ImGui::SameLine();
  helpMarker("Reverse drag direction for POV 3D rotation about the current eye position");

  ImGui::Spacing();
  ImGui::Checkbox("Show crosshairs glyph in 3D", &renderData.m_showCrosshairsIn3D);
  ImGui::SameLine();
  helpMarker("Render a small depth-correct sphere at the crosshairs position in 3D raycast views");

  if (renderData.m_showCrosshairsIn3D) {
    ImGui::DragFloat(
      "Glyph diameter",
      &renderData.m_crosshairs3DGlyphDiameterVoxelDiagonals,
      0.05f,
      0.1f,
      10.0f,
      "%0.2f vox",
      ImGuiSliderFlags_AlwaysClamp);
    ImGui::SameLine();
    helpMarker("Sphere diameter as a multiple of the current image voxel diagonal");
  }

  ImGui::Spacing();
  ImGui::Checkbox("Show 3D camera frustum in 2D views", &renderData.m_showThreeDCameraFrustumIn2DViews);
  ImGui::SameLine();
  helpMarker("Show the last-interacted 3D raycast camera eye and frustum footprint in 2D views");

  if (renderData.m_showThreeDCameraFrustumIn2DViews) {
    ImGui::ColorEdit4(
      "Camera frustum line color",
      glm::value_ptr(renderData.m_threeDCameraFrustumColor),
      k_colorAlphaEditFlags);
    ImGui::SameLine();
    helpMarker("Color used for the 3D camera eye dot and frustum lines in 2D views");
  }

  ImGui::Spacing();
  ImGui::Dummy(ImVec2(0.0f, 1.0f));

  ImGui::Text("Masking behavior:");

  if (ImGui::RadioButton("Disable", RenderData::SegMaskingForRaycasting::Disabled == renderData.m_segMasking)) {
    renderData.m_segMasking = RenderData::SegMaskingForRaycasting::Disabled;
  }

  ImGui::SameLine();
  if (ImGui::RadioButton("Mask in", RenderData::SegMaskingForRaycasting::SegMasksIn == renderData.m_segMasking)) {
    renderData.m_segMasking = RenderData::SegMaskingForRaycasting::SegMasksIn;
  }

  ImGui::SameLine();
  if (ImGui::RadioButton("Mask out", RenderData::SegMaskingForRaycasting::SegMasksOut == renderData.m_segMasking)) {
    renderData.m_segMasking = RenderData::SegMaskingForRaycasting::SegMasksOut;
  }
  ImGui::SameLine();
  helpMarker(
    "Controls how the active image segmentation affects 3D raycasting.\n\n"
    "Disable: ignore segmentation values and raycast the full image volume.\n"
    "Mask in: render only samples inside non-background segmentation labels.\n"
    "Mask out: hide samples inside non-background segmentation labels and render the rest of the image.");

  ImGui::PopID(); /*** PopID raycasting ***/
}

/**
 * @brief Render the Rendering settings page contents.
 */
void renderRenderingTab(RenderData& renderData)
{
  RenderData& rd = renderData;

  const bool frameRateOpen = ImGui::CollapsingHeader("Frame Rate", ImGuiTreeNodeFlags_DefaultOpen);
  if (frameRateOpen) {
    ImGui::Checkbox("Limit frame rate", &(renderData.m_manualFramerateLimiter));
    ImGui::SameLine();
    helpMarker("Manually limit the rendering frame rate");

    if (renderData.m_manualFramerateLimiter) {
      constexpr float hzSpeed = 1.0e-1f;
      constexpr double hzMin = 1.0;
      constexpr double hzMax = 240.0;

      constexpr float secSpeed = 1.0e-4f;
      constexpr double secMin = 1.0 / hzMax;
      constexpr double secMax = 1.0 / hzMin;

      double hz = 1.0 / renderData.m_targetFrameTimeSeconds;
      if (ImGui::DragScalar(
            "Frame rate",
            ImGuiDataType_Double,
            &hz,
            hzSpeed,
            &hzMin,
            &hzMax,
            "%.1f Hz",
            ImGuiSliderFlags_ClampOnInput))
      {
        renderData.m_targetFrameTimeSeconds = 1.0 / hz;
      }

      double sec = renderData.m_targetFrameTimeSeconds;
      if (ImGui::DragScalar(
            "Frame period",
            ImGuiDataType_Double,
            &sec,
            secSpeed,
            &secMin,
            &secMax,
            "%.4f sec",
            ImGuiSliderFlags_ClampOnInput))
      {
        renderData.m_targetFrameTimeSeconds = sec;
      }
    }
  }
  finishSettingsSection(frameRateOpen);

  const bool raycastingOpen = ImGui::CollapsingHeader("Raycasting", ImGuiTreeNodeFlags_DefaultOpen);
  if (raycastingOpen) {
    renderRaycastingTab(renderData);
  }
  finishSettingsSection(raycastingOpen);

  const bool isosurfacesOpen = ImGui::CollapsingHeader("Isosurfaces", ImGuiTreeNodeFlags_DefaultOpen);
  if (isosurfacesOpen) {
    ImGui::Checkbox("Floating-point interpolation", &rd.m_isocontourFloatingPointInterpolation);
    ImGui::SameLine();
    helpMarker("Use floating-point instead of 8-bit fixed-point linear image interpolation for isocontours");

    ImGui::Checkbox("Modulate opacity with image", &rd.m_modulateIsocontourOpacityWithImageOpacity);
    ImGui::SameLine();
    helpMarker("Modulate isocontour opacity with image opacity");
  }
  finishSettingsSection(isosurfacesOpen);

  const bool asciiOpen = ImGui::CollapsingHeader("ASCII Shading", ImGuiTreeNodeFlags_DefaultOpen);
  if (asciiOpen) {
    ImGui::PushID("ascii");

    ImGui::Checkbox("Enable ASCII shading", &rd.m_asciiEnabled);
    ImGui::SameLine();
    helpMarker("Render grayscale images as ASCII art");

    if (rd.m_asciiEnabled) {
      static const char* charsetNames[] = {" .,:-=+*#%@  (short)", "Paul Bourke 70-char", "01 (binary)"};
      if (ImGui::Combo("Character set", &rd.m_asciiCharsetIndex, charsetNames, IM_ARRAYSIZE(charsetNames))) {
        // Signal that the atlas needs to be rebuilt — caller checks this flag
        rd.m_asciiAtlasNeedsRebuild = true;
      }

      ImGui::SliderFloat("Cell size", &rd.m_asciiCellSizePx.y, 4.f, 64.f, "%.0f px");
      ImGui::SameLine();
      helpMarker(
        "Size of each ASCII character cell in screen pixels. Larger cells produce coarser, more readable ASCII "
        "shading.");

      ImGui::Checkbox("Use colormap as foreground", &rd.m_asciiUseColormap);
      ImGui::SameLine();
      helpMarker("Replace the foreground color with the image colormap color");

      if (!rd.m_asciiUseColormap) {
        ImGui::ColorEdit3("Foreground color", &rd.m_asciiFgColor.x);
      }
      ImGui::ColorEdit3("Background color", &rd.m_asciiBgColor.x);
      ImGui::SliderFloat("Background alpha", &rd.m_asciiBgAlpha, 0.f, 1.f);

      ImGui::Spacing();
      ImGui::Checkbox("Spatial matching", &rd.m_asciiSpatialMode);
      ImGui::SameLine();
      helpMarker(
        "Choose ASCII characters by comparing a 3 by 2 regional luminance pattern within each cell, rather than using "
        "only the cell's average brightness. This can preserve local structure better, but costs more GPU work.");
      if (rd.m_asciiSpatialMode) {
        ImGui::SliderFloat("Spatial Exponent", &rd.m_asciiSpatialExponent, 0.25f, 4.0f, "%.2f");
        ImGui::SameLine();
        helpMarker(
          "Controls how strongly regional luminance differences influence spatial glyph matching. Higher values favor "
          "sharper local structure; lower values behave more like average-brightness matching.");
      }
    }

    ImGui::PopID(); /*** PopID ascii ***/
  }
}

/**
 * @brief Confirm and then restore built-in application settings.
 * @param persistenceCallbacks File-backed user settings callbacks.
 */
void requestRestoreDefaults(const SettingsPersistenceCallbacks& persistenceCallbacks)
{
  const auto result = native_dialog::showMessageDialog(
    {"Restore default settings?",
     "Restore all application settings to their built-in defaults?",
     "This changes UI, tool, synchronization, logging, and other application preferences. Project settings, loaded "
     "data, layouts, transformations, and warp assignments are not reset. Use Save to write the restored application "
     "settings to disk.",
     "Restore Defaults",
     "Cancel",
     ""});

  if (result && native_dialog::MessageDialogResult::FirstButton == *result) {
    if (persistenceCallbacks.restoreDefaults) {
      persistenceCallbacks.restoreDefaults();
    }
    return;
  }

  if (!result) {
    ImGui::OpenPopup("Restore default settings?");
  }
}

/**
 * @brief Render the ImGui fallback confirmation popup for restoring defaults.
 * @param persistenceCallbacks File-backed user settings callbacks.
 */
void renderRestoreDefaultsPopup(const SettingsPersistenceCallbacks& persistenceCallbacks)
{
  if (ImGui::BeginPopupModal("Restore default settings?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("Restore all application settings to their built-in defaults?");
    ImGui::Spacing();
    ImGui::TextWrapped(
      "This changes UI, tool, synchronization, logging, and other application preferences. Project settings, loaded "
      "data, layouts, transformations, and warp assignments are not reset. Use Save to write the restored application "
      "settings to disk.");
    ImGui::Spacing();

    if (ImGui::Button("Restore Defaults")) {
      if (persistenceCallbacks.restoreDefaults) {
        persistenceCallbacks.restoreDefaults();
      }
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

/**
 * @brief Confirm and then reset ImGui interface state.
 * @param persistenceCallbacks File-backed user settings callbacks.
 */
void requestResetInterfaceSettings(const SettingsPersistenceCallbacks& persistenceCallbacks)
{
  const auto result = native_dialog::showMessageDialog(
    {"Reset UI settings?",
     "Reset saved UI layout and panel settings?",
     "This clears saved window positions, docking layout, table columns, and similar ImGui interface state.",
     "Reset UI Settings",
     "Cancel",
     ""});

  if (result && native_dialog::MessageDialogResult::FirstButton == *result) {
    if (persistenceCallbacks.resetInterfaceSettings) {
      persistenceCallbacks.resetInterfaceSettings();
    }
    return;
  }

  if (!result) {
    ImGui::OpenPopup("Reset UI settings?");
  }
}

/**
 * @brief Render the ImGui fallback confirmation popup for resetting interface state.
 * @param persistenceCallbacks File-backed user settings callbacks.
 */
void renderResetInterfaceSettingsPopup(const SettingsPersistenceCallbacks& persistenceCallbacks)
{
  if (ImGui::BeginPopupModal("Reset UI settings?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("Reset saved UI layout and panel settings?");
    ImGui::Spacing();
    ImGui::TextWrapped(
      "This clears saved window positions, docking layout, table columns, and similar ImGui interface state.");
    ImGui::Spacing();

    if (ImGui::Button("Reset UI Settings")) {
      if (persistenceCallbacks.resetInterfaceSettings) {
        persistenceCallbacks.resetInterfaceSettings();
      }
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

/**
 * @brief Render the Precision settings section contents.
 */
void renderPrecisionTab(AppData& appData)
{
  static constexpr uint32_t k_stepPrecision = 1;

  ImGui::PushID("precision"); /*** PushID precision ***/

  uint32_t valuePrecision = appData.guiData().m_imageValuePrecision;
  uint32_t coordPrecision = appData.guiData().m_coordsPrecision;
  uint32_t txPrecision = appData.guiData().m_txPrecision;
  uint32_t percentilePrecision = appData.guiData().m_percentilePrecision;
  uint32_t timeValuePrecision = appData.guiData().m_timeValuePrecision;

  ImGui::Text("Floating-point precision in user interface:");

  if (ImGui::InputScalar("Image values", ImGuiDataType_U32, &valuePrecision, &k_stepPrecision, &k_stepPrecision, "%d"))
  {
    appData.guiData().m_imageValuePrecision = ui_settings::clampPrecision(valuePrecision);
    appData.guiData().m_imageValuePrecisionFormat =
      ui_settings::precisionFormat(appData.guiData().m_imageValuePrecision);
  }
  ImGui::SameLine();
  helpMarker("Decimal places for displayed image and segmentation values.");

  if (ImGui::InputScalar("Coordinates", ImGuiDataType_U32, &coordPrecision, &k_stepPrecision, &k_stepPrecision, "%d")) {
    appData.guiData().m_coordsPrecision = ui_settings::clampPrecision(coordPrecision);
    appData.guiData().setCoordsPrecisionFormat();
  }
  ImGui::SameLine();
  helpMarker("Decimal places for displayed spatial coordinates and physical length labels.");

  if (ImGui::InputScalar("Transformations", ImGuiDataType_U32, &txPrecision, &k_stepPrecision, &k_stepPrecision, "%d"))
  {
    appData.guiData().m_txPrecision = ui_settings::clampPrecision(txPrecision);
    appData.guiData().setTxPrecisionFormat();
  }
  ImGui::SameLine();
  helpMarker("Decimal places for displayed affine transformation values.");

  if (ImGui::InputScalar(
        "Percentiles",
        ImGuiDataType_U32,
        &percentilePrecision,
        &k_stepPrecision,
        &k_stepPrecision,
        "%d"))
  {
    appData.guiData().m_percentilePrecision = ui_settings::clampPrecision(percentilePrecision);
    appData.guiData().m_percentilePrecisionFormat =
      ui_settings::precisionFormat(appData.guiData().m_percentilePrecision);
  }
  ImGui::SameLine();
  helpMarker("Decimal places for displayed percentile values.");

  if (ImGui::InputScalar(
        "Time values",
        ImGuiDataType_U32,
        &timeValuePrecision,
        &k_stepPrecision,
        &k_stepPrecision,
        "%d"))
  {
    appData.guiData().m_timeValuePrecision = ui_settings::clampPrecision(timeValuePrecision);
    appData.guiData().setTimeValuePrecisionFormat();
  }
  ImGui::SameLine();
  helpMarker("Decimal places for displayed time points, time spacing, and time ranges.");

  ImGui::PopID(); /*** PopID precision ***/
}

} // namespace

/**
 * @brief Render the side navigation and update the selected settings page.
 * @param[in,out] selectedPage Currently selected settings page.
 */
static void renderSettingsNavigation(GuiData::SettingsTab& selectedPage)
{
  for (const ui_settings::SettingsPageChoice& choice : ui_settings::settingsPageChoices()) {
    if (ImGui::Selectable(choice.label, selectedPage == choice.page)) {
      selectedPage = choice.page;
    }
  }
}

float settingsNavigationAutoWidth()
{
  float width = 0.0f;
  for (const ui_settings::SettingsPageChoice& choice : ui_settings::settingsPageChoices()) {
    width = std::max(width, ImGui::CalcTextSize(choice.label).x);
  }

  const ImGuiStyle& style = ImGui::GetStyle();
  return width + (2.0f * style.FramePadding.x) + (2.0f * style.WindowPadding.x);
}

/**
 * @brief Render the selected settings page.
 * @param page Settings page to render.
 * @param appData Application data containing mutable settings.
 * @param renderData Rendering settings.
 * @param getNumImageColorMaps Callback returning the number of color maps.
 * @param getImageColorMap Callback returning a color map by index.
 * @param updateMetricUniforms Callback that refreshes metric rendering uniforms.
 * @param setUiScaleOverride Callback that applies or clears a manual UI scale override.
 * @param requestFontReload Callback that rebuilds ImGui/NanoVG font resources.
 * @param applyUiColorPreset Callback that applies a color preset immediately.
 * @param applyUiDensityPreset Callback that applies a density preset immediately.
 * @param applyUiWindowBgOpacity Callback that applies window background opacity immediately.
 * @param recenterAllViews Callback used by settings that reposition views.
 */
static void renderSettingsPage(
  GuiData::SettingsTab page,
  AppData& appData,
  RenderData& renderData,
  const std::function<size_t(void)>& getNumImageColorMaps,
  const std::function<const ImageColorMap*(std::size_t cmapIndex)>& getImageColorMap,
  const std::function<void(void)>& updateMetricUniforms,
  const std::function<void(std::optional<float> scale)>& setUiScaleOverride,
  const std::function<void()>& requestFontReload,
  const std::function<void(UiColorPreset preset)>& applyUiColorPreset,
  const std::function<void(UiDensityPreset preset)>& applyUiDensityPreset,
  const std::function<void(float opacity)>& applyUiWindowBgOpacity,
  const std::function<void()>& readjustViewport,
  const SettingsPersistenceCallbacks& persistenceCallbacks,
  const AllViewsRecenterType& recenterAllViews)
{
  ImGui::TextUnformatted(ui_settings::settingsPageLabel(page));
  ImGui::Separator();
  ImGui::Spacing();

  switch (page) {
    case GuiData::SettingsTab::Interface:
      renderInterfaceTab(
        appData,
        setUiScaleOverride,
        requestFontReload,
        applyUiColorPreset,
        applyUiDensityPreset,
        applyUiWindowBgOpacity,
        readjustViewport,
        persistenceCallbacks);
      if (ImGui::CollapsingHeader("Precision", ImGuiTreeNodeFlags_DefaultOpen)) {
        renderPrecisionTab(appData);
      }
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();
      renderUiSettingsFileSection(persistenceCallbacks);
      break;
    case GuiData::SettingsTab::Views:
      renderViewsTab(appData, renderData, recenterAllViews);
      break;
    case GuiData::SettingsTab::Annotations:
      renderViewsTab(appData, renderData, recenterAllViews);
      break;
    case GuiData::SettingsTab::Images:
      renderImagesTab(appData);
      break;
    case GuiData::SettingsTab::Segmentation:
      renderSegmentationTab(appData, renderData);
      break;
    case GuiData::SettingsTab::Comparison:
      finishSettingsSection(renderComparisonModesTab(renderData));
      renderMetricsTab(appData, renderData, updateMetricUniforms, getNumImageColorMaps, getImageColorMap);
      break;
    case GuiData::SettingsTab::Rendering:
      renderRenderingTab(renderData);
      break;
    case GuiData::SettingsTab::Registration:
      renderRegistrationTab(appData);
      break;
    case GuiData::SettingsTab::System:
      renderSystemTab(appData);
      break;
    case GuiData::SettingsTab::Synchronization:
      renderSynchronizeTab(appData);
      break;
  }
}

void renderSettingsWindow(
  AppData& appData,
  const std::function<size_t(void)>& getNumImageColorMaps,
  const std::function<const ImageColorMap*(std::size_t cmapIndex)>& getImageColorMap,
  const std::function<void(void)>& updateMetricUniforms,
  const std::function<void(std::optional<float> scale)>& setUiScaleOverride,
  const std::function<void()>& requestFontReload,
  const std::function<void(UiColorPreset preset)>& applyUiColorPreset,
  const std::function<void(UiDensityPreset preset)>& applyUiDensityPreset,
  const std::function<void(float opacity)>& applyUiWindowBgOpacity,
  const std::function<void()>& readjustViewport,
  const SettingsPersistenceCallbacks& persistenceCallbacks,
  const AllViewsRecenterType& recenterAllViews)
{
  static GuiData::SettingsTab s_selectedPage = GuiData::SettingsTab::Views;

  if (appData.guiData().m_requestedSettingsTab) {
    s_selectedPage = *appData.guiData().m_requestedSettingsTab;
    appData.guiData().m_requestedSettingsTab = std::nullopt;
  }

  setNextWindowSizeConstraintsToMainViewport(560.0f, 420.0f);
  ImGui::SetNextWindowSize(ImVec2{760.0f, 560.0f}, ImGuiCond_FirstUseEver);

  const bool settingsDirty = appData.guiData().m_appSettingsDirty;
  ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;
  if (settingsDirty) {
    windowFlags |= ImGuiWindowFlags_UnsavedDocument;
  }
  if (ImGui::Begin("Application Settings", &(appData.guiData().m_showSettingsWindow), windowFlags)) {
    RenderData& renderData = appData.renderData();

    const ImGuiStyle& style = ImGui::GetStyle();
    static constexpr float k_navigationWidth = 156.0f;
    const float footerHeight =
      (2.0f * ImGui::GetFrameHeightWithSpacing()) + style.ItemSpacing.y + style.WindowPadding.y;

    if (ImGui::BeginChild("##SettingsBody", ImVec2{0.0f, -footerHeight}, ImGuiChildFlags_None)) {
      const float navigationWidth = std::max(k_navigationWidth, settingsNavigationAutoWidth());
      constexpr ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings;
      if (ImGui::BeginTable("##SettingsLayout", 2, tableFlags, ImVec2{0.0f, 0.0f})) {
        ImGui::TableSetupColumn("##SettingsNavigationColumn", ImGuiTableColumnFlags_WidthFixed, navigationWidth);
        ImGui::TableSetupColumn("##SettingsPageColumn", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (ImGui::BeginChild("##SettingsNavigation", ImVec2{0.0f, 0.0f}, ImGuiChildFlags_Borders)) {
          renderSettingsNavigation(s_selectedPage);
        }
        ImGui::EndChild();

        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginChild("##SettingsPage", ImVec2{0.0f, 0.0f}, ImGuiChildFlags_Borders)) {
          ImGuiContext& imguiContext = *ImGui::GetCurrentContext();
          const bool settingsEditedBeforePage = imguiContext.ActiveIdHasBeenEditedThisFrame;
          renderSettingsPage(
            s_selectedPage,
            appData,
            renderData,
            getNumImageColorMaps,
            getImageColorMap,
            updateMetricUniforms,
            setUiScaleOverride,
            requestFontReload,
            applyUiColorPreset,
            applyUiDensityPreset,
            applyUiWindowBgOpacity,
            readjustViewport,
            persistenceCallbacks,
            recenterAllViews);
          if (!settingsEditedBeforePage && imguiContext.ActiveIdHasBeenEditedThisFrame) {
            appData.guiData().m_appSettingsDirty = true;
          }
        }
        ImGui::EndChild();

        ImGui::EndTable();
      }
    }
    ImGui::EndChild();

    if (ImGui::BeginChild("##SettingsFooter", ImVec2{0.0f, 0.0f}, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) {
      ImGui::Separator();
      renderReadOnlyPathField("Application settings file", persistenceCallbacks.settingsFile);

      if (const std::string statusText =
            persistenceCallbacks.statusText ? persistenceCallbacks.statusText() : std::string{};
          !statusText.empty())
      {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", statusText.c_str());
      }

      const float buttonY = std::max(ImGui::GetCursorPosY(), ImGui::GetContentRegionMax().y - ImGui::GetFrameHeight());
      ImGui::SetCursorPosY(buttonY);
      if (ImGui::Button("Restore Defaults")) {
        requestRestoreDefaults(persistenceCallbacks);
      }
      ImGui::SameLine();
      if (ImGui::Button("Save")) {
        if (persistenceCallbacks.saveSettings) {
          persistenceCallbacks.saveSettings();
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Save As...")) {
        const std::filesystem::path defaultDirectory = persistenceCallbacks.settingsFile.parent_path();
        const std::string defaultName = persistenceCallbacks.settingsFile.filename().empty()
                                          ? std::string{"entropy-settings.json"}
                                          : persistenceCallbacks.settingsFile.filename().string();
        if (
          const auto selectedFile =
            native_dialog::saveFile({native_dialog::Filter{"Entropy settings", "json"}}, defaultDirectory, defaultName))
        {
          if (persistenceCallbacks.saveSettingsAs) {
            persistenceCallbacks.saveSettingsAs(*selectedFile);
          }
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Close")) {
        appData.guiData().m_showSettingsWindow = false;
      }
    }
    ImGui::EndChild();

    renderRestoreDefaultsPopup(persistenceCallbacks);
    renderResetInterfaceSettingsPopup(persistenceCallbacks);
  }

  ImGui::End();
}

// enum class PopupWindowPosition
//{
//     Custom,
//     TopLeft,
//     TopRight,
//     BottomLeft,
//     BottomRight
// };
