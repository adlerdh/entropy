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

void renderAnnotationToolbar(
  AppData& appData,
  const FrameBounds& mindowFrameBounds,
  const std::function<void()> paintActiveAnnotation)
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

  if (corner != -1) {
    //        windowFlags |= ImGuiWindowFlags_NoMove;

    const ImVec2 windowPos(
      (corner & 1) ? mindowFrameBounds.bounds.xoffset + mindowFrameBounds.bounds.width - padSize.x
                   : mindowFrameBounds.bounds.xoffset + padSize.x,
      (corner & 2) ? mindowFrameBounds.bounds.yoffset + mindowFrameBounds.bounds.height - padSize.y
                   : mindowFrameBounds.bounds.yoffset + padSize.y);

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

  const char* title = ((isHoriz /*| isCollapsed*/) ? "Annotation###AnnotToolbarWindow" : "###AnnotToolbarWindow");

  if (ImGui::Begin(title, toolbarWindowOpen, sk_toolbarWindowFlags)) {
    int id = 0;

    ImGui::PushStyleColor(ImGuiCol_Button, inactiveColor); // PUSH color

    bool needsSpace = false;

    if (state::annot::showToolbarInsertVertexButton()) {
      if (isHoriz) ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_remove = std::string(ICON_FK_PLUS_SQUARE_O) + " Insert vertex";

        if (ImGui::Button(sk_remove.c_str())) {
          send_event(state::annot::InsertVertexEvent());
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Insert a vertex after the selected polygon vertex");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::annot::showToolbarRemoveSelectedVertexButton()) {
      if (needsSpace) {
        if (isHoriz) ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz) ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_remove = std::string(ICON_FK_MINUS_SQUARE_O) + " Remove vertex";

        if (ImGui::Button(sk_remove.c_str())) {
          send_event(state::annot::RemoveSelectedVertexEvent());
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Remove the selected polygon vertex");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::annot::showToolbarUndoButton()) {
      if (needsSpace) {
        if (isHoriz) ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz) ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_cancel = std::string(ICON_FK_UNDO) + " Undo vertex";

        if (ImGui::Button(sk_cancel.c_str())) {
          send_event(state::annot::UndoVertexEvent());
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Undo the last polygon vertex");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::annot::showToolbarCreateButton()) {
      if (needsSpace) {
        if (isHoriz) ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz) ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_addNew = std::string(ICON_FK_PLUS) + " New polygon";
        if (ImGui::Button(sk_addNew.c_str())) {
          send_event(state::annot::CreateNewAnnotationEvent());
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Create a new polygon");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::annot::showToolbarCloseButton()) {
      if (needsSpace) {
        if (isHoriz) ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz) ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_close = std::string(ICON_FK_CIRCLE_O_NOTCH) + " Close polygon";

        if (ImGui::Button(sk_close.c_str())) {
          send_event(state::annot::CloseNewAnnotationEvent());
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Close the polygon");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::annot::showToolbarCompleteButton()) {
      if (needsSpace) {
        if (isHoriz) ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz) ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_complete = std::string(ICON_FK_CHECK) + " Complete";

        if (ImGui::Button(sk_complete.c_str())) {
          send_event(state::annot::CompleteNewAnnotationEvent());
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Complete the polygon");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::annot::showToolbarCancelButton()) {
      if (needsSpace) {
        if (isHoriz) ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz) ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_cancel = std::string(ICON_FK_TIMES) + " Cancel";

        if (ImGui::Button(sk_cancel.c_str())) {
          send_event(state::annot::CancelNewAnnotationEvent());
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Cancel creating the polygon");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::annot::showToolbarRemoveSelectedAnnotationButton()) {
      if (needsSpace) {
        if (isHoriz) ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz) ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_remove = std::string(ICON_FK_TRASH_O) + " Remove polygon";

        if (ImGui::Button(sk_remove.c_str())) {
          send_event(state::annot::RemoveSelectedAnnotationEvent());
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Remove the selected polygon");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::annot::showToolbarCutSelectedAnnotationButton()) {
      if (needsSpace) {
        if (isHoriz) ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz) ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_cut = std::string(ICON_FK_SCISSORS) + " Cut";

        if (ImGui::Button(sk_cut.c_str())) {
          send_event(state::annot::CutSelectedAnnotationEvent());
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Cut the selected polygon to the clipboard");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::annot::showToolbarCopySelectedAnnotationButton()) {
      if (needsSpace) {
        if (isHoriz) ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz) ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_copy = std::string(ICON_FK_FILES_O) + " Copy";

        if (ImGui::Button(sk_copy.c_str())) {
          send_event(state::annot::CopySelectedAnnotationEvent());
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Copy the selected polygon to the clipboard");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::annot::showToolbarPasteSelectedAnnotationButton()) {
      if (needsSpace) {
        if (isHoriz) ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz) ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_paste = std::string(ICON_FK_CLIPBOARD) + " Paste";

        if (ImGui::Button(sk_paste.c_str())) {
          send_event(state::annot::PasteAnnotationEvent());
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Paste the polygon from the clipboard");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::annot::showToolbarFlipAnnotationButtons()) {
      if (needsSpace) {
        if (isHoriz) ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz) ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_flipHoriz = std::string(ICON_FK_ARROWS_H) + " Flip";

        if (ImGui::Button(sk_flipHoriz.c_str())) {
          send_event(state::annot::HorizontallyFlipSelectedAnnotationEvent());
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Flip the polygon horizontally");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;

      if (needsSpace) {
        if (isHoriz) ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz) ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_flipHoriz = std::string(ICON_FK_ARROWS_V) + " Flip";

        if (ImGui::Button(sk_flipHoriz.c_str())) {
          send_event(state::annot::VerticallyFlipSelectedAnnotationEvent());
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Flip the polygon vertically");
        }
        ++id;
      }
      ImGui::PopID();

      needsSpace = true;
    }

    if (state::annot::showToolbarFillButton()) {
      if (needsSpace) {
        if (isHoriz) ImGui::SameLine();
        ImGui::Dummy(buttonSpace);
      }

      if (isHoriz) ImGui::SameLine();
      ImGui::PushID(id);
      {
        static const std::string sk_fill = std::string(ICON_FK_PAINT_BRUSH) + " Fill";

        if (ImGui::Button(sk_fill.c_str())) {
          paintActiveAnnotation();
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", "Fill the active image segmentation with the selected annotation polygon");
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

    if (ImGui::BeginPopupContextWindow()) {
      /// @todo Disable placement in top-left corner
      renderPlacementContextMenu(corner, isHoriz);
      ImGui::EndPopup();
    }
  }

  ImGui::End(); // End toolbar

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
