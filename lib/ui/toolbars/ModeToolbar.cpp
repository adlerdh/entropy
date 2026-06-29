#include "ui/toolbars/Toolbars.h"
#include "ui/toolbars/ToolbarCommon.h"
#include "ui/GuiData.h"
#include "ui/Helpers.h"
#include "ui/widgets/Widgets.h"

#include "logic/app/Data.h"
#include "logic/states/FsmList.hpp"
#include "logic/states/annotation/AnnotationStateHelpers.h"

#include <IconsForkAwesome.h>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>
#include <imgui/imgui.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

using namespace entropy::ui::toolbars;

void renderModeToolbar(
  AppData& appData,
  const std::function<MouseMode(void)>& getMouseMode,
  const std::function<void(MouseMode)>& setMouseMode,
  const std::function<void(void)>& readjustViewport,
  const AllViewsRecenterType& recenterAllViews,
  const std::function<bool(void)>& getOverlayVisibility,
  const std::function<void(bool)>& setOverlayVisibility,
  const std::function<void(int step)>& cycleViews,
  size_t numImages,
  const std::function<std::pair<std::string, std::string>(size_t index)>& getImageDisplayAndFileName,
  const std::function<size_t(void)>& getActiveImageIndex,
  const std::function<void(size_t)>& setActiveImageIndex)
{
  GuiData& guiData = appData.guiData();

  const auto buttonSize = scaledToolbarButtonSize(appData.windowData().getContentScaleRatios());
  const auto padSize = scaledPad(appData.windowData().getContentScaleRatios());

  static bool s_lastShowState = guiData.m_showModeToolbar;

  if (!guiData.m_showModeToolbar) {
    readjustViewport();
    s_lastShowState = false;
    return;
  }

  // Always keep the toolbar open by setting this to null
  static bool* toolbarWindowOpen = nullptr;

  const ImVec4* colors = ImGui::GetStyle().Colors;
  ImVec4 activeColor = colors[ImGuiCol_ButtonActive];
  ImVec4 inactiveColor = colors[ImGuiCol_Button];
  //    ImVec4 highlightColor( 0.64f, 0.44f, 0.64f, 0.40f );

  activeColor.w = 0.94f;
  inactiveColor.w = 0.7f;

  ImGuiIO& io = ImGui::GetIO();
  //    ImGuiWindowFlags windowFlags = k_toolbarWindowFlags;

  const bool isHoriz = guiData.m_isModeToolbarHorizontal;
  const int corner = guiData.m_modeToolbarCorner;

  const ImVec2 buttonSpace = (appData.guiData().m_isModeToolbarHorizontal ? ImVec2(2.0f, 0.0f) : ImVec2(0.0f, 2.0f));

  if (corner != -1) {
    //        windowFlags |= ImGuiWindowFlags_NoMove;

    ImVec2 windowPos;
    if (guiData.m_renderViewport) {
      windowPos = toolbarWindowPositionForViewport(*guiData.m_renderViewport, io.DisplaySize.y, corner, padSize);
    }
    else {
      windowPos = ImVec2(
        (corner & 1) ? io.DisplaySize.x - padSize.x : padSize.x,
        (corner & 2) ? io.DisplaySize.y - padSize.y : padSize.y);

      if (0 == corner || 1 == corner) {
        windowPos.y += guiData.topDockOffset();
      }
      else if (2 == corner || 3 == corner) {
        windowPos.y -= guiData.bottomDockOffset();
      }
    }

    const ImVec2 windowPosPivot((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPosPivot);
  }

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

  ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, activeColor);

  //    static bool isCollapsed = false;

  /// @note The trick with isCollapsed does not work: the toolbar is too narrow in vertical
  /// orientation to show the text "Tools".

  const char* title = ((isHoriz /*| isCollapsed*/) ? "Tools###ToolbarWindow" : "###ToolbarWindow");

  ImGui::PushID("toolbar");

  if (ImGui::Begin(title, toolbarWindowOpen, k_toolbarWindowFlags)) {
    //        isCollapsed = false;

    if (s_lastShowState != guiData.m_showModeToolbar) {
      const ImVec2 winSize = ImGui::GetContentRegionAvail();
      guiData.m_modeToolbarDockDims = glm::vec2{winSize.x, winSize.y} + 2.0f * glm::vec2{padSize.x, padSize.y};
      readjustViewport();
      s_lastShowState = guiData.m_showModeToolbar;
    }

    int id = 0;

    const MouseMode activeMouseMode = getMouseMode();

    for (const MouseMode& mouseMode : AllMouseModes) {
      ImGui::PushID(id);
      {
        bool isModeActive = (activeMouseMode == mouseMode);

        if (isHoriz) {
          ImGui::SameLine();
        }

        ImGui::PushStyleColor(ImGuiCol_Button, (isModeActive ? activeColor : inactiveColor));
        {
          if (ImGui::Button(toolbarButtonIcon(mouseMode), buttonSize)) {
            isModeActive = !isModeActive;
            if (isModeActive) {
              setMouseMode(mouseMode);
            }
          }

          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", typeString(mouseMode).c_str());
          }

          if (
            MouseMode::CameraTranslate == mouseMode || MouseMode::CrosshairsRotate == mouseMode ||
            MouseMode::Annotate == mouseMode)
          {
            // Put a small dummy space after these buttons
            if (isHoriz) {
              ImGui::SameLine();
            }
            ImGui::Dummy(buttonSpace);
          }
        }

        ImGui::PopStyleColor(1); // ImGuiCol_Button
        ++id;
      }
      ImGui::PopID();
    }

    // These are not checkable (toggle) buttons, so style them using the
    // inactive button color
    ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
    {
      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::Dummy(buttonSpace);

      if (isHoriz) {
        ImGui::SameLine();
      }

      if (ImGui::Button(ICON_FK_PICTURE_O, buttonSize)) {
        ImGui::OpenPopup("imagePopup");
      }

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "Set active image");
      }

      if (ImGui::BeginPopup("imagePopup")) {
        const size_t activeIndex = getActiveImageIndex();

        for (size_t i = 0; i < numImages; ++i) {
          ImGui::PushID(static_cast<int>(i));
          {
            const auto displayAndFileName = getImageDisplayAndFileName(i);

            bool isSelected = (i == activeIndex);
            if (ImGui::MenuItem(displayAndFileName.first.c_str(), "", &isSelected)) {
              if (isSelected) {
                setActiveImageIndex(i);
                ImGui::SetItemDefaultFocus();
              }
            }

            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip("%s", displayAndFileName.second.c_str());
            }
          }
          ImGui::PopID(); // i
        }

        ImGui::EndPopup();
      }

      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::PushID(id);
      {
        ImGui::PushStyleColor(ImGuiCol_Button, (guiData.m_showImagePropertiesWindow ? activeColor : inactiveColor));
        {
          if (ImGui::Button(ICON_FK_SLIDERS, buttonSize)) {
            guiData.m_showImagePropertiesWindow = !guiData.m_showImagePropertiesWindow;
          }

          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", "Show Image Panel");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();

      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::PushID(id);
      {
        ImGui::PushStyleColor(ImGuiCol_Button, (guiData.m_showSegmentationsWindow ? activeColor : inactiveColor));
        {
          if (ImGui::Button(ICON_FK_STAR_O, buttonSize)) // ICON_FK_LIST_OL
          {
            guiData.m_showSegmentationsWindow = !guiData.m_showSegmentationsWindow;
          }

          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", "Show Segmentation Panel");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();

      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::PushID(id);
      {
        ImGui::PushStyleColor(ImGuiCol_Button, (guiData.m_showRegistrationSetupWindow ? activeColor : inactiveColor));
        {
          if (ImGui::Button(ICON_FK_RANDOM, buttonSize)) {
            guiData.m_showRegistrationSetupWindow = !guiData.m_showRegistrationSetupWindow;
          }

          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", "Show Image Registration Panel");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();

      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::PushID(id);
      {
        ImGui::PushStyleColor(ImGuiCol_Button, (guiData.m_showAnnotationsWindow ? activeColor : inactiveColor));
        {
          if (ImGui::Button(ICON_FK_OBJECT_UNGROUP, buttonSize)) // ICON_FK_STAR_O
          {
            guiData.m_showAnnotationsWindow = !guiData.m_showAnnotationsWindow;
          }

          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", "Show Annotation Panel");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();

      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::PushID(id);
      {
        ImGui::PushStyleColor(ImGuiCol_Button, (guiData.m_showLandmarksWindow ? activeColor : inactiveColor));
        {
          if (ImGui::Button(ICON_FK_MAP_MARKER, buttonSize)) {
            guiData.m_showLandmarksWindow = !guiData.m_showLandmarksWindow;
          }

          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", "Show Landmark Panel");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();

      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::PushID(id);
      {
        ImGui::PushStyleColor(ImGuiCol_Button, (guiData.m_showIsosurfacesWindow ? activeColor : inactiveColor));
        {
          if (ImGui::Button(ICON_FK_SHIP, buttonSize)) {
            guiData.m_showIsosurfacesWindow = !guiData.m_showIsosurfacesWindow;
          }

          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", "Show Isosurfaces Panel");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();

      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::PushID(id);
      {
        ImGui::PushStyleColor(ImGuiCol_Button, (guiData.m_showSettingsWindow ? activeColor : inactiveColor));
        {
          if (ImGui::Button(ICON_FK_COGS, buttonSize)) {
            guiData.m_showSettingsWindow = !guiData.m_showSettingsWindow;
          }

          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", "Show Application Settings");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();

      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::PushID(id);
      {
        ImGui::PushStyleColor(ImGuiCol_Button, (guiData.m_showInspectionWindow ? activeColor : inactiveColor));
        {
          if (ImGui::Button(ICON_FK_INFO_CIRCLE, buttonSize)) {
            guiData.m_showInspectionWindow = !guiData.m_showInspectionWindow;
          }

          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", "Show Voxel Inspector Panel");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();

      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::Dummy(buttonSpace);

      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::PushID(id);
      {
        if (ImGui::Button(ICON_FK_CROSSHAIRS, buttonSize)) {
          // Shift does a "hard" reset of the crosshairs, oblique orientations, rotated crosshairs,
          // and zoom
          const bool hardReset = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
          const bool recenterCrosshairs = hardReset;
          const bool realignCrosshairs = hardReset;
          const bool resetObliqueOrientation = hardReset;
          static constexpr bool recenterOnCurrentCrosshairsPosition = true;

          const bool resetZoom = hardReset ? true : recenterOnCurrentCrosshairsPosition;

          recenterAllViews(
            recenterCrosshairs,
            realignCrosshairs,
            recenterOnCurrentCrosshairsPosition,
            resetObliqueOrientation,
            resetZoom);
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Recenter views (C)");
        }
        ++id;
      }
      ImGui::PopID();

      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::PushID(id);
      {
        bool isOverlayVisible = getOverlayVisibility();

        ImGui::PushStyleColor(ImGuiCol_Button, (isOverlayVisible ? activeColor : inactiveColor));
        {
          if (ImGui::Button(ICON_FK_CLONE, buttonSize)) {
            isOverlayVisible = !isOverlayVisible;
            setOverlayVisibility(isOverlayVisible);
          }

          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", "Toggle view overlays (O)");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();

      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::PushID(id);
      {
        //                ImGui::PushStyleColor( ImGuiCol_Button, highlightColor );
        {
          if (ImGui::Button(ICON_FK_CHEVRON_LEFT, buttonSize)) {
            cycleViews(-1);
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", "Previous layout ([)");
          }
        }
        //                ImGui::PopStyleColor( 1 ); // ImGuiCol_Button
        ++id;
      }
      ImGui::PopID();

      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::PushID(id);
      {
        //                ImGui::PushStyleColor( ImGuiCol_Button, highlightColor );
        {
          if (ImGui::Button(ICON_FK_CHEVRON_RIGHT, buttonSize)) {
            cycleViews(1);
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", "Next layout (])");
          }
        }
        //                ImGui::PopStyleColor( 1 ); // ImGuiCol_Button
        ++id;
      }
      ImGui::PopID();

      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::PushID(id);
      {
        bool syncEnabled = appData.settings().entropyInstanceSyncEnabled();
        ImGui::PushStyleColor(ImGuiCol_Button, (syncEnabled ? activeColor : inactiveColor));
        {
          if (ImGui::Button(ICON_FK_EXCHANGE, buttonSize)) {
            appData.settings().setEntropyInstanceSyncEnabled(!syncEnabled);
          }

          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", "Synchronize crosshairs position between Entropy instances");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();
    }
    ImGui::PopStyleColor(1); // ImGuiCol_Button

    //        isCollapsed = false;

    // Save the new toolbar size:
    const ImVec2 winSize = ImGui::GetContentRegionAvail();
    guiData.m_modeToolbarDockDims = glm::vec2{winSize.x, winSize.y} + 2.0f * glm::vec2{padSize.x, padSize.y};

    if (ImGui::BeginPopupContextWindow()) {
      renderPlacementContextMenu(guiData.m_modeToolbarCorner, guiData.m_isModeToolbarHorizontal);
      readjustViewport();
      ImGui::EndPopup();
    }
  }
  //    else
  //    {
  //        isCollapsed = true;
  //    }

  ImGui::End(); // End toolbar

  // ImGuiCol_TitleBgCollapsed
  ImGui::PopStyleColor(1);

  // ImGuiStyleVar_FramePadding, ImGuiStyleVar_ItemSpacing,
  // ImGuiStyleVar_WindowBorderSize, ImGuiStyleVar_WindowPadding,
  // ImGuiStyleVar_FrameRounding, ImGuiStyleVar_WindowRounding
  ImGui::PopStyleVar(6);

  ImGui::PopID();
}
