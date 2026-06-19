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

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ui_settings = entropy::ui::settings;

namespace
{

static constexpr bool k_recenterCrosshairs = true;
static constexpr bool k_realignCrosshairs = true;
static constexpr bool k_doNotRecenterOnCurrentCrosshairsPosition = false;
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

/**
 * @brief Render a copyable, read-only path field.
 * @param[in] label Field label.
 * @param[in] path Path text shown in the field.
 */
void renderReadOnlyPathField(const char* label, const std::filesystem::path& path)
{
  std::string text = canonicalDisplayPath(path).string();
  std::vector<char> buffer(text.begin(), text.end());
  buffer.push_back('\0');

  ImGui::PushItemWidth(480.0f);
  ImGui::InputText(label, buffer.data(), buffer.size(), ImGuiInputTextFlags_ReadOnly);
  ImGui::PopItemWidth();
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

  ImGui::Spacing();
  renderReadOnlyPathField("Application log", app_paths::logDirectory() / "entropy.txt");
  renderReadOnlyPathField("ImGui log", app_paths::logDirectory() / "entropy_ui.log");
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
  const std::function<const ImageColorMap*(std::size_t cmapIndex)>& getImageColorMap)
{
  // Metric windowing range slider:
  const float slope = metricParams.m_slopeIntercept[0];
  const float xcept = metricParams.m_slopeIntercept[1];

  const float windowWidth = glm::clamp(1.0f / slope, 0.0f, 1.0f);
  const float windowCenter = glm::clamp((0.5f - xcept) / slope, 0.0f, 1.0f);

  float windowLow = std::max(windowCenter - 0.5f * windowWidth, 0.0f);
  float windowHigh = std::min(windowCenter + 0.5f * windowWidth, 1.0f);

  if (ImGui::DragFloatRange2(
        "Window",
        &windowLow,
        &windowHigh,
        0.01f,
        k_windowMin,
        k_windowMax,
        "Min: %.2f",
        "Max: %.2f",
        ImGuiSliderFlags_AlwaysClamp))
  {
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
  helpMarker("Minimum and maximum of the metric window range");

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

/**
 * @brief Render the Views settings page contents.
 */
void renderViewsTab(AppData& appData, RenderData& renderData, const AllViewsRecenterType& recenterAllViews)
{
  // Show image-view intersection border
  ImGui::Checkbox(
    "Show image borders",
    &(renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections));
  ImGui::SameLine();
  helpMarker("Show borders of image intersections with views");

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
  ImGui::Dummy(ImVec2(0.0f, 1.0f));

  // Crosshairs
  if (ImGui::CollapsingHeader("Crosshairs", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::ColorEdit4("Color", glm::value_ptr(renderData.m_crosshairsColor), k_colorAlphaEditFlags);

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

  // View centering:
  if (ImGui::CollapsingHeader("View recentering", ImGuiTreeNodeFlags_DefaultOpen)) {
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

  // View backgrounds:
  if (ImGui::CollapsingHeader("Background color", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::ColorEdit3("2D background color", glm::value_ptr(renderData.m_2dBackgroundColor), k_colorEditFlags);

    ImGui::ColorEdit4("3D background color", glm::value_ptr(renderData.m_3dBackgroundColor), k_colorAlphaEditFlags);
  }

  // Anatomical labels:
  if (ImGui::CollapsingHeader("Anatomical labels", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::ColorEdit4("Text color", glm::value_ptr(renderData.m_anatomicalLabelColor), k_colorAlphaEditFlags);
    ImGui::Dummy(ImVec2(0.0f, 1.0f));

    ImGui::Text("Anatomical directions:");

    if (ImGui::RadioButton("Human", AnatomicalLabelType::Human == renderData.m_anatomicalLabelType)) {
      renderData.m_anatomicalLabelType = AnatomicalLabelType::Human;
    }
    ImGui::SameLine();
    helpMarker("Left, Right, Posterior, Anterior, Superior, Inferior");

    if (ImGui::RadioButton("Cartesian", AnatomicalLabelType::Cartesian == renderData.m_anatomicalLabelType)) {
      renderData.m_anatomicalLabelType = AnatomicalLabelType::Cartesian;
    }
    ImGui::SameLine();
    helpMarker("+x, -x, +y, -y, +z, -z");

    if (ImGui::RadioButton("Rodent", AnatomicalLabelType::Rodent == renderData.m_anatomicalLabelType)) {
      renderData.m_anatomicalLabelType = AnatomicalLabelType::Rodent;
    }
    ImGui::SameLine();
    helpMarker("Left, Right, Dorsal, Ventral, Caudal, Rostral");

    if (ImGui::RadioButton("Disabled", AnatomicalLabelType::Disabled == renderData.m_anatomicalLabelType)) {
      renderData.m_anatomicalLabelType = AnatomicalLabelType::Disabled;
    }
    ImGui::SameLine();
    helpMarker("Disable anatomical labels");

    ImGui::Dummy(ImVec2(0.0f, 1.0f));

    ImGui::Text("View orientation convention:");

    static constexpr bool kOrientChangeRecenterCrosshairs = false;
    static constexpr bool kOrientChangeRealignCrosshairs = false;
    static constexpr bool kOrientChangeRecenterOnXhairs = true;
    static constexpr bool kOrientChangeResetObliqueOrientation = false;
    static constexpr bool kOrientChangeResetZoom = false;

    if (ImGui::RadioButton(
          "Radiological",
          ViewConvention::Radiological == appData.windowData().getViewOrientationConvention()))
    {
      appData.windowData().setViewOrientationConvention(ViewConvention::Radiological);

      recenterAllViews(
        kOrientChangeRecenterCrosshairs,
        kOrientChangeRealignCrosshairs,
        kOrientChangeRecenterOnXhairs,
        kOrientChangeResetObliqueOrientation,
        kOrientChangeResetZoom);
    }
    ImGui::SameLine();
    helpMarker("Anatomical left is on view right; anatomical right is on view left");

    if (ImGui::RadioButton(
          "Neurological",
          ViewConvention::Neurological == appData.windowData().getViewOrientationConvention()))
    {
      appData.windowData().setViewOrientationConvention(ViewConvention::Neurological);

      recenterAllViews(
        kOrientChangeRecenterCrosshairs,
        kOrientChangeRealignCrosshairs,
        kOrientChangeRecenterOnXhairs,
        kOrientChangeResetObliqueOrientation,
        kOrientChangeResetZoom);
    }
    ImGui::SameLine();
    helpMarker("Anatomical left is on view left; anatomical right is on view right");
  }

  if (ImGui::CollapsingHeader("Scale bars", ImGuiTreeNodeFlags_DefaultOpen)) {
    bool showScaleBars = renderData.m_showScaleBars;
    if (ImGui::Checkbox("Show scale bars", &showScaleBars)) {
      renderData.m_showScaleBars = showScaleBars;
    }
    ImGui::SameLine();
    helpMarker("Show physical scale bars on anatomical views");

    if (showScaleBars) {
      bool showScaleBarsInLightboxViews = renderData.m_showScaleBarsInLightboxViews;
      if (ImGui::Checkbox("Show in lightbox views", &showScaleBarsInLightboxViews)) {
        renderData.m_showScaleBarsInLightboxViews = showScaleBarsInLightboxViews;
      }
      ImGui::SameLine();
      helpMarker("Show scale bars in each tile of lightbox layouts");

      ImGui::ColorEdit4("Color", glm::value_ptr(renderData.m_scaleBarColor), k_colorAlphaEditFlags);

      auto setScaleBarPosition = [&](ScaleBarPosition position) {
        renderData.m_scaleBarPosition = position;
        renderData.m_scaleBarOrientation =
          ui_settings::normalizedScaleBarOrientation(position, renderData.m_scaleBarOrientation);
      };

      ImGui::Spacing();
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
      ImGui::PushItemWidth(180);
      if (ImGui::SliderInt("Target length", &targetLengthPercent, 5, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp)) {
        renderData.m_scaleBarTargetFraction = ui_settings::targetLengthFractionFromPercent(targetLengthPercent);
      }
      ImGui::PopItemWidth();
      ImGui::SameLine();
      helpMarker(
        "Approximate fraction of the view occupied by the scale bar before rounding to a clean physical length");

      int scaleBarMarginPx = ui_settings::marginPixelsFromFloat(renderData.m_scaleBarMarginPx);
      ImGui::PushItemWidth(180);
      if (ImGui::SliderInt("Margin", &scaleBarMarginPx, 12, 96, "%d px", ImGuiSliderFlags_AlwaysClamp)) {
        renderData.m_scaleBarMarginPx = ui_settings::marginFloatFromPixels(scaleBarMarginPx);
      }
      ImGui::PopItemWidth();
      ImGui::SameLine();
      helpMarker("Offset scale bars away from view edges and corners");

      ImGui::Spacing();
      ImGui::Text("Ticks:");
      if (ImGui::RadioButton("Endpoints##scaleBarTicks", ScaleBarTicks::Endpoints == renderData.m_scaleBarTicks)) {
        renderData.m_scaleBarTicks = ScaleBarTicks::Endpoints;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton(
            "Automatic extra ticks##scaleBarTicks",
            ScaleBarTicks::Automatic == renderData.m_scaleBarTicks))
      {
        renderData.m_scaleBarTicks = ScaleBarTicks::Automatic;
      }
      ImGui::SameLine();
      helpMarker("Add evenly spaced extra ticks when the scale bar is long enough for readable subdivisions");
    }
  }

  if (ImGui::CollapsingHeader("Lightbox views", ImGuiTreeNodeFlags_DefaultOpen)) {
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
  const std::function<void()>& readjustViewport)
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
    if (ImGui::RadioButton("Bottom##layoutTabBarPosition", !layoutTabsTop)) {
      appData.settings().setLayoutTabPlacement(UiLayoutTabPlacement::Bottom);
      appData.guiData().m_layoutTabPlacement = guiLayoutTabPlacement(appData.settings().layoutTabPlacement());
      if (readjustViewport) {
        readjustViewport();
      }
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  if (ImGui::CollapsingHeader("Look and feel", ImGuiTreeNodeFlags_DefaultOpen)) {
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
}

/**
 * @brief Render the System settings page contents.
 */
void renderSystemTab(AppData& appData)
{
  if (ImGui::CollapsingHeader("Diagnostics", ImGuiTreeNodeFlags_DefaultOpen)) {
    renderDiagnosticsSettings();
    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader("Developer tools")) {
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
  if (ImGui::CollapsingHeader("Image display defaults", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox(
      "Floating-point linear image interpolation",
      &appData.renderData().m_imageGrayFloatingPointInterpolation);
    ImGui::SameLine();
    helpMarker("Use floating-point instead of 8-bit fixed-point linear interpolation for grayscale images");
  }

  renderIntensityProjectionDefaults(appData.renderData());
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

  ImGui::Spacing();

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

/**
 * @brief Render the Segmentation settings page contents.
 */
void renderSegmentationTab(AppData& appData, RenderData& renderData)
{
  if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen)) {
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

  if (ImGui::CollapsingHeader("Brush")) {
    AppSettings& settings = appData.settings();

    bool useRound = settings.useRoundBrush();
    if (ImGui::RadioButton("Round", useRound)) {
      settings.setUseRoundBrush(true);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Square", !useRound)) {
      settings.setUseRoundBrush(false);
    }
    ImGui::SameLine();
    helpMarker("Default segmentation brush shape");

    bool use3d = settings.use3dBrush();
    if (ImGui::RadioButton("2D", !use3d)) {
      settings.setUse3dBrush(false);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("3D", use3d)) {
      settings.setUse3dBrush(true);
    }
    ImGui::SameLine();
    helpMarker("Default segmentation brush dimensionality");

    bool useIso = settings.useIsotropicBrush();
    if (ImGui::Checkbox("Isotropic brush", &useIso)) {
      settings.setUseIsotropicBrush(useIso);
    }

    bool replaceBgWithFg = settings.replaceBackgroundWithForeground();
    if (ImGui::Checkbox("Replace background with foreground", &replaceBgWithFg)) {
      settings.setReplaceBackgroundWithForeground(replaceBgWithFg);
    }

    bool xhairsMove = settings.crosshairsMoveWithBrush();
    if (ImGui::Checkbox("Crosshairs move with brush", &xhairsMove)) {
      settings.setCrosshairsMoveWithBrush(xhairsMove);
    }
  }

  if (ImGui::CollapsingHeader("Brush preview")) {
    AppSettings& settings = appData.settings();
    BrushPreviewMode previewMode = settings.brushPreviewMode();

    if (ImGui::RadioButton("Hover##brushPreview", BrushPreviewMode::Hover == previewMode)) {
      settings.setBrushPreviewMode(BrushPreviewMode::Hover);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Off##brushPreview", BrushPreviewMode::Disabled == previewMode)) {
      settings.setBrushPreviewMode(BrushPreviewMode::Disabled);
    }
    ImGui::SameLine();
    helpMarker("Show the voxels that the brush would affect");

    if (BrushPreviewMode::Disabled != settings.brushPreviewMode()) {
      BrushPreviewVoxels previewVoxels = settings.brushPreviewVoxels();
      if (ImGui::RadioButton("Changed voxels##brushPreviewVoxels", BrushPreviewVoxels::Changed == previewVoxels)) {
        settings.setBrushPreviewVoxels(BrushPreviewVoxels::Changed);
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("All voxels##brushPreviewVoxels", BrushPreviewVoxels::All == previewVoxels)) {
        settings.setBrushPreviewVoxels(BrushPreviewVoxels::All);
      }

      BrushPreviewStyle previewStyle = settings.brushPreviewStyle();
      if (ImGui::RadioButton("Outline##brushPreviewStyle", BrushPreviewStyle::Outline == previewStyle)) {
        settings.setBrushPreviewStyle(BrushPreviewStyle::Outline);
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("Outline + fill##brushPreviewStyle", BrushPreviewStyle::OutlineAndFill == previewStyle)) {
        settings.setBrushPreviewStyle(BrushPreviewStyle::OutlineAndFill);
      }

      if (BrushPreviewStyle::OutlineAndFill == settings.brushPreviewStyle()) {
        float previewFillOpacityPercent = 100.0f * settings.brushPreviewFillOpacity();
        ImGui::PushItemWidth(150);
        if (ImGui::SliderFloat(
              "Fill opacity##brushPreviewFillOpacity",
              &previewFillOpacityPercent,
              0.0f,
              100.0f,
              "%.0f%%"))
        {
          settings.setBrushPreviewFillOpacity(previewFillOpacityPercent / 100.0f);
        }
        ImGui::PopItemWidth();
      }

      bool previewWhilePainting = settings.brushPreviewWhilePainting();
      if (ImGui::Checkbox("Show while painting##brushPreviewWhilePainting", &previewWhilePainting)) {
        settings.setBrushPreviewWhilePainting(previewWhilePainting);
      }

      SegmentationOutlineStyle previewOutlineStyle = settings.brushPreviewOutlineStyle();
      if (ImGui::RadioButton(
            "Pixel outline##brushPreviewOutlineStyle",
            SegmentationOutlineStyle::ViewPixel == previewOutlineStyle))
      {
        settings.setBrushPreviewOutlineStyle(SegmentationOutlineStyle::ViewPixel);
      }
      ImGui::SameLine();
      if (ImGui::RadioButton(
            "Voxel outline##brushPreviewOutlineStyle",
            SegmentationOutlineStyle::ImageVoxel == previewOutlineStyle))
      {
        settings.setBrushPreviewOutlineStyle(SegmentationOutlineStyle::ImageVoxel);
      }
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
  {
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

  /*
  ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
  if (ImGui::TreeNode("Cross-correlation"))
  {
    ImGui::PushID("crosscorr");
    {
      renderMetricSettingsPanel(
        renderData.m_crossCorrelationParams,
        appData.guiData().m_showCorrelationColormapWindow,
        "crosscorr"
      );
    }
    ImGui::PopID(); // "crosscorr"
    ImGui::TreePop();
  }
  */

  ImGui::PopID(); /*** PopID metrics ***/
}

/**
 * @brief Render the Comparison modes settings section contents.
 */
void renderComparisonModesTab(RenderData& renderData)
{
  ImGui::PushID("comparison"); /*** PushID metrics ***/

  //                if ( ImGui::TreeNode( "Comparison comparison" ) )
  //                {
  // Overlap style:
  ImGui::Text("Overlap:");

  if (ImGui::RadioButton("Cyan, magenta, white", true == renderData.m_overlayMagentaCyan)) {
    renderData.m_overlayMagentaCyan = true;
  }
  if (ImGui::RadioButton("Red, green, yellow", false == renderData.m_overlayMagentaCyan)) {
    renderData.m_overlayMagentaCyan = false;
  }

  ImGui::SameLine();
  helpMarker("Color style for 'overlay' views");
  ImGui::Spacing();
  ImGui::Separator();

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
  ImGui::Separator();

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
  ImGui::Separator();

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
}

/**
 * @brief Render intensity projection default settings.
 */
void renderIntensityProjectionDefaults(RenderData& renderData)
{
  if (!ImGui::CollapsingHeader("Intensity projection defaults", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  ImGui::TextWrapped(
    "These values are used when intensity projection is enabled from a view overlay. Per-view projection modes are "
    "still selected from the view overlay.");

  ImGui::Spacing();

  bool doMaxExtent = renderData.m_doMaxExtentIntensityProjection;
  if (ImGui::Checkbox("Use maximum image extent", &doMaxExtent)) {
    renderData.m_doMaxExtentIntensityProjection = doMaxExtent;
  }
  ImGui::SameLine();
  helpMarker("Compute maximum, minimum, mean, and x-ray projections over the full image extent");

  if (!renderData.m_doMaxExtentIntensityProjection) {
    float thickness = renderData.m_intensityProjectionSlabThickness;
    ImGui::PushItemWidth(180.0f);
    if (ImGui::InputFloat("Slab thickness (mm)", &thickness, 0.1f, 1.0f, "%0.2f")) {
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
  ImGui::PushItemWidth(180.0f);
  if (ImGui::DragFloat(
        "Energy",
        &energy,
        10.0f,
        1.0f,
        20.0e3f,
        "%0.3f KeV",
        ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic))
  {
    renderData.setXrayEnergy(energy / 1000.0f);
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
  static constexpr float k_minFactor = 0.1f;
  static constexpr float k_maxFactor = 5.0f;

  ImGui::Text("Raycasting sampling rate:");
  ImGui::SameLine();
  helpMarker("Sampling rate as a fraction of the voxel size along the ray path");

  if (ImGui::DragFloat(
        "##SamplingRate",
        &(renderData.m_raycastSamplingFactor),
        k_factorStep,
        k_minFactor,
        k_maxFactor,
        "%0.1f",
        ImGuiSliderFlags_AlwaysClamp))
  {
    // Update uniforms if m_raycastSamplingFactor gets added to uniforms
  }

  ImGui::Spacing();
  ImGui::Dummy(ImVec2(0.0f, 1.0f));

  // Should the no-hit zone of raycast views be transparent, so that the view background is
  // visible?
  ImGui::Checkbox("Transparent background", &renderData.m_3dTransparentIfNoHit);
  ImGui::SameLine();
  helpMarker("Background of view is transparent outside of image volume");

  // Should the front and back faces be rendered in 3D raycasting?
  ImGui::Checkbox("Render front faces", &renderData.m_renderFrontFaces);
  ImGui::SameLine();
  helpMarker("Render front faces in raycasting");

  ImGui::Checkbox("Render back faces", &renderData.m_renderBackFaces);
  ImGui::SameLine();
  helpMarker("Render back faces in raycasting");

  ImGui::Spacing();
  ImGui::Dummy(ImVec2(0.0f, 1.0f));

  ImGui::Text("Masking behavior:");

  ImGui::SameLine();
  helpMarker("Mask image based on segmentation value");

  if (ImGui::RadioButton("Disable", RenderData::SegMaskingForRaycasting::Disabled == renderData.m_segMasking)) {
    renderData.m_segMasking = RenderData::SegMaskingForRaycasting::Disabled;
  }
  ImGui::SameLine();
  helpMarker("Segmentation masking disabled");

  if (ImGui::RadioButton("Mask in", RenderData::SegMaskingForRaycasting::SegMasksIn == renderData.m_segMasking)) {
    renderData.m_segMasking = RenderData::SegMaskingForRaycasting::SegMasksIn;
  }
  ImGui::SameLine();
  helpMarker("Segmentation masks image in");

  if (ImGui::RadioButton("Mask out", RenderData::SegMaskingForRaycasting::SegMasksOut == renderData.m_segMasking)) {
    renderData.m_segMasking = RenderData::SegMaskingForRaycasting::SegMasksOut;
  }
  ImGui::SameLine();
  helpMarker("Segmentation masks image out");

  ImGui::PopID(); /*** PopID raycasting ***/
}

/**
 * @brief Render the Rendering settings page contents.
 */
void renderRenderingTab(RenderData& renderData)
{
  ImGui::Checkbox("Limit frame rate", &(renderData.m_manualFramerateLimiter));
  ImGui::SameLine();
  helpMarker("Manually limit the rendering frame rate");

  RenderData& rd = renderData;
  ImGui::Checkbox("Enable ASCII shading", &rd.m_asciiEnabled);
  ImGui::SameLine();
  helpMarker("Render grayscale images as ASCII art");

  if (renderData.m_manualFramerateLimiter) {
    constexpr float hzSpeed = 1.0e-1f;
    constexpr double hzMin = 1.0;
    constexpr double hzMax = 240.0;

    constexpr float secSpeed = 1.0e-4f;
    constexpr double secMin = 1.0 / hzMax;
    constexpr double secMax = 1.0 / hzMin;

    double hz = 1.0 / renderData.m_targetFrameTimeSeconds;
    if (ImGui::DragScalar(
          "Hz",
          ImGuiDataType_Double,
          &hz,
          hzSpeed,
          &hzMin,
          &hzMax,
          "%.1f",
          ImGuiSliderFlags_ClampOnInput))
    {
      renderData.m_targetFrameTimeSeconds = 1.0 / hz;
    }

    double sec = renderData.m_targetFrameTimeSeconds;
    if (ImGui::DragScalar(
          "sec",
          ImGuiDataType_Double,
          &sec,
          secSpeed,
          &secMin,
          &secMax,
          "%.4f",
          ImGuiSliderFlags_ClampOnInput))
    {
      renderData.m_targetFrameTimeSeconds = sec;
    }
  }

  if (ImGui::CollapsingHeader("Raycasting")) {
    renderRaycastingTab(renderData);
  }

  // ASCII rendering controls
  if (ImGui::CollapsingHeader("ASCII shading")) {
    ImGui::PushID("ascii");

    if (rd.m_asciiEnabled) {
      static const char* charsetNames[] = {" .,:-=+*#%@  (short)", "Paul Bourke 70-char", "01 (binary)"};
      if (ImGui::Combo("Charset", &rd.m_asciiCharsetIndex, charsetNames, IM_ARRAYSIZE(charsetNames))) {
        // Signal that the atlas needs to be rebuilt — caller checks this flag
        rd.m_asciiAtlasNeedsRebuild = true;
      }

      ImGui::SliderFloat("Cell size (px)", &rd.m_asciiCellSizePx.y, 4.f, 64.f, "%.0f");

      ImGui::Checkbox("Use colormap as foreground", &rd.m_asciiUseColormap);
      ImGui::SameLine();
      helpMarker("Replace the foreground color with the image colormap color");

      if (!rd.m_asciiUseColormap) {
        ImGui::ColorEdit3("Foreground color", &rd.m_asciiFgColor.x);
      }
      ImGui::ColorEdit3("Background color", &rd.m_asciiBgColor.x);
      ImGui::SliderFloat("Background alpha", &rd.m_asciiBgAlpha, 0.f, 1.f);

      ImGui::Checkbox("Spatial matching (3x2)", &rd.m_asciiSpatialMode);
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Match glyphs by a 3x2 region luminance profile instead of total brightness");
      if (rd.m_asciiSpatialMode)
        ImGui::SliderFloat("Spatial Exponent", &rd.m_asciiSpatialExponent, 0.25f, 4.0f, "%.2f");
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
     "This changes the current session immediately. Use Save to write the restored settings to disk.",
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
      "This changes the current session immediately. Use Save to write the restored settings to disk.");
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
 * @brief Render the Annotations settings page contents.
 */
void renderAnnotationsTab(RenderData& renderData)
{
  ImGui::PushID("landmarks"); /*** PushID landmarks ***/

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

  ImGui::PopID(); /*** PopID landmarks ***/
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

  ImGui::Text("Floating-point precision in user interface:");

  if (ImGui::InputScalar("Image values", ImGuiDataType_U32, &valuePrecision, &k_stepPrecision, &k_stepPrecision, "%d"))
  {
    appData.guiData().m_imageValuePrecision = ui_settings::clampPrecision(valuePrecision);
    appData.guiData().m_imageValuePrecisionFormat =
      ui_settings::precisionFormat(appData.guiData().m_imageValuePrecision);
  }
  ImGui::SameLine();
  helpMarker("Floating-point precision of image values (e.g. in Inspector window)");

  if (ImGui::InputScalar("Coordinates", ImGuiDataType_U32, &coordPrecision, &k_stepPrecision, &k_stepPrecision, "%d")) {
    appData.guiData().m_coordsPrecision = ui_settings::clampPrecision(coordPrecision);
    appData.guiData().setCoordsPrecisionFormat();
  }
  ImGui::SameLine();
  helpMarker("Floating-point precision of image spatial coordinates (e.g. in Inspector window)");

  if (ImGui::InputScalar("Transformations", ImGuiDataType_U32, &txPrecision, &k_stepPrecision, &k_stepPrecision, "%d"))
  {
    appData.guiData().m_txPrecision = ui_settings::clampPrecision(txPrecision);
    appData.guiData().setTxPrecisionFormat();
  }
  ImGui::SameLine();
  helpMarker("Floating-point precision of image transformation parameters");

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
  helpMarker("Floating-point precision of percentiles (e.g. in histogram)");

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
        readjustViewport);
      if (ImGui::CollapsingHeader("Precision", ImGuiTreeNodeFlags_DefaultOpen)) {
        renderPrecisionTab(appData);
      }
      break;
    case GuiData::SettingsTab::Views:
      renderViewsTab(appData, renderData, recenterAllViews);
      break;
    case GuiData::SettingsTab::Images:
      renderImagesTab(appData);
      break;
    case GuiData::SettingsTab::Segmentation:
      renderSegmentationTab(appData, renderData);
      break;
    case GuiData::SettingsTab::Comparison:
      renderMetricsTab(appData, renderData, updateMetricUniforms, getNumImageColorMaps, getImageColorMap);
      renderComparisonModesTab(renderData);
      break;
    case GuiData::SettingsTab::Rendering:
      renderRenderingTab(renderData);
      break;
    case GuiData::SettingsTab::Annotations:
      renderAnnotationsTab(renderData);
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

  if (ImGui::Begin("Settings", &(appData.guiData().m_showSettingsWindow))) {
    RenderData& renderData = appData.renderData();

    const ImGuiStyle& style = ImGui::GetStyle();
    static constexpr float k_navigationWidth = 156.0f;
    const float footerHeight =
      (2.0f * ImGui::GetFrameHeightWithSpacing()) + style.ItemSpacing.y + style.WindowPadding.y;

    if (ImGui::BeginChild("##SettingsBody", ImVec2{0.0f, -footerHeight}, ImGuiChildFlags_None)) {
      if (ImGui::BeginChild("##SettingsNavigation", ImVec2{k_navigationWidth, 0.0f}, ImGuiChildFlags_Borders)) {
        renderSettingsNavigation(s_selectedPage);
      }
      ImGui::EndChild();

      ImGui::SameLine();

      if (ImGui::BeginChild("##SettingsPage", ImVec2{0.0f, 0.0f}, ImGuiChildFlags_Borders)) {
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
          recenterAllViews);
      }
      ImGui::EndChild();
    }
    ImGui::EndChild();

    if (ImGui::BeginChild("##SettingsFooter", ImVec2{0.0f, 0.0f}, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) {
      ImGui::Separator();
      renderReadOnlyPathField("Settings file", persistenceCallbacks.settingsFile);

      if (const std::string statusText =
            persistenceCallbacks.statusText ? persistenceCallbacks.statusText() : std::string{};
          !statusText.empty())
      {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", statusText.c_str());
      }

      const float buttonY = std::max(ImGui::GetCursorPosY(), ImGui::GetContentRegionMax().y - ImGui::GetFrameHeight());
      ImGui::SetCursorPosY(buttonY);
      if (ImGui::Button("Close")) {
        appData.guiData().m_showSettingsWindow = false;
      }

      ImGui::SameLine();
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
    }
    ImGui::EndChild();

    renderRestoreDefaultsPopup(persistenceCallbacks);
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
