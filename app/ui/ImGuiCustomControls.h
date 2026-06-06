#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui.h>
#include <ImGuiPaletteButton.h>

#include <glm/fwd.hpp>

#include "ui/NativeFileDialogs.h"

#include <optional>
#include <string>
#include <vector>

// Custom controls added to the ImGui system
namespace ImGui
{

std::optional<std::string> renderFileButtonDialogAndWindow(
  const char* buttonText,
  const char* dialogTitle,
  const std::vector<native_dialog::Filter>& dialogFilters);

bool SliderScalarN_multiComp(
  const char* label,
  ImGuiDataType data_type,
  void* v,
  int components,
  const void** v_min,
  const void** v_max,
  const char** format,
  ImGuiSliderFlags flags);

} // namespace ImGui
