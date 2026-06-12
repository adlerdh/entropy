#pragma once

#include "logic/app/Settings.h"

#include <imgui/imgui.h>

/// @see More ImGui themes are found here
/// https://github.com/ocornut/imgui/issues/707

///  Return the short display name for a UI color preset.
const char* uiColorPresetName(UiColorPreset preset);
///  Apply a UI color preset to the supplied style or current ImGui style.
void applyUiStylePreset(UiColorPreset preset, ImGuiStyle* dst = nullptr);

///  Return the short display name for a UI density preset.
const char* uiDensityPresetName(UiDensityPreset preset);
///  Apply a UI density preset to the supplied style or current ImGui style.
void applyUiDensityPreset(UiDensityPreset preset, ImGuiStyle* dst = nullptr);

///  Apply the opacity used for regular ImGui window backgrounds.
void applyUiWindowBgOpacity(float opacity, ImGuiStyle* dst = nullptr);

///  Apply Entropy dark colors to the supplied style or current ImGui style.
void applyCustomDarkStyle(ImGuiStyle* dst = nullptr);
void applyCustomDarkStyle2();

/// @brief Light style from Pacôme Danhiez (user itamago)
/// @see https://github.com/ocornut/imgui/pull/511#issuecomment-175719267
void applyCustomLightStyle(bool bStyleDark_, float alpha_);

void applyCustomSoftStyle(ImGuiStyle* dst);
