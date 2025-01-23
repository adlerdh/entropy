#ifndef UI_STYLE_H
#define UI_STYLE_H

#include <imgui/imgui.h>

/// @see More ImGui themes are found here
/// https://github.com/ocornut/imgui/issues/707

void applyCustomDarkStyle();
void applyCustomDarkStyle2();

/// @brief Light style from Pacôme Danhiez (user itamago)
/// @see https://github.com/ocornut/imgui/pull/511#issuecomment-175719267
void applyCustomLightStyle(bool bStyleDark_, float alpha_);

void applyCustomSoftStyle(ImGuiStyle* dst);

#endif // UI_STYLE_H
