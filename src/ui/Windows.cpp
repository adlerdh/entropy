#include "ui/Windows.h"
#include "ui/Headers.h"
#include "ui/Helpers.h"
#include "ui/ImGuiCustomControls.h"
#include "ui/IsosurfaceHeader.h"
#include "ui/Widgets.h"
#include "ui/imgui/imGuIZMO.quat/imGuIZMOquat.h"

#include "common/DirectionMaps.h"

#include "image/Image.h"

#include "logic/app/Data.h"
#include "logic/states/AnnotationStateHelpers.h"

#include <IconFontCppHeaders/IconsForkAwesome.h>

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>
#include <glm/gtx/component_wise.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <inttypes.h>
#include <string>

namespace
{

static const ImVec4 sk_whiteText(1, 1, 1, 1);
static const ImVec4 sk_blackText(0, 0, 0, 1);

static const std::string sk_NA("<N/A>");

ImVec2 scaledToolbarButtonSize(const glm::vec2& contentScale)
{
  static const ImVec2 sk_toolbarButtonSize(32, 32);
  return ImVec2{contentScale.x * sk_toolbarButtonSize.x, contentScale.y * sk_toolbarButtonSize.y};
}

} // namespace

void renderViewSettingsComboWindow(
  const uuids::uuid& viewOrLayoutUid,

  const FrameBounds& mindowFrameBounds,
  const UiControls& uiControls,
  bool /*hasFrameAndBackground*/,
  bool showApplyToAllButton,

  const glm::vec2& contentScales,

  size_t numImages,

  const std::function<bool(size_t index)>& isImageRendered,
  const std::function<void(size_t index, bool visible)>& setImageRendered,

  const std::function<bool(size_t index)>& isImageUsedForMetric,
  const std::function<void(size_t index, bool visible)>& setImageUsedForMetric,

  const std::function<std::pair<const char*, const char*>(size_t index)>& getImageDisplayAndFileName,
  const std::function<bool(size_t imageIndex)>& getImageVisibilitySetting,
  const std::function<bool(size_t imageIndex)>& getImageIsActive,

  const ViewType& viewType,
  const camera::ViewRenderMode& renderMode,
  const camera::IntensityProjectionMode& intensityProjMode,

  const std::function<void(const ViewType& viewType)>& setViewType,
  const std::function<void(const camera::ViewRenderMode& renderMode)>& setRenderMode,
  const std::function<void(const camera::IntensityProjectionMode& projMode)>&
    setIntensityProjectionMode,
  const std::function<void()>& recenter,

  const std::function<void(const uuids::uuid& viewUid)>& applyImageSelectionAndShaderToAllViews,

  const std::function<float()>& getIntensityProjectionSlabThickness,
  const std::function<void(float thickness)>& setIntensityProjectionSlabThickness,

  const std::function<bool()>& getDoMaxExtentIntensityProjection,
  const std::function<void(bool set)>& setDoMaxExtentIntensityProjection,

  const std::function<float()>& getXrayProjectionWindow,
  const std::function<void(float window)>& setXrayProjectionWindow,

  const std::function<float()>& getXrayProjectionLevel,
  const std::function<void(float window)>& setXrayProjectionLevel,

  const std::function<float()>& getXrayProjectionEnergy,
  const std::function<void(float window)>& setXrayProjectionEnergy
)
{
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

    switch (renderMode)
    {
    case camera::ViewRenderMode::Image:
    case camera::ViewRenderMode::VolumeRender:
    {
      label = ICON_FK_EYE;
      break;
    }
    case camera::ViewRenderMode::Quadrants:
    case camera::ViewRenderMode::Checkerboard:
    case camera::ViewRenderMode::Flashlight:
    case camera::ViewRenderMode::Overlay:
    case camera::ViewRenderMode::Difference:
    case camera::ViewRenderMode::CrossCorrelation:
    case camera::ViewRenderMode::JointHistogram:
    {
      label = ICON_FK_EYE;
      break;
    }
    case camera::ViewRenderMode::Disabled:
    default:
    {
      label = ICON_FK_EYE_SLASH;
      break;
    }
    }

    const ImVec2 mindowTopLeftPos(
      mindowFrameBounds.bounds.xoffset + sk_framePad.x,
      mindowFrameBounds.bounds.yoffset + sk_framePad.y
    );

    ImGui::SetNextWindowPos(mindowTopLeftPos, ImGuiCond_Always);

    static const ImGuiWindowFlags sk_defaultWindowFlags = ImGuiWindowFlags_NoMove
                                                          | ImGuiWindowFlags_AlwaysAutoResize
                                                          | ImGuiWindowFlags_NoSavedSettings
                                                          | ImGuiWindowFlags_NoDecoration
                                                          | ImGuiWindowFlags_NoFocusOnAppearing
                                                          | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGuiWindowFlags windowFlags = sk_defaultWindowFlags;

    //        if ( ! hasFrameAndBackground )
    //        {
    windowFlags |= ImGuiWindowFlags_NoBackground;
    //        }

    ImGui::SetNextWindowBgAlpha(0.3f);

    ImGui::PushID(uidString.c_str()); /*** ID = uidString ***/

    // Windows still need a unique ID set in title (with ##ID) despite having pushed an ID on the stack
    if (ImGui::Begin(uidString.c_str(), &windowOpen, windowFlags))
    {
      // Popup window with images to be rendered and their visibility:
      if (uiControls.m_hasImageComboBox)
      {
        if (camera::ViewRenderMode::Image == renderMode || camera::ViewRenderMode::VolumeRender == renderMode)
        {
          // Image visibility:
          if (ImGui::Button(label))
          {
            ImGui::OpenPopup("imageVisibilityPopup");
          }

          if (ImGui::IsItemHovered())
          {
            static const std::string sk_selectImages("Select visible images");
            ImGui::SetTooltip("%s", (sk_selectImages).c_str());
          }

          if (ImGui::BeginPopup("imageVisibilityPopup"))
          {
            ImGui::Text("Visible images:");

            for (size_t i = 0; i < numImages; ++i)
            {
              ImGui::PushID(static_cast<int>(i)); /*** ID = i ***/

              auto displayAndFileName = getImageDisplayAndFileName(i);

              std::string displayName = displayAndFileName.first;

              if (false == getImageVisibilitySetting(i))
              {
                displayName += " (hidden)";
              }

              if (getImageIsActive(i))
              {
                displayName += " (active)";
              }

              bool rendered = isImageRendered(i);
              const bool oldRendered = rendered;

              ImGui::MenuItem(displayName.c_str(), "", &rendered);

              if (oldRendered != rendered)
              {
                setImageRendered(i, rendered);
              }

              if (ImGui::IsItemHovered())
              {
                ImGui::SetTooltip("%s", displayAndFileName.second);
              }

              ImGui::PopID(); /*** ID = i ***/
            }

            ImGui::EndPopup();
          }
        }
        else if (camera::ViewRenderMode::Disabled == renderMode)
        {
          ImGui::Button(label);
        }
        else
        {
          // Image choice for the metric calculation:

          if (ImGui::Button(label))
          {
            ImGui::OpenPopup("metricVisibilityPopup");
          }

          if (ImGui::IsItemHovered())
          {
            static const std::string sk_selectImages("Select images to compare");
            ImGui::SetTooltip("%s", (sk_selectImages).c_str());
          }

          if (ImGui::BeginPopup("metricVisibilityPopup"))
          {
            ImGui::Text("Compared images:");

            for (size_t i = 0; i < numImages; ++i)
            {
              ImGui::PushID(static_cast<int>(i)); /*** ID = i ***/

              const auto displayAndFileName = getImageDisplayAndFileName(i);

              std::string displayName = displayAndFileName.first;

              if (false == getImageVisibilitySetting(i))
              {
                displayName += " (hidden)";
              }

              if (getImageIsActive(i))
              {
                displayName += " (active)";
              }

              bool rendered = isImageUsedForMetric(i);
              const bool oldRendered = rendered;

              ImGui::MenuItem(displayName.c_str(), "", &rendered);

              if (oldRendered != rendered)
              {
                setImageUsedForMetric(i, rendered);
              }

              if (ImGui::IsItemHovered())
              {
                ImGui::SetTooltip("%s", displayAndFileName.second);
              }

              ImGui::PopID(); /*** ID = i ***/
            }

            ImGui::EndPopup();
          }
        }
      }

      // Shader type combo box:
      if (uiControls.m_hasShaderTypeComboBox)
      {
        ImGui::SameLine();
        ImGui::PushItemWidth(buttonSize.x + 2.0f * ImGui::GetStyle().FramePadding.x);

        if (ImGui::BeginCombo("##shaderTypeCombo", ICON_FK_TELEVISION))
        {
          auto renderSelectablesForRenderModes =
            [&renderMode, &setRenderMode](const std::vector<camera::ViewRenderMode>& renderModes)
          {
            for (const auto& st : renderModes)
            {
              const bool isSelected = (st == renderMode);

              if (ImGui::Selectable(camera::typeString(st).c_str(), isSelected))
              {
                setRenderMode(st);
              }

              if (isSelected)
              {
                ImGui::SetItemDefaultFocus();
              }
            }
          };

          if (numImages > 1)
          {
            // If there are two or more images, all shader types can be used:
            const auto allRenderModes = (ViewType::ThreeD != viewType)
                                          ? camera::All2dViewRenderModes
                                          : camera::All3dViewRenderModes;

            renderSelectablesForRenderModes(allRenderModes);
          }
          else if (1 == numImages)
          {
            // If there is only one image, then only non-metric shader types can be used:
            const auto singleImageRenderModes = (ViewType::ThreeD != viewType)
                                                  ? camera::All2dNonMetricRenderModes
                                                  : camera::All3dNonMetricRenderModes;

            renderSelectablesForRenderModes(singleImageRenderModes);
          }

          ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        if (ImGui::IsItemHovered())
        {
          static const std::string sk_viewTypeString("Render mode: ");
          ImGui::SetTooltip("%s", (sk_viewTypeString + camera::descriptionString(renderMode)).c_str());
        }
      }

      // Popup window with intensity projection mode:
      if (uiControls.m_hasMipTypeComboBox && (camera::ViewRenderMode::VolumeRender != renderMode))
      {
        ImGui::SameLine();
        ImGui::PushItemWidth(buttonSize.x + 2.0f * ImGui::GetStyle().FramePadding.x);

        if (ImGui::BeginCombo("##mipModeCombo", ICON_FK_FILM, ImGuiComboFlags_HeightLargest))
        {
          ImGui::Text("Intensity projection mode:");
          ImGui::Spacing();

          for (const auto& ip : camera::AllIntensityProjectionModes)
          {
            const bool isSelected = (ip == intensityProjMode);

            if (ImGui::Selectable(camera::typeString(ip).c_str(), isSelected))
            {
              setIntensityProjectionMode(ip);
            }

            if (ImGui::IsItemHovered())
            {
              ImGui::SetTooltip("%s", camera::descriptionString(ip).c_str());
            }

            if (isSelected)
            {
              ImGui::SetItemDefaultFocus();
            }
          }

          if (camera::IntensityProjectionMode::None != intensityProjMode)
          {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            bool doMaxExtent = getDoMaxExtentIntensityProjection();

            if (!doMaxExtent)
            {
              float thickness = getIntensityProjectionSlabThickness();

              ImGui::Spacing();
              ImGui::Text("Slab thickness (mm):");
              ImGui::SameLine();
              helpMarker("Intensity projection slab thickness");

              ImGui::PushItemWidth(150.0f);
              if (ImGui::InputFloat("##slabThickness", &thickness, 0.1f, 1.0f, "%0.2f"))
              {
                if (thickness >= 0.0f)
                {
                  setIntensityProjectionSlabThickness(thickness);
                }
              }
              ImGui::PopItemWidth();
            }

            ImGui::Spacing();
            if (ImGui::Checkbox("Use maximum image extent", &doMaxExtent))
            {
              setDoMaxExtentIntensityProjection(doMaxExtent);
            }
            ImGui::SameLine();
            helpMarker("Compute intensity projection over the full image extent");
          }

          if (camera::IntensityProjectionMode::Xray == intensityProjMode)
          {
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
                  ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic
                ))
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

            if (mySliderF32("Width", &window, 1.0e-3f, 1.0f, "%0.3f"))
            {
              setXrayProjectionWindow(window);
            }
            ImGui::SameLine();
            helpMarker("Window width");

            if (mySliderF32("Level", &level, 0.0f, 1.0f, "%0.3f"))
            {
              setXrayProjectionLevel(level);
            }
            ImGui::SameLine();
            helpMarker("Window level (center)");
          }

          ImGui::EndCombo();
        }
        ImGui::PopItemWidth();

        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", camera::descriptionString(intensityProjMode).c_str());
        }
      }

      if (showApplyToAllButton)
      {
        ImGui::SameLine();
        if (ImGui::Button(ICON_FK_RSS))
        {
          // Apply image and shader settings to all views in this layout
          applyImageSelectionAndShaderToAllViews(viewOrLayoutUid);
        }
        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip(
            "%s", "Apply this view's image selection and render mode to all views in the layout"
          );
        }
      }

      // View type combo box (with preview text):
      if (uiControls.m_hasViewTypeComboBox)
      {
        ImGui::SameLine();
        // ImGui::PushItemWidth( 100.0f + 2.0f * ImGui::GetStyle().FramePadding.x );
        ImGui::PushItemWidth(
          ImGui::CalcTextSize("Sagittal").x + 2.0f * ImGui::GetStyle().FramePadding.x
          + ImGui::GetTextLineHeightWithSpacing()
        );

        const bool isOblique = (ViewType::Oblique == viewType);

        if (isOblique)
        {
          // Set text marking oblique view type with different color
          ImGui::PushStyleColor(ImGuiCol_Text, activeColor);
        }

        // Disable opening the view type combo box if the ASM is in a state where
        // it should not change.

        static constexpr bool sk_xhairsNotRotated = false;

        const bool clickedViewTypeCombo
          = ImGui::BeginCombo("##viewTypeCombo", typeString(viewType, sk_xhairsNotRotated).c_str());

        if (isOblique)
        {
          ImGui::PopStyleColor(1); // ImGuiCol_Text
        }

        if (clickedViewTypeCombo)
        {
          if (state::isInStateWhereViewTypeCanChange(viewOrLayoutUid))
          {
            for (const auto& vt : AllViewTypes)
            {
              const bool isSelected = (vt == viewType);

              if (ImGui::Selectable(typeString(vt, sk_xhairsNotRotated).c_str(), isSelected))
              {
                setViewType(vt);
                recenter();
              }

              if (isSelected)
              {
                ImGui::SetItemDefaultFocus();
              }
            }

            if (ViewType::ThreeD == viewType)
            {
              ImGui::Spacing();
              ImGui::Separator();
              ImGui::Spacing();
              ImGui::Spacing();

              static bool viewPositionFollowsXhairs = false;
              if (ImGui::Checkbox("View position follows crosshairs", &viewPositionFollowsXhairs))
              {
                viewPositionFollowsXhairs = !viewPositionFollowsXhairs;
              }
              ImGui::SameLine();
              helpMarker("Set view position to be at the crosshairs");
            }
          }

          ImGui::EndCombo();
        }

        ImGui::PopItemWidth();
      }

      // Text label of visible images:
      /// @todo Replace this with NanoVG text
      {
        std::string imageNamesText;

        bool first = true; // The first image gets no comma in front of it

        if (camera::ViewRenderMode::Image == renderMode || camera::ViewRenderMode::VolumeRender == renderMode)
        {
          for (size_t i = 0; i < numImages; ++i)
          {
            if (isImageRendered(i) && getImageVisibilitySetting(i))
            {
              const std::string comma = (first ? "" : ", ");

              imageNamesText += comma + std::string(getImageDisplayAndFileName(i).first)
                                + (getImageIsActive(i) ? " (active)" : "");

              first = false;
            }
          }
        }
        else if (camera::ViewRenderMode::Disabled == renderMode)
        {
          // render no text
          imageNamesText = "";
        }
        else
        {
          for (size_t i = 0; i < numImages; ++i)
          {
            if (isImageUsedForMetric(i) && getImageVisibilitySetting(i))
            {
              const std::string comma = (first ? "" : ", ");
              imageNamesText += comma + std::string(getImageDisplayAndFileName(i).first);
              first = false;
            }
          }
        }

        static const ImVec4 s_textColor(0.75f, 0.75f, 0.75f, 1.0f);
        ImGui::TextColored(s_textColor, "%s", imageNamesText.c_str());
      }

      ImGui::End(); // window
    }

    ImGui::PopID(); /*** ID = uidString.c_str() ***/
  }

  // ImGuiStyleVar_WindowPadding, ImGuiStyleVar_ItemSpacing, ImGuiStyleVar_WindowRounding
  ImGui::PopStyleVar(3);
}

void renderViewOrientationToolWindow(
  const uuids::uuid& viewOrLayoutUid,
  const FrameBounds& mindowFrameBounds,
  const UiControls& /*uiControls*/,
  bool /*hasFrameAndBackground*/,
  const ViewType& viewType,
  const std::function<glm::quat()>& getViewCameraRotation,
  const std::function<void(const glm::quat& camera_T_world_rotationDelta)>& setViewCameraRotation,
  const std::function<void(const glm::vec3& worldDirection)>& setViewCameraDirection,
  const std::function<glm::vec3()>& getViewNormal,
  const std::function<std::vector<glm::vec3>(const uuids::uuid& viewUidToExclude)>&
    getObliqueViewDirections
)
{
  static const glm::vec2 sk_framePad{4.0f, 4.0f};
  static const ImVec2 sk_windowPadding(0.0f, 0.0f);
  static const float sk_windowRounding(0.0f);
  static const ImVec2 sk_itemSpacing(0.0f, 0.0f);

  static const ImGuiWindowFlags sk_defaultWindowFlags = ImGuiWindowFlags_NoMove
                                                        | ImGuiWindowFlags_AlwaysAutoResize
                                                        | ImGuiWindowFlags_NoSavedSettings
                                                        | ImGuiWindowFlags_NoDecoration
                                                        | ImGuiWindowFlags_NoFocusOnAppearing
                                                        | ImGuiWindowFlags_NoBringToFrontOnFocus;

  static constexpr float sk_gizmoSize = 96.0f;
  static constexpr int sk_gizmoMode = (imguiGizmo::mode3Axes | imguiGizmo::cubeAtOrigin);

  static constexpr int sk_corner = 2; // bottom-left

  if (ViewType::Oblique != viewType)
  {
    return;
  }

  const std::string uidString = std::string("OrientationTool##")
                                + uuids::to_string(viewOrLayoutUid);

  bool windowOpen = false;

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, sk_itemSpacing);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, sk_windowPadding);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, sk_windowRounding);

  ImGuiWindowFlags windowFlags = sk_defaultWindowFlags;

  //    if ( ! hasFrameAndBackground )
  //    {
  windowFlags |= ImGuiWindowFlags_NoBackground;
  //    }

  const ImVec2 mindowBottomLeftPos(
    mindowFrameBounds.bounds.xoffset + sk_framePad.x,
    mindowFrameBounds.bounds.yoffset + mindowFrameBounds.bounds.height - sk_framePad.y
  );

  const ImVec2 windowPosPivot((sk_corner & 1) ? 1.0f : 0.0f, (sk_corner & 2) ? 1.0f : 0.0f);

  ImGui::SetNextWindowPos(mindowBottomLeftPos, ImGuiCond_Always, windowPosPivot);
  ImGui::SetNextWindowBgAlpha(0.3f);

  ImGui::PushID(uidString.c_str()); /*** ID = uidString ***/

  if (ImGui::Begin(uidString.c_str(), &windowOpen, windowFlags))
  {
    const glm::quat oldQuat = getViewCameraRotation();
    glm::quat newQuat = oldQuat;

    if (ImGui::gizmo3D("", newQuat, sk_gizmoSize, sk_gizmoMode))
    {
      setViewCameraRotation(newQuat * glm::inverse(oldQuat));
    }

    if (ImGui::IsItemHovered())
    {
      const glm::dvec3 worldFwdDir{-getViewNormal()};

      if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
      {
        ImGui::SetTooltip(
          "View direction: (%0.3f, %0.3f, %0.3f)\n"
          "Drag or double-click to set direction",
          worldFwdDir.x,
          worldFwdDir.y,
          worldFwdDir.z
        );
      }
      else
      {
        ImGui::SetTooltip("(%0.3f, %0.3f, %0.3f)", worldFwdDir.x, worldFwdDir.y, worldFwdDir.z);
      }

      if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
      {
        ImGui::OpenPopup("setViewDirection");
      }
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

    if (ImGui::BeginPopup("setViewDirection"))
    {
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
            "##xyz", ImGuiDataType_Float, glm::value_ptr(worldNewFwdDir), 3, nullptr, nullptr, "%0.3f"
          ))
      {
        worldNewFwdDir = glm::clamp(worldNewFwdDir, sk_min, sk_max);

        static constexpr float sk_minLen = 1.0e-4f;
        if (glm::length(worldNewFwdDir) > sk_minLen)
        {
          worldNewFwdDir = glm::normalize(worldNewFwdDir);
          applyRotation = true;
        }
      }
      ImGui::PopItemWidth();

      if (ImGui::Button("Flip"))
      {
        worldNewFwdDir = -worldNewFwdDir;
        applyRotation = true;
      }
      ImGui::SameLine();
      helpMarker("Flip forward view direction vector");

      ImGui::Separator();
      ImGui::Spacing();

      ImGui::Text("Orthogonal direction:");
      ImGui::Spacing();

      if (ImGui::Button("+X (L)"))
      {
        worldNewFwdDir = Directions::get(Directions::Cartesian::X);
        applyRotation = true;
      }

      ImGui::SameLine();
      if (ImGui::Button("-X (R)"))
      {
        worldNewFwdDir = -Directions::get(Directions::Cartesian::X);
        applyRotation = true;
      }

      ImGui::SameLine();
      ImGui::Text("Sagittal");

      if (ImGui::Button("+Y (P)"))
      {
        worldNewFwdDir = Directions::get(Directions::Cartesian::Y);
        applyRotation = true;
      }

      ImGui::SameLine();
      if (ImGui::Button("-Y (A)"))
      {
        worldNewFwdDir = -Directions::get(Directions::Cartesian::Y);
        applyRotation = true;
      }

      ImGui::SameLine();
      ImGui::Text("Coronal");

      if (ImGui::Button("+Z (S)"))
      {
        worldNewFwdDir = Directions::get(Directions::Cartesian::Z);
        applyRotation = true;
      }

      ImGui::SameLine();
      if (ImGui::Button("-Z (I)"))
      {
        worldNewFwdDir = -Directions::get(Directions::Cartesian::Z);
        applyRotation = true;
      }

      ImGui::SameLine();
      ImGui::Text("Axial");

      const std::vector<glm::vec3> obliqueDirs = getObliqueViewDirections(viewOrLayoutUid);

      if (!obliqueDirs.empty())
      {
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("Oblique direction:");
        ImGui::SameLine();
        helpMarker("Choose among view directions in other oblique views");
        ImGui::Spacing();

        if (ImGui::BeginListBox("##obliqueDirsList"))
        {
          size_t index = 0;

          for (const glm::vec3& dir : obliqueDirs)
          {
            ImGui::PushID(static_cast<int>(index++));

            char str[25];
            snprintf(
              str,
              25,
              "(%0.3f, %0.3f, %0.3f)",
              static_cast<double>(dir.x),
              static_cast<double>(dir.y),
              static_cast<double>(dir.z)
            );

            if (ImGui::Selectable(str, false))
            {
              worldNewFwdDir = dir;
              applyRotation = true;
            }

            ImGui::PopID(); // index
          }

          ImGui::EndListBox();
        }
      }

      if (applyRotation)
      {
        setViewCameraDirection(worldNewFwdDir);
      }

      ImGui::EndPopup();
    }

    // ImGuiStyleVar_FramePadding, ImGuiStyleVar_ItemSpacing, ImGuiStyleVar_WindowPadding
    ImGui::PopStyleVar(3);

    ImGui::End();
  }

  ImGui::PopID(); /*** ID = uidString.c_str() ***/

  // ImGuiStyleVar_ItemSpacing, ImGuiStyleVar_WindowPadding, ImGuiStyleVar_WindowRounding
  ImGui::PopStyleVar(3);
}

void renderImagePropertiesWindow(
  AppData& appData,
  size_t numImages,
  const std::function<std::pair<const char*, const char*>(size_t index)>& getImageDisplayAndFileName,
  const std::function<size_t(void)>& getActiveImageIndex,
  const std::function<void(size_t)>& setActiveImageIndex,
  const std::function<size_t(void)>& getNumImageColorMaps,
  const std::function<ImageColorMap*(size_t cmapIndex)>& getImageColorMap,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageBackward,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageForward,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageToBack,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageToFront,
  const std::function<void(void)>& updateAllImageUniforms,
  const std::function<void(const uuids::uuid& imageUid)>& updateImageUniforms,
  const std::function<void(const uuids::uuid& imageUid)>& updateImageInterpolationMode,
  const std::function<void(std::size_t cmapIndex)>& updateImageColorMapInterpolationMode,
  const std::function<bool(const uuids::uuid& imageUid, bool locked)>&
    setLockManualImageTransformation,
  const AllViewsRecenterType& recenterAllViews
)
{
  static const std::string sk_showOpacityMixer = std::string(ICON_FK_SLIDERS)
                                                 + " Show opacity mixer";

  if (ImGui::Begin("Images##Images", &(appData.guiData().m_showImagePropertiesWindow)))
  // ImGuiWindowFlags_AlwaysAutoResize ) )
  /// @todo Do auto resize only on initial loading
  /// look at constrained resize demo in imgui
  {
    renderActiveImageSelectionCombo(
      numImages, getImageDisplayAndFileName, getActiveImageIndex, setActiveImageIndex
    );

    if (ImGui::Button(sk_showOpacityMixer.c_str()))
    {
      appData.guiData().m_showOpacityBlenderWindow = true;
    }

    ImGui::Separator();

    size_t imageIndex = 0;
    const auto activeUid = appData.activeImageUid();

    for (const auto& imageUid : appData.imageUidsOrdered())
    {
      if (Image* image = appData.image(imageUid))
      {
        const bool isActiveImage = activeUid && (imageUid == *activeUid);

        renderImageHeader(
          appData,
          appData.guiData(),
          imageUid,
          imageIndex++,
          image,
          isActiveImage,
          appData.numImages(),
          updateAllImageUniforms,
          [&imageUid, updateImageUniforms]() { updateImageUniforms(imageUid); },
          [&imageUid, updateImageInterpolationMode]() { updateImageInterpolationMode(imageUid); },
          [updateImageColorMapInterpolationMode](std::size_t cmapIndex)
          { updateImageColorMapInterpolationMode(cmapIndex); },
          getNumImageColorMaps,
          getImageColorMap,
          moveImageBackward,
          moveImageForward,
          moveImageToBack,
          moveImageToFront,
          setLockManualImageTransformation,
          recenterAllViews
        );
      }
    }

    //        const double frameRate = static_cast<double>( ImGui::GetIO().Framerate );
    //        ImGui::Text( "Rendering: %.3f ms/frame (%.1f FPS)", 1000.0 / frameRate, frameRate );

    ImGui::End();
  }
}

void renderSegmentationPropertiesWindow(
  AppData& appData,
  const std::function<ParcellationLabelTable*(size_t tableIndex)>& getLabelTable,
  const std::function<void(const uuids::uuid& imageUid)>& updateImageUniforms,
  const std::function<void(size_t labelColorTableIndex)>& updateLabelColorTableTexture,
  const std::function<void(const uuids::uuid& imageUid, size_t labelIndex)>&
    moveCrosshairsToSegLabelCentroid,
  const std::function<std::optional<uuids::uuid>(
    const uuids::uuid& matchingImageUid, const std::string& segDisplayName
  )>& createBlankSeg,
  const std::function<bool(const uuids::uuid& segUid)>& clearSeg,
  const std::function<bool(const uuids::uuid& segUid)>& removeSeg,
  const AllViewsRecenterType& recenterAllViews
)
{
  static bool firstRun = false;

  ImGuiWindowFlags flags = firstRun ? ImGuiWindowFlags_AlwaysAutoResize : ImGuiWindowFlags_None;

  if (ImGui::Begin(
        "Segmentations##Segmentations", &(appData.guiData().m_showSegmentationsWindow), flags
      ))
  {
    firstRun = false;

    size_t imageIndex = 0;
    const auto activeUid = appData.activeImageUid();

    for (const auto& imageUid : appData.imageUidsOrdered())
    {
      if (Image* image = appData.image(imageUid))
      {
        const bool isActiveImage = activeUid && (imageUid == *activeUid);

        renderSegmentationHeader(
          appData,
          imageUid,
          imageIndex++,
          image,
          isActiveImage,
          [&imageUid, updateImageUniforms]() { updateImageUniforms(imageUid); },
          getLabelTable,
          updateLabelColorTableTexture,
          [&imageUid, moveCrosshairsToSegLabelCentroid](size_t labelIndex)
          { moveCrosshairsToSegLabelCentroid(imageUid, labelIndex); },
          createBlankSeg,
          clearSeg,
          removeSeg,
          recenterAllViews
        );
      }
    }

    ImGui::End();
  }
}

void renderLandmarkPropertiesWindow(
  AppData& appData, const AllViewsRecenterType& recenterAllViewsOnCurrentCrosshairsPosition
)
{
  if (ImGui::Begin(
        "Landmarks", &(appData.guiData().m_showLandmarksWindow), ImGuiWindowFlags_AlwaysAutoResize
      ))
  {
    size_t imageIndex = 0;
    const auto activeUid = appData.activeImageUid();

    for (const auto& imageUid : appData.imageUidsOrdered())
    {
      const bool isActiveImage = activeUid && (imageUid == *activeUid);

      renderLandmarkGroupHeader(
        appData, imageUid, imageIndex++, isActiveImage, recenterAllViewsOnCurrentCrosshairsPosition
      );
    }

    ImGui::End();
  }
}

void renderAnnotationWindow(
  AppData& appData,
  const std::function<void(const uuids::uuid& viewUid, const glm::vec3& worldFwdDirection)>&
    setViewCameraDirection,
  const std::function<void()>& paintActiveSegmentationWithActivePolygon,
  const AllViewsRecenterType& recenterAllViews
)
{
  if (ImGui::Begin(
        "Annotations", &(appData.guiData().m_showAnnotationsWindow), ImGuiWindowFlags_AlwaysAutoResize
      ))
  {
    size_t imageIndex = 0;
    const auto activeUid = appData.activeImageUid();

    for (const auto& imageUid : appData.imageUidsOrdered())
    {
      const bool isActiveImage = activeUid && (imageUid == *activeUid);

      renderAnnotationsHeader(
        appData,
        imageUid,
        imageIndex++,
        isActiveImage,
        setViewCameraDirection,
        paintActiveSegmentationWithActivePolygon,
        recenterAllViews
      );
    }

    ImGui::End();
  }
}

void renderIsosurfacesWindow(
  AppData& appData,
  std::function<void(const uuids::uuid& taskUid, std::future<AsyncTaskDetails> future)> storeFuture,
  std::function<void(const uuids::uuid& taskUid)> addTaskToIsosurfaceGpuMeshGenerationQueue
)
{
  if (ImGui::Begin(
        "Isosurfaces", &(appData.guiData().m_showIsosurfacesWindow), ImGuiWindowFlags_AlwaysAutoResize
      ))
  {
    size_t imageIndex = 0;
    const auto activeUid = appData.activeImageUid();

    for (const auto& imageUid : appData.imageUidsOrdered())
    {
      const bool isActiveImage = activeUid && (imageUid == *activeUid);

      renderIsosurfacesHeader(
        appData,
        imageUid,
        imageIndex++,
        isActiveImage,
        storeFuture,
        addTaskToIsosurfaceGpuMeshGenerationQueue
      );
    }

    ImGui::End();
  }
}

void renderSettingsWindow(
  AppData& appData,
  const std::function<size_t(void)>& getNumImageColorMaps,
  const std::function<const ImageColorMap*(size_t cmapIndex)>& getImageColorMap,
  const std::function<void(void)>& updateMetricUniforms,
  const AllViewsRecenterType& recenterAllViews
)
{
  static constexpr bool sk_recenterCrosshairs = true;
  static constexpr bool sk_doNotRecenterOnCurrentCrosshairsPosition = false;
  static constexpr bool sk_doNotResetObliqueOrientation = false;
  static constexpr bool sk_resetZoom = true;

  static const float sk_windowMin = 0.0f;
  static const float sk_windowMax = 1.0f;

  static const ImGuiColorEditFlags sk_colorEditFlags = ImGuiColorEditFlags_NoInputs
                                                       | ImGuiColorEditFlags_PickerHueBar
                                                       | ImGuiColorEditFlags_DisplayRGB
                                                       | ImGuiColorEditFlags_DisplayHex
                                                       | ImGuiColorEditFlags_Uint8
                                                       | ImGuiColorEditFlags_InputRGB;

  static const ImGuiColorEditFlags sk_colorAlphaEditFlags = ImGuiColorEditFlags_NoInputs
                                                            | ImGuiColorEditFlags_PickerHueBar
                                                            | ImGuiColorEditFlags_DisplayRGB
                                                            | ImGuiColorEditFlags_DisplayHex
                                                            | ImGuiColorEditFlags_AlphaBar
                                                            | ImGuiColorEditFlags_AlphaPreviewHalf
                                                            | ImGuiColorEditFlags_Uint8
                                                            | ImGuiColorEditFlags_InputRGB;

  auto renderMetricSettingsTab =
    [&updateMetricUniforms, &getNumImageColorMaps, &getImageColorMap](
      RenderData::MetricParams& metricParams, bool& showColormapWindow, const char* name
    )
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
          ImGuiSliderFlags_AlwaysClamp
        ))
    {
      if ((windowHigh - windowLow) < 0.01f)
      {
        if (windowLow >= 0.99f)
        {
          windowLow = windowHigh - 0.01f;
        }
        else
        {
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

    // Metric masking:
    bool doMasking = metricParams.m_doMasking;
    if (ImGui::Checkbox("Masking", &doMasking))
    {
      metricParams.m_doMasking = doMasking;
      updateMetricUniforms();
    }
    ImGui::SameLine();
    helpMarker("Only compute the metric within masked regions");

    // Metric colormap dialog:
    bool* showImageColormapWindow = &showColormapWindow;
    *showImageColormapWindow |= ImGui::Button("Colormap");

    bool invertedCmap = metricParams.m_invertCmap;

    ImGui::SameLine();
    if (ImGui::Checkbox("Inverted", &invertedCmap))
    {
      metricParams.m_invertCmap = invertedCmap;
      updateMetricUniforms();
    }
    ImGui::SameLine();
    helpMarker("Select/invert the metric colormap");

    auto getColormapIndex = [&metricParams]() { return metricParams.m_colorMapIndex; };
    auto setColormapIndex = [&metricParams](size_t cmapIndex)
    { metricParams.m_colorMapIndex = cmapIndex; };

    auto getImageColorMapInverted = [&metricParams]() { return metricParams.m_invertCmap; };

    auto getImageColorMapContinuous = [&metricParams]() { return metricParams.m_cmapContinuous; };

    auto getImageColorMapLevels = [&metricParams]()
    { return metricParams.m_cmapQuantizationLevels; };

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
      updateMetricUniforms
    );

    // Colormap preview:
    const float contentWidth = ImGui::GetContentRegionAvail().x;
    const float height = (ImGui::GetIO().Fonts->Fonts[0]->FontSize * ImGui::GetIO().FontGlobalScale);

    char label[128];
    const ImageColorMap* cmap = getImageColorMap(getColormapIndex());
    snprintf(label, 128, "%s##cmap_%s", cmap->name().c_str(), name);

    const bool doQuantize
      = (!metricParams.m_cmapContinuous && (ImageColorMap::InterpolationMode::Linear == cmap->interpolationMode()));

    ImGui::paletteButton(
      label,
      cmap->data_RGBA_asVector(),
      metricParams.m_invertCmap,
      doQuantize,
      metricParams.m_cmapQuantizationLevels,
      glm::vec3{0.0f, 1.0f, 1.0f},
      ImVec2(contentWidth, height)
    );

    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("%s", cmap->description().c_str());
    }
  };

  if (ImGui::Begin(
        "Settings", &(appData.guiData().m_showSettingsWindow), ImGuiWindowFlags_AlwaysAutoResize
      ))
  {
    static const ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;

    RenderData& renderData = appData.renderData();

    if (ImGui::BeginTabBar("##SettingsTabs", tab_bar_flags))
    {
      if (ImGui::BeginTabItem("Views"))
      {
        // Show image-view intersection border
        ImGui::Checkbox(
          "Show image borders",
          &(renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections)
        );
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
        bool lockDirectionsToReference = appData.settings()
                                           .lockAnatomicalCoordinateAxesWithReferenceImage();
        if (ImGui::Checkbox(
              "Lock anatomical directions to reference image", &(lockDirectionsToReference)
            ))
        {
          appData.settings().setLockAnatomicalCoordinateAxesWithReferenceImage(
            lockDirectionsToReference
          );
        }
        ImGui::SameLine();
        helpMarker("Lock anatomical directions and crosshairs to reference image orientation");

        // Image masking
        ImGui::Checkbox("Mask images by segmentation", &(renderData.m_maskedImages));
        ImGui::SameLine();
        helpMarker("Render images only in regions masked by a segmentation label");

        // Modulate opacity of segmentation with opacity of image:
        ImGui::Checkbox(
          "Modulate segmentation with image opacity",
          &renderData.m_modulateSegOpacityWithImageOpacity
        );
        ImGui::SameLine();
        helpMarker("Modulate opacity of segmentation with opacity of image");

        ImGui::Dummy(ImVec2(0.0f, 1.0f));

        ImGui::Text("Segmentation boundary outline:");
        if (ImGui::RadioButton(
              "Outline image voxels",
              SegmentationOutlineStyle::ImageVoxel == renderData.m_segOutlineStyle
            ))
        {
          renderData.m_segOutlineStyle = SegmentationOutlineStyle::ImageVoxel;
        }
        ImGui::SameLine();
        helpMarker("Outline the outer voxels of the image segmentation regions");

        if (ImGui::RadioButton(
              "Outline view pixels",
              SegmentationOutlineStyle::ViewPixel == renderData.m_segOutlineStyle
            ))
        {
          renderData.m_segOutlineStyle = SegmentationOutlineStyle::ViewPixel;
        }
        ImGui::SameLine();
        helpMarker("Outline the outer view pixels of the image segmentation regions");

        if (ImGui::RadioButton(
              "Disabled", SegmentationOutlineStyle::Disabled == renderData.m_segOutlineStyle
            ))
        {
          renderData.m_segOutlineStyle = SegmentationOutlineStyle::Disabled;
        }
        ImGui::SameLine();
        helpMarker("Disable segmentation outlining");

        if (SegmentationOutlineStyle::Disabled != renderData.m_segOutlineStyle)
        {
          ImGui::Spacing();
          ImGui::Dummy(ImVec2(0.0f, 1.0f));

          // Modulate opacity of interior of segmentation:
          mySliderF32("Opacity of seg. interior", &(renderData.m_segInteriorOpacity), 0.0f, 1.0f);
          ImGui::SameLine();
          helpMarker("Modulate opacity of interior of segmentation");
        }

        // check if using interpolation of segs
        // if (  )
        {
          ImGui::Spacing();
          ImGui::Dummy(ImVec2(0.0f, 1.0f));

          mySliderF32("Seg. interpolation cutoff", &(renderData.m_segInterpCutoff), 0.05f, 0.95f);
          ImGui::SameLine();
          helpMarker("Interpolation cutoff");
        }

        ImGui::Spacing();
        ImGui::Dummy(ImVec2(0.0f, 1.0f));

        // Crosshairs
        ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);

        if (ImGui::TreeNode("Crosshairs"))
        {
          ImGui::ColorEdit4(
            "Color", glm::value_ptr(renderData.m_crosshairsColor), sk_colorAlphaEditFlags
          );

          ImGui::Dummy(ImVec2(0.0f, 1.0f));

          ImGui::Text("Snap crosshairs:");
          if (ImGui::RadioButton(
                "To reference image voxels",
                CrosshairsSnapping::ReferenceImage == renderData.m_snapCrosshairs
              ))
          {
            renderData.m_snapCrosshairs = CrosshairsSnapping::ReferenceImage;
          }
          ImGui::SameLine();
          helpMarker("Snap crosshairs to reference image voxel centers");

          if (ImGui::RadioButton(
                "To active image voxels",
                CrosshairsSnapping::ActiveImage == renderData.m_snapCrosshairs
              ))
          {
            renderData.m_snapCrosshairs = CrosshairsSnapping::ActiveImage;
          }
          ImGui::SameLine();
          helpMarker("Snap crosshairs to active image voxel centers");

          if (ImGui::RadioButton(
                "Disabled", CrosshairsSnapping::Disabled == renderData.m_snapCrosshairs
              ))
          {
            renderData.m_snapCrosshairs = CrosshairsSnapping::Disabled;
          }
          ImGui::SameLine();
          helpMarker("Do not snap crosshairs to image voxels");

          ImGui::Spacing();
          ImGui::TreePop();
        }

        // View centering:
        ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);

        if (ImGui::TreeNode("View Recentering"))
        {
          ImGui::Text("Center views and crosshairs on:");

          ImGui::SameLine();
          helpMarker("Default view and crosshairs centering behavior");

          if (ImGui::RadioButton(
                "Reference image",
                ImageSelection::ReferenceImage == appData.state().recenteringMode()
              ))
          {
            appData.state().setRecenteringMode(ImageSelection::ReferenceImage);

            recenterAllViews(
              sk_recenterCrosshairs,
              sk_doNotRecenterOnCurrentCrosshairsPosition,
              sk_doNotResetObliqueOrientation,
              sk_resetZoom
            );
          }
          ImGui::SameLine();
          helpMarker("Recenter views and crosshairs on the reference image");

          if (ImGui::RadioButton(
                "Active image", ImageSelection::ActiveImage == appData.state().recenteringMode()
              ))
          {
            appData.state().setRecenteringMode(ImageSelection::ActiveImage);

            recenterAllViews(
              sk_recenterCrosshairs,
              sk_doNotRecenterOnCurrentCrosshairsPosition,
              sk_doNotResetObliqueOrientation,
              sk_resetZoom
            );
          }
          ImGui::SameLine();
          helpMarker("Recenter views and crosshairs on the active image");

          if (ImGui::RadioButton(
                "Reference and active images",
                ImageSelection::ReferenceAndActiveImages == appData.state().recenteringMode()
              ))
          {
            appData.state().setRecenteringMode(ImageSelection::ReferenceAndActiveImages);

            recenterAllViews(
              sk_recenterCrosshairs,
              sk_doNotRecenterOnCurrentCrosshairsPosition,
              sk_doNotResetObliqueOrientation,
              sk_resetZoom
            );
          }
          ImGui::SameLine();
          helpMarker("Recenter views and crosshairs on the reference and active images");

          /// @todo These don't work yet
          /*
                    if ( ImGui::RadioButton( "All visible images", ImageSelection::VisibleImagesInView == appData.state().recenteringMode() ) )
                    {
                        appData.state().setRecenteringMode( ImageSelection::VisibleImagesInView );
                        recenterAllViews( sk_recenterCrosshairs, sk_doNotRecenterOnCurrentCrosshairsPosition );
                    }
                    ImGui::SameLine(); helpMarker( "Recenter views and crosshairs on the visible images in each view" );

                    if ( ImGui::RadioButton( "Fixed image", ImageSelection::FixedImageInView == appData.state().recenteringMode() ) )
                    {
                        appData.state().setRecenteringMode( ImageSelection::FixedImageInView );
                        recenterAllViews( sk_recenterCrosshairs, sk_doNotRecenterOnCurrentCrosshairsPosition );
                    }
                    ImGui::SameLine(); helpMarker( "Recenter views on the fixed image in each view" );

                    if ( ImGui::RadioButton( "Moving image", ImageSelection::MovingImageInView == appData.state().recenteringMode() ) )
                    {
                        appData.state().setRecenteringMode( ImageSelection::MovingImageInView );
                        recenterAllViews( sk_recenterCrosshairs, sk_doNotRecenterOnCurrentCrosshairsPosition );
                    }
                    ImGui::SameLine(); helpMarker( "Recenter views on the moving image in each view" );

                    if ( ImGui::RadioButton( "Fixed and moving images", ImageSelection::FixedAndMovingImagesInView == appData.state().recenteringMode() ) )
                    {
                        appData.state().setRecenteringMode( ImageSelection::FixedAndMovingImagesInView );
                        recenterAllViews( sk_recenterCrosshairs, sk_doNotRecenterOnCurrentCrosshairsPosition );
                    }
                    ImGui::SameLine(); helpMarker( "Recenter views on the fixed and moving images in each view" );
                    */

          if (ImGui::RadioButton(
                "All loaded images",
                ImageSelection::AllLoadedImages == appData.state().recenteringMode()
              ))
          {
            appData.state().setRecenteringMode(ImageSelection::AllLoadedImages);

            recenterAllViews(
              sk_recenterCrosshairs,
              sk_doNotRecenterOnCurrentCrosshairsPosition,
              sk_doNotResetObliqueOrientation,
              sk_resetZoom
            );
          }
          ImGui::SameLine();
          helpMarker("Recenter views and crosshairs on all loaded images");

          ImGui::Spacing();
          ImGui::TreePop();
        }

        // View backgrounds:
        ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);

        if (ImGui::TreeNode("View Backgrounds"))
        {
          ImGui::ColorEdit3(
            "2D background color", glm::value_ptr(renderData.m_2dBackgroundColor), sk_colorEditFlags
          );

          ImGui::ColorEdit4(
            "3D background color",
            glm::value_ptr(renderData.m_3dBackgroundColor),
            sk_colorAlphaEditFlags
          );

          ImGui::Spacing();
          ImGui::TreePop();
        }

        // Anatomical labels:
        ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);

        if (ImGui::TreeNode("Anatomical Labels"))
        {
          ImGui::ColorEdit4(
            "Text color", glm::value_ptr(renderData.m_anatomicalLabelColor), sk_colorAlphaEditFlags
          );

          ImGui::Dummy(ImVec2(0.0f, 1.0f));

          ImGui::Text("Anatomical directions:");

          if (ImGui::RadioButton(
                "Human", AnatomicalLabelType::Human == renderData.m_anatomicalLabelType
              ))
          {
            renderData.m_anatomicalLabelType = AnatomicalLabelType::Human;
          }
          ImGui::SameLine();
          helpMarker("Left, Right, Posterior, Anterior, Superior, Inferior");

          if (ImGui::RadioButton(
                "Rodent", AnatomicalLabelType::Rodent == renderData.m_anatomicalLabelType
              ))
          {
            renderData.m_anatomicalLabelType = AnatomicalLabelType::Rodent;
          }
          ImGui::SameLine();
          helpMarker("Left, Right, Dorsal, Ventral, Caudal, Rostral");

          if (ImGui::RadioButton(
                "Disabled", AnatomicalLabelType::Disabled == renderData.m_anatomicalLabelType
              ))
          {
            renderData.m_anatomicalLabelType = AnatomicalLabelType::Disabled;
          }
          ImGui::SameLine();
          helpMarker("Disable anatomical labels");

          ImGui::Dummy(ImVec2(0.0f, 1.0f));

          ImGui::Text("View orientation convention:");

          static constexpr bool sk_orientChangeRecenterCrosshairs = false;
          static constexpr bool sk_orientChangeRecenterOnXhairs = true;
          static constexpr bool sk_orientChangeResetObliqueOrientation = false;
          static constexpr bool sk_orientChangeResetZoom = false;

          if (ImGui::RadioButton(
                "Radiological",
                ViewConvention::Radiological == appData.windowData().getViewOrientationConvention()
              ))
          {
            appData.windowData().setViewOrientationConvention(ViewConvention::Radiological);

            recenterAllViews(
              sk_orientChangeRecenterCrosshairs,
              sk_orientChangeRecenterOnXhairs,
              sk_orientChangeResetObliqueOrientation,
              sk_orientChangeResetZoom
            );
          }
          ImGui::SameLine();
          helpMarker("Anatomical left is on view right; anatomical right is on view left");

          if (ImGui::RadioButton(
                "Neurological",
                ViewConvention::Neurological == appData.windowData().getViewOrientationConvention()
              ))
          {
            appData.windowData().setViewOrientationConvention(ViewConvention::Neurological);

            recenterAllViews(
              sk_orientChangeRecenterCrosshairs,
              sk_orientChangeRecenterOnXhairs,
              sk_orientChangeResetObliqueOrientation,
              sk_orientChangeResetZoom
            );
          }
          ImGui::SameLine();
          helpMarker("Anatomical left is on view left; anatomical right is on view right");

          ImGui::Spacing();
          ImGui::TreePop();
        }

        ImGui::Separator();
        ImGui::Checkbox("Show ImGui demo window", &(appData.guiData().m_showImGuiDemoWindow));
        ImGui::Checkbox("Show ImPlot demo window", &(appData.guiData().m_showImPlotDemoWindow));

        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Metrics"))
      {
        ImGui::PushID("metrics"); /*** PushID metrics ***/

        ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
        if (ImGui::TreeNode("Difference"))
        {
          ImGui::PushID("diff");
          {
            // Difference type:
            if (ImGui::RadioButton("Absolute", false == renderData.m_useSquare))
            {
              renderData.m_useSquare = false;
            }

            ImGui::SameLine();
            if (ImGui::RadioButton("Squared difference", true == renderData.m_useSquare))
            {
              renderData.m_useSquare = true;
            }
            ImGui::SameLine();
            helpMarker("Compute absolute or squared difference");

            renderMetricSettingsTab(
              renderData.m_squaredDifferenceParams,
              appData.guiData().m_showDifferenceColormapWindow,
              "sqdiff"
            );
          }
          ImGui::PopID(); // "diff"

          ImGui::Separator();
          ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
        if (ImGui::TreeNode("Cross-correlation"))
        {
          ImGui::PushID("crosscorr");
          {
            renderMetricSettingsTab(
              renderData.m_crossCorrelationParams,
              appData.guiData().m_showCorrelationColormapWindow,
              "crosscorr"
            );
          }
          ImGui::PopID(); // "crosscorr"
          ImGui::TreePop();
        }

        ImGui::PopID(); /*** PopID metrics ***/
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Comparison modes"))
      {
        ImGui::PushID("comparison"); /*** PushID metrics ***/

        //                if ( ImGui::TreeNode( "Comparison comparison" ) )
        //                {
        // Overlap style:
        ImGui::Text("Overlap:");

        if (ImGui::RadioButton("Magenta/cyan", true == renderData.m_overlayMagentaCyan))
        {
          renderData.m_overlayMagentaCyan = true;
        }

        ImGui::SameLine();
        if (ImGui::RadioButton("Red/green overlay", false == renderData.m_overlayMagentaCyan))
        {
          renderData.m_overlayMagentaCyan = false;
        }
        ImGui::SameLine();
        helpMarker("Color style for 'overlay' views");
        ImGui::Spacing();
        ImGui::Separator();

        // Quadrants style:
        ImGui::Text("Quadrants:");

        const glm::ivec2 Q = renderData.m_quadrants;

        if (ImGui::RadioButton("X", true == (Q.x && !Q.y)))
        {
          renderData.m_quadrants = glm::ivec2{true, false};
        }

        ImGui::SameLine();
        if (ImGui::RadioButton("Y", true == (!Q.x && Q.y)))
        {
          renderData.m_quadrants = glm::ivec2{false, true};
        }

        ImGui::SameLine();
        if (ImGui::RadioButton("X and Y comparison", true == (Q.x && Q.y)))
        {
          renderData.m_quadrants = glm::ivec2{true, true};
        }

        ImGui::SameLine();
        helpMarker("Comparison directions in 'quadrant' views");
        ImGui::Spacing();
        ImGui::Separator();

        // Checkerboard squares
        ImGui::Text("Checkerboard:");

        int numSquares = renderData.m_numCheckerboardSquares;
        if (ImGui::InputInt("Number of checkers", &numSquares))
        {
          if (2 <= numSquares && numSquares <= 2048)
          {
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

        if (ImGui::SliderScalar(
              "Circle size", ImGuiDataType_S32, &radiusPercent, &k_minRadius, &k_maxRadius, "%d"
            ))
        {
          renderData.m_flashlightRadius = static_cast<float>(radiusPercent) / 100.0f;
        }
        ImGui::SameLine();
        helpMarker("Circle size (as a percentage of the view size) for Flashlight rendering");

        ImGui::Spacing();
        if (ImGui::RadioButton(
              "Overlay moving image atop fixed image", true == renderData.m_flashlightOverlays
            ))
        {
          renderData.m_flashlightOverlays = true;
        }

        if (ImGui::RadioButton(
              "Replace fixed image with moving image", false == renderData.m_flashlightOverlays
            ))
        {
          renderData.m_flashlightOverlays = false;
        }
        ImGui::SameLine();
        helpMarker("Mode for Flashlight rendering: overlay or replacement");

        //                    ImGui::Separator();
        //                    ImGui::TreePop();
        //                }

        ImGui::PopID(); /*** PopID comparison ***/
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Raycasting"))
      {
        ImGui::PushID("raycasting"); /*** PushID raycasting ***/

        /// @todo if these are added to the uniforms, then we'll have update uniforms when they change

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
              ImGuiSliderFlags_AlwaysClamp
            ))
        {
          // Update uniforms if m_raycastSamplingFactor gets added to uniforms
        }

        ImGui::Spacing();
        ImGui::Dummy(ImVec2(0.0f, 1.0f));

        // Should the no-hit zone of raycast views be transparent, so that the view background is visible?
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

        if (ImGui::RadioButton(
              "Disabled", RenderData::SegMaskingForRaycasting::Disabled == renderData.m_segMasking
            ))
        {
          renderData.m_segMasking = RenderData::SegMaskingForRaycasting::Disabled;
        }
        ImGui::SameLine();
        helpMarker("Segmentation masking disabled");

        if (ImGui::RadioButton(
              "Mask in", RenderData::SegMaskingForRaycasting::SegMasksIn == renderData.m_segMasking
            ))
        {
          renderData.m_segMasking = RenderData::SegMaskingForRaycasting::SegMasksIn;
        }
        ImGui::SameLine();
        helpMarker("Segmentation masks image in");

        if (ImGui::RadioButton(
              "Mask out", RenderData::SegMaskingForRaycasting::SegMasksOut == renderData.m_segMasking
            ))
        {
          renderData.m_segMasking = RenderData::SegMaskingForRaycasting::SegMasksOut;
        }
        ImGui::SameLine();
        helpMarker("Segmentation masks image out");

        ImGui::PopID(); /*** PopID raycasting ***/
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Annotations"))
      {
        ImGui::PushID("landmarks"); /*** PushID landmarks ***/

        bool annotOnTop = renderData.m_globalAnnotationParams.renderOnTopOfAllImagePlanes;
        if (ImGui::Checkbox("Annotations on top", &annotOnTop))
        {
          renderData.m_globalAnnotationParams.renderOnTopOfAllImagePlanes = annotOnTop;
        }
        ImGui::SameLine();
        helpMarker("Render annotations on top of all image layers");

        bool lmOnTop = renderData.m_globalLandmarkParams.renderOnTopOfAllImagePlanes;
        if (ImGui::Checkbox("Landmarks on top", &lmOnTop))
        {
          renderData.m_globalLandmarkParams.renderOnTopOfAllImagePlanes = lmOnTop;
        }
        ImGui::SameLine();
        helpMarker("Render landmarks on top of all image layers");

        bool hideVertices = renderData.m_globalAnnotationParams.hidePolygonVertices;
        if (ImGui::Checkbox("Hide all annotation vertices", &hideVertices))
        {
          renderData.m_globalAnnotationParams.hidePolygonVertices = hideVertices;
        }
        ImGui::SameLine();
        helpMarker("Hide all annotation vertices");

        ImGui::PopID(); /*** PopID landmarks ***/
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Precision"))
      {
        static constexpr uint32_t sk_minPrecision = 0;
        static constexpr uint32_t sk_maxPrecision = 9;
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
              "%d"
            ))
        {
          appData.guiData().m_imageValuePrecision
            = std::min(std::max(valuePrecision, sk_minPrecision), sk_maxPrecision);

          appData.guiData().m_imageValuePrecisionFormat = std::string("%0.")
                                                          + std::to_string(
                                                            appData.guiData().m_imageValuePrecision
                                                          )
                                                          + std::string("f");
        }
        ImGui::SameLine();
        helpMarker("Floating-point precision of image values (e.g. in Inspector window)");

        if (ImGui::InputScalar(
              "Coordinates",
              ImGuiDataType_U32,
              &coordPrecision,
              &sk_stepPrecision,
              &sk_stepPrecision,
              "%d"
            ))
        {
          appData.guiData().m_coordsPrecision
            = std::min(std::max(coordPrecision, sk_minPrecision), sk_maxPrecision);
          appData.guiData().setCoordsPrecisionFormat();
        }
        ImGui::SameLine();
        helpMarker(
          "Floating-point precision of image spatial coordinates (e.g. in Inspector window)"
        );

        if (ImGui::InputScalar(
              "Transformations",
              ImGuiDataType_U32,
              &txPrecision,
              &sk_stepPrecision,
              &sk_stepPrecision,
              "%d"
            ))
        {
          appData.guiData().m_txPrecision
            = std::min(std::max(txPrecision, sk_minPrecision), sk_maxPrecision);
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
              "%d"
            ))
        {
          appData.guiData().m_percentilePrecision
            = std::min(std::max(percentilePrecision, sk_minPrecision), sk_maxPrecision);

          appData.guiData().m_percentilePrecisionFormat = std::string("%0.")
                                                          + std::to_string(
                                                            appData.guiData().m_percentilePrecision
                                                          )
                                                          + std::string("f");
        }
        ImGui::SameLine();
        helpMarker("Floating-point precision of percentiles (e.g. in histogram)");

        ImGui::PopID(); /*** PopID precision ***/
        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }

    ImGui::End();
  }
}

//enum class PopupWindowPosition
//{
//    Custom,
//    TopLeft,
//    TopRight,
//    BottomLeft,
//    BottomRight
//};

void renderInspectionWindow(
  AppData& appData,
  size_t numImages,
  const std::function<std::pair<const char*, const char*>(size_t index)>& getImageDisplayAndFileName,
  const std::function<glm::vec3()>& getWorldDeformedPos,
  const std::function<std::optional<glm::vec3>(size_t imageIndex)>& getSubjectPos,
  const std::function<std::optional<glm::ivec3>(size_t imageIndex)>& getVoxelPos,
  const std::function<std::optional<double>(size_t imageIndex)>& getImageValueNN,
  const std::function<std::optional<double>(size_t imageIndex)>& getImageValueLinear,
  const std::function<std::optional<int64_t>(size_t imageIndex)>& getSegLabel,
  const std::function<ParcellationLabelTable*(size_t tableIndex)>& getLabelTable
)
{
  std::ignore = getImageValueLinear;

  static constexpr size_t sk_refIndex = 0; // Index of the reference image
  static bool s_firstRun = false;          // Is this the first run?

  static constexpr float sk_pad = 10.0f;
  static int corner = 2;
  //    static PopupWindowPosition pos = PopupWindowPosition::BottomLeft;

  // Show World coordinates?
  static bool s_showWorldCoords = false;

  bool selectionButtonShown = false;

  static const ImVec4 buttonColor(0.0f, 0.0f, 0.0f, 0.0f);
  static const ImVec4 blueColor(0.0f, 0.5f, 1.0f, 1.0f);

  // For which images to show coordinates?
  static std::unordered_map<uuids::uuid, bool> s_showSubject;

  if (s_firstRun)
  {
    // Show the first (reference) image coordinates by default:
    if (const auto imageUid = appData.imageUid(sk_refIndex))
    {
      s_showSubject.insert({*imageUid, true});
    }

    s_firstRun = false;
  }

  auto contextMenu = [&numImages, &appData, &getImageDisplayAndFileName]()
  {
    if (ImGui::BeginMenu("Show"))
    {
      //                if ( ImGui::MenuItem( "World", nullptr, s_showWorldCoords ) )
      //                {
      //                    s_showWorldCoords = ! s_showWorldCoords;
      //                }
      //                if ( ImGui::IsItemHovered() )
      //                {
      //                    ImGui::SetTooltip( "Show World-space crosshairs coordinates" );
      //                }

      for (size_t imageIndex = 0; imageIndex < numImages; ++imageIndex)
      {
        const auto imageUid = appData.imageUid(imageIndex);
        if (!imageUid)
          continue;

        bool& visible = s_showSubject[*imageUid];

        const auto names = getImageDisplayAndFileName(imageIndex);

        if (ImGui::MenuItem(names.first, nullptr, visible))
        {
          visible = !visible;
        }

        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", names.second);
        }
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Position"))
    {
      if (ImGui::MenuItem("Custom", nullptr, corner == -1))
        corner = -1;
      if (ImGui::MenuItem("Top-left", nullptr, corner == 0))
        corner = 0;
      if (ImGui::MenuItem("Top-right", nullptr, corner == 1))
        corner = 1;
      if (ImGui::MenuItem("Bottom-left", nullptr, corner == 2))
        corner = 2;
      if (ImGui::MenuItem("Bottom-right", nullptr, corner == 3))
        corner = 3;

      ImGui::EndMenu();
    }

    if (appData.guiData().m_showInspectionWindow && ImGui::MenuItem("Close"))
    {
      appData.guiData().m_showInspectionWindow = false;
    }

    ImGui::EndPopup();
  };

  auto showSelectionButton = []()
  {
    ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
    if (ImGui::Button(ICON_FK_LIST_ALT))
    {
      ImGui::OpenPopup("selectionPopup");
    }
    ImGui::PopStyleColor(1);

    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("Select image(s) to inspect");
    }
  };

  ImGuiIO& io = ImGui::GetIO();

  ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
                                 | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

  if (corner != -1)
  {
    windowFlags |= ImGuiWindowFlags_NoMove;

    const ImVec2 windowPos = ImVec2(
      (corner & 1) ? io.DisplaySize.x - sk_pad : sk_pad,
      (corner & 2) ? io.DisplaySize.y - sk_pad : sk_pad
    );

    const ImVec2 windowPosPivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPosPivot);
  }

  ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background

  if (ImGui::Begin("##InspectionWindow", &(appData.guiData().m_showInspectionWindow), windowFlags))
  {
    //        if ( ImGui::IsMousePosValid() && ImGui::IsAnyMouseDown() )
    //        {
    //            ImGui::Text( "Mouse Position: (%.0f, %.0f)",
    //                         static_cast<double>( io.MousePos.x ),
    //                         static_cast<double>( io.MousePos.y ) );
    //        }

    if (s_showWorldCoords)
    {
      const glm::vec3 worldPos = getWorldDeformedPos();

      //            ImGui::Text( "World:" );
      ImGui::Text(
        "(%.3f, %.3f, %.3f) mm",
        static_cast<double>(worldPos.x),
        static_cast<double>(worldPos.y),
        static_cast<double>(worldPos.z)
      );

      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("World-space coordinates");
      }
    }

    bool firstImageShown = true;
    bool showedAtLeastOneImage = false; // is info for at least one image shown?

    for (size_t imageIndex = 0; imageIndex < numImages; ++imageIndex)
    {
      const auto imageUid = appData.imageUid(imageIndex);
      const Image* image = (imageUid ? appData.image(*imageUid) : nullptr);
      if (!image)
        continue;

      if (0 == s_showSubject.count(*imageUid))
      {
        // The reference image is shown by default in this list
        s_showSubject.insert({*imageUid, (sk_refIndex == imageIndex) ? true : false});
      }

      const bool visible = s_showSubject[*imageUid];
      if (!visible)
        continue;

      showedAtLeastOneImage = true;

      if (s_showWorldCoords || !firstImageShown)
      {
        ImGui::Separator();
      }

      firstImageShown = false;

      const auto names = getImageDisplayAndFileName(imageIndex);

      if (sk_refIndex == imageIndex)
      {
        ImGui::TextColored(blueColor, "%s (ref.):", names.first);
      }
      else
      {
        ImGui::TextColored(blueColor, "%s:", names.first);
      }

      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("%s", names.second);
      }

      /// @todo Do we want to show subject coords for all images?
      if (sk_refIndex == imageIndex)
      {
        // Show subject coordinates for the reference image only:
        if (const auto subjectPos = getSubjectPos(imageIndex))
        {
          const glm::dvec3 p{*subjectPos};
          ImGui::Text("(%.3f, %.3f, %.3f) mm", p.x, p.y, p.z);
        }
      }

      if (const auto voxelPos = getVoxelPos(imageIndex))
      {
        ImGui::Text("(%d, %d, %d) vox", voxelPos->x, voxelPos->y, voxelPos->z);
      }
      else
      {
        ImGui::Text("<N/A>");
      }

      if (const auto imageValue = getImageValueNN(imageIndex))
      {
        if (isComponentFloatingPoint(image->header().memoryComponentType()))
        {
          if (image->header().numComponentsPerPixel() > 1)
          {
            ImGui::Text("Value (comp. %d): %0.3f", image->settings().activeComponent(), *imageValue);
          }
          else
          {
            ImGui::Text("Value: %0.3f", *imageValue);
          }
        }
        else
        {
          if (image->header().numComponentsPerPixel() > 1)
          {
            // Multi-component case: show the value of the active component
            ImGui::Text(
              "Value (comp. %d): %d",
              image->settings().activeComponent(),
              static_cast<int>(*imageValue)
            );
          }
          else
          {
            // Single component case
            ImGui::Text("Value: %d", static_cast<int32_t>(*imageValue));
          }
        }
      }

      const auto segUid = appData.imageToActiveSegUid(*imageUid);
      const Image* seg = (segUid ? appData.seg(*segUid) : nullptr);
      if (!seg)
        continue;

      if (const auto segLabel = getSegLabel(imageIndex))
      {
        ImGui::Text("Label: %" PRId64, static_cast<int64_t>(*segLabel));

        const auto* table = getLabelTable(seg->settings().labelTableIndex());
        if (table && 0 != *segLabel)
        {
          const char* labelName = table->getName(static_cast<size_t>(*segLabel)).c_str();
          ImGui::SameLine();
          ImGui::Text("(%s)", labelName);
        }
      }

      if (!selectionButtonShown)
      {
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 24);
        showSelectionButton();
        selectionButtonShown = true;
      }
    }

    if (!showedAtLeastOneImage)
    {
      showSelectionButton();
    }

    if (ImGui::BeginPopupContextWindow())
    {
      // Show context menu on right-button click:
      contextMenu();
    }
    else if (ImGui::BeginPopup("selectionPopup"))
    {
      // Show context menu if the user has clicked the popup button:
      contextMenu();
      ImGui::EndPopup();
    }
  }

  ImGui::End();
}

void renderInspectionWindowWithTable(
  AppData& appData,
  const std::function<std::pair<const char*, const char*>(size_t index)>& getImageDisplayAndFileName,
  const std::function<std::optional<glm::vec3>(size_t imageIndex)>& getSubjectPos,
  const std::function<std::optional<glm::ivec3>(size_t imageIndex)>& getVoxelPos,
  const std::function<void(size_t imageIndex, const glm::vec3& subjectPos)> setSubjectPos,
  const std::function<void(size_t imageIndex, const glm::ivec3& voxelPos)> setVoxelPos,
  const std::function<std::vector<double>(size_t imageIndex, bool getOnlyActiveComponent)>&
    getImageValuesNN,
  const std::function<std::vector<double>(size_t imageIndex, bool getOnlyActiveComponent)>&
    getImageValuesLinear,
  const std::function<std::optional<int64_t>(size_t imageIndex)>& getSegLabel,
  const std::function<ParcellationLabelTable*(size_t tableIndex)>& getLabelTable
)
{
  static bool s_firstRun = true; // Is this the first run?

  static constexpr float sk_pad = 10.0f;
  static int corner = 2;

  //    bool selectionButtonShown = false;

  static const ImVec2 sk_windowPadding(0.0f, 0.0f);
  static const ImVec2 sk_framePadding(0.0f, 0.0f);
  static const ImVec2 sk_itemInnerSpacing(1.0f, 1.0f);
  static const ImVec2 sk_cellPadding(0.0f, 0.0f);
  static const float sk_windowRounding(0.0f);

  static const ImVec4 buttonColor(0.0f, 0.0f, 0.0f, 0.0f);
  //    static const ImVec4 blueColor( 0.0f, 0.5f, 1.0f, 1.0f );

  static const ImGuiTableFlags sk_tableFlags = ImGuiTableFlags_Resizable
                                               | ImGuiTableFlags_Reorderable
                                               | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders
                                               | ImGuiTableFlags_SizingFixedFit
                                               | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;

  static const ImGuiWindowFlags sk_windowFlags = ImGuiWindowFlags_MenuBar
                                                 | ImGuiWindowFlags_AlwaysAutoResize
                                                 | ImGuiWindowFlags_NoFocusOnAppearing
                                                 | ImGuiWindowFlags_NoResize
                                                 | ImGuiWindowFlags_NoScrollbar
                                                 | ImGuiWindowFlags_NoBackground
                                                 | ImGuiWindowFlags_NoNav;

  static bool s_showTitleBar = false;

  // For which images to show coordinates?
  static std::unordered_map<uuids::uuid, bool> s_showSubject;

  if (s_firstRun)
  {
    // Show all images by default:
    for (const auto& imageUid : appData.imageUidsOrdered())
    {
      s_showSubject.insert({imageUid, true});
    }

    s_firstRun = false;
  }

  auto contextMenu = [&appData, &getImageDisplayAndFileName]()
  {
    if (ImGui::BeginMenu("Show..."))
    {
      for (size_t imageIndex = 0; imageIndex < appData.numImages(); ++imageIndex)
      {
        const auto imageUid = appData.imageUid(imageIndex);
        if (!imageUid)
          continue;

        auto it = s_showSubject.find(*imageUid);
        if (std::end(s_showSubject) == it)
        {
          s_showSubject.insert({*imageUid, true});
        }

        bool& visible = s_showSubject[*imageUid];
        const auto names = getImageDisplayAndFileName(imageIndex);

        if (ImGui::MenuItem(names.first, nullptr, visible))
        {
          visible = !visible;
        }

        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", names.second);
        }
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window"))
    {
      if (ImGui::BeginMenu("Position"))
      {
        if (ImGui::MenuItem("Custom", nullptr, corner == -1))
          corner = -1;
        if (ImGui::MenuItem("Top-left", nullptr, corner == 0))
          corner = 0;
        if (ImGui::MenuItem("Top-right", nullptr, corner == 1))
          corner = 1;
        if (ImGui::MenuItem("Bottom-left", nullptr, corner == 2))
          corner = 2;
        if (ImGui::MenuItem("Bottom-right", nullptr, corner == 3))
          corner = 3;
        ImGui::EndMenu();
      }

      if (ImGui::MenuItem("Show title bar", nullptr, s_showTitleBar))
      {
        s_showTitleBar = !s_showTitleBar;
      }

      ImGui::Separator();
      if (appData.guiData().m_showInspectionWindow && ImGui::MenuItem("Close"))
      {
        appData.guiData().m_showInspectionWindow = false;
      }

      ImGui::EndMenu();
    }

    //        ImGui::EndPopup();
  };

  auto showSelectionButton = []()
  {
    ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
    if (ImGui::Button("..."))
    {
      ImGui::OpenPopup("selectionPopup");
    }
    ImGui::PopStyleColor(1);

    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("Select image(s) to inspect");
    }
  };

  ImGuiWindowFlags windowFlags = sk_windowFlags;

  if (corner != -1)
  {
    windowFlags |= ImGuiWindowFlags_NoMove;

    ImGuiIO& io = ImGui::GetIO();

    const ImVec2 windowPos(
      (corner & 1) ? io.DisplaySize.x - sk_pad : sk_pad,
      (corner & 2) ? io.DisplaySize.y - sk_pad : sk_pad
    );

    const ImVec2 windowPosPivot((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPosPivot);
  }

  if (!s_showTitleBar)
  {
    windowFlags |= ImGuiWindowFlags_NoDecoration;
  }

  const ImVec4* colors = ImGui::GetStyle().Colors;
  ImVec4 menuBarBgColor = colors[ImGuiCol_MenuBarBg];
  menuBarBgColor.w /= 2.0f;

  ImGui::SetNextWindowBgAlpha(0.0f); // Transparent background

  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, sk_cellPadding);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, sk_framePadding);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, sk_itemInnerSpacing);
  ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, sk_windowPadding);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, sk_windowRounding);

  if (ImGui::Begin(
        "Voxel Inspector##InspectionWindow", &(appData.guiData().m_showInspectionWindow), windowFlags
      ))
  {
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, menuBarBgColor);
    if (ImGui::BeginMenuBar())
    {
      contextMenu();
      ImGui::EndMenuBar();
    }
    ImGui::PopStyleColor(1); // ImGuiCol_MenuBarBg

    if (ImGui::BeginTable("Image Information", 7, sk_tableFlags))
    {
      ImGui::TableSetupScrollFreeze(1, 1);

      // The default widths are approximate
      ImGui::TableSetupColumn("Image", ImGuiTableColumnFlags_WidthFixed, 150.0f);

      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 75.0f);
      ImGui::TableSetupColumn(
        "Value (interp.)",
        ImGuiTableColumnFlags_DefaultHide | ImGuiTableColumnFlags_WidthFixed,
        75.0f
      );
      ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 50.0f);
      ImGui::TableSetupColumn(
        "Region", ImGuiTableColumnFlags_DefaultHide | ImGuiTableColumnFlags_WidthFixed, 100.0f
      );

      ImGui::TableSetupColumn("Voxel", ImGuiTableColumnFlags_WidthFixed, 125.0f);
      ImGui::TableSetupColumn("Subject (mm)", ImGuiTableColumnFlags_WidthFixed, 225.0f);

      ImGui::TableHeadersRow();

      for (size_t imageIndex = 0; imageIndex < appData.numImages(); ++imageIndex)
      {
        const auto imageUid = appData.imageUid(imageIndex);
        Image* image = (imageUid ? appData.image(*imageUid) : nullptr);
        if (!image)
          continue;

        auto it = s_showSubject.find(*imageUid);
        if (std::end(s_showSubject) == it)
        {
          s_showSubject.insert({*imageUid, true});
        }

        if (!s_showSubject[*imageUid])
          continue;

        ImGui::PushID(static_cast<int>(imageIndex)); /** PushID: imageIndex **/

        const auto segUid = appData.imageToActiveSegUid(*imageUid);
        const Image* seg = (segUid ? appData.seg(*segUid) : nullptr);

        ParcellationLabelTable* table
          = (seg ? getLabelTable(seg->settings().labelTableIndex()) : nullptr);

        // Get all image component values
        static constexpr bool sk_getOnlyActiveComponent = false;
        std::vector<double> imageValuesNN = getImageValuesNN(imageIndex, sk_getOnlyActiveComponent);
        std::vector<double> imageValuesLinear
          = getImageValuesLinear(imageIndex, sk_getOnlyActiveComponent);

        const std::optional<int64_t> segLabel = getSegLabel(imageIndex);

        const std::optional<glm::ivec3> voxelPos = getVoxelPos(imageIndex);
        const std::optional<glm::vec3> subjectPos = getSubjectPos(imageIndex);

        ImGui::TableNextColumn(); // "Image"

        glm::vec3 darkerBorderColorHsv = glm::hsvColor(image->settings().borderColor());
        darkerBorderColorHsv[2] = std::max(0.5f * darkerBorderColorHsv[2], 0.0f);
        const glm::vec3 darkerBorderColorRgb = glm::rgbColor(darkerBorderColorHsv);

        const ImVec4 inputTextBgColor(
          darkerBorderColorRgb.r, darkerBorderColorRgb.g, darkerBorderColorRgb.b, 1.0f
        );
        const ImVec4 inputTextFgColor = (glm::luminosity(darkerBorderColorRgb) < 0.75f)
                                          ? sk_whiteText
                                          : sk_blackText;

        ImGui::PushStyleColor(ImGuiCol_FrameBg, inputTextBgColor);
        ImGui::PushStyleColor(ImGuiCol_Text, inputTextFgColor);
        ImGui::PushItemWidth(-1);
        {
          std::string displayName = image->settings().displayName();
          if (ImGui::InputText("##displayName", &displayName))
          {
            image->settings().setDisplayName(displayName);
          }
        }
        ImGui::PopItemWidth();
        ImGui::PopStyleColor(2); // ImGuiCol_FrameBg, ImGuiCol_Text

        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", image->header().fileName().c_str());
        }

        ImGui::SameLine();
        showSelectionButton();

        ImGui::TableNextColumn(); // "Value (NN)"

        if (!imageValuesNN.empty())
        {
          if (isComponentFloatingPoint(image->header().memoryComponentType()))
          {
            if (image->header().numComponentsPerPixel() > 1)
            {
              ImGui::PushItemWidth(-1);
              ImGui::InputScalarN(
                "##imageValuesNN",
                ImGuiDataType_Double,
                imageValuesNN.data(),
                imageValuesNN.size(),
                nullptr,
                nullptr,
                appData.guiData().m_imageValuePrecisionFormat.c_str(),
                ImGuiInputTextFlags_ReadOnly
              );
              ImGui::PopItemWidth();

              if (ImGui::IsItemHovered())
              {
                ImGui::SetTooltip("Active component: %d", image->settings().activeComponent());
              }
            }
            else
            {
              double a = imageValuesNN[0];

              ImGui::PushItemWidth(-1);
              ImGui::InputScalar(
                "##imageValuesNN",
                ImGuiDataType_Double,
                &a,
                nullptr,
                nullptr,
                appData.guiData().m_imageValuePrecisionFormat.c_str(),
                ImGuiInputTextFlags_ReadOnly
              );
              ImGui::PopItemWidth();
            }
          }
          else
          {
            if (image->header().numComponentsPerPixel() > 1)
            {
              std::vector<int64_t> imageValuesNNInt;

              for (size_t i = 0; i < imageValuesNN.size(); ++i)
              {
                imageValuesNNInt.push_back(static_cast<int64_t>(imageValuesNN[i]));
              }

              ImGui::PushItemWidth(-1);
              ImGui::InputScalarN(
                "##imageValuesNN",
                ImGuiDataType_S64,
                imageValuesNNInt.data(),
                imageValuesNNInt.size(),
                nullptr,
                nullptr,
                "%ld",
                ImGuiInputTextFlags_ReadOnly
              );
              ImGui::PopItemWidth();

              if (ImGui::IsItemHovered())
              {
                ImGui::SetTooltip("Active component: %d", image->settings().activeComponent());
              }
            }
            else
            {
              int64_t a = static_cast<int64_t>(imageValuesNN[0]);

              ImGui::PushItemWidth(-1);
              ImGui::InputScalar(
                "##imageValuesNN",
                ImGuiDataType_S64,
                &a,
                nullptr,
                nullptr,
                "%ld",
                ImGuiInputTextFlags_ReadOnly
              );
              ImGui::PopItemWidth();
            }
          }
        }
        else
        {
          ImGui::Text("<N/A>");
        }

        ImGui::TableNextColumn(); // "Value (Linear)"

        if (!imageValuesLinear.empty())
        {
          // Display linearly interpolated image values using doubles for both floating point and integer images
          //if ( isComponentFloatingPoint( image->header().memoryComponentType() ) )
          {
            if (image->header().numComponentsPerPixel() > 1)
            {
              ImGui::PushItemWidth(-1);
              ImGui::InputScalarN(
                "##imageValuesLinear",
                ImGuiDataType_Double,
                imageValuesLinear.data(),
                imageValuesLinear.size(),
                nullptr,
                nullptr,
                appData.guiData().m_imageValuePrecisionFormat.c_str(),
                ImGuiInputTextFlags_ReadOnly
              );
              ImGui::PopItemWidth();

              if (ImGui::IsItemHovered())
              {
                ImGui::SetTooltip("Active component: %d", image->settings().activeComponent());
              }
            }
            else
            {
              double a = imageValuesLinear[0];

              ImGui::PushItemWidth(-1);
              ImGui::InputScalar(
                "##imageValuesLinear",
                ImGuiDataType_Double,
                &a,
                nullptr,
                nullptr,
                appData.guiData().m_imageValuePrecisionFormat.c_str(),
                ImGuiInputTextFlags_ReadOnly
              );
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
                                imageValuesLinearInt.push_back( static_cast<int64_t>( imageValuesLinear[i] ) );
                            }

                            ImGui::PushItemWidth( -1 );
                            ImGui::InputScalarN( "##imageValuesLinear", ImGuiDataType_S64,
                                                imageValuesLinearInt.data(), imageValuesLinearInt.size(),
                                                nullptr, nullptr, "%ld", ImGuiInputTextFlags_ReadOnly );
                            ImGui::PopItemWidth();

                            if ( ImGui::IsItemHovered() )
                            {
                                ImGui::SetTooltip( "Active component: %d", image->settings().activeComponent() );
                            }
                        }
                        else
                        {
                            int64_t a = static_cast<int64_t>( imageValuesLinear[0] );

                            ImGui::PushItemWidth( -1 );
                            ImGui::InputScalar( "##imageValuesLinear", ImGuiDataType_S64, &a, nullptr, nullptr, "%ld",
                                               ImGuiInputTextFlags_ReadOnly );
                            ImGui::PopItemWidth();
                        }
                    }
                    */
        }
        else
        {
          ImGui::Text("<N/A>");
        }

        if (segLabel)
        {
          ImGui::TableNextColumn(); // "Label"

          // Segmentation labels are unsigned, so we can cast:
          uint64_t l = static_cast<uint64_t>(*segLabel);
          ImGui::PushItemWidth(-1);
          ImGui::InputScalar("##segLabel", ImGuiDataType_U64, &l, nullptr, nullptr, "%ld");
          ImGui::PopItemWidth();

          if (table)
          {
            std::string labelName = table->getName(l);

            if (ImGui::IsItemHovered())
            {
              // Tooltip for the segmentation label
              ImGui::SetTooltip("%s", labelName.c_str());
            }

            ImGui::TableNextColumn(); // "Region"

            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##labelName", &labelName))
            {
              table->setName(l, labelName);
            }
            ImGui::PopItemWidth();
          }
          else
          {
            ImGui::TableNextColumn(); // "Region"
            ImGui::Text("<N/A>");
          }
        }
        else
        {
          ImGui::TableNextColumn(); // "Label"
          ImGui::Text("<N/A>");

          ImGui::TableNextColumn(); // "Region"
          ImGui::Text("<N/A>");
        }

        if (voxelPos)
        {
          static const glm::ivec3 sk_zero{0};
          static const glm::ivec3 sk_minDim{0};

          ImGui::TableNextColumn(); // "Voxel"

          const glm::ivec3 sk_maxDim = static_cast<glm::ivec3>(image->header().pixelDimensions())
                                       - glm::ivec3{1, 1, 1};

          glm::ivec3 a = *voxelPos;
          ImGui::PushItemWidth(-1);
          if (ImGui::DragScalarN(
                "##voxelPos",
                ImGuiDataType_S32,
                glm::value_ptr(a),
                3,
                1.0f,
                glm::value_ptr(sk_minDim),
                glm::value_ptr(sk_maxDim),
                "%d"
              ))
          {
            if ( glm::all( glm::greaterThanEqual( a, sk_zero ) ) &&
                             glm::all( glm::lessThan( a, glm::ivec3{ image->header().pixelDimensions() } ) ) )
            {
              setVoxelPos(imageIndex, a);
            }
          }
          ImGui::PopItemWidth();

          if (ImGui::IsItemHovered())
          {
            ImGui::SetTooltip("Voxel index (i: column, j: row, k: slice)");
          }
        }
        else
        {
          ImGui::TableNextColumn(); // "Voxel"
          ImGui::Text("<N/A>");
        }

        if (subjectPos)
        {
          ImGui::TableNextColumn(); // "Physical"

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
                appData.guiData().m_coordsPrecisionFormat.c_str()
              ))
          {
            setSubjectPos(imageIndex, a);
          }
          ImGui::PopItemWidth();

          if (ImGui::IsItemHovered())
          {
            ImGui::SetTooltip("Physical subject-space coordinate (x: R->L, y: A->P, z: I->S)");
          }
        }
        else
        {
          ImGui::TableNextColumn(); // "Physical"
          ImGui::Text("<N/A>");
        }

        ImGui::PopID(); /** PopID: imageIndex **/
      }

      ImGui::EndTable();
    }

    if (ImGui::BeginPopupContextWindow())
    {
      // Show context menu on right-button click:
      contextMenu();
    }
    else if (ImGui::BeginPopup("selectionPopup"))
    {
      // Show context menu if the user has clicked the popup button:
      contextMenu();
      ImGui::EndPopup();
    }

    ImGui::End();
  }

  // ImGuiStyleVar_CellPadding
  // ImGuiStyleVar_FramePadding
  // ImGuiStyleVar_ItemInnerSpacing
  // ImGuiStyleVar_ScrollbarSize
  // ImGuiStyleVar_WindowBorderSize
  // ImGuiStyleVar_WindowPadding
  // ImGuiStyleVar_WindowRounding
  ImGui::PopStyleVar(7);
}

void renderOpacityBlenderWindow(
  AppData& appData, const std::function<void(const uuids::uuid& imageUid)>& updateImageUniforms
)
{
  /// @todo Use the "Drag and drop to copy/swap items" ImGui demo in order to allow reordering image layers
  /// by dragging the opacity sliders

  RenderData& renderData = appData.renderData();

  static const char* windowName = "Image Opacity Mixer";

  if (!appData.guiData().m_showOpacityBlenderWindow)
    return;

  const bool showWindow = ImGui::Begin(
    windowName,
    &(appData.guiData().m_showOpacityBlenderWindow),
    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize
  );

  if (!showWindow)
  {
    ImGui::End();
    return;
  }

  int imageIndex = 0;

  for (const auto& imageUid : appData.imageUidsOrdered())
  {
    Image* image = appData.image(imageUid);
    if (!image)
      continue;

    ImageSettings& imgSettings = image->settings();
    const ImageHeader& imgHeader = image->header();

    const glm::vec3 borderColorHsv = glm::hsvColor(imgSettings.borderColor());

    const float hue = borderColorHsv[0];
    const float sat = borderColorHsv[1];
    const float val = borderColorHsv[2];

    const glm::vec3 frameBgColor = glm::rgbColor(glm::vec3{hue, 0.5f * sat, 0.5f * val});
    const glm::vec3 frameBgActiveColor = glm::rgbColor(glm::vec3{hue, 0.7f * sat, 0.5f * val});
    const glm::vec3 frameBgHoveredColor = glm::rgbColor(glm::vec3{hue, 0.6f * sat, 0.5f * val});
    const glm::vec3 sliderGrabColor = glm::rgbColor(glm::vec3{hue, sat, val});

    ImGui::PushID(imageIndex);

    ImGui::PushStyleColor(
      ImGuiCol_FrameBg, ImVec4(frameBgColor.r, frameBgColor.g, frameBgColor.b, 1.0f)
    );
    ImGui::PushStyleColor(
      ImGuiCol_FrameBgActive,
      ImVec4(frameBgActiveColor.r, frameBgActiveColor.g, frameBgActiveColor.b, 1.0f)
    );
    ImGui::PushStyleColor(
      ImGuiCol_FrameBgHovered,
      ImVec4(frameBgHoveredColor.r, frameBgHoveredColor.g, frameBgHoveredColor.b, 1.0f)
    );
    ImGui::PushStyleColor(
      ImGuiCol_SliderGrab, ImVec4(sliderGrabColor.r, sliderGrabColor.g, sliderGrabColor.b, 1.0f)
    );

    const std::string name = imgSettings.displayName() + "##" + std::to_string(imageIndex);

    if (imgSettings.displayImageAsColor())
    {
      double opacity = imgSettings.globalOpacity();

      if (mySliderF64(name.c_str(), &opacity, 0.0, 1.0) && !renderData.m_opacityMixMode)
      {
        imgSettings.setGlobalOpacity(opacity);
        updateImageUniforms(imageUid);
      }
    }
    else
    {
      double opacity = imgSettings.opacity();

      if (mySliderF64(name.c_str(), &opacity, 0.0, 1.0) && !renderData.m_opacityMixMode)
      {
        imgSettings.setOpacity(opacity);
        updateImageUniforms(imageUid);
      }
    }

    if (ImGui::IsItemActive() || ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("%s", imgHeader.fileName().c_str());
    }

    // ImGuiCol_FrameBg
    // ImGuiCol_FrameBgActive
    // ImGuiCol_FrameBgHovered
    // ImGuiCol_SliderGrab
    ImGui::PopStyleColor(4);
    ImGui::PopID();
  }

  static double mix = 0.0;

  if (appData.numImages() > 1)
  {
    ImGui::Checkbox("Comparison blender", &(renderData.m_opacityMixMode));
    ImGui::SameLine();
    helpMarker("Use a single slider to blend across all adjacent image layers");
  }
  else
  {
    renderData.m_opacityMixMode = false;
  }

  if (renderData.m_opacityMixMode)
  {
    mySliderF64("Blend", &mix, 0.0, static_cast<double>(appData.numImages() - 1));

    const double imgIndex = mix;

    const double frac = imgIndex - std::floor(imgIndex);

    size_t imgIndexLo = static_cast<size_t>(std::floor(imgIndex));
    size_t imgIndexHi = static_cast<size_t>(std::ceil(imgIndex));

    for (size_t i = 0; i < appData.numImages(); ++i)
    {
      const auto imgUid = appData.imageUid(i);
      if (!imgUid)
        continue;

      Image* img = appData.image(*imgUid);
      if (!img)
        continue;

      double op = 0.0;

      if (i < imgIndexLo || imgIndexHi < i)
      {
        op = 0.0;
      }
      else if (imgIndexLo == i)
      {
        op = 1.0 - frac;
      }
      else if (imgIndexHi == i)
      {
        op = frac;
      }

      if (img->settings().displayImageAsColor())
      {
        img->settings().setGlobalOpacity(op);
      }
      else
      {
        img->settings().setOpacity(op);
      }

      updateImageUniforms(*imgUid);
    }
  }

  ImGui::End();
}
