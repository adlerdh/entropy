#include "ui/Toolbars.h"
#include "ui/GuiData.h"
#include "ui/Helpers.h"
#include "ui/Popups.h"
#include "ui/Widgets.h"

#include "logic/app/Data.h"
#include "logic/states/AnnotationStateHelpers.h"
#include "logic/states/FsmList.hpp"

#include <IconFontCppHeaders/IconsForkAwesome.h>

#include <imgui/imgui.h>

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

namespace
{

static const ImVec4 sk_darkTextColor(0.0f, 0.0f, 0.0f, 1.0f);
static const ImVec4 sk_lightTextColor(1.0f, 1.0f, 1.0f, 1.0f);

static const ImGuiWindowFlags sk_toolbarWindowFlags = 0 | ImGuiWindowFlags_AlwaysAutoResize
                                                      | ImGuiWindowFlags_NoFocusOnAppearing
                                                      | ImGuiWindowFlags_NoResize
                                                      | ImGuiWindowFlags_NoScrollbar
                                                      | ImGuiWindowFlags_NoBackground
                                                      | ImGuiWindowFlags_NoNav;

/**
 * @brief renderPlacementContextMenu
 * @param corner
 * @param isHoriz
 */
void renderPlacementContextMenu(int& corner, bool& /*isHoriz*/)
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

  //    if ( ImGui::BeginMenu( "Orientation" ) )
  //    {
  //        if ( ImGui::MenuItem( "Horizontal", nullptr, ( isHoriz ) ) ) isHoriz = ! isHoriz;
  //        if ( ImGui::MenuItem( "Vertical", nullptr, ( ! isHoriz ) ) ) isHoriz = ! isHoriz;
  //        ImGui::EndMenu();
  //    }

  ImGui::EndPopup();
}

ImVec2 scaledToolbarButtonSize(const glm::vec2& contentScale)
{
  static const ImVec2 sk_toolbarButtonSize(32, 32);
  return ImVec2{contentScale.x * sk_toolbarButtonSize.x, contentScale.y * sk_toolbarButtonSize.y};
}

ImVec2 scaledPad(const glm::vec2& contentScale)
{
  static constexpr float sk_pad = 8.0f;
  return ImVec2{contentScale.x * sk_pad, contentScale.y * sk_pad};
}

} // namespace

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
  const std::function<std::pair<const char*, const char*>(size_t index)>& getImageDisplayAndFileName,
  const std::function<size_t(void)>& getActiveImageIndex,
  const std::function<void(size_t)>& setActiveImageIndex
)
{
  GuiData& guiData = appData.guiData();

  const auto buttonSize = scaledToolbarButtonSize(appData.windowData().getContentScaleRatios());
  const auto padSize = scaledPad(appData.windowData().getContentScaleRatios());

  static bool s_lastShowState = guiData.m_showModeToolbar;

  if (!guiData.m_showModeToolbar)
  {
    readjustViewport();
    s_lastShowState = false;
    return;
  }

  // Always keep the toolbar open by setting this to null
  static bool* toolbarWindowOpen = nullptr;

  bool openAddLayoutPopup = false;
  bool openAboutDialogPopup = false;

  const ImVec4* colors = ImGui::GetStyle().Colors;
  ImVec4 activeColor = colors[ImGuiCol_ButtonActive];
  ImVec4 inactiveColor = colors[ImGuiCol_Button];
  //    ImVec4 highlightColor( 0.64f, 0.44f, 0.64f, 0.40f );

  activeColor.w = 0.94f;
  inactiveColor.w = 0.7f;

  ImGuiIO& io = ImGui::GetIO();
  //    ImGuiWindowFlags windowFlags = sk_toolbarWindowFlags;

  const bool isHoriz = guiData.m_isModeToolbarHorizontal;
  const int corner = guiData.m_modeToolbarCorner;

  const ImVec2 buttonSpace
    = (appData.guiData().m_isModeToolbarHorizontal ? ImVec2(2.0f, 0.0f) : ImVec2(0.0f, 2.0f));

  if (corner != -1)
  {
    //        windowFlags |= ImGuiWindowFlags_NoMove;

    ImVec2 windowPos(
      (corner & 1) ? io.DisplaySize.x - padSize.x : padSize.x,
      (corner & 2) ? io.DisplaySize.y - padSize.y : padSize.y
    );

    if (guiData.m_showMainMenuBar)
    {
      if (0 == corner || 1 == corner)
      {
        windowPos.y += guiData.m_mainMenuBarDims.y;
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

  /// @note The trick with isCollapsed does not work: the toolbar is too narrow in vertical orientation
  /// to show the text "Tools".

  const char* title = ((isHoriz /*| isCollapsed*/) ? "Tools###ToolbarWindow" : "###ToolbarWindow");

  ImGui::PushID("toolbar");

  if (ImGui::Begin(title, toolbarWindowOpen, sk_toolbarWindowFlags))
  {
    //        isCollapsed = false;

    if (s_lastShowState != guiData.m_showModeToolbar)
    {
      const ImVec2 winSize = ImGui::GetWindowSize();

      guiData.m_modeToolbarDockDims = glm::vec2{winSize.x, winSize.y}
                                      + 2.0f * glm::vec2{padSize.x, padSize.y};

      readjustViewport();

      s_lastShowState = guiData.m_showModeToolbar;
    }

    int id = 0;

    const MouseMode activeMouseMode = getMouseMode();

    for (const MouseMode& mouseMode : AllMouseModes)
    {
      ImGui::PushID(id); // PUSH ID

      bool isModeActive = (activeMouseMode == mouseMode);

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Button, (isModeActive ? activeColor : inactiveColor));
      {
        if (ImGui::Button(toolbarButtonIcon(mouseMode), buttonSize))
        {
          isModeActive = !isModeActive;
          if (isModeActive)
          {
            setMouseMode(mouseMode);
          }
        }

        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", typeString(mouseMode).c_str());
        }

        if (MouseMode::CameraZoom == mouseMode || MouseMode::Annotate == mouseMode)
        {
          // Put a small dummy space after these buttons
          if (isHoriz)
            ImGui::SameLine();
          ImGui::Dummy(buttonSpace);
        }
      }
      ImGui::PopStyleColor(1); // ImGuiCol_Button
      ++id;

      ImGui::PopID();
    }

    // These are not checkable (toggle) buttons, so style them using the
    // inactive button color
    ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor);
    {
      if (isHoriz)
        ImGui::SameLine();
      ImGui::Dummy(buttonSpace);

      if (isHoriz)
        ImGui::SameLine();
      if (ImGui::Button(ICON_FK_PICTURE_O, buttonSize))
      {
        ImGui::OpenPopup("imagePopup");
      }

      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("%s", "Set active image");
      }

      if (ImGui::BeginPopup("imagePopup"))
      {
        const size_t activeIndex = getActiveImageIndex();

        for (size_t i = 0; i < numImages; ++i)
        {
          ImGui::PushID(static_cast<int>(i));
          {
            const auto displayAndFileName = getImageDisplayAndFileName(i);

            bool isSelected = (i == activeIndex);
            if (ImGui::MenuItem(displayAndFileName.first, "", &isSelected))
            {
              if (isSelected)
              {
                setActiveImageIndex(i);
                ImGui::SetItemDefaultFocus();
              }
            }

            if (ImGui::IsItemHovered())
            {
              ImGui::SetTooltip("%s", displayAndFileName.second);
            }
          }
          ImGui::PopID(); // i
        }

        ImGui::EndPopup();
      }

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        ImGui::PushStyleColor(
          ImGuiCol_Button, (guiData.m_showImagePropertiesWindow ? activeColor : inactiveColor)
        );
        {
          if (ImGui::Button(ICON_FK_SLIDERS, buttonSize))
          {
            guiData.m_showImagePropertiesWindow = !guiData.m_showImagePropertiesWindow;
          }

          if (ImGui::IsItemHovered())
          {
            ImGui::SetTooltip("%s", "Show image properties");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        ImGui::PushStyleColor(
          ImGuiCol_Button, (guiData.m_showSegmentationsWindow ? activeColor : inactiveColor)
        );
        {
          //if ( ImGui::Button( ICON_FK_TH, buttonSize) )
          if (ImGui::Button(ICON_FK_LIST_OL, buttonSize))
          {
            guiData.m_showSegmentationsWindow = !guiData.m_showSegmentationsWindow;
          }

          if (ImGui::IsItemHovered())
          {
            ImGui::SetTooltip("%s", "Show segmentation properties");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        ImGui::PushStyleColor(
          ImGuiCol_Button, (guiData.m_showLandmarksWindow ? activeColor : inactiveColor)
        );
        {
          if (ImGui::Button(ICON_FK_MAP_MARKER, buttonSize))
          {
            guiData.m_showLandmarksWindow = !guiData.m_showLandmarksWindow;
          }

          if (ImGui::IsItemHovered())
          {
            ImGui::SetTooltip("%s", "Show landmark properties");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        ImGui::PushStyleColor(
          ImGuiCol_Button, (guiData.m_showAnnotationsWindow ? activeColor : inactiveColor)
        );
        {
          if (ImGui::Button(ICON_FK_STAR_O, buttonSize))
          {
            guiData.m_showAnnotationsWindow = !guiData.m_showAnnotationsWindow;
          }

          if (ImGui::IsItemHovered())
          {
            ImGui::SetTooltip("%s", "Show annotation properties");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        ImGui::PushStyleColor(
          ImGuiCol_Button, (guiData.m_showIsosurfacesWindow ? activeColor : inactiveColor)
        );
        {
          if (ImGui::Button(ICON_FK_SHIP, buttonSize))
          {
            guiData.m_showIsosurfacesWindow = !guiData.m_showIsosurfacesWindow;
          }

          if (ImGui::IsItemHovered())
          {
            ImGui::SetTooltip("%s", "Show isosurface properties");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        ImGui::PushStyleColor(
          ImGuiCol_Button, (guiData.m_showSettingsWindow ? activeColor : inactiveColor)
        );
        {
          if (ImGui::Button(ICON_FK_COGS, buttonSize))
          {
            guiData.m_showSettingsWindow = !guiData.m_showSettingsWindow;
          }

          if (ImGui::IsItemHovered())
          {
            ImGui::SetTooltip("%s", "Show settings");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        ImGui::PushStyleColor(
          ImGuiCol_Button, (guiData.m_showInspectionWindow ? activeColor : inactiveColor)
        );
        {
          if (ImGui::Button(ICON_FK_EYEDROPPER, buttonSize))
          {
            guiData.m_showInspectionWindow = !guiData.m_showInspectionWindow;
          }

          if (ImGui::IsItemHovered())
          {
            ImGui::SetTooltip("%s", "Show cursor inspector");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();

      if (isHoriz)
        ImGui::SameLine();
      ImGui::Dummy(buttonSpace);

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        if (ImGui::Button(ICON_FK_CROSSHAIRS, buttonSize))
        {
          // Shift does a "hard" reset of the crosshairs, oblique orientations, and zoom
          const bool hardReset = ImGui::IsKeyDown(ImGuiKey_LeftShift)
                                 || ImGui::IsKeyDown(ImGuiKey_RightShift);
          const bool recenterCrosshairs = hardReset;
          const bool resetObliqueOrientation = hardReset;
          static constexpr bool recenterOnCurrentCrosshairsPosition = true;

          std::optional<bool> resetZoom = std::nullopt;
          if (hardReset)
          {
            resetZoom = true;
          }

          recenterAllViews(
            recenterCrosshairs,
            recenterOnCurrentCrosshairsPosition,
            resetObliqueOrientation,
            resetZoom
          );
        }
        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Recenter views (C)");
        }
        ++id;
      }
      ImGui::PopID();

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        bool isOverlayVisible = getOverlayVisibility();

        ImGui::PushStyleColor(ImGuiCol_Button, (isOverlayVisible ? activeColor : inactiveColor));
        {
          if (ImGui::Button(ICON_FK_CLONE, buttonSize))
          {
            isOverlayVisible = !isOverlayVisible;
            setOverlayVisibility(isOverlayVisible);
          }

          if (ImGui::IsItemHovered())
          {
            ImGui::SetTooltip("%s", "Toggle view overlays (O)");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        //                ImGui::PushStyleColor( ImGuiCol_Button, highlightColor );
        {
          if (ImGui::Button(ICON_FK_CHEVRON_LEFT, buttonSize))
          {
            cycleViews(-1);
          }
          if (ImGui::IsItemHovered())
          {
            ImGui::SetTooltip("%s", "Previous layout ([)");
          }
        }
        //                ImGui::PopStyleColor( 1 ); // ImGuiCol_Button
        ++id;
      }
      ImGui::PopID();

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        //                ImGui::PushStyleColor( ImGuiCol_Button, highlightColor );
        {
          if (ImGui::Button(ICON_FK_CHEVRON_RIGHT, buttonSize))
          {
            cycleViews(1);
          }
          if (ImGui::IsItemHovered())
          {
            ImGui::SetTooltip("%s", "Next layout (])");
          }
        }
        //                ImGui::PopStyleColor( 1 ); // ImGuiCol_Button
        ++id;
      }
      ImGui::PopID();

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        if (ImGui::Button(ICON_FK_TH, buttonSize))
        {
          openAddLayoutPopup = true;
        }

        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Add new layout");
        }

        ++id;
      }
      ImGui::PopID();

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        if (ImGui::Button(ICON_FK_WINDOW_CLOSE_O, buttonSize))
        {
          auto& wd = appData.windowData();
          if (wd.numLayouts() >= 2)
          {
            // Only delete a layout if there are at least two, so that one is left.
            const size_t layoutToDelete = wd.currentLayoutIndex();
            wd.cycleCurrentLayout(-1);
            wd.removeLayout(layoutToDelete);
          }
        }

        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Remove current layout");
        }

        ++id;
      }
      ImGui::PopID();

      if (isHoriz)
        ImGui::SameLine();
      ImGui::Dummy(buttonSpace);

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        if (ImGui::Button(ICON_FK_INFO, buttonSize))
        {
          openAboutDialogPopup = true;
        }

        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "About Entropy");
        }

        ++id;
      }
      ImGui::PopID();
    }
    ImGui::PopStyleColor(1); // ImGuiCol_Button

    //        isCollapsed = false;

    // Save the new toolbar size:
    const ImVec2 winSize = ImGui::GetWindowSize();

    guiData.m_modeToolbarDockDims = glm::vec2{winSize.x, winSize.y}
                                    + 2.0f * glm::vec2{padSize.x, padSize.y};

    if (ImGui::BeginPopupContextWindow())
    {
      renderPlacementContextMenu(guiData.m_modeToolbarCorner, guiData.m_isModeToolbarHorizontal);

      readjustViewport();
    }

    ImGui::End(); // End toolbar
  }
  //    else
  //    {
  //        isCollapsed = true;
  //    }

  // ImGuiCol_TitleBgCollapsed
  ImGui::PopStyleColor(1);

  // ImGuiStyleVar_FramePadding, ImGuiStyleVar_ItemSpacing,
  // ImGuiStyleVar_WindowBorderSize, ImGuiStyleVar_WindowPadding,
  // ImGuiStyleVar_FrameRounding, ImGuiStyleVar_WindowRounding
  ImGui::PopStyleVar(6);

  ImGui::PopID();

  // Shift does a "hard" reset of the crosshairs, oblique orientations, and zoom
  const bool hardReset = ImGui::IsKeyDown(ImGuiKey_LeftShift)
                         || ImGui::IsKeyDown(ImGuiKey_RightShift);
  const bool recenterCrosshairs = hardReset;
  const bool resetObliqueOrientation = hardReset;
  static constexpr bool recenterOnCurrentCrosshairsPosition = true;

  std::optional<bool> resetZoom = std::nullopt;
  if (hardReset)
  {
    resetZoom = true;
  }

  renderAddLayoutModalPopup(
    appData,
    openAddLayoutPopup,
    [&recenterAllViews, &recenterCrosshairs, &resetObliqueOrientation, &resetZoom]()
    {
      recenterAllViews(
        recenterCrosshairs, recenterOnCurrentCrosshairsPosition, resetObliqueOrientation, resetZoom
      );
    }
  );

  renderAboutDialogModalPopup(openAboutDialogPopup);
}

void renderSegToolbar(
  AppData& appData,
  size_t numImages,
  const std::function<std::pair<const char*, const char*>(size_t index)>& getImageDisplayAndFileName,
  const std::function<size_t(void)>& getActiveImageIndex,
  const std::function<void(size_t)>& setActiveImageIndex,
  const std::function<bool(size_t imageIndex)>& getImageHasActiveSeg,
  const std::function<void(size_t imageIndex, bool set)>& setImageHasActiveSeg,
  const std::function<void(void)>& readjustViewport,
  const std::function<void(const uuids::uuid& imageUid)>& updateImageUniforms,
  const std::function<
    bool(const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType&)>&
    executeGraphCutsSeg,
  const std::function<
    bool(const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType&)>&
    executePoissonSeg
)
{
  // Show the segmentation toolbar in either Segmentation mode,
  // in Annotation mode (when the Fill button is also visible),
  // or when the Annotations Window is visible

  const bool inSegmentationMode = (MouseMode::Segment == appData.state().mouseMode());
  const bool inAnnotationMode
    = (state::isInStateWhereToolbarVisible() && state::showToolbarFillButton());

  GuiData& guiData = appData.guiData();

  const auto buttonSize = scaledToolbarButtonSize(appData.windowData().getContentScaleRatios());
  const auto padSize = scaledPad(appData.windowData().getContentScaleRatios());

  static bool s_lastShowState = guiData.m_showSegToolbar;

  guiData.m_showSegToolbar
    = (inSegmentationMode && !inAnnotationMode && !appData.guiData().m_showAnnotationsWindow);

  if (!guiData.m_showSegToolbar)
  {
    readjustViewport();
    s_lastShowState = false;
    return;
  }

  // Always keep the toolbar open by setting this to null
  static bool* toolbarWindowOpen = nullptr;

  const bool isHoriz = guiData.m_isSegToolbarHorizontal;
  const int corner = guiData.m_segToolbarCorner;

  const auto activeImageUid = appData.activeImageUid();
  if (!activeImageUid)
  {
    spdlog::error("There is no active image to segment");
    return;
  }

  const auto activeSegUid = appData.imageToActiveSegUid(*activeImageUid);
  if (!activeSegUid)
  {
    spdlog::error("There is no active segmentation for image {}", *activeImageUid);
    return;
  }

  const Image* activeSeg = appData.seg(*activeSegUid);
  if (!activeSeg)
  {
    spdlog::error("The active segmentation {} is null for image {}", *activeSegUid, *activeImageUid);
    return;
  }

  const size_t activeLabelTableIndex = activeSeg->settings().labelTableIndex();
  const auto activeLabelTableUid = appData.labelTableUid(activeLabelTableIndex);
  if (!activeLabelTableUid)
  {
    spdlog::error("There is no label table for active segmentation {}", *activeSegUid);
    return;
  }

  const ParcellationLabelTable* activeLabelTable = appData.labelTable(*activeLabelTableUid);

  if (!activeLabelTable)
  {
    spdlog::error(
      "The label table {} for active segmentation {} is null", *activeLabelTableUid, *activeSegUid
    );
    return;
  }

  const ImVec2 buttonSpace
    = (appData.guiData().m_isSegToolbarHorizontal ? ImVec2(2.0f, 0.0f) : ImVec2(0.0f, 2.0f));

  const ImVec4* colors = ImGui::GetStyle().Colors;
  ImVec4 activeColor = colors[ImGuiCol_ButtonActive];
  ImVec4 inactiveColor = colors[ImGuiCol_Button];
  //    ImVec4 highlightColor( 0.64f, 0.44f, 0.64f, 0.40f );

  activeColor.w = 0.94f;
  inactiveColor.w = 0.7f;

  ImGui::PushID("segtoolbar");

  ImGuiIO& io = ImGui::GetIO();

  //    ImGuiWindowFlags windowFlags = sk_toolbarWindowFlags;

  if (corner != -1)
  {
    //        windowFlags |= ImGuiWindowFlags_NoMove;

    ImVec2 windowPos = ImVec2(
      (corner & 1) ? io.DisplaySize.x - padSize.x : padSize.x,
      (corner & 2) ? io.DisplaySize.y - padSize.y : padSize.y
    );

    if (guiData.m_showMainMenuBar)
    {
      if (0 == corner || 1 == corner)
      {
        windowPos.y += guiData.m_mainMenuBarDims.y;
      }
    }

    const ImVec2 windowPosPivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPosPivot);
  }

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

  ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, activeColor);

  const char* title
    = ((isHoriz /*| isCollapsed*/) ? "Segmentation###SegToolbarWindow" : "###SegToolbarWindow");

  if (ImGui::Begin(title, toolbarWindowOpen, sk_toolbarWindowFlags))
  {
    if (s_lastShowState != guiData.m_showSegToolbar)
    {
      const ImVec2 winSize = ImGui::GetWindowSize();

      guiData.m_segToolbarDockDims = glm::vec2{winSize.x, winSize.y}
                                     + 2.0f * glm::vec2{padSize.x, padSize.y};

      readjustViewport();

      s_lastShowState = guiData.m_showSegToolbar;
    }

    int id = 0;

    const size_t fgLabel = appData.settings().foregroundLabel();
    const size_t bgLabel = appData.settings().backgroundLabel();

    const glm::vec3 fgColor = glm::vec3{activeLabelTable->getColor(fgLabel)} / 255.0f;
    const glm::vec3 bgColor = glm::vec3{activeLabelTable->getColor(bgLabel)} / 255.0f;

    ImVec4 fgImGuiColor(fgColor.r, fgColor.g, fgColor.b, 1.0f);
    ImVec4 bgImGuiColor(bgColor.r, bgColor.g, bgColor.b, 1.0f);

    const bool useDarkTextForFgColor = (glm::luminosity(fgColor) > 0.5f);
    const bool useDarkTextForBgColor = (glm::luminosity(bgColor) > 0.5f);

    const std::string fgButtonLabel = std::to_string(fgLabel) + "###fgButton";
    const std::string bgButtonLabel = std::to_string(bgLabel) + "###bgButton";

    ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor); // PUSH color

    if (isHoriz)
      ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, fgImGuiColor);
    ImGui::PushStyleColor(
      ImGuiCol_Text, (useDarkTextForFgColor ? sk_darkTextColor : sk_lightTextColor)
    );
    {
      if (ImGui::Button(fgButtonLabel.c_str(), buttonSize))
      {
        ImGui::OpenPopup("foregroundLabelPopup");
      }
    }
    ImGui::PopStyleColor(2); // ImGuiCol_Button, ImGuiCol_Text
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("%s", "Select foreground label (<,>)");
    }

    if (isHoriz)
      ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, bgImGuiColor);
    ImGui::PushStyleColor(
      ImGuiCol_Text, (useDarkTextForBgColor ? sk_darkTextColor : sk_lightTextColor)
    );
    {
      if (ImGui::Button(bgButtonLabel.c_str(), buttonSize))
      {
        ImGui::OpenPopup("backgroundLabelPopup");
      }
    }
    ImGui::PopStyleColor(2); // ImGuiCol_Button, ImGuiCol_Text
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("%s", "Select background label (shift + <,>)");
    }

    if (activeLabelTable && ImGui::BeginPopup("foregroundLabelPopup"))
    {
      const float sz = ImGui::GetTextLineHeight();

      for (size_t i = 0; i < activeLabelTable->numLabels(); ++i)
      {
        const std::string labelName = std::to_string(i) + ") " + activeLabelTable->getName(i);
        const glm::vec3 labelColor = glm::vec3{activeLabelTable->getColor(i)} / 255.0f;
        const ImU32 labelColorU32 = ImGui::ColorConvertFloat4ToU32(
          ImVec4(labelColor.r, labelColor.g, labelColor.b, 1.0f)
        );

        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + sz, p.y + sz), labelColorU32);
        ImGui::Dummy(ImVec2(sz, sz));
        ImGui::SameLine();

        bool isSelected = (fgLabel == i);
        if (ImGui::MenuItem(labelName.c_str(), "", &isSelected))
        {
          if (isSelected)
          {
            appData.settings().setForegroundLabel(i, *activeLabelTable);
            ImGui::SetItemDefaultFocus();
          }
        }
      }

      ImGui::EndPopup();
    }

    if (activeLabelTable && ImGui::BeginPopup("backgroundLabelPopup"))
    {
      const float sz = ImGui::GetTextLineHeight();

      for (size_t i = 0; i < activeLabelTable->numLabels(); ++i)
      {
        const std::string labelName = std::to_string(i) + ") " + activeLabelTable->getName(i);
        const glm::vec3 labelColor = glm::vec3{activeLabelTable->getColor(i)} / 255.0f;
        const ImU32 labelColorU32 = ImGui::ColorConvertFloat4ToU32(
          ImVec4(labelColor.r, labelColor.g, labelColor.b, 1.0f)
        );

        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + sz, p.y + sz), labelColorU32);
        ImGui::Dummy(ImVec2(sz, sz));
        ImGui::SameLine();

        bool isSelected = (bgLabel == i);
        if (ImGui::MenuItem(labelName.c_str(), "", &isSelected))
        {
          if (isSelected)
          {
            appData.settings().setBackgroundLabel(i, *activeLabelTable);
            ImGui::SetItemDefaultFocus();
          }
        }
      }

      ImGui::EndPopup();
    }

    if (isHoriz)
      ImGui::SameLine();
    ImGui::PushID(id);
    {
      if (ImGui::Button(ICON_FK_RANDOM, buttonSize))
      {
        appData.settings().swapForegroundAndBackgroundLabels(*activeLabelTable);
      }
      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("%s", "Swap foreground and background labels");
      }
      ++id;
    }
    ImGui::PopID();

    if (isHoriz)
      ImGui::SameLine();
    ImGui::Dummy(buttonSpace);

    if (isHoriz)
      ImGui::SameLine();
    ImGui::PushID(id);
    {
      bool replaceBgWithFg = appData.settings().replaceBackgroundWithForeground();
      ImGui::PushStyleColor(ImGuiCol_Button, (replaceBgWithFg ? activeColor : inactiveColor));
      {
        if (ImGui::Button(ICON_FK_PENCIL_SQUARE, buttonSize))
        {
          replaceBgWithFg = !replaceBgWithFg;
          appData.settings().setReplaceBackgroundWithForeground(replaceBgWithFg);
        }

        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Draw foreground label only on top of background label");
        }
      }
      ImGui::PopStyleColor(1); // ImGuiCol_Button

      ++id;
    }
    ImGui::PopID();

    // Only show these segmentation toolbar buttons when in Segmentation mode
    if (inSegmentationMode)
    {
      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        bool use3d = appData.settings().use3dBrush();
        ImGui::PushStyleColor(ImGuiCol_Button, (use3d ? activeColor : inactiveColor));
        {
          if (ImGui::Button(ICON_FK_CUBE, buttonSize))
          {
            use3d = !use3d;
            appData.settings().setUse3dBrush(use3d);
          }

          if (ImGui::IsItemHovered())
          {
            ImGui::SetTooltip("%s", "Set 2D/3D brush");
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        ++id;
      }
      ImGui::PopID();

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        bool roundBrush = appData.settings().useRoundBrush();

        if (ImGui::Button(roundBrush ? ICON_FK_CIRCLE_THIN : ICON_FK_SQUARE_O, buttonSize))
        {
          roundBrush = !roundBrush;
          appData.settings().setUseRoundBrush(roundBrush);
        }

        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Set round/square brush shape");
        }

        ++id;
      }
      ImGui::PopID();

      if (isHoriz)
        ImGui::SameLine();
      if (ImGui::Button(ICON_FK_BULLSEYE, buttonSize))
      {
        ImGui::OpenPopup("brushSizePopup");
      }

      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("%s", "Brush options");
      }

      if (isHoriz)
        ImGui::SameLine();
      ImGui::Dummy(buttonSpace);

      if (isHoriz)
        ImGui::SameLine();
      if (ImGui::Button(ICON_FK_PLUS_CIRCLE, buttonSize))
      {
        /// @todo replace with EntropyApp::cycleBrushSize
        uint32_t brushSizeVox = appData.settings().brushSizeInVoxels();
        brushSizeVox = std::max(brushSizeVox + 1, 1u);
        appData.settings().setBrushSizeInVoxels(brushSizeVox);
      }
      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("%s", "Increase brush size (+)");
      }

      if (isHoriz)
        ImGui::SameLine();

      ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors[ImGuiCol_Button]);
      //        ImGui::PushItemWidth( buttonSize.x );
      {
        const uint32_t brushSizeVox = appData.settings().brushSizeInVoxels();
        const std::string brushSizeString = std::to_string(brushSizeVox);
        ImGui::Button(brushSizeString.c_str(), buttonSize);

        //        static constexpr uint32_t sk_step = 1;
        //        static constexpr uint32_t sk_stepBig = 2;
        //        ImGui::Text( "%d", brushSizeVox );
        //        if ( ImGui::InputScalar( "##brushSizeInput", ImGuiDataType_U32,
        //                                 &brushSizeVox, nullptr, nullptr, "%d" ) )
        //        {
        //            brushSizeVox = glm::clamp( brushSizeVox, minBrushVox, maxBrushVox );
        //            appData.settings().setBrushSizeInVoxels( brushSizeVox );
        //        }

        //        ImGui::PopItemWidth();
        ImGui::PopStyleColor(1); // ImGuiCol_ButtonActive
      }

      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("%s", "Brush size (voxels)");
      }

      if (isHoriz)
        ImGui::SameLine();
      if (ImGui::Button(ICON_FK_MINUS_CIRCLE, buttonSize))
      {
        /// @todo replace with EntropyApp::cycleBrushSize
        uint32_t brushSizeVox = appData.settings().brushSizeInVoxels();
        brushSizeVox = std::max(brushSizeVox - 1, 1u);
        appData.settings().setBrushSizeInVoxels(brushSizeVox);
      }
      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("%s", "Decrease brush size (-)");
      }

      /// @todo Should save off default values (prior to toolbar's change) and push them here:
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
      ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);

      if (ImGui::BeginPopup("brushSizePopup"))
      {
        bool useVoxels = appData.settings().useVoxelBrushSize();
        bool replaceBgWithFg = appData.settings().replaceBackgroundWithForeground();
        bool use3d = appData.settings().use3dBrush();
        bool useIso = appData.settings().useIsotropicBrush();
        bool useRound = appData.settings().useRoundBrush();
        bool xhairsMove = appData.settings().crosshairsMoveWithBrush();

        ImGui::Text("Brush options:");
        ImGui::Separator();
        ImGui::Spacing();

        if (useVoxels)
        {
          uint32_t brushSizeVox = appData.settings().brushSizeInVoxels();
          uint32_t stepSmall = 1;
          uint32_t stepBig = 5;

          ImGui::PushItemWidth(120);
          if (ImGui::InputScalar(
                " width (vox)##brushSizeVox", ImGuiDataType_U32, &brushSizeVox, &stepSmall, &stepBig
              ))
          {
            static constexpr uint32_t minBrushVox = 1;
            static constexpr uint32_t maxBrushVox = 511;

            brushSizeVox = glm::clamp(brushSizeVox, minBrushVox, maxBrushVox);
            appData.settings().setBrushSizeInVoxels(brushSizeVox);
          }
          ImGui::PopItemWidth();
        }
        //                else
        //                {
        //                    float brushSizeMm = appData.brushSizeInMm();

        //                    if ( ImGui::SliderFloat( "size (mm)##brushSizeMmSlider", &brushSizeMm, 0.001f, 100.000f, "%.3f" ) )
        //                    {
        //                        appData.setBrushSizeInMm( brushSizeMm );
        //                    }
        //                }
        ImGui::SameLine();
        helpMarker("Brush width in voxels");

        //                if ( ImGui::RadioButton( "Voxels", useVoxels ) )
        //                {
        //                    useVoxels = true;
        //                    appData.setUseVoxelBrushSize( useVoxels );
        //                }

        //                ImGui::SameLine();
        //                if ( ImGui::RadioButton( "Millimeters", ! useVoxels ) )
        //                {
        //                    useVoxels = false;
        //                    appData.setUseVoxelBrushSize( useVoxels );
        //                }
        //                ImGui::SameLine(); ImGui::Dummy( ImVec2( 2.0f, 0.0f ) );
        //                ImGui::SameLine(); HelpMarker( "Set brush size in units of either voxels or millimeters" );

        if (ImGui::RadioButton("Round", useRound))
        {
          useRound = true;
          appData.settings().setUseRoundBrush(useRound);
        }

        ImGui::SameLine();
        if (ImGui::RadioButton("Square", !useRound))
        {
          useRound = false;
          appData.settings().setUseRoundBrush(useRound);
        }
        ImGui::SameLine();
        helpMarker("Set either round or square brush shape");

        if (ImGui::RadioButton("2D", !use3d))
        {
          use3d = false;
          appData.settings().setUse3dBrush(use3d);
        }

        ImGui::SameLine();
        if (ImGui::RadioButton("3D", use3d))
        {
          use3d = true;
          appData.settings().setUse3dBrush(use3d);
        }
        ImGui::SameLine();
        helpMarker("Set either 2D (planar) or 3D (volumetric) brush shape");

        if (ImGui::Checkbox("Isotropic brush", &useIso))
        {
          appData.settings().setUseIsotropicBrush(useIso);
        }
        ImGui::SameLine();
        helpMarker("Set either anisotropic or isotropic brush dimensions");

        if (ImGui::Checkbox("Replace background with foreground", &replaceBgWithFg))
        {
          appData.settings().setReplaceBackgroundWithForeground(replaceBgWithFg);
        }
        ImGui::SameLine();
        helpMarker(
          "When enabled, the brush only draws the foreground label on top of the background label"
        );

        if (ImGui::Checkbox("Crosshairs move with brush", &xhairsMove))
        {
          appData.settings().setCrosshairsMoveWithBrush(xhairsMove);
        }
        ImGui::SameLine();
        helpMarker("Crosshairs movement is linked with brush movement");

        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::Text("Graph Cuts edge weights:");

        ImGui::Separator();
        ImGui::Spacing();

        double amplitude = appData.settings().graphCutsWeightsAmplitude();
        double sigma = appData.settings().graphCutsWeightsSigma();

        if (ImGui::InputScalar(
              "Amplitude", ImGuiDataType_Double, &amplitude, nullptr, nullptr, "%.3f"
            ))
        {
          appData.settings().setGraphCutsWeightsAmplitude(amplitude);
        }
        ImGui::SameLine();
        helpMarker("Amplitude");

        //                if ( ImGui::InputScalar( "Sigma", ImGuiDataType_Double, &sigma ) )
        //                {
        //                    appData.settings().setGraphCutsWeightsSigma( sigma );
        //                }
        if (mySliderF64("Std. dev.", &sigma, 0.0, 0.05, "%.3f"))
        {
          appData.settings().setGraphCutsWeightsSigma(sigma);
        }
        ImGui::SameLine();
        helpMarker("Sigma");

        GraphNeighborhoodType hoodType = appData.settings().graphCutsNeighborhood();

        ImGui::Text("Neighborhood type: ");
        ImGui::SameLine();
        if (ImGui::RadioButton("6", GraphNeighborhoodType::Neighbors6 == hoodType))
        {
          hoodType = GraphNeighborhoodType::Neighbors6;
          appData.settings().setGraphCutsNeighborhood(hoodType);
        }

        ImGui::SameLine();
        if (ImGui::RadioButton("26", GraphNeighborhoodType::Neighbors26 == hoodType))
        {
          hoodType = GraphNeighborhoodType::Neighbors26;
          appData.settings().setGraphCutsNeighborhood(hoodType);
        }
        ImGui::SameLine();
        helpMarker("Set 3D neighborhood type for graph construction");

        ImGui::EndPopup();
      }

      // ImGuiStyleVar_FramePadding, ImGuiStyleVar_ItemSpacing,
      // ImGuiStyleVar_WindowBorderSize, ImGuiStyleVar_WindowPadding,
      // ImGuiStyleVar_FrameRounding, ImGuiStyleVar_WindowRounding
      ImGui::PopStyleVar(6);

      if (isHoriz)
        ImGui::SameLine();
      ImGui::Dummy(buttonSpace);

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        bool xhairsMove = appData.settings().crosshairsMoveWithBrush();

        ImGui::PushStyleColor(ImGuiCol_Button, (xhairsMove ? activeColor : inactiveColor));
        {
          if (ImGui::Button(xhairsMove ? ICON_FK_LINK : ICON_FK_CHAIN_BROKEN, buttonSize))
          {
            xhairsMove = !xhairsMove;
            appData.settings().setCrosshairsMoveWithBrush(xhairsMove);
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Crosshairs linked to brush");
        }

        ++id;
      }
      ImGui::PopID();

      if (isHoriz)
        ImGui::SameLine();
      if (ImGui::Button(ICON_FK_RSS, buttonSize))
      {
        ImGui::OpenPopup("segSyncPopup");
      }

      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("%s", "Synchronize drawing of segmentations on multiple images");
      }

      if (isHoriz)
        ImGui::SameLine();
      if (ImGui::Button(ICON_FK_CUBE, buttonSize))
      {
        const auto imageUid = appData.activeImageUid();
        const auto seedSegUid = appData.imageToActiveSegUid(*imageUid);

        if (imageUid && seedSegUid)
        {
          executeGraphCutsSeg(*imageUid, *seedSegUid, SeedSegmentationType::Binary);
          updateImageUniforms(*imageUid);
        }
      }
      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("%s", "Execute binary Graph Cuts segmentation");
      }

      if (isHoriz)
        ImGui::SameLine();
      if (ImGui::Button(ICON_FK_CUBES, buttonSize))
      {
        const auto imageUid = appData.activeImageUid();
        const auto seedSegUid = appData.imageToActiveSegUid(*imageUid);

        if (imageUid && seedSegUid)
        {
          executeGraphCutsSeg(*imageUid, *seedSegUid, SeedSegmentationType::MultiLabel);
          updateImageUniforms(*imageUid);
        }
      }
      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("%s", "Execute multi-label Graph Cuts segmentation");
      }

      if (isHoriz)
        ImGui::SameLine();
      if (ImGui::Button(ICON_FK_PLUG, buttonSize))
      {
        if (const auto imageUid = appData.activeImageUid())
        {
          if (const auto seedSegUid = appData.imageToActiveSegUid(*imageUid))
          {
            executePoissonSeg(*imageUid, *seedSegUid, SeedSegmentationType::Binary);
            updateImageUniforms(*imageUid);
          }
        }
      }
      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("%s", "Execute multi-label Poisson segmentation");
      }
    }

    /// @todo Should save off default values (prior to toolbar's change) and push them here:
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);

    if (ImGui::BeginPopup("segSyncPopup"))
    {
      const size_t activeIndex = getActiveImageIndex();

      /*
                for ( size_t i = 0; i < numImages; ++i )
                {
                    ImGui::PushID( static_cast<int>( i ) );
                    {
                        const auto displayAndFileName = getImageDisplayAndFileName(i);

                        // Active image is selected
                        bool isSelected = ( i == activeIndex );

                        // Image is selected if its seg is active
                        isSelected |= getImageHasActiveSeg( i );

                        if ( ImGui::MenuItem( displayAndFileName.first, "", &isSelected ) )
                        {
                            if ( isSelected )
                            {
                                setImageHasActiveSeg( i, true );
                                ImGui::SetItemDefaultFocus();
                            }
                            else
                            {
                                setImageHasActiveSeg( i, false );
                            }
                        }

                        if ( ImGui::IsItemHovered() )
                        {
                            ImGui::SetTooltip( "%s", displayAndFileName.second );
                        }
                    }
                    ImGui::PopID(); // i
                }
                */

      ImGui::Text("Select the active image to segment:");

      renderActiveImageSelectionCombo(
        numImages, getImageDisplayAndFileName, getActiveImageIndex, setActiveImageIndex, false
      );

      ImGui::Separator();

      ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);

      if (ImGui::TreeNode("Synchronize drawing on additional images:"))
      {
        for (size_t i = 0; i < numImages; ++i)
        {
          // Active image is not shown
          if (i == activeIndex)
            continue;

          const auto displayAndFileName = getImageDisplayAndFileName(i);

          // Image is selected if its seg is active
          bool isSelected = getImageHasActiveSeg(i);

          if (ImGui::Selectable(displayAndFileName.first, &isSelected))
          {
            if (isSelected)
            {
              setImageHasActiveSeg(i, true);
              ImGui::SetItemDefaultFocus();
            }
            else
            {
              setImageHasActiveSeg(i, false);
            }
          }

          if (ImGui::IsItemHovered())
          {
            ImGui::SetTooltip("%s", displayAndFileName.second);
          }
        }

        ImGui::TreePop();
      }

      ImGui::EndPopup();
    }

    // ImGuiStyleVar_FramePadding, ImGuiStyleVar_ItemSpacing,
    // ImGuiStyleVar_WindowBorderSize, ImGuiStyleVar_WindowPadding,
    // ImGuiStyleVar_FrameRounding, ImGuiStyleVar_WindowRounding
    ImGui::PopStyleVar(6);

    ImGui::PopStyleColor(1); // ImGuiCol_Button

    // Save the new toolbar size:
    const ImVec2 winSize = ImGui::GetWindowSize();

    guiData.m_segToolbarDockDims = glm::vec2{winSize.x, winSize.y}
                                   + 2.0f * glm::vec2{padSize.x, padSize.y};

    if (ImGui::BeginPopupContextWindow())
    {
      renderPlacementContextMenu(guiData.m_segToolbarCorner, guiData.m_isSegToolbarHorizontal);

      readjustViewport();
    }

    ImGui::End(); // End toolbar
  }

  // ImGuiStyleVar_FramePadding, ImGuiStyleVar_ItemSpacing,
  // ImGuiStyleVar_WindowBorderSize, ImGuiStyleVar_WindowPadding,
  // ImGuiStyleVar_FrameRounding, ImGuiStyleVar_WindowRounding
  ImGui::PopStyleVar(6);

  // ImGuiCol_TitleBgCollapsed
  ImGui::PopStyleColor(1);

  ImGui::PopID();
}

void renderAnnotationToolbar(
  AppData& appData,
  const FrameBounds& mindowFrameBounds,
  const std::function<void()> paintActiveAnnotation
)
{
  // Always keep the toolbar open by setting this to null
  static bool* toolbarWindowOpen = nullptr;

  static int corner = 3;
  static bool isHoriz = true;

  const auto padSize = scaledPad(appData.windowData().getContentScaleRatios());

  const ImVec2 buttonSpace = (isHoriz ? ImVec2(2.0f, 0.0f) : ImVec2(0.0f, 2.0f));

  const ImVec4* colors = ImGui::GetStyle().Colors;
  ImVec4 activeColor = colors[ImGuiCol_ButtonActive];
  ImVec4 inactiveColor = colors[ImGuiCol_Button];
  //    ImVec4 highlightColor( 0.64f, 0.44f, 0.64f, 0.40f );

  activeColor.w = 0.94f;
  inactiveColor.w = 0.7f;

  ImGui::PushID("annotToolbar");

  //    ImGuiWindowFlags windowFlags = sk_toolbarWindowFlags;

  if (corner != -1)
  {
    //        windowFlags |= ImGuiWindowFlags_NoMove;

    const ImVec2 windowPos(
      (corner & 1) ? mindowFrameBounds.bounds.xoffset + mindowFrameBounds.bounds.width - padSize.x
                   : mindowFrameBounds.bounds.xoffset + padSize.x,
      (corner & 2) ? mindowFrameBounds.bounds.yoffset + mindowFrameBounds.bounds.height - padSize.y
                   : mindowFrameBounds.bounds.yoffset + padSize.y
    );

    const ImVec2 windowPosPivot((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPosPivot);
  }

  //    ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 0.0f, 0.0f ) );
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

  ImGui::PushStyleColor(ImGuiCol_TitleBg, activeColor);
  ImGui::PushStyleColor(ImGuiCol_TitleBgActive, activeColor);
  ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, activeColor);

  const char* title
    = ((isHoriz /*| isCollapsed*/) ? "Annotation###AnnotToolbarWindow" : "###AnnotToolbarWindow");

  if (ImGui::Begin(title, toolbarWindowOpen, sk_toolbarWindowFlags))
  {
    int id = 0;

    ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor); // PUSH color

    bool needsSpace = false;

    if (state::showToolbarInsertVertexButton())
    {
      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_remove = std::string(ICON_FK_PLUS_SQUARE_O) + " Insert vertex";

        if (ImGui::Button(sk_remove.c_str()))
        {
          send_event(state::InsertVertexEvent());
        }
        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Insert a vertex after the selected polygon vertex");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::showToolbarRemoveSelectedVertexButton())
    {
      if (needsSpace)
      {
        if (isHoriz)
          ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_remove = std::string(ICON_FK_MINUS_SQUARE_O) + " Remove vertex";

        if (ImGui::Button(sk_remove.c_str()))
        {
          send_event(state::RemoveSelectedVertexEvent());
        }
        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Remove the selected polygon vertex");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::showToolbarUndoButton())
    {
      if (needsSpace)
      {
        if (isHoriz)
          ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_cancel = std::string(ICON_FK_UNDO) + " Undo vertex";

        if (ImGui::Button(sk_cancel.c_str()))
        {
          send_event(state::UndoVertexEvent());
        }
        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Undo the last polygon vertex");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::showToolbarCreateButton())
    {
      if (needsSpace)
      {
        if (isHoriz)
          ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_addNew = std::string(ICON_FK_PLUS) + " New polygon";
        if (ImGui::Button(sk_addNew.c_str()))
        {
          send_event(state::CreateNewAnnotationEvent());
        }
        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Create a new polygon");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::showToolbarCloseButton())
    {
      if (needsSpace)
      {
        if (isHoriz)
          ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_close = std::string(ICON_FK_CIRCLE_O_NOTCH) + " Close polygon";

        if (ImGui::Button(sk_close.c_str()))
        {
          send_event(state::CloseNewAnnotationEvent());
        }
        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Close the polygon");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::showToolbarCompleteButton())
    {
      if (needsSpace)
      {
        if (isHoriz)
          ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_complete = std::string(ICON_FK_CHECK) + " Complete";

        if (ImGui::Button(sk_complete.c_str()))
        {
          send_event(state::CompleteNewAnnotationEvent());
        }
        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Complete the polygon");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::showToolbarCancelButton())
    {
      if (needsSpace)
      {
        if (isHoriz)
          ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_cancel = std::string(ICON_FK_TIMES) + " Cancel";

        if (ImGui::Button(sk_cancel.c_str()))
        {
          send_event(state::CancelNewAnnotationEvent());
        }
        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Cancel creating the polygon");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::showToolbarRemoveSelectedAnnotationButton())
    {
      if (needsSpace)
      {
        if (isHoriz)
          ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_remove = std::string(ICON_FK_TRASH_O) + " Remove polygon";

        if (ImGui::Button(sk_remove.c_str()))
        {
          send_event(state::RemoveSelectedAnnotationEvent());
        }
        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Remove the selected polygon");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::showToolbarCutSelectedAnnotationButton())
    {
      if (needsSpace)
      {
        if (isHoriz)
          ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_cut = std::string(ICON_FK_SCISSORS) + " Cut";

        if (ImGui::Button(sk_cut.c_str()))
        {
          send_event(state::CutSelectedAnnotationEvent());
        }
        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Cut the selected polygon to the clipboard");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::showToolbarCopySelectedAnnotationButton())
    {
      if (needsSpace)
      {
        if (isHoriz)
          ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_copy = std::string(ICON_FK_FILES_O) + " Copy";

        if (ImGui::Button(sk_copy.c_str()))
        {
          send_event(state::CopySelectedAnnotationEvent());
        }
        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Copy the selected polygon to the clipboard");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::showToolbarPasteSelectedAnnotationButton())
    {
      if (needsSpace)
      {
        if (isHoriz)
          ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_paste = std::string(ICON_FK_CLIPBOARD) + " Paste";

        if (ImGui::Button(sk_paste.c_str()))
        {
          send_event(state::PasteAnnotationEvent());
        }
        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Paste the polygon from the clipboard");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::showToolbarFlipAnnotationButtons())
    {
      if (needsSpace)
      {
        if (isHoriz)
          ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_flipHoriz = std::string(ICON_FK_ARROWS_H) + " Flip";

        if (ImGui::Button(sk_flipHoriz.c_str()))
        {
          send_event(state::HorizontallyFlipSelectedAnnotationEvent());
        }
        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Flip the polygon horizontally");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;

      if (needsSpace)
      {
        if (isHoriz)
          ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_flipHoriz = std::string(ICON_FK_ARROWS_V) + " Flip";

        if (ImGui::Button(sk_flipHoriz.c_str()))
        {
          send_event(state::VerticallyFlipSelectedAnnotationEvent());
        }
        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", "Flip the polygon vertically");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::showToolbarFillButton())
    {
      if (needsSpace)
      {
        if (isHoriz)
          ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz)
        ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_fill = std::string(ICON_FK_PAINT_BRUSH) + " Fill";

        if (ImGui::Button(sk_fill.c_str()))
        {
          paintActiveAnnotation();
        }
        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip(
            "%s", "Fill the active image segmentation with the selected annotation polygon"
          );
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

#if 0
        if ( isHoriz ) ImGui::SameLine();
        ImGui::PushID( id );
        {
//            bool replaceBgWithFg = appData.settings().replaceBackgroundWithForeground();
//            ImGui::PushStyleColor( ImGuiCol_Button, ( replaceBgWithFg ? activeColor : inactiveColor ) );
            {
                if ( ImGui::Button( "Edit" ) )
                {
//                    replaceBgWithFg = ! replaceBgWithFg;
//                    appData.settings().setReplaceBackgroundWithForeground( replaceBgWithFg );
                }

                if ( ImGui::IsItemHovered() ) {
                    ImGui::SetTooltip( "%s", "Edit annotation" );
                }
            }
//            ImGui::PopStyleColor( 1 ); // ImGuiCol_Button

            ++id;
        }
        ImGui::PopID();
//        ImGui::Dummy( buttonSpace );
#endif

#if 0
        if ( isHoriz ) ImGui::SameLine();
        ImGui::PushID( id );
        {
            if ( ImGui::Button( "Undo" ) )
            {
            }
            if ( ImGui::IsItemHovered() )
            {
                ImGui::SetTooltip( "%s", "Undo vertex" );
            }
            ++id;
        }
        ImGui::PopID();


        if ( isHoriz ) ImGui::SameLine();
//        ImGui::Dummy( buttonSpace );
#endif

    ImGui::PopStyleColor(1); // ImGuiCol_Button

    if (ImGui::BeginPopupContextWindow())
    {
      /// @todo Disable placement in top-left corner
      renderPlacementContextMenu(corner, isHoriz);
    }

    ImGui::End(); // End toolbar
  }

  // // ImGuiStyleVar_FramePadding,
  // ImGuiStyleVar_ItemSpacing,
  // ImGuiStyleVar_WindowBorderSize,
  // ImGuiStyleVar_WindowPadding,
  // ImGuiStyleVar_FrameRounding, ImGuiStyleVar_WindowRounding
  ImGui::PopStyleVar(5);

  // ImGuiCol_TitleBg
  // ImGuiCol_TitleBgActive
  // ImGuiCol_TitleBgCollapsed
  ImGui::PopStyleColor(3);

  ImGui::PopID();
}
