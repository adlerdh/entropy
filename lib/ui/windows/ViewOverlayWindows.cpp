#include "ui/windows/ViewOverlayWindows.h"

#include "common/DirectionMaps.h"
#include "ui/Helpers.h"
#include "ui/ImGuiCustomControls.h"
#include "ui/windows/ViewOverlayModel.h"

#include "logic/camera/MathUtility.h"
#include "logic/states/annotation/AnnotationStateHelpers.h"

#include <IconsForkAwesome.h>
#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <imGuIZMOquat.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace view_overlay = entropy::ui::view_overlay;

namespace
{
using uuid = uuids::uuid;

ImVec2 scaledToolbarButtonSize(const glm::vec2& contentScale)
{
  static const ImVec2 sk_toolbarButtonSize(32, 32);
  (void)contentScale;
  const float scale = ImGui::GetFontSize() / 16.0f;
  return ImVec2{scale * sk_toolbarButtonSize.x, scale * sk_toolbarButtonSize.y};
}

bool iconButtonWithTooltip(const char* icon, const char* tooltip)
{
  const bool clicked = ImGui::Button(icon);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }
  return clicked;
}

void helpTooltip(const char* text)
{
  ImGui::SameLine();
  ImGui::TextDisabled("%s", ICON_FK_QUESTION_CIRCLE_O);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", text);
  }
}

} // namespace

void renderViewSettingsComboWindow(
  const ViewOverlayWindowContext& context,
  const ViewOverlayImageCallbacks& images,
  const ViewOverlayModeCallbacks& modes,
  const ViewOverlayProjectionCallbacks& projection)
{
  const uuid& viewOrLayoutUid = context.viewOrLayoutUid;
  const FrameBounds& viewFrameBounds = context.viewFrameBounds;
  const UiControls& uiControls = context.uiControls;
  const bool showApplyToAllButton = context.showApplyToAllButton;
  const bool allowImageSelection = context.allowImageSelection;
  const CoordinateFrame& worldCrosshairs = context.worldCrosshairs;
  const glm::vec2& contentScales = context.contentScales;

  const std::size_t numImages = images.numImages;
  const auto& isImageRendered = images.isImageRendered;
  const auto& setImageRendered = images.setImageRendered;
  const auto& applyImageVisibilityToAllViews = images.applyImageVisibilityToAllViews;
  const auto& isImageUsedForMetric = images.isImageUsedForMetric;
  const auto& setImageUsedForMetric = images.setImageUsedForMetric;
  const auto& getImageDisplayAndFileName = images.getImageDisplayAndFileName;
  const auto& getImageVisibilitySetting = images.getImageVisibilitySetting;
  const auto& getImageIsActive = images.getImageIsActive;
  const auto getImageIsReference = [&images](std::size_t imageIndex) {
    return images.getImageIsReference ? images.getImageIsReference(imageIndex) : false;
  };
  const auto canImageBeVolumeRendered = [&images](std::size_t imageIndex) {
    return images.canImageBeVolumeRendered ? images.canImageBeVolumeRendered(imageIndex) : true;
  };

  const ViewType& viewType = modes.viewType;
  const ViewRenderMode& renderMode = modes.renderMode;
  const IntensityProjectionMode& intensityProjMode = modes.intensityProjectionMode;
  const auto& setViewType = modes.setViewType;
  const auto& setRenderMode = modes.setRenderMode;
  const auto& setIntensityProjectionMode = modes.setIntensityProjectionMode;
  const auto& recenter = modes.recenter;
  const auto& applyImageSelectionAndShaderToAllViews = modes.applyImageSelectionAndShaderToAllViews;

  const auto& getIntensityProjectionSlabThickness = projection.getIntensityProjectionSlabThickness;
  const auto& setIntensityProjectionSlabThickness = projection.setIntensityProjectionSlabThickness;
  const auto& getDoMaxExtentIntensityProjection = projection.getDoMaxExtentIntensityProjection;
  const auto& setDoMaxExtentIntensityProjection = projection.setDoMaxExtentIntensityProjection;
  const auto& getXrayProjectionWindow = projection.getXrayProjectionWindow;
  const auto& setXrayProjectionWindow = projection.setXrayProjectionWindow;
  const auto& getXrayProjectionLevel = projection.getXrayProjectionLevel;
  const auto& setXrayProjectionLevel = projection.setXrayProjectionLevel;
  const auto& getXrayProjectionEnergy = projection.getXrayProjectionEnergy;
  const auto& setXrayProjectionEnergy = projection.setXrayProjectionEnergy;

  const bool singleVolumeImageSelection = ViewType::ThreeD == viewType && ViewRenderMode::VolumeRender == renderMode;

  static const glm::vec2 sk_framePad{4.0f, 4.0f};
  static const ImVec2 sk_windowPadding(0.0f, 0.0f);
  static const float sk_windowRounding(0.0f);
  static const ImVec2 sk_itemSpacing(4.0f, 4.0f);

  const ImVec4 activeColor(0.05f, 0.6f, 1.0f, 1.0f);

  const std::string uidString = std::string("##") + uuids::to_string(viewOrLayoutUid);

  const auto buttonSize = scaledToolbarButtonSize(contentScales);

  // This needs to be saved somewhere
  bool windowOpen = false;

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, sk_itemSpacing);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, sk_windowPadding);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, sk_windowRounding);
  {
    const char* label;

    label = view_overlay::usesDisabledVisibilityIcon(renderMode) ? ICON_FK_EYE_SLASH : ICON_FK_EYE;

    const ImVec2 viewTopLeftPos(
      viewFrameBounds.bounds.xoffset + sk_framePad.x,
      viewFrameBounds.bounds.yoffset + sk_framePad.y);

    ImGui::SetNextWindowPos(viewTopLeftPos, ImGuiCond_Always);

    static const ImGuiWindowFlags sk_defaultWindowFlags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoDocking;

    ImGuiWindowFlags windowFlags = sk_defaultWindowFlags;

    //        if ( ! hasFrameAndBackground )
    //        {
    windowFlags |= ImGuiWindowFlags_NoBackground;
    //        }

    ImGui::SetNextWindowBgAlpha(0.3f);

    ImGui::PushID(uidString.c_str()); /*** ID = uidString ***/

    // Windows still need a unique ID set in title (with ##ID) despite having pushed an ID on the
    // stack
    setNextWindowSizeConstraintsToMainViewport();
    if (ImGui::Begin(uidString.c_str(), &windowOpen, windowFlags)) {
      // Popup window with images to be rendered and their visibility:
      if (uiControls.m_hasImageComboBox && allowImageSelection) {
        if (view_overlay::usesVisibleImageSelection(renderMode)) {
          // Image visibility:
          if (ImGui::Button(label)) {
            ImGui::OpenPopup("imageVisibilityPopup");
          }

          if (ImGui::IsItemHovered()) {
            static const std::string sk_selectImages("Select visible images");
            ImGui::SetTooltip("%s", (sk_selectImages).c_str());
          }

          ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
          if (ImGui::BeginPopup("imageVisibilityPopup")) {
            ImGui::Text("Visible images:");
            ImGui::PushID("visibleimages"); /*** ID = visibleimages ***/

            for (std::size_t i = 0; i < numImages; ++i) {
              ImGui::PushID(static_cast<int>(i)); /*** ID = i ***/
              auto displayAndFileName = getImageDisplayAndFileName(i);
              const std::string displayName = view_overlay::imageChoiceLabel(
                {displayAndFileName.first,
                 getImageVisibilitySetting(i),
                 getImageIsActive(i),
                 getImageIsReference(i),
                 isImageRendered(i)});

              bool rendered = isImageRendered(i);
              const bool oldRendered = rendered;

              if (singleVolumeImageSelection) {
                const bool canVolumeRender = canImageBeVolumeRendered(i);
                if (!canVolumeRender) {
                  ImGui::BeginDisabled();
                }
                if (ImGui::RadioButton(displayName.c_str(), rendered) && canVolumeRender) {
                  setImageRendered(i, true);
                }
                if (!canVolumeRender) {
                  ImGui::EndDisabled();
                }
              }
              else {
                ImGui::Checkbox(displayName.c_str(), &rendered);

                if (oldRendered != rendered) {
                  setImageRendered(i, rendered);
                }
              }

              if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                if (singleVolumeImageSelection && !canImageBeVolumeRendered(i)) {
                  ImGui::SetTooltip(
                    "%s",
                    "This image is uploaded as a 2D texture. It can be shown in 2D views but cannot be volume "
                    "rendered.");
                }
                else {
                  ImGui::SetTooltip("%s", displayAndFileName.second.c_str());
                }
              }

              ImGui::PopID(); /*** ID = i ***/
            }

            if (!singleVolumeImageSelection) {
              ImGui::Separator();
              if (iconButtonWithTooltip(ICON_FK_EYE, "Show all images in this view")) {
                for (std::size_t i = 0; i < numImages; ++i) {
                  setImageRendered(i, true);
                }
              }
              ImGui::SameLine();
              if (iconButtonWithTooltip(ICON_FK_EYE_SLASH, "Hide all images in this view")) {
                for (std::size_t i = 0; i < numImages; ++i) {
                  setImageRendered(i, false);
                }
              }
              if (applyImageVisibilityToAllViews) {
                ImGui::SameLine();
                if (iconButtonWithTooltip(ICON_FK_RSS, "Apply this image visibility to all views in the layout")) {
                  applyImageVisibilityToAllViews(viewOrLayoutUid);
                }
              }
            }

            ImGui::PopID(); /*** ID = visibleimages ***/

            ImGui::EndPopup();
          }
          ImGui::PopStyleVar();
        }
        else if (ViewRenderMode::Disabled == renderMode) {
          ImGui::Button(label);
        }
        else if (view_overlay::usesMetricImageSelection(renderMode)) {
          // Image choice for the metric calculation:
          if (ImGui::Button(label)) {
            ImGui::OpenPopup("metricVisibilityPopup");
          }

          if (ImGui::IsItemHovered()) {
            static const std::string sk_selectImages("Select images to compare");
            ImGui::SetTooltip("%s", (sk_selectImages).c_str());
          }

          if (ImGui::BeginPopup("metricVisibilityPopup")) {
            ImGui::Text("Compared images:");

            for (std::size_t i = 0; i < numImages; ++i) {
              ImGui::PushID(static_cast<int>(i)); /*** ID = i ***/

              const auto displayAndFileName = getImageDisplayAndFileName(i);
              const std::string displayName = view_overlay::imageChoiceLabel(
                {displayAndFileName.first,
                 getImageVisibilitySetting(i),
                 getImageIsActive(i),
                 getImageIsReference(i),
                 isImageUsedForMetric(i)});

              bool rendered = isImageUsedForMetric(i);
              const bool oldRendered = rendered;

              ImGui::Checkbox(displayName.c_str(), &rendered);

              if (oldRendered != rendered) {
                setImageUsedForMetric(i, rendered);
              }

              if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", displayAndFileName.second.c_str());
              }

              ImGui::PopID(); /*** ID = i ***/
            }

            ImGui::EndPopup();
          }
        }
      }

      // Shader type combo box:
      if (uiControls.m_hasShaderTypeComboBox) {
        ImGui::SameLine();
        ImGui::PushItemWidth(buttonSize.x + 2.0f * ImGui::GetStyle().FramePadding.x);

        if (ImGui::BeginCombo("##shaderTypeCombo", ICON_FK_TELEVISION)) {
          auto renderSelectablesForRenderModes = [&renderMode,
                                                  &setRenderMode](const std::vector<ViewRenderMode>& renderModes) {
            for (const auto& st : renderModes) {
              const bool isSelected = (st == renderMode);
              if (ImGui::Selectable(typeString(st).c_str(), isSelected)) {
                setRenderMode(st);
              }

              if (isSelected) {
                ImGui::SetItemDefaultFocus();
              }
            }
          };

          if (numImages > 1) {
            // If there are two or more images, all shader types can be used:
            const auto allRenderModes = (ViewType::ThreeD != viewType) ? All2dViewRenderModes : All3dViewRenderModes;
            renderSelectablesForRenderModes(allRenderModes);
          }
          else if (1 == numImages) {
            // If there is only one image, then only non-metric shader types can be used:
            const auto singleImageRenderModes =
              (ViewType::ThreeD != viewType) ? All2dNonMetricRenderModes : All3dNonMetricRenderModes;
            renderSelectablesForRenderModes(singleImageRenderModes);
          }

          ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        if (ImGui::IsItemHovered()) {
          static const std::string sk_viewTypeString("Render mode: ");
          ImGui::SetTooltip("%s", (sk_viewTypeString + descriptionString(renderMode)).c_str());
        }
      }

      // Popup window with intensity projection mode:
      if (uiControls.m_hasMipTypeComboBox && (ViewRenderMode::VolumeRender != renderMode)) {
        ImGui::SameLine();
        ImGui::PushItemWidth(buttonSize.x + 2.0f * ImGui::GetStyle().FramePadding.x);

        if (ImGui::BeginCombo("##mipModeCombo", ICON_FK_FILM, ImGuiComboFlags_HeightLargest)) {
          ImGui::Text("Intensity projection mode:");
          ImGui::Spacing();

          for (const auto& ip : AllIntensityProjectionModes) {
            const bool isSelected = (ip == intensityProjMode);

            if (ImGui::Selectable(typeString(ip).c_str(), isSelected)) {
              setIntensityProjectionMode(ip);
            }

            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("%s", descriptionString(ip).c_str());
            }

            if (isSelected) {
              ImGui::SetItemDefaultFocus();
            }
          }

          if (IntensityProjectionMode::None != intensityProjMode) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            bool doMaxExtent = getDoMaxExtentIntensityProjection();

            if (!doMaxExtent) {
              float thickness = getIntensityProjectionSlabThickness();

              ImGui::Spacing();
              ImGui::Text("Slab thickness (mm):");
              ImGui::SameLine();
              helpMarker("Intensity projection slab thickness");

              ImGui::PushItemWidth(150.0f);
              if (ImGui::InputFloat("##slabThickness", &thickness, 0.1f, 1.0f, "%0.2f")) {
                if (thickness >= 0.0f) {
                  setIntensityProjectionSlabThickness(thickness);
                }
              }
              ImGui::PopItemWidth();
            }

            ImGui::Spacing();
            if (ImGui::Checkbox("Use maximum image extent", &doMaxExtent)) {
              setDoMaxExtentIntensityProjection(doMaxExtent);
            }
            ImGui::SameLine();
            helpMarker("Compute intensity projection over the full image extent");

            if (IntensityProjectionMode::Xray != intensityProjMode) {
              ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().FramePadding.y));
            }
          }

          if (IntensityProjectionMode::Xray == intensityProjMode) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            float energy = getXrayProjectionEnergy();

            ImGui::Text("X-ray energy:");
            ImGui::SameLine();
            helpMarker("Adjust x-ray energy (KeV)");

            // User can select energy from 1 KeV (1.0e-3 MeV) to 20e3 KeV (20 MeV):
            static constexpr float speed = 10.0f;

            if (ImGui::DragFloat(
                  "Energy",
                  &energy,
                  speed,
                  1.0f,
                  20.0e3f,
                  "%0.3f KeV",
                  ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic))
            {
              setXrayProjectionEnergy(energy);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            float window = getXrayProjectionWindow();
            float level = getXrayProjectionLevel();

            ImGui::Text("X-ray contrast:");
            ImGui::SameLine();
            helpMarker("Adjust x-ray projection contrast with window/leveling");

            if (mySliderF32("Width", &window, 1.0e-3f, 1.0f, "%0.3f")) {
              setXrayProjectionWindow(window);
            }
            ImGui::SameLine();
            helpMarker("Window width");

            if (mySliderF32("Level", &level, 0.0f, 1.0f, "%0.3f")) {
              setXrayProjectionLevel(level);
            }
            ImGui::SameLine();
            helpMarker("Window level (center)");

            ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().FramePadding.y));
          }

          ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", descriptionString(intensityProjMode).c_str());
        }
      }

      if (showApplyToAllButton) {
        ImGui::SameLine();
        if (ImGui::Button(ICON_FK_RSS)) {
          // Apply image and shader settings to all views in this layout
          applyImageSelectionAndShaderToAllViews(viewOrLayoutUid);
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Apply this view's image selection and render mode to all views in the layout");
        }
      }

      // View type combo box (with preview text):
      if (uiControls.m_hasViewTypeComboBox) {
        ImGui::SameLine();
        // ImGui::PushItemWidth( 100.0f + 2.0f * ImGui::GetStyle().FramePadding.x );
        ImGui::PushItemWidth(
          ImGui::CalcTextSize("Sagittal").x + 2.0f * ImGui::GetStyle().FramePadding.x +
          ImGui::GetTextLineHeightWithSpacing());

        const bool isOblique = (ViewType::Oblique == viewType);

        if (isOblique) {
          // Set text marking oblique view type with different color
          ImGui::PushStyleColor(ImGuiCol_Text, activeColor);
        }

        // Disable opening the view type combo box if the ASM is in a state where it should not
        // change.
        const bool xhairsRotated = math::isRotationIdentity(worldCrosshairs.world_T_frame_rotation());
        static const ImVec2 sk_viewTypePopupPadding(8.0f, 8.0f);
        if (ViewType::ThreeD == viewType) {
          const ImGuiStyle& style = ImGui::GetStyle();
          const float minPopupWidth =
            ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize("Camera follows crosshairs").x +
            style.ItemSpacing.x + ImGui::CalcTextSize(ICON_FK_QUESTION_CIRCLE_O).x + 2.0f * sk_viewTypePopupPadding.x;
          ImGui::SetNextWindowSizeConstraints(ImVec2(minPopupWidth, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
        }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, sk_viewTypePopupPadding);
        const bool clickedViewTypeCombo =
          ImGui::BeginCombo("##viewTypeCombo", to_string(viewType, !xhairsRotated).c_str());
        ImGui::PopStyleVar();

        if (isOblique) {
          ImGui::PopStyleColor(1); // ImGuiCol_Text
        }

        if (clickedViewTypeCombo) {
          ViewType selectedViewType = viewType;
          if (state::annot::isInStateWhereViewTypeCanChange(viewOrLayoutUid)) {
            auto renderViewTypeChoice = [&](const ViewType& vt) {
              const bool isSelected = (vt == viewType);
              if (ImGui::Selectable(
                    to_string(vt, !xhairsRotated).c_str(),
                    isSelected,
                    ImGuiSelectableFlags_DontClosePopups))
              {
                selectedViewType = vt;
                setViewType(vt);
                recenter();
              }

              if (isSelected) {
                ImGui::SetItemDefaultFocus();
              }
            };

            if (modes.selectableViewTypes.empty()) {
              for (const auto& vt : AllViewTypes) {
                renderViewTypeChoice(vt);
              }
            }
            else {
              for (const auto& vt : modes.selectableViewTypes) {
                renderViewTypeChoice(vt);
              }
            }

            if (ViewType::ThreeD == selectedViewType) {
              ImGui::Spacing();
              ImGui::Separator();
              ImGui::Spacing();

              ProjectionType projectionType = ProjectionType::Perspective;
              if (modes.getThreeDProjectionType && modes.setThreeDProjectionType) {
                projectionType = modes.getThreeDProjectionType();
                ImGui::Spacing();
                ImGui::TextUnformatted("Projection:");
                if (ImGui::RadioButton("Perspective", ProjectionType::Perspective == projectionType)) {
                  modes.setThreeDProjectionType(ProjectionType::Perspective);
                  projectionType = ProjectionType::Perspective;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Orthographic", ProjectionType::Orthographic == projectionType)) {
                  modes.setThreeDProjectionType(ProjectionType::Orthographic);
                  projectionType = ProjectionType::Orthographic;
                }

                if (modes.getThreeDFovAngleDegrees && modes.setThreeDFovAngleDegrees) {
                  float fovDegrees = modes.getThreeDFovAngleDegrees();
                  const bool isPerspective = ProjectionType::Perspective == projectionType;
                  if (!isPerspective) {
                    ImGui::BeginDisabled();
                  }
                  ImGui::SetNextItemWidth(120.0f);
                  if (ImGui::DragFloat(
                        "Field of view",
                        &fovDegrees,
                        0.1f,
                        0.5f,
                        150.0f,
                        "%.1f deg",
                        ImGuiSliderFlags_AlwaysClamp))
                  {
                    modes.setThreeDFovAngleDegrees(fovDegrees);
                  }
                  if (!isPerspective) {
                    ImGui::EndDisabled();
                  }
                }
              }

              if (modes.getThreeDOrbitTargetMode && modes.setThreeDOrbitTargetMode) {
                camera3d::OrbitTargetMode targetMode = modes.getThreeDOrbitTargetMode();
                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::TextUnformatted("Orbit around:");
                if (ImGui::RadioButton("Image center", camera3d::OrbitTargetMode::VisibleImages == targetMode)) {
                  modes.setThreeDOrbitTargetMode(camera3d::OrbitTargetMode::VisibleImages);
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Crosshairs", camera3d::OrbitTargetMode::Crosshairs == targetMode)) {
                  modes.setThreeDOrbitTargetMode(camera3d::OrbitTargetMode::Crosshairs);
                }
              }

              if (modes.getThreeDViewPositionFollowsCrosshairs && modes.setThreeDViewPositionFollowsCrosshairs) {
                ImGui::Spacing();
                ImGui::Spacing();
                bool followsCrosshairs = modes.getThreeDViewPositionFollowsCrosshairs();
                const bool isPerspective = ProjectionType::Perspective == projectionType;
                if (!isPerspective && followsCrosshairs) {
                  followsCrosshairs = false;
                  modes.setThreeDViewPositionFollowsCrosshairs(false);
                }
                if (!isPerspective) {
                  ImGui::BeginDisabled();
                }
                if (ImGui::Checkbox("Camera follows crosshairs", &followsCrosshairs)) {
                  modes.setThreeDViewPositionFollowsCrosshairs(followsCrosshairs);
                }
                if (!isPerspective) {
                  ImGui::EndDisabled();
                }
                helpTooltip("Move the 3D camera eye to the crosshairs position when the crosshairs move.");
              }

              if (modes.getThreeDRenderImageBox && modes.setThreeDRenderImageBox) {
                bool renderImageBox = modes.getThreeDRenderImageBox();
                if (ImGui::Checkbox("Show image box", &renderImageBox)) {
                  modes.setThreeDRenderImageBox(renderImageBox);
                }
                helpTooltip("Render a subtle outline of the raycast image box in 3D views.");
              }
            }
          }

          ImGui::EndCombo();
        }

        ImGui::PopItemWidth();
      }

      if (ViewType::ThreeD == viewType && ViewRenderMode::Disabled != renderMode && modes.showIsosurfacesPanel) {
        ImGui::SameLine();
        const bool isosurfacesPanelVisible =
          modes.isIsosurfacesPanelVisible ? modes.isIsosurfacesPanelVisible() : false;
        const ImVec4 activeButtonColor = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
        if (isosurfacesPanelVisible) {
          ImGui::PushStyleColor(ImGuiCol_Button, activeButtonColor);
        }
        if (ImGui::Button(ICON_FK_CUBE)) {
          if (modes.showIsosurfacesPanelForRaycastImage) {
            modes.showIsosurfacesPanelForRaycastImage();
          }
          else {
            modes.showIsosurfacesPanel();
          }
        }
        if (isosurfacesPanelVisible) {
          ImGui::PopStyleColor();
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Show Isosurfaces Panel");
        }
      }

      // Text label of visible images:
      /// @todo Replace this with NanoVG text
      {
        std::string imageNamesText;

        std::vector<view_overlay::ImageChoice> choices;
        choices.reserve(numImages);

        if (view_overlay::usesVisibleImageSelection(renderMode)) {
          for (std::size_t i = 0; i < numImages; ++i) {
            const auto displayAndFileName = getImageDisplayAndFileName(i);
            choices.push_back(
              {displayAndFileName.first,
               getImageVisibilitySetting(i),
               getImageIsActive(i),
               getImageIsReference(i),
               isImageRendered(i)});
          }
          imageNamesText = view_overlay::selectedVisibleImageNames(choices);
        }
        else if (ViewRenderMode::Disabled == renderMode) {
          // render no text
          imageNamesText = "";
        }
        else {
          for (std::size_t i = 0; i < numImages; ++i) {
            const auto displayAndFileName = getImageDisplayAndFileName(i);
            choices.push_back(
              {displayAndFileName.first,
               getImageVisibilitySetting(i),
               false,
               getImageIsReference(i),
               isImageUsedForMetric(i)});
          }
          imageNamesText = view_overlay::selectedVisibleImageNames(choices);
        }

        static const ImVec4 s_textColor(0.75f, 0.75f, 0.75f, 1.0f);
        const float wrapWidth = std::max(0.0f, viewFrameBounds.bounds.width - 2.0f * sk_framePad.x);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapWidth);
        ImGui::TextColored(s_textColor, "%s", imageNamesText.c_str());
        ImGui::PopTextWrapPos();
      }
    }

    ImGui::End(); // window

    ImGui::PopID(); /*** ID = uidString.c_str() ***/
  }

  // ImGuiStyleVar_WindowPadding, ImGuiStyleVar_ItemSpacing, ImGuiStyleVar_WindowRounding
  ImGui::PopStyleVar(3);
}

void renderViewOrientationToolWindow(
  const ViewOverlayWindowContext& context,
  const ViewOrientationOverlayCallbacks& callbacks)
{
  const uuid& viewOrLayoutUid = context.viewOrLayoutUid;
  const FrameBounds& viewFrameBounds = context.viewFrameBounds;
  const ViewType& viewType = callbacks.viewType;
  const auto& getViewCameraRotation = callbacks.getViewCameraRotation;
  const auto& setViewCameraRotation = callbacks.setViewCameraRotation;
  const auto& setViewCameraDirection = callbacks.setViewCameraDirection;
  const auto& getViewNormal = callbacks.getViewNormal;
  const auto& getObliqueViewDirections = callbacks.getObliqueViewDirections;

  static const glm::vec2 sk_framePad{4.0f, 4.0f};
  static const ImVec2 sk_windowPadding(0.0f, 0.0f);
  static const float sk_windowRounding(0.0f);
  static const ImVec2 sk_itemSpacing(0.0f, 0.0f);

  static const ImGuiWindowFlags sk_defaultWindowFlags =
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus |
    ImGuiWindowFlags_NoDocking;

  static constexpr float sk_gizmoSizeFraction = 0.14f;
  static constexpr float sk_minGizmoSize = 72.0f;
  static constexpr float sk_maxGizmoSize = 128.0f;
  static constexpr int sk_gizmoMode = (imguiGizmo::mode3Axes | imguiGizmo::cubeAtOrigin);

  static constexpr int sk_corner = 2; // bottom-left

  if (ViewType::Oblique != viewType) {
    return;
  }

  const std::string uidString = std::string("OrientationTool##") + uuids::to_string(viewOrLayoutUid);

  bool windowOpen = false;

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, sk_itemSpacing);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, sk_windowPadding);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, sk_windowRounding);

  ImGuiWindowFlags windowFlags = sk_defaultWindowFlags;

  //    if ( ! hasFrameAndBackground )
  //    {
  windowFlags |= ImGuiWindowFlags_NoBackground;
  //    }

  const ImVec2 viewBottomLeftPos(
    viewFrameBounds.bounds.xoffset + sk_framePad.x,
    viewFrameBounds.bounds.yoffset + viewFrameBounds.bounds.height - sk_framePad.y);

  const ImVec2 windowPosPivot((sk_corner & 1) ? 1.0f : 0.0f, (sk_corner & 2) ? 1.0f : 0.0f);

  ImGui::SetNextWindowPos(viewBottomLeftPos, ImGuiCond_Always, windowPosPivot);
  ImGui::SetNextWindowBgAlpha(0.3f);

  ImGui::PushID(uidString.c_str()); /*** ID = uidString ***/

  setNextWindowSizeConstraintsToMainViewport();
  if (ImGui::Begin(uidString.c_str(), &windowOpen, windowFlags)) {
    const glm::quat oldQuat = getViewCameraRotation();
    glm::quat newQuat = oldQuat;

    const float viewMinDimension = std::min(viewFrameBounds.bounds.width, viewFrameBounds.bounds.height);
    const float gizmoSize = std::clamp(sk_gizmoSizeFraction * viewMinDimension, sk_minGizmoSize, sk_maxGizmoSize);

    if (ImGui::gizmo3D("", newQuat, gizmoSize, sk_gizmoMode)) {
      setViewCameraRotation(newQuat * glm::inverse(oldQuat));
    }

    if (ImGui::IsItemHovered()) {
      const glm::dvec3 worldFwdDir{-getViewNormal()};

      if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImGui::SetTooltip(
          "View direction: (%0.3f, %0.3f, %0.3f)\n"
          "Drag or double-click to set direction",
          worldFwdDir.x,
          worldFwdDir.y,
          worldFwdDir.z);
      }
      else {
        ImGui::SetTooltip("(%0.3f, %0.3f, %0.3f)", worldFwdDir.x, worldFwdDir.y, worldFwdDir.z);
      }

      if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        ImGui::OpenPopup("setViewDirection");
      }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

    if (ImGui::BeginPopup("setViewDirection")) {
      static const glm::vec3 sk_min(-1, -1, -1);
      static const glm::vec3 sk_max(1, 1, 1);

      const glm::vec3 worldOldFwdDir = -getViewNormal();
      glm::vec3 worldNewFwdDir = worldOldFwdDir;

      ImGui::Text("Set view direction (x, y, z):");
      ImGui::SameLine();
      helpMarker("Set forward view direction vector (in World space)");
      ImGui::Spacing();

      bool applyRotation = false;

      ImGui::PushItemWidth(-1);
      if (ImGui::InputScalarN(
            "##xyz",
            ImGuiDataType_Float,
            glm::value_ptr(worldNewFwdDir),
            3,
            nullptr,
            nullptr,
            "%0.3f"))
      {
        worldNewFwdDir = glm::clamp(worldNewFwdDir, sk_min, sk_max);

        static constexpr float sk_minLen = 1.0e-4f;
        if (glm::length(worldNewFwdDir) > sk_minLen) {
          worldNewFwdDir = glm::normalize(worldNewFwdDir);
          applyRotation = true;
        }
      }
      ImGui::PopItemWidth();

      if (ImGui::Button("Flip")) {
        worldNewFwdDir = -worldNewFwdDir;
        applyRotation = true;
      }
      ImGui::SameLine();
      helpMarker("Flip forward view direction vector");

      ImGui::Separator();
      ImGui::Spacing();

      ImGui::Text("Orthogonal direction:");
      ImGui::Spacing();

      if (ImGui::Button("+X (L)")) {
        worldNewFwdDir = Directions::get(Directions::Cartesian::PosX);
        applyRotation = true;
      }

      ImGui::SameLine();
      if (ImGui::Button("-X (R)")) {
        worldNewFwdDir = -Directions::get(Directions::Cartesian::PosX);
        applyRotation = true;
      }

      ImGui::SameLine();
      ImGui::Text("Sagittal");

      if (ImGui::Button("+Y (P)")) {
        worldNewFwdDir = Directions::get(Directions::Cartesian::PosY);
        applyRotation = true;
      }

      ImGui::SameLine();
      if (ImGui::Button("-Y (A)")) {
        worldNewFwdDir = -Directions::get(Directions::Cartesian::PosY);
        applyRotation = true;
      }

      ImGui::SameLine();
      ImGui::Text("Coronal");

      if (ImGui::Button("+Z (S)")) {
        worldNewFwdDir = Directions::get(Directions::Cartesian::PosZ);
        applyRotation = true;
      }

      ImGui::SameLine();
      if (ImGui::Button("-Z (I)")) {
        worldNewFwdDir = -Directions::get(Directions::Cartesian::PosZ);
        applyRotation = true;
      }

      ImGui::SameLine();
      ImGui::Text("Axial");

      const std::vector<glm::vec3> obliqueDirs = getObliqueViewDirections(viewOrLayoutUid);

      if (!obliqueDirs.empty()) {
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("Oblique direction:");
        ImGui::SameLine();
        helpMarker("Choose among view directions in other oblique views");
        ImGui::Spacing();

        if (ImGui::BeginListBox("##obliqueDirsList")) {
          size_t index = 0;

          for (const glm::vec3& dir : obliqueDirs) {
            ImGui::PushID(static_cast<int>(index++));

            char str[25];
            snprintf(
              str,
              25,
              "(%0.3f, %0.3f, %0.3f)",
              static_cast<double>(dir.x),
              static_cast<double>(dir.y),
              static_cast<double>(dir.z));

            if (ImGui::Selectable(str, false)) {
              worldNewFwdDir = dir;
              applyRotation = true;
            }

            ImGui::PopID(); // index
          }

          ImGui::EndListBox();
        }
      }

      if (applyRotation) {
        setViewCameraDirection(worldNewFwdDir);
      }

      ImGui::EndPopup();
    }

    // ImGuiStyleVar_FramePadding, ImGuiStyleVar_ItemSpacing, ImGuiStyleVar_WindowPadding
    ImGui::PopStyleVar(3);
  }

  ImGui::End();

  ImGui::PopID(); /*** ID = uidString.c_str() ***/

  // ImGuiStyleVar_ItemSpacing, ImGuiStyleVar_WindowPadding, ImGuiStyleVar_WindowRounding
  ImGui::PopStyleVar(3);
}
