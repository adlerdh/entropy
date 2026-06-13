#include "ui/windows/SettingsWindow.h"

#include "ui/Helpers.h"
#include "ui/ImGuiCustomControls.h"
#include "ui/Style.h"
#include "ui/Widgets.h"
#include "ui/settings/SettingsModel.h"

#include "logic/app/Data.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <string>

namespace ui_settings = entropy::ui::settings;

namespace
{

static constexpr bool sk_recenterCrosshairs = true;
static constexpr bool sk_realignCrosshairs = true;
static constexpr bool sk_doNotRecenterOnCurrentCrosshairsPosition = false;
static constexpr bool sk_doNotResetObliqueOrientation = false;
static constexpr bool sk_resetZoom = true;

static constexpr float sk_windowMin = 0.0f;
static constexpr float sk_windowMax = 1.0f;

static constexpr ImGuiColorEditFlags sk_colorEditFlags =
  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_DisplayRGB |
  ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_InputRGB;

static constexpr ImGuiColorEditFlags sk_colorAlphaEditFlags =
  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_DisplayRGB |
  ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf |
  ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_InputRGB;

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
        sk_windowMin,
        sk_windowMax,
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
 * @brief Render the Views settings tab contents.
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
  ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);

  if (ImGui::TreeNode("Crosshairs")) {
    ImGui::ColorEdit4("Color", glm::value_ptr(renderData.m_crosshairsColor), sk_colorAlphaEditFlags);

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

    ImGui::Spacing();
    ImGui::TreePop();
  }

  // View centering:
  ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);

  if (ImGui::TreeNode("View Recentering")) {
    ImGui::Text("Center views and crosshairs on:");

    ImGui::SameLine();
    helpMarker("Default view and crosshairs centering behavior");

    if (ImGui::RadioButton("Reference image", ImageSelection::ReferenceImage == appData.state().recenteringMode())) {
      appData.state().setRecenteringMode(ImageSelection::ReferenceImage);

      recenterAllViews(
        sk_recenterCrosshairs,
        sk_realignCrosshairs,
        sk_doNotRecenterOnCurrentCrosshairsPosition,
        sk_doNotResetObliqueOrientation,
        sk_resetZoom);
    }
    ImGui::SameLine();
    helpMarker("Recenter views and crosshairs on the reference image");

    if (ImGui::RadioButton("Active image", ImageSelection::ActiveImage == appData.state().recenteringMode())) {
      appData.state().setRecenteringMode(ImageSelection::ActiveImage);

      recenterAllViews(
        sk_recenterCrosshairs,
        sk_realignCrosshairs,
        sk_doNotRecenterOnCurrentCrosshairsPosition,
        sk_doNotResetObliqueOrientation,
        sk_resetZoom);
    }
    ImGui::SameLine();
    helpMarker("Recenter views and crosshairs on the active image");

    if (ImGui::RadioButton(
          "Reference and active images",
          ImageSelection::ReferenceAndActiveImages == appData.state().recenteringMode()))
    {
      appData.state().setRecenteringMode(ImageSelection::ReferenceAndActiveImages);

      recenterAllViews(
        sk_recenterCrosshairs,
        sk_realignCrosshairs,
        sk_doNotRecenterOnCurrentCrosshairsPosition,
        sk_doNotResetObliqueOrientation,
        sk_resetZoom);
    }
    ImGui::SameLine();
    helpMarker("Recenter views and crosshairs on the reference and active images");

    /// @todo These don't work yet
    /*
              if ( ImGui::RadioButton( "All visible images",
       ImageSelection::VisibleImagesInView == appData.state().recenteringMode() ) )
              {
                  appData.state().setRecenteringMode( ImageSelection::VisibleImagesInView );
                  recenterAllViews( sk_recenterCrosshairs,
       sk_doNotRecenterOnCurrentCrosshairsPosition );
              }
              ImGui::SameLine(); helpMarker( "Recenter views and crosshairs on the visible
       images in each view" );

              if ( ImGui::RadioButton( "Fixed image", ImageSelection::FixedImageInView ==
       appData.state().recenteringMode() ) )
              {
                  appData.state().setRecenteringMode( ImageSelection::FixedImageInView );
                  recenterAllViews( sk_recenterCrosshairs,
       sk_doNotRecenterOnCurrentCrosshairsPosition );
              }
              ImGui::SameLine(); helpMarker( "Recenter views on the fixed image in each view"
       );

              if ( ImGui::RadioButton( "Moving image", ImageSelection::MovingImageInView ==
       appData.state().recenteringMode() ) )
              {
                  appData.state().setRecenteringMode( ImageSelection::MovingImageInView );
                  recenterAllViews( sk_recenterCrosshairs,
       sk_doNotRecenterOnCurrentCrosshairsPosition );
              }
              ImGui::SameLine(); helpMarker( "Recenter views on the moving image in each view"
       );

              if ( ImGui::RadioButton( "Fixed and moving images",
       ImageSelection::FixedAndMovingImagesInView == appData.state().recenteringMode() ) )
              {
                  appData.state().setRecenteringMode(
       ImageSelection::FixedAndMovingImagesInView ); recenterAllViews( sk_recenterCrosshairs,
       sk_doNotRecenterOnCurrentCrosshairsPosition );
              }
              ImGui::SameLine(); helpMarker( "Recenter views on the fixed and moving images in
       each view" );
              */

    if (ImGui::RadioButton("All loaded images", ImageSelection::AllLoadedImages == appData.state().recenteringMode())) {
      appData.state().setRecenteringMode(ImageSelection::AllLoadedImages);

      recenterAllViews(
        sk_recenterCrosshairs,
        sk_realignCrosshairs,
        sk_doNotRecenterOnCurrentCrosshairsPosition,
        sk_doNotResetObliqueOrientation,
        sk_resetZoom);
    }
    ImGui::SameLine();
    helpMarker("Recenter views and crosshairs on all loaded images");

    ImGui::Spacing();
    ImGui::TreePop();
  }

  // View backgrounds:
  ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);

  if (ImGui::TreeNode("View Backgrounds")) {
    ImGui::ColorEdit3("2D background color", glm::value_ptr(renderData.m_2dBackgroundColor), sk_colorEditFlags);

    ImGui::ColorEdit4("3D background color", glm::value_ptr(renderData.m_3dBackgroundColor), sk_colorAlphaEditFlags);

    ImGui::Spacing();
    ImGui::TreePop();
  }

  // Anatomical labels:
  ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);

  if (ImGui::TreeNode("Anatomical Labels")) {
    ImGui::ColorEdit4("Text color", glm::value_ptr(renderData.m_anatomicalLabelColor), sk_colorAlphaEditFlags);
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

    static constexpr bool sk_orientChangeRecenterCrosshairs = false;
    static constexpr bool sk_orientChangeRealignCrosshairs = false;
    static constexpr bool sk_orientChangeRecenterOnXhairs = true;
    static constexpr bool sk_orientChangeResetObliqueOrientation = false;
    static constexpr bool sk_orientChangeResetZoom = false;

    if (ImGui::RadioButton(
          "Radiological",
          ViewConvention::Radiological == appData.windowData().getViewOrientationConvention()))
    {
      appData.windowData().setViewOrientationConvention(ViewConvention::Radiological);

      recenterAllViews(
        sk_orientChangeRecenterCrosshairs,
        sk_orientChangeRealignCrosshairs,
        sk_orientChangeRecenterOnXhairs,
        sk_orientChangeResetObliqueOrientation,
        sk_orientChangeResetZoom);
    }
    ImGui::SameLine();
    helpMarker("Anatomical left is on view right; anatomical right is on view left");

    if (ImGui::RadioButton(
          "Neurological",
          ViewConvention::Neurological == appData.windowData().getViewOrientationConvention()))
    {
      appData.windowData().setViewOrientationConvention(ViewConvention::Neurological);

      recenterAllViews(
        sk_orientChangeRecenterCrosshairs,
        sk_orientChangeRealignCrosshairs,
        sk_orientChangeRecenterOnXhairs,
        sk_orientChangeResetObliqueOrientation,
        sk_orientChangeResetZoom);
    }
    ImGui::SameLine();
    helpMarker("Anatomical left is on view left; anatomical right is on view right");

    ImGui::Spacing();
    ImGui::TreePop();
  }

  ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);

  if (ImGui::TreeNode("Scale Bars")) {
    bool showScaleBars = renderData.m_showScaleBars;
    if (ImGui::Checkbox("Show scale bars", &showScaleBars)) {
      renderData.m_showScaleBars = showScaleBars;
    }
    ImGui::SameLine();
    helpMarker("Show physical scale bars on anatomical views");

    if (showScaleBars) {
      ImGui::ColorEdit4("Color", glm::value_ptr(renderData.m_scaleBarColor), sk_colorAlphaEditFlags);

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
        static constexpr ImVec2 sk_buttonSize{32.0f, 24.0f};
        const bool selected = button.position == renderData.m_scaleBarPosition;
        if (selected) {
          ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::Button(button.label, sk_buttonSize)) {
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

    ImGui::Spacing();
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Lightbox")) {
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
        sk_colorAlphaEditFlags);
    }

    ImGui::Spacing();
    ImGui::TreePop();
  }
}

/**
 * @brief Render the Interface settings tab contents.
 */
void renderInterfaceTab(
  AppData& appData,
  const std::function<void(std::optional<float> scale)>& setUiScaleOverride,
  const std::function<void()>& requestFontReload,
  const std::function<void(UiColorPreset preset)>& applyUiColorPreset,
  const std::function<void(UiDensityPreset preset)>& applyUiDensityPreset,
  const std::function<void(float opacity)>& applyUiWindowBgOpacity)
{
  const auto currentScale = appData.settings().uiScaleOverride();
  const auto& uiScaleChoices = ui_settings::uiScaleChoices();
  auto currentChoice =
    std::find_if(uiScaleChoices.begin(), uiScaleChoices.end(), [&currentScale](const ui_settings::ScaleChoice& choice) {
      return choice.scale == currentScale;
    });
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
  if (ImGui::BeginCombo("UI colors", uiColorPresetName(currentColorPreset))) {
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

/**
 * @brief Render the Synchronize settings tab contents.
 */
void renderSynchronizeTab(AppData& appData)
{
  bool syncEnabled = appData.settings().cursorSyncEnabled();
  if (ImGui::Checkbox("Synchronize with ITK-SNAP", &syncEnabled)) {
    appData.settings().setCursorSyncEnabled(syncEnabled);
  }
  ImGui::SameLine();
  helpMarker("Synchronize with ITK-SNAP through shared memory");

  if (syncEnabled) {
    ImGui::Spacing();
    if (ImGui::BeginTable("##snapSyncOptions", 4, ImGuiTableFlags_SizingFixedFit)) {
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

      renderSyncRow(
        "Cursor:",
        "Send##cursorSync",
        "Receive##cursorSync",
        appData.settings().sendCursorSync(),
        appData.settings().receiveCursorSync(),
        [&appData](bool value) { appData.settings().setSendCursorSync(value); },
        [&appData](bool value) { appData.settings().setReceiveCursorSync(value); },
        "Send Entropy crosshairs movement to ITK-SNAP and receive ITK-SNAP cursor movement in Entropy. "
        "ITK-SNAP cursor messages use NIFTI/RAS coordinates; Entropy converts between internal LPS and "
        "ITK-SNAP RAS at the IPC boundary.");

      renderSyncRow(
        "Zoom:",
        "Send##zoomSync",
        "Receive##zoomSync",
        appData.settings().sendZoomSync(),
        appData.settings().receiveZoomSync(),
        [&appData](bool value) { appData.settings().setSendZoomSync(value); },
        [&appData](bool value) { appData.settings().setReceiveZoomSync(value); },
        "Send Entropy view zoom to ITK-SNAP and receive ITK-SNAP view zoom in Entropy.");

      renderSyncRow(
        "Pan:",
        "Send##panSync",
        "Receive##panSync",
        appData.settings().sendPanSync(),
        appData.settings().receivePanSync(),
        [&appData](bool value) { appData.settings().setSendPanSync(value); },
        [&appData](bool value) { appData.settings().setReceivePanSync(value); },
        "Send Entropy view pan to ITK-SNAP and receive ITK-SNAP view pan in Entropy.");

      ImGui::EndTable();
    }
  }
}

/**
 * @brief Render the Segmentation settings tab contents.
 */
void renderSegmentationTab(RenderData& renderData)
{
  // Modulate opacity of segmentation with opacity of image:
  ImGui::Checkbox("Modulate segmentation with image opacity", &renderData.m_modulateSegOpacityWithImageOpacity);
  ImGui::SameLine();
  helpMarker("Modulate opacity of segmentation with opacity of image");

  ImGui::Dummy(ImVec2(0.0f, 1.0f));

  ImGui::Text("Boundary outline:");
  if (ImGui::RadioButton("Outline view pixels", SegmentationOutlineStyle::ViewPixel == renderData.m_segOutlineStyle)) {
    renderData.m_segOutlineStyle = SegmentationOutlineStyle::ViewPixel;
  }
  ImGui::SameLine();
  helpMarker("Outline the outer view pixels of the image segmentation regions");

  if (ImGui::RadioButton("Outline image voxels", SegmentationOutlineStyle::ImageVoxel == renderData.m_segOutlineStyle))
  {
    renderData.m_segOutlineStyle = SegmentationOutlineStyle::ImageVoxel;
  }
  ImGui::SameLine();
  helpMarker("Outline the outer voxels of the image segmentation regions");

  if (ImGui::RadioButton("Disable (no outline)", SegmentationOutlineStyle::Disabled == renderData.m_segOutlineStyle)) {
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

/**
 * @brief Render the Metrics settings tab contents.
 */
void renderMetricsTab(
  AppData& appData,
  RenderData& renderData,
  const std::function<void(void)>& updateMetricUniforms,
  const std::function<std::size_t(void)>& getNumImageColorMaps,
  const std::function<const ImageColorMap*(std::size_t cmapIndex)>& getImageColorMap)
{
  ImGui::PushID("metrics"); /*** PushID metrics ***/

  ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
  if (ImGui::TreeNode("Difference")) {
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

    ImGui::Separator();
    ImGui::TreePop();
  }

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
 * @brief Render the Comparison modes settings tab contents.
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
 * @brief Render the Raycasting settings tab contents.
 */
void renderRaycastingTab(RenderData& renderData)
{
  ImGui::PushID("raycasting"); /*** PushID raycasting ***/

  /// @todo if these are added to the uniforms, then we'll have update uniforms when they
  /// change

  static constexpr float sk_factorStep = 0.1f;
  static constexpr float sk_minFactor = 0.1f;
  static constexpr float sk_maxFactor = 5.0f;

  ImGui::Text("Raycasting sampling rate:");
  ImGui::SameLine();
  helpMarker("Sampling rate as a fraction of the voxel size along the ray path");

  if (ImGui::DragFloat(
        "##SamplingRate",
        &(renderData.m_raycastSamplingFactor),
        sk_factorStep,
        sk_minFactor,
        sk_maxFactor,
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
 * @brief Render the Rendering settings tab contents.
 */
void renderRenderingTab(AppData& appData, RenderData& renderData)
{
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

  ImGui::Spacing();
  ImGui::Dummy(ImVec2(0.0f, 1.0f));

  ImGui::Checkbox(
    "Floating-point linear image interpolation",
    &appData.renderData().m_imageGrayFloatingPointInterpolation);
  ImGui::SameLine();
  helpMarker(
    "Use floating-point (instead of 8-bit fixed-point) linear image interpolation for the "
    "images");

  ImGui::Separator();

  // ASCII rendering controls
  if (ImGui::CollapsingHeader("ASCII Shading")) {
    RenderData& rd = appData.renderData();

    ImGui::PushID("ascii");

    ImGui::Checkbox("Enable ASCII shading", &rd.m_asciiEnabled);
    ImGui::SameLine();
    helpMarker("Render grayscale images as ASCII art");

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

  ImGui::Separator();
  ImGui::Checkbox("Show ImGui demo window", &(appData.guiData().m_showImGuiDemoWindow));
  ImGui::Checkbox("Show ImPlot demo window", &(appData.guiData().m_showImPlotDemoWindow));
}

/**
 * @brief Render the Annotations settings tab contents.
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
 * @brief Render the Precision settings tab contents.
 */
void renderPrecisionTab(AppData& appData)
{
  static constexpr uint32_t sk_stepPrecision = 1;

  ImGui::PushID("precision"); /*** PushID precision ***/

  uint32_t valuePrecision = appData.guiData().m_imageValuePrecision;
  uint32_t coordPrecision = appData.guiData().m_coordsPrecision;
  uint32_t txPrecision = appData.guiData().m_txPrecision;
  uint32_t percentilePrecision = appData.guiData().m_percentilePrecision;

  ImGui::Text("Floating-point precision in user interface:");

  if (ImGui::InputScalar(
        "Image values",
        ImGuiDataType_U32,
        &valuePrecision,
        &sk_stepPrecision,
        &sk_stepPrecision,
        "%d"))
  {
    appData.guiData().m_imageValuePrecision = ui_settings::clampPrecision(valuePrecision);
    appData.guiData().m_imageValuePrecisionFormat =
      ui_settings::precisionFormat(appData.guiData().m_imageValuePrecision);
  }
  ImGui::SameLine();
  helpMarker("Floating-point precision of image values (e.g. in Inspector window)");

  if (ImGui::InputScalar("Coordinates", ImGuiDataType_U32, &coordPrecision, &sk_stepPrecision, &sk_stepPrecision, "%d"))
  {
    appData.guiData().m_coordsPrecision = ui_settings::clampPrecision(coordPrecision);
    appData.guiData().setCoordsPrecisionFormat();
  }
  ImGui::SameLine();
  helpMarker("Floating-point precision of image spatial coordinates (e.g. in Inspector window)");

  if (ImGui::InputScalar(
        "Transformations",
        ImGuiDataType_U32,
        &txPrecision,
        &sk_stepPrecision,
        &sk_stepPrecision,
        "%d"))
  {
    appData.guiData().m_txPrecision = ui_settings::clampPrecision(txPrecision);
    appData.guiData().setTxPrecisionFormat();
  }
  ImGui::SameLine();
  helpMarker("Floating-point precision of image transformation parameters");

  if (ImGui::InputScalar(
        "Pecentiles",
        ImGuiDataType_U32,
        &percentilePrecision,
        &sk_stepPrecision,
        &sk_stepPrecision,
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
  const AllViewsRecenterType& recenterAllViews)
{
  if (ImGui::Begin("Settings", &(appData.guiData().m_showSettingsWindow), ImGuiWindowFlags_AlwaysAutoResize)) {
    static const ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;

    RenderData& renderData = appData.renderData();

    if (ImGui::BeginTabBar("##SettingsTabs", tab_bar_flags)) {
      const auto requestedTab = appData.guiData().m_requestedSettingsTab;
      if (ImGui::BeginTabItem("Views")) {
        renderViewsTab(appData, renderData, recenterAllViews);
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Interface")) {
        renderInterfaceTab(
          appData,
          setUiScaleOverride,
          requestFontReload,
          applyUiColorPreset,
          applyUiDensityPreset,
          applyUiWindowBgOpacity);
        ImGui::EndTabItem();
      }

      const ImGuiTabItemFlags syncTabFlags =
        (requestedTab == GuiData::SettingsTab::Synchronize) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
      if (ImGui::BeginTabItem("Synchronize", nullptr, syncTabFlags)) {
        renderSynchronizeTab(appData);
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Segmentation")) {
        renderSegmentationTab(renderData);
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Metrics")) {
        renderMetricsTab(appData, renderData, updateMetricUniforms, getNumImageColorMaps, getImageColorMap);
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Comparison modes")) {
        renderComparisonModesTab(renderData);
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Raycasting")) {
        renderRaycastingTab(renderData);
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Rendering")) {
        renderRenderingTab(appData, renderData);
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Annotations")) {
        renderAnnotationsTab(renderData);
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Precision")) {
        renderPrecisionTab(appData);
        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
      appData.guiData().m_requestedSettingsTab = std::nullopt;
    }
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
