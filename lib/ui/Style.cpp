#include "ui/Style.h"

#include <algorithm>

void applyCustomLightStyle(bool bStyleDark_, float alpha_)
{
  ImGuiStyle& style = ImGui::GetStyle();

  style.Alpha = 1.0f;
  style.FrameRounding = 3.0f;

  ImVec4* colors = style.Colors;
  if (!colors) return;

  colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 0.94f);
  colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
  colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
  colors[ImGuiCol_BorderShadow] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
  colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.98f, 0.98f, 0.98f, 0.53f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
  colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.50f);
  colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
  colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
  colors[ImGuiCol_PlotLines] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
  colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
  colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
  colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
  colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
  colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

  if (bStyleDark_) {
    for (int i = 0; i < ImGuiCol_COUNT; i++) {
      ImVec4& col = colors[i];
      float H, S, V;
      ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, H, S, V);

      if (S < 0.1f) {
        V = 1.0f - V;
      }

      ImGui::ColorConvertHSVtoRGB(H, S, V, col.x, col.y, col.z);

      if (col.w < 1.00f) {
        col.w *= alpha_;
      }
    }
  }
  else {
    for (int i = 0; i < ImGuiCol_COUNT; i++) {
      ImVec4& col = colors[i];
      if (col.w < 1.00f) {
        col.x *= alpha_;
        col.y *= alpha_;
        col.z *= alpha_;
        col.w *= alpha_;
      }
    }
  }
}

void applyCustomDarkStyle(ImGuiStyle* dst)
{
  ImGuiStyle& style = dst ? *dst : ImGui::GetStyle();
  ImGui::StyleColorsDark(&style);

  style.ChildRounding = 3;
  style.FrameRounding = 4;
  style.GrabRounding = 3;
  style.PopupRounding = 4;
  style.TabRounding = 4;
  style.WindowRounding = 5;

  ImVec4* colors = style.Colors;
  if (!colors) return;

  colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
  colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
  colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.21f, 0.22f, 0.54f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.40f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.22f, 0.25f, 0.30f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
  colors[ImGuiCol_CheckboxSelectedBg] = ImVec4(0.20f, 0.41f, 0.68f, 0.65f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.44f, 0.44f, 0.44f, 0.40f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.46f, 0.47f, 0.48f, 1.00f);
  // colors[ImGuiCol_ButtonActive]         = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.22f, 0.29f, 0.38f, 1.00f);
  colors[ImGuiCol_Header] = colors[ImGuiCol_HeaderActive];
  colors[ImGuiCol_HeaderHovered] = colors[ImGuiCol_HeaderActive];
  colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
  colors[ImGuiCol_SeparatorHovered] = ImVec4(0.72f, 0.72f, 0.72f, 0.78f);
  colors[ImGuiCol_SeparatorActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
  colors[ImGuiCol_ResizeGrip] = ImVec4(0.91f, 0.91f, 0.91f, 0.25f);
  colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.81f, 0.81f, 0.81f, 0.67f);
  colors[ImGuiCol_ResizeGripActive] = ImVec4(0.46f, 0.46f, 0.46f, 0.95f);
  colors[ImGuiCol_InputTextCursor] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.46f, 0.47f, 0.48f, 1.00f);
  colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.16f, 0.17f, 0.92f);
  colors[ImGuiCol_TabSelected] = ImVec4(0.29f, 0.29f, 0.29f, 1.00f);
  colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
  colors[ImGuiCol_TabDimmed] = ImVec4(0.10f, 0.10f, 0.11f, 0.92f);
  colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.20f, 0.20f, 0.21f, 1.00f);
  colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.20f, 0.41f, 0.68f, 0.55f);
  colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
  colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
  colors[ImGuiCol_PlotHistogram] = ImVec4(0.73f, 0.60f, 0.15f, 1.00f);
  colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
  colors[ImGuiCol_TextSelectedBg] = ImVec4(0.87f, 0.87f, 0.87f, 0.35f);
  colors[ImGuiCol_TreeLines] = ImVec4(0.50f, 0.50f, 0.56f, 0.50f);
  colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
  colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
  colors[ImGuiCol_DragDropTargetBg] = ImVec4(1.00f, 1.00f, 0.00f, 0.12f);
  colors[ImGuiCol_UnsavedMarker] = ImVec4(0.86f, 0.68f, 0.18f, 1.00f);
  colors[ImGuiCol_NavHighlight] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
  colors[ImGuiCol_NavCursor] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
  colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
  colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
}

void applyCustomDarkStyle2()
{
  ImGuiStyle* style = &ImGui::GetStyle();

  ImVec4* colors = style->Colors;
  if (!colors) return;

  colors[ImGuiCol_Text] = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.500f, 0.500f, 0.500f, 1.000f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.180f, 0.180f, 0.180f, 0.940f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.280f, 0.280f, 0.280f, 0.000f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.313f, 0.313f, 0.313f, 0.940f);
  colors[ImGuiCol_Border] = ImVec4(0.266f, 0.266f, 0.266f, 1.000f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.160f, 0.160f, 0.160f, 0.540f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.200f, 0.200f, 0.200f, 0.400f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.280f, 0.280f, 0.280f, 0.670f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.148f, 0.148f, 0.148f, 1.000f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.148f, 0.148f, 0.148f, 1.000f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.148f, 0.148f, 0.148f, 0.510f);
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.195f, 0.195f, 0.195f, 1.000f);
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.160f, 0.160f, 0.160f, 0.530f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.277f, 0.277f, 0.277f, 1.000f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.300f, 0.300f, 0.300f, 1.000f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
  colors[ImGuiCol_CheckMark] = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.391f, 0.391f, 0.391f, 1.000f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
  colors[ImGuiCol_Button] = ImVec4(1.000f, 1.000f, 1.000f, 0.400f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
  colors[ImGuiCol_ButtonActive] = ImVec4(1.000f, 1.000f, 1.000f, 0.391f);
  colors[ImGuiCol_Header] = ImVec4(0.313f, 0.313f, 0.313f, 0.31f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.469f, 0.469f, 0.469f, 1.000f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.469f, 0.469f, 0.469f, 1.000f);
  colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
  colors[ImGuiCol_SeparatorHovered] = ImVec4(0.391f, 0.391f, 0.391f, 0.78f);
  colors[ImGuiCol_SeparatorActive] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
  colors[ImGuiCol_ResizeGrip] = ImVec4(1.000f, 1.000f, 1.000f, 0.250f);
  colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.000f, 1.000f, 1.000f, 0.670f);
  colors[ImGuiCol_ResizeGripActive] = ImVec4(1.000f, 0.391f, 0.000f, 0.95f);
  colors[ImGuiCol_Tab] = ImVec4(0.098f, 0.098f, 0.098f, 1.000f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.352f, 0.352f, 0.352f, 1.000f);
  colors[ImGuiCol_TabActive] = ImVec4(0.195f, 0.195f, 0.195f, 1.000f);
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.098f, 0.098f, 0.098f, 1.000f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.195f, 0.195f, 0.195f, 1.000f);
  colors[ImGuiCol_PlotLines] = ImVec4(0.469f, 0.469f, 0.469f, 1.000f);
  colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
  colors[ImGuiCol_PlotHistogram] = ImVec4(0.586f, 0.586f, 0.586f, 1.000f);
  colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
  colors[ImGuiCol_TextSelectedBg] = ImVec4(1.000f, 1.000f, 1.000f, 0.35f);
  colors[ImGuiCol_DragDropTarget] = ImVec4(1.000f, 0.391f, 0.000f, 0.90f);
  colors[ImGuiCol_NavHighlight] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
  colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.000f, 0.391f, 0.000f, 0.70f);
  colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.000f, 0.000f, 0.000f, 0.20f);
  colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.000f, 0.000f, 0.000f, 0.35f);
}

void applyCustomSoftStyle(ImGuiStyle* dst)
{
  ImGuiStyle* style = dst ? dst : &ImGui::GetStyle();
  if (!style) return;

  float hspacing = 8;
  float vspacing = 6;

  style->DisplaySafeAreaPadding = ImVec2(0, 0);
  style->WindowPadding = ImVec2(hspacing / 2, vspacing);
  style->FramePadding = ImVec2(hspacing, vspacing);
  style->ItemSpacing = ImVec2(hspacing, vspacing);
  style->ItemInnerSpacing = ImVec2(hspacing, vspacing);
  style->IndentSpacing = 20.0f;

  style->WindowRounding = 0.0f;
  style->FrameRounding = 0.0f;

  style->WindowBorderSize = 0.0f;
  style->FrameBorderSize = 1.0f;
  style->PopupBorderSize = 1.0f;

  style->ScrollbarSize = 20.0f;
  style->ScrollbarRounding = 0.0f;
  style->GrabMinSize = 5.0f;
  style->GrabRounding = 0.0f;

  ImVec4* colors = style->Colors;
  if (!colors) return;

  ImVec4 white = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
  ImVec4 transparent = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  ImVec4 dark = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
  ImVec4 darker = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);

  ImVec4 background = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
  ImVec4 text = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
  ImVec4 border = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
  ImVec4 grab = ImVec4(0.69f, 0.69f, 0.69f, 1.00f);
  ImVec4 header = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
  ImVec4 active = ImVec4(0.00f, 0.47f, 0.84f, 1.00f);
  ImVec4 hover = ImVec4(0.00f, 0.47f, 0.84f, 0.20f);

  colors[ImGuiCol_Text] = text;
  colors[ImGuiCol_WindowBg] = background;
  colors[ImGuiCol_ChildBg] = background;
  colors[ImGuiCol_PopupBg] = white;

  colors[ImGuiCol_Border] = border;
  colors[ImGuiCol_BorderShadow] = transparent;

  colors[ImGuiCol_Button] = header;
  colors[ImGuiCol_ButtonHovered] = hover;
  colors[ImGuiCol_ButtonActive] = active;

  colors[ImGuiCol_FrameBg] = white;
  colors[ImGuiCol_FrameBgHovered] = hover;
  colors[ImGuiCol_FrameBgActive] = active;

  colors[ImGuiCol_MenuBarBg] = header;
  colors[ImGuiCol_Header] = header;
  colors[ImGuiCol_HeaderHovered] = hover;
  colors[ImGuiCol_HeaderActive] = active;

  colors[ImGuiCol_CheckMark] = text;
  colors[ImGuiCol_SliderGrab] = grab;
  colors[ImGuiCol_SliderGrabActive] = darker;

  colors[ImGuiCol_ScrollbarBg] = header;
  colors[ImGuiCol_ScrollbarGrab] = grab;
  colors[ImGuiCol_ScrollbarGrabHovered] = dark;
  colors[ImGuiCol_ScrollbarGrabActive] = darker;
}

namespace
{
ImVec4 withAlpha(const ImVec4& color, float alpha)
{
  return ImVec4(color.x, color.y, color.z, alpha);
}

void applyDarkAccentPalette(
  ImGuiStyle& style,
  const ImVec4& bg,
  const ImVec4& panel,
  const ImVec4& frame,
  const ImVec4& accent)
{
  ImVec4* colors = style.Colors;
  if (!colors) {
    return;
  }

  const ImVec4 text(0.90f, 0.93f, 0.96f, 1.00f);
  const ImVec4 textDisabled(0.48f, 0.52f, 0.56f, 1.00f);
  const ImVec4 border(0.28f, 0.32f, 0.38f, 0.58f);
  const ImVec4 hover = withAlpha(accent, 0.62f);
  const ImVec4 active = withAlpha(accent, 0.92f);

  colors[ImGuiCol_Text] = text;
  colors[ImGuiCol_TextDisabled] = textDisabled;
  colors[ImGuiCol_WindowBg] = withAlpha(bg, 0.96f);
  colors[ImGuiCol_ChildBg] = withAlpha(bg, 0.00f);
  colors[ImGuiCol_PopupBg] = withAlpha(panel, 0.96f);
  colors[ImGuiCol_Border] = border;
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

  colors[ImGuiCol_FrameBg] = withAlpha(frame, 0.70f);
  colors[ImGuiCol_FrameBgHovered] = withAlpha(accent, 0.34f);
  colors[ImGuiCol_FrameBgActive] = withAlpha(accent, 0.62f);
  colors[ImGuiCol_TitleBg] = withAlpha(bg, 1.00f);
  colors[ImGuiCol_TitleBgActive] = withAlpha(panel, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = withAlpha(bg, 0.75f);
  colors[ImGuiCol_MenuBarBg] = withAlpha(panel, 1.00f);

  colors[ImGuiCol_ScrollbarBg] = withAlpha(bg, 0.55f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.32f, 0.36f, 0.40f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.42f, 0.46f, 0.50f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = active;
  colors[ImGuiCol_CheckMark] = active;
  colors[ImGuiCol_CheckboxSelectedBg] = withAlpha(accent, 0.45f);
  colors[ImGuiCol_SliderGrab] = withAlpha(accent, 0.78f);
  colors[ImGuiCol_SliderGrabActive] = active;

  colors[ImGuiCol_Button] = withAlpha(frame, 0.78f);
  colors[ImGuiCol_ButtonHovered] = hover;
  colors[ImGuiCol_ButtonActive] = active;
  colors[ImGuiCol_Header] = withAlpha(accent, 0.28f);
  colors[ImGuiCol_HeaderHovered] = hover;
  colors[ImGuiCol_HeaderActive] = active;
  colors[ImGuiCol_Separator] = border;
  colors[ImGuiCol_SeparatorHovered] = hover;
  colors[ImGuiCol_SeparatorActive] = active;
  colors[ImGuiCol_ResizeGrip] = withAlpha(accent, 0.25f);
  colors[ImGuiCol_ResizeGripHovered] = withAlpha(accent, 0.62f);
  colors[ImGuiCol_ResizeGripActive] = active;

  colors[ImGuiCol_InputTextCursor] = text;

  colors[ImGuiCol_Tab] = withAlpha(frame, 0.82f);
  colors[ImGuiCol_TabHovered] = hover;
  colors[ImGuiCol_TabSelected] = withAlpha(panel, 1.00f);
  colors[ImGuiCol_TabSelectedOverline] = active;
  colors[ImGuiCol_TabDimmed] = withAlpha(bg, 0.86f);
  colors[ImGuiCol_TabDimmedSelected] = withAlpha(frame, 0.92f);
  colors[ImGuiCol_TabDimmedSelectedOverline] = withAlpha(accent, 0.55f);

  colors[ImGuiCol_PlotLines] = ImVec4(0.68f, 0.72f, 0.76f, 1.00f);
  colors[ImGuiCol_PlotLinesHovered] = active;
  colors[ImGuiCol_PlotHistogram] = withAlpha(accent, 0.78f);
  colors[ImGuiCol_PlotHistogramHovered] = active;
  colors[ImGuiCol_TableHeaderBg] = withAlpha(frame, 0.92f);
  colors[ImGuiCol_TableBorderStrong] = ImVec4(0.34f, 0.38f, 0.44f, 1.00f);
  colors[ImGuiCol_TableBorderLight] = ImVec4(0.22f, 0.26f, 0.30f, 1.00f);
  colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.05f);
  colors[ImGuiCol_TextLink] = withAlpha(accent, 1.00f);
  colors[ImGuiCol_TextSelectedBg] = withAlpha(accent, 0.35f);
  colors[ImGuiCol_TreeLines] = border;
  colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 0.86f, 0.26f, 0.90f);
  colors[ImGuiCol_DragDropTargetBg] = ImVec4(1.00f, 0.86f, 0.26f, 0.12f);
  colors[ImGuiCol_UnsavedMarker] = ImVec4(1.00f, 0.70f, 0.16f, 1.00f);
  colors[ImGuiCol_NavHighlight] = active;
  colors[ImGuiCol_NavCursor] = active;
  colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
  colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.35f);
  colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.45f);
}

void applySoftLightPalette(ImGuiStyle& style)
{
  ImGui::StyleColorsLight(&style);
  ImVec4* colors = style.Colors;
  if (!colors) {
    return;
  }

  const ImVec4 accent(0.12f, 0.36f, 0.70f, 1.00f);
  colors[ImGuiCol_Text] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.48f, 0.52f, 0.56f, 1.00f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.90f, 0.92f, 0.94f, 0.96f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.90f, 0.92f, 0.94f, 0.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.96f, 0.97f, 0.98f, 0.98f);
  colors[ImGuiCol_Border] = ImVec4(0.56f, 0.60f, 0.66f, 0.55f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.98f, 0.98f, 0.99f, 0.95f);
  colors[ImGuiCol_FrameBgHovered] = withAlpha(accent, 0.20f);
  colors[ImGuiCol_FrameBgActive] = withAlpha(accent, 0.34f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.82f, 0.85f, 0.89f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.74f, 0.79f, 0.85f, 1.00f);
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.82f, 0.85f, 0.89f, 1.00f);
  colors[ImGuiCol_CheckMark] = accent;
  colors[ImGuiCol_SliderGrab] = withAlpha(accent, 0.72f);
  colors[ImGuiCol_SliderGrabActive] = accent;
  colors[ImGuiCol_Button] = ImVec4(0.78f, 0.83f, 0.89f, 0.80f);
  colors[ImGuiCol_ButtonHovered] = withAlpha(accent, 0.28f);
  colors[ImGuiCol_ButtonActive] = withAlpha(accent, 0.55f);
  colors[ImGuiCol_Header] = ImVec4(0.78f, 0.83f, 0.89f, 0.70f);
  colors[ImGuiCol_HeaderHovered] = withAlpha(accent, 0.25f);
  colors[ImGuiCol_HeaderActive] = withAlpha(accent, 0.45f);
  colors[ImGuiCol_TextSelectedBg] = withAlpha(accent, 0.25f);
}
} // namespace

const char* uiColorPresetName(UiColorPreset preset)
{
  switch (preset) {
    case UiColorPreset::EntropyDark:
      return "Entropy Dark";
    case UiColorPreset::ImGuiDark:
      return "ImGui Dark";
    case UiColorPreset::ImGuiClassic:
      return "ImGui Classic";
    case UiColorPreset::ImGuiLight:
      return "ImGui Light";
    case UiColorPreset::SlateBlue:
      return "Slate Blue";
    case UiColorPreset::Graphite:
      return "Graphite";
    case UiColorPreset::DeepTeal:
      return "Deep Teal";
    case UiColorPreset::Midnight:
      return "Midnight";
    case UiColorPreset::SoftLight:
      return "Soft Light";
  }

  return "Entropy Dark";
}

const char* uiDensityPresetName(UiDensityPreset preset)
{
  switch (preset) {
    case UiDensityPreset::Compact:
      return "Compact";
    case UiDensityPreset::Default:
      return "Default";
    case UiDensityPreset::Comfortable:
      return "Comfortable";
  }

  return "Default";
}

void applyUiWindowBgOpacity(float opacity, ImGuiStyle* dst)
{
  ImGuiStyle& style = dst ? *dst : ImGui::GetStyle();
  style.Colors[ImGuiCol_WindowBg].w = std::clamp(opacity, 0.2f, 1.0f);
}

void applyUiDensityPreset(UiDensityPreset preset, ImGuiStyle* dst)
{
  ImGuiStyle& style = dst ? *dst : ImGui::GetStyle();

  switch (preset) {
    case UiDensityPreset::Compact:
      style.WindowPadding = ImVec2(6.0f, 5.0f);
      style.FramePadding = ImVec2(6.0f, 3.0f);
      style.ItemSpacing = ImVec2(6.0f, 4.0f);
      style.ItemInnerSpacing = ImVec2(5.0f, 3.0f);
      style.CellPadding = ImVec2(4.0f, 2.0f);
      style.IndentSpacing = 16.0f;
      style.ScrollbarSize = 12.0f;
      style.GrabMinSize = 8.0f;
      style.WindowRounding = 3.0f;
      style.ChildRounding = 2.0f;
      style.PopupRounding = 3.0f;
      style.FrameRounding = 3.0f;
      style.GrabRounding = 2.0f;
      style.TabRounding = 3.0f;
      break;
    case UiDensityPreset::Default:
      style.WindowPadding = ImVec2(8.0f, 8.0f);
      style.FramePadding = ImVec2(8.0f, 4.0f);
      style.ItemSpacing = ImVec2(8.0f, 6.0f);
      style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
      style.CellPadding = ImVec2(4.0f, 2.0f);
      style.IndentSpacing = 20.0f;
      style.ScrollbarSize = 14.0f;
      style.GrabMinSize = 10.0f;
      style.WindowRounding = 5.0f;
      style.ChildRounding = 3.0f;
      style.PopupRounding = 4.0f;
      style.FrameRounding = 4.0f;
      style.GrabRounding = 3.0f;
      style.TabRounding = 4.0f;
      break;
    case UiDensityPreset::Comfortable:
      style.WindowPadding = ImVec2(12.0f, 10.0f);
      style.FramePadding = ImVec2(10.0f, 6.0f);
      style.ItemSpacing = ImVec2(10.0f, 8.0f);
      style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
      style.CellPadding = ImVec2(6.0f, 4.0f);
      style.IndentSpacing = 24.0f;
      style.ScrollbarSize = 17.0f;
      style.GrabMinSize = 12.0f;
      style.WindowRounding = 7.0f;
      style.ChildRounding = 5.0f;
      style.PopupRounding = 6.0f;
      style.FrameRounding = 6.0f;
      style.GrabRounding = 5.0f;
      style.TabRounding = 6.0f;
      break;
  }
}

void applyUiStylePreset(UiColorPreset preset, ImGuiStyle* dst)
{
  ImGuiStyle& style = dst ? *dst : ImGui::GetStyle();

  switch (preset) {
    case UiColorPreset::EntropyDark:
      applyCustomDarkStyle(&style);
      break;
    case UiColorPreset::ImGuiDark:
      ImGui::StyleColorsDark(&style);
      break;
    case UiColorPreset::ImGuiClassic:
      ImGui::StyleColorsClassic(&style);
      break;
    case UiColorPreset::ImGuiLight:
      ImGui::StyleColorsLight(&style);
      break;
    case UiColorPreset::SlateBlue:
      ImGui::StyleColorsDark(&style);
      applyDarkAccentPalette(
        style,
        ImVec4(0.055f, 0.070f, 0.095f, 1.00f),
        ImVec4(0.090f, 0.120f, 0.165f, 1.00f),
        ImVec4(0.120f, 0.165f, 0.230f, 1.00f),
        ImVec4(0.200f, 0.480f, 0.800f, 1.00f));
      break;
    case UiColorPreset::Graphite:
      ImGui::StyleColorsDark(&style);
      applyDarkAccentPalette(
        style,
        ImVec4(0.070f, 0.070f, 0.070f, 1.00f),
        ImVec4(0.120f, 0.120f, 0.120f, 1.00f),
        ImVec4(0.185f, 0.185f, 0.185f, 1.00f),
        ImVec4(0.780f, 0.560f, 0.240f, 1.00f));
      break;
    case UiColorPreset::DeepTeal:
      ImGui::StyleColorsDark(&style);
      applyDarkAccentPalette(
        style,
        ImVec4(0.040f, 0.070f, 0.075f, 1.00f),
        ImVec4(0.065f, 0.110f, 0.115f, 1.00f),
        ImVec4(0.095f, 0.155f, 0.160f, 1.00f),
        ImVec4(0.000f, 0.560f, 0.530f, 1.00f));
      break;
    case UiColorPreset::Midnight:
      ImGui::StyleColorsDark(&style);
      applyDarkAccentPalette(
        style,
        ImVec4(0.030f, 0.035f, 0.060f, 1.00f),
        ImVec4(0.055f, 0.065f, 0.105f, 1.00f),
        ImVec4(0.085f, 0.100f, 0.155f, 1.00f),
        ImVec4(0.360f, 0.520f, 0.950f, 1.00f));
      break;
    case UiColorPreset::SoftLight:
      applySoftLightPalette(style);
      break;
  }
}
