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

void renderSegToolbar(
  AppData& appData,
  size_t numImages,
  const std::function<std::pair<std::string, std::string>(size_t index)>& getImageDisplayAndFileName,
  const std::function<size_t(void)>& getActiveImageIndex,
  const std::function<void(size_t)>& setActiveImageIndex,
  const std::function<bool(size_t imageIndex)>& getImageHasActiveSeg,
  const std::function<void(size_t imageIndex, bool set)>& setImageHasActiveSeg,
  const std::function<
    std::optional<uuids::uuid>(const uuids::uuid& matchingImageUid, const std::string& segDisplayName)>& createBlankSeg,
  const std::function<void(const uuids::uuid& imageUid)>& updateImageUniforms,
  const std::function<void(MouseMode)>& setMouseMode,
  const std::function<void(void)>& readjustViewport,
  const std::function<bool(const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType&)>&
    executePoissonSeg)
{
  // Show the segmentation toolbar in either Segmentation mode,
  // in Annotation mode (when the Fill button is also visible),
  // or when the Annotations Window is visible

  const bool inSegmentationMode = (MouseMode::Segment == appData.state().mouseMode());
  const bool inAnnotationMode = (state::annot::isInStateWhereToolbarVisible() && state::annot::showToolbarFillButton());

  GuiData& guiData = appData.guiData();

  static_cast<void>(executePoissonSeg);

  const auto buttonSize = scaledToolbarButtonSize(appData.windowData().getContentScaleRatios());
  const auto padSize = scaledPad(appData.windowData().getContentScaleRatios());

  guiData.m_showSegToolbar = (inSegmentationMode && !inAnnotationMode && !appData.guiData().m_showAnnotationsWindow);

  if (!guiData.m_showSegToolbar) {
    readjustViewport();
    return;
  }

  // Always keep the toolbar open by setting this to null
  static bool* toolbarWindowOpen = nullptr;

  const bool isHoriz = guiData.m_isSegToolbarHorizontal;
  const int corner = guiData.m_segToolbarCorner;

  const auto activeImageUid = appData.activeImageUid();
  if (!activeImageUid) {
    spdlog::error("There is no active image to segment");
    return;
  }

  const Image* activeImage = appData.image(*activeImageUid);
  if (!activeImage) {
    spdlog::error("The active image {} is null", *activeImageUid);
    return;
  }

  const auto activeSegUid = appData.imageToActiveSegUid(*activeImageUid);
  if (!activeSegUid) {
    static constexpr const char* k_createSegPopupTitle = "Create segmentation?";
    ImGui::OpenPopup(k_createSegPopupTitle, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::BeginPopupModal(
          k_createSegPopupTitle,
          nullptr,
          ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize))
    {
      ImGui::TextWrapped(
        "There is no segmentation for the active image \"%s\".",
        activeImage->settings().displayName().c_str());
      ImGui::TextUnformatted("Create a new blank segmentation for painting?");
      ImGui::Spacing();

      if (ImGui::Button("Create", ImVec2(100.0f, 0.0f))) {
        const size_t numSegsForImage = appData.imageToSegUids(*activeImageUid).size();
        const std::string segDisplayName = "Untitled segmentation " + std::to_string(numSegsForImage + 1) +
                                           " for image " + activeImage->settings().displayName();

        if (createBlankSeg && createBlankSeg(*activeImageUid, segDisplayName)) {
          if (updateImageUniforms) {
            updateImageUniforms(*activeImageUid);
          }
          ImGui::CloseCurrentPopup();
        }
        else {
          spdlog::error("Error creating new blank segmentation for image {}", *activeImageUid);
        }
      }

      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) {
        setMouseMode(MouseMode::Pointer);
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }

    return;
  }

  const Image* activeSeg = appData.seg(*activeSegUid);
  if (!activeSeg) {
    spdlog::error("The active segmentation {} is null for image {}", *activeSegUid, *activeImageUid);
    return;
  }

  const size_t activeLabelTableIndex = activeSeg->settings().labelTableIndex();
  const auto activeLabelTableUid = appData.labelTableUid(activeLabelTableIndex);
  if (!activeLabelTableUid) {
    spdlog::error("There is no label table for active segmentation {}", *activeSegUid);
    return;
  }

  const ParcellationLabelTable* activeLabelTable = appData.labelTable(*activeLabelTableUid);

  if (!activeLabelTable) {
    spdlog::error("The label table {} for active segmentation {} is null", *activeLabelTableUid, *activeSegUid);
    return;
  }

  const ImVec2 buttonSpace = (appData.guiData().m_isSegToolbarHorizontal ? ImVec2(2.0f, 0.0f) : ImVec2(0.0f, 2.0f));

  const ImVec4* colors = ImGui::GetStyle().Colors;
  ImVec4 activeColor = colors[ImGuiCol_ButtonActive];
  ImVec4 inactiveColor = colors[ImGuiCol_Button];
  //    ImVec4 highlightColor( 0.64f, 0.44f, 0.64f, 0.40f );

  activeColor.w = 0.94f;
  inactiveColor.w = 0.7f;

  ImGui::PushID("segtoolbar");

  ImGuiIO& io = ImGui::GetIO();

  //    ImGuiWindowFlags windowFlags = k_toolbarWindowFlags;

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

  const char* title = ((isHoriz /*| isCollapsed*/) ? "Segmentation###SegToolbarWindow" : "###SegToolbarWindow");

  if (ImGui::Begin(title, toolbarWindowOpen, k_toolbarWindowFlags)) {
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

    if (isHoriz) {
      ImGui::SameLine();
    }

    ImGui::PushStyleColor(ImGuiCol_Button, fgImGuiColor);
    ImGui::PushStyleColor(ImGuiCol_Text, (useDarkTextForFgColor ? k_darkTextColor : k_lightTextColor));
    {
      if (ImGui::Button(fgButtonLabel.c_str(), buttonSize)) {
        ImGui::OpenPopup("foregroundLabelPopup");
      }
    }
    ImGui::PopStyleColor(2); // ImGuiCol_Button, ImGuiCol_Text
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", "Select foreground label (<,>)");
    }

    if (isHoriz) {
      ImGui::SameLine();
    }

    ImGui::PushStyleColor(ImGuiCol_Button, bgImGuiColor);
    ImGui::PushStyleColor(ImGuiCol_Text, (useDarkTextForBgColor ? k_darkTextColor : k_lightTextColor));
    {
      if (ImGui::Button(bgButtonLabel.c_str(), buttonSize)) {
        ImGui::OpenPopup("backgroundLabelPopup");
      }
    }
    ImGui::PopStyleColor(2); // ImGuiCol_Button, ImGuiCol_Text
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", "Select background label (shift + <,>)");
    }

    if (activeLabelTable && ImGui::BeginPopup("foregroundLabelPopup")) {
      const float sz = ImGui::GetTextLineHeight();

      for (size_t i = 0; i < activeLabelTable->numLabels(); ++i) {
        const std::string labelName = std::to_string(i) + ") " + activeLabelTable->getName(i);
        const glm::vec3 labelColor = glm::vec3{activeLabelTable->getColor(i)} / 255.0f;
        const ImU32 labelColorU32 =
          ImGui::ColorConvertFloat4ToU32(ImVec4(labelColor.r, labelColor.g, labelColor.b, 1.0f));

        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + sz, p.y + sz), labelColorU32);
        ImGui::Dummy(ImVec2(sz, sz));
        ImGui::SameLine();

        bool isSelected = (fgLabel == i);
        if (ImGui::MenuItem(labelName.c_str(), "", &isSelected)) {
          if (isSelected) {
            appData.settings().setForegroundLabel(i, *activeLabelTable);
            ImGui::SetItemDefaultFocus();
          }
        }
      }

      ImGui::EndPopup();
    }

    if (activeLabelTable && ImGui::BeginPopup("backgroundLabelPopup")) {
      const float sz = ImGui::GetTextLineHeight();

      for (size_t i = 0; i < activeLabelTable->numLabels(); ++i) {
        const std::string labelName = std::to_string(i) + ") " + activeLabelTable->getName(i);
        const glm::vec3 labelColor = glm::vec3{activeLabelTable->getColor(i)} / 255.0f;
        const ImU32 labelColorU32 =
          ImGui::ColorConvertFloat4ToU32(ImVec4(labelColor.r, labelColor.g, labelColor.b, 1.0f));

        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + sz, p.y + sz), labelColorU32);
        ImGui::Dummy(ImVec2(sz, sz));
        ImGui::SameLine();

        bool isSelected = (bgLabel == i);
        if (ImGui::MenuItem(labelName.c_str(), "", &isSelected)) {
          if (isSelected) {
            appData.settings().setBackgroundLabel(i, *activeLabelTable);
            ImGui::SetItemDefaultFocus();
          }
        }
      }

      ImGui::EndPopup();
    }

    if (isHoriz) {
      ImGui::SameLine();
    }

    ImGui::PushID(id);
    {
      if (ImGui::Button(ICON_FK_RANDOM, buttonSize)) {
        appData.settings().swapForegroundAndBackgroundLabels(*activeLabelTable);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "Swap foreground and background labels");
      }
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
      bool replaceBgWithFg = appData.settings().replaceBackgroundWithForeground();
      ImGui::PushStyleColor(ImGuiCol_Button, (replaceBgWithFg ? activeColor : inactiveColor));
      {
        if (ImGui::Button(ICON_FK_PENCIL_SQUARE, buttonSize)) {
          replaceBgWithFg = !replaceBgWithFg;
          appData.settings().setReplaceBackgroundWithForeground(replaceBgWithFg);
        }

        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Draw foreground label only on top of background label");
        }
      }
      ImGui::PopStyleColor(1); // ImGuiCol_Button

      ++id;
    }
    ImGui::PopID();

    // Only show these segmentation toolbar buttons when in Segmentation mode
    if (inSegmentationMode) {
      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::PushID(id);
      {
        bool use3d = appData.settings().use3dBrush();
        ImGui::PushStyleColor(ImGuiCol_Button, (use3d ? activeColor : inactiveColor));
        {
          if (ImGui::Button(ICON_FK_CUBE, buttonSize)) {
            use3d = !use3d;
            appData.settings().setUse3dBrush(use3d);
          }

          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", "Set 2D/3D brush");
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
        bool roundBrush = appData.settings().useRoundBrush();

        if (ImGui::Button(roundBrush ? ICON_FK_CIRCLE_THIN : ICON_FK_SQUARE_O, buttonSize)) {
          roundBrush = !roundBrush;
          appData.settings().setUseRoundBrush(roundBrush);
        }

        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Set round/square brush shape");
        }

        ++id;
      }
      ImGui::PopID();

      if (isHoriz) {
        ImGui::SameLine();
      }

      if (ImGui::Button(ICON_FK_BULLSEYE, buttonSize)) {
        ImGui::OpenPopup("brushSizePopup");
      }

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "Brush options");
      }

      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::Dummy(buttonSpace);

      if (isHoriz) {
        ImGui::SameLine();
      }

      if (ImGui::Button(ICON_FK_PLUS_CIRCLE, buttonSize)) {
        /// @todo replace with EntropyApp::cycleBrushSize
        uint32_t brushSizeVox = appData.settings().brushSizeInVoxels();
        brushSizeVox = std::max(brushSizeVox + 1, 1u);
        appData.settings().setBrushSizeInVoxels(brushSizeVox);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "Increase brush size (+)");
      }

      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors[ImGuiCol_Button]);
      //        ImGui::PushItemWidth( buttonSize.x );
      {
        const uint32_t brushSizeVox = appData.settings().brushSizeInVoxels();
        const std::string brushSizeString = std::to_string(brushSizeVox);
        ImGui::Button(brushSizeString.c_str(), buttonSize);

        //        static constexpr uint32_t k_step = 1;
        //        static constexpr uint32_t k_stepBig = 2;
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

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "Brush size (voxels)");
      }

      if (isHoriz) {
        ImGui::SameLine();
      }

      if (ImGui::Button(ICON_FK_MINUS_CIRCLE, buttonSize)) {
        /// @todo replace with EntropyApp::cycleBrushSize
        uint32_t brushSizeVox = appData.settings().brushSizeInVoxels();
        brushSizeVox = std::max(brushSizeVox - 1, 1u);
        appData.settings().setBrushSizeInVoxels(brushSizeVox);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "Decrease brush size (-)");
      }

      /// @todo Should save off default values (prior to toolbar's change) and push them here:
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
      ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);

      if (ImGui::BeginPopup("brushSizePopup")) {
        bool useVoxels = appData.settings().useVoxelBrushSize();
        bool replaceBgWithFg = appData.settings().replaceBackgroundWithForeground();
        bool use3d = appData.settings().use3dBrush();
        bool useIso = appData.settings().useIsotropicBrush();
        bool useRound = appData.settings().useRoundBrush();
        bool xhairsMove = appData.settings().crosshairsMoveWithBrush();
        BrushPreviewMode previewMode = appData.settings().brushPreviewMode();
        BrushPreviewVoxels previewVoxels = appData.settings().brushPreviewVoxels();
        BrushPreviewStyle previewStyle = appData.settings().brushPreviewStyle();
        float previewFillOpacityPercent = 100.0f * appData.settings().brushPreviewFillOpacity();
        bool previewWhilePainting = appData.settings().brushPreviewWhilePainting();
        SegmentationOutlineStyle previewOutlineStyle = appData.settings().brushPreviewOutlineStyle();

        ImGui::Text("Brush options:");
        ImGui::Separator();
        ImGui::Spacing();

        if (useVoxels) {
          uint32_t brushSizeVox = appData.settings().brushSizeInVoxels();
          uint32_t stepSmall = 1;
          uint32_t stepBig = 5;

          ImGui::PushItemWidth(120);
          if (ImGui::InputScalar(" width (vox)##brushSizeVox", ImGuiDataType_U32, &brushSizeVox, &stepSmall, &stepBig))
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

        //                    if ( ImGui::SliderFloat( "size (mm)##brushSizeMmSlider", &brushSizeMm,
        //                    0.001f, 100.000f, "%.3f" ) )
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
        //                ImGui::SameLine(); HelpMarker( "Set brush size in units of either voxels
        //                or millimeters" );

        if (ImGui::RadioButton("Round", useRound)) {
          useRound = true;
          appData.settings().setUseRoundBrush(useRound);
        }

        ImGui::SameLine();
        if (ImGui::RadioButton("Square", !useRound)) {
          useRound = false;
          appData.settings().setUseRoundBrush(useRound);
        }
        ImGui::SameLine();
        helpMarker("Set either round or square brush shape");

        if (ImGui::RadioButton("2D", !use3d)) {
          use3d = false;
          appData.settings().setUse3dBrush(use3d);
        }

        ImGui::SameLine();
        if (ImGui::RadioButton("3D", use3d)) {
          use3d = true;
          appData.settings().setUse3dBrush(use3d);
        }
        ImGui::SameLine();
        helpMarker("Set either 2D (planar) or 3D (volumetric) brush shape");

        if (ImGui::Checkbox("Isotropic brush", &useIso)) {
          appData.settings().setUseIsotropicBrush(useIso);
        }
        ImGui::SameLine();
        helpMarker("Set either anisotropic or isotropic brush dimensions");

        if (ImGui::Checkbox("Replace background with foreground", &replaceBgWithFg)) {
          appData.settings().setReplaceBackgroundWithForeground(replaceBgWithFg);
        }
        ImGui::SameLine();
        helpMarker(
          "When enabled, the brush only draws the foreground label on top of the background "
          "label");

        if (ImGui::Checkbox("Crosshairs move with brush", &xhairsMove)) {
          appData.settings().setCrosshairsMoveWithBrush(xhairsMove);
        }
        ImGui::SameLine();
        helpMarker("Crosshairs movement is linked with brush movement");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Brush preview:");

        if (ImGui::RadioButton("Visible on hover##brushPreview", BrushPreviewMode::Hover == previewMode)) {
          previewMode = BrushPreviewMode::Hover;
          appData.settings().setBrushPreviewMode(previewMode);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Off##brushPreview", BrushPreviewMode::Disabled == previewMode)) {
          previewMode = BrushPreviewMode::Disabled;
          appData.settings().setBrushPreviewMode(previewMode);
        }
        ImGui::SameLine();
        helpMarker("Show the voxels that the brush would affect");

        if (BrushPreviewMode::Disabled != previewMode) {
          if (ImGui::RadioButton(
                "Preview changed voxels##brushPreviewVoxels",
                BrushPreviewVoxels::Changed == previewVoxels))
          {
            previewVoxels = BrushPreviewVoxels::Changed;
            appData.settings().setBrushPreviewVoxels(previewVoxels);
          }
          ImGui::SameLine();
          if (ImGui::RadioButton("All voxels in brush##brushPreviewVoxels", BrushPreviewVoxels::All == previewVoxels)) {
            previewVoxels = BrushPreviewVoxels::All;
            appData.settings().setBrushPreviewVoxels(previewVoxels);
          }
          ImGui::SameLine();
          helpMarker("Choose whether the preview ignores voxels whose labels would not change");

          if (ImGui::RadioButton("Outline##brushPreviewStyle", BrushPreviewStyle::Outline == previewStyle)) {
            previewStyle = BrushPreviewStyle::Outline;
            appData.settings().setBrushPreviewStyle(previewStyle);
          }
          ImGui::SameLine();
          if (ImGui::RadioButton(
                "Outline and fill##brushPreviewStyle",
                BrushPreviewStyle::OutlineAndFill == previewStyle))
          {
            previewStyle = BrushPreviewStyle::OutlineAndFill;
            appData.settings().setBrushPreviewStyle(previewStyle);
          }
          ImGui::SameLine();
          helpMarker("Render the preview as an outline or an outline with faint fill");

          if (BrushPreviewStyle::OutlineAndFill == previewStyle) {
            ImGui::PushItemWidth(150);
            if (ImGui::SliderFloat(
                  "Fill opacity##brushPreviewFillOpacity",
                  &previewFillOpacityPercent,
                  0.0f,
                  100.0f,
                  "%.0f%%"))
            {
              appData.settings().setBrushPreviewFillOpacity(previewFillOpacityPercent / 100.0f);
            }
            ImGui::PopItemWidth();
            ImGui::SameLine();
            helpMarker("Set the opacity of the brush preview fill");
          }

          if (ImGui::Checkbox("Show preview while painting##brushPreviewWhilePainting", &previewWhilePainting)) {
            appData.settings().setBrushPreviewWhilePainting(previewWhilePainting);
          }
          ImGui::SameLine();
          helpMarker("Keep showing the brush outline while the mouse button is down and painting");

          if (ImGui::RadioButton(
                "Pixel outline##brushPreviewOutlineStyle",
                SegmentationOutlineStyle::ViewPixel == previewOutlineStyle))
          {
            previewOutlineStyle = SegmentationOutlineStyle::ViewPixel;
            appData.settings().setBrushPreviewOutlineStyle(previewOutlineStyle);
          }
          ImGui::SameLine();
          if (ImGui::RadioButton(
                "Voxel outline##brushPreviewOutlineStyle",
                SegmentationOutlineStyle::ImageVoxel == previewOutlineStyle))
          {
            previewOutlineStyle = SegmentationOutlineStyle::ImageVoxel;
            appData.settings().setBrushPreviewOutlineStyle(previewOutlineStyle);
          }
          ImGui::SameLine();
          helpMarker("Choose whether preview outlines are measured in screen pixels or image voxels");
        }

        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::EndPopup();
      }

      // ImGuiStyleVar_FramePadding, ImGuiStyleVar_ItemSpacing,
      // ImGuiStyleVar_WindowBorderSize, ImGuiStyleVar_WindowPadding,
      // ImGuiStyleVar_FrameRounding, ImGuiStyleVar_WindowRounding
      ImGui::PopStyleVar(6);

      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::Dummy(buttonSpace);

      if (isHoriz) {
        ImGui::SameLine();
      }

      ImGui::PushID(id);
      {
        bool xhairsMove = appData.settings().crosshairsMoveWithBrush();

        ImGui::PushStyleColor(ImGuiCol_Button, (xhairsMove ? activeColor : inactiveColor));
        {
          if (ImGui::Button(xhairsMove ? ICON_FK_LINK : ICON_FK_CHAIN_BROKEN, buttonSize)) {
            xhairsMove = !xhairsMove;
            appData.settings().setCrosshairsMoveWithBrush(xhairsMove);
          }
        }
        ImGui::PopStyleColor(1); // ImGuiCol_Button

        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Crosshairs linked to brush");
        }

        ++id;
      }
      ImGui::PopID();

      if (isHoriz) {
        ImGui::SameLine();
      }

      if (ImGui::Button(ICON_FK_RSS, buttonSize)) {
        ImGui::OpenPopup("segSyncPopup");
      }

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", "Synchronize drawing of segmentations on multiple images");
      }

      if (isHoriz) {
        ImGui::SameLine();
      }
    }

    /// @todo Should save off default values (prior to toolbar's change) and push them here:
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);

    if (ImGui::BeginPopup("segSyncPopup")) {
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
        numImages,
        getImageDisplayAndFileName,
        getActiveImageIndex,
        setActiveImageIndex,
        false);

      ImGui::Separator();

      ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);

      if (ImGui::TreeNode("Synchronize drawing on additional images:")) {
        for (size_t i = 0; i < numImages; ++i) {
          // Active image is not shown
          if (i == activeIndex) continue;

          const auto displayAndFileName = getImageDisplayAndFileName(i);

          // Image is selected if its seg is active
          bool isSelected = getImageHasActiveSeg(i);

          if (ImGui::Selectable(displayAndFileName.first.c_str(), &isSelected)) {
            if (isSelected) {
              setImageHasActiveSeg(i, true);
              ImGui::SetItemDefaultFocus();
            }
            else {
              setImageHasActiveSeg(i, false);
            }
          }

          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", displayAndFileName.second.c_str());
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

    if (ImGui::BeginPopupContextWindow()) {
      renderPlacementContextMenu(guiData.m_segToolbarCorner, guiData.m_isSegToolbarHorizontal);
      ImGui::EndPopup();
    }

    // Save the new toolbar size:
    const ImVec2 winSize = ImGui::GetContentRegionAvail();
    guiData.m_segToolbarDockDims = glm::vec2{winSize.x, winSize.y} + 2.0f * glm::vec2{padSize.x, padSize.y};
    readjustViewport();
  }

  ImGui::End(); // End toolbar

  // ImGuiStyleVar_FramePadding, ImGuiStyleVar_ItemSpacing,
  // ImGuiStyleVar_WindowBorderSize, ImGuiStyleVar_WindowPadding,
  // ImGuiStyleVar_FrameRounding, ImGuiStyleVar_WindowRounding
  ImGui::PopStyleVar(6);

  // ImGuiCol_TitleBgCollapsed
  ImGui::PopStyleColor(1);

  ImGui::PopID();
}
