#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui.h>
#include <ImGuiPaletteButton.h>

#include <glm/fwd.hpp>

#include "ui/NativeFileDialogs.h"

#include <optional>
#include <string>
#include <vector>

/**
 * @brief Custom controls added to the ImGui namespace for Entropy-specific UI widgets.
 */
namespace ImGui
{

/**
 * @brief Render a button that opens a native file dialog and returns the selected file path.
 *
 * @param buttonText Text displayed on the ImGui button.
 * @param dialogTitle Title shown by the native file dialog.
 * @param dialogFilters File type filters passed to the native file dialog.
 * @return Selected file path when the user chooses a file; std::nullopt when the dialog is canceled or no file is
 * selected.
 */
std::optional<std::string> renderFileButtonDialogAndWindow(
  const char* buttonText,
  const char* dialogTitle,
  const std::vector<native_dialog::Filter>& dialogFilters);

/**
 * @brief Render a multi-component scalar slider with per-component bounds and display formats.
 *
 * @param label ImGui label for the slider group.
 * @param data_type ImGui scalar data type stored in @p v.
 * @param v Pointer to the first component value.
 * @param components Number of scalar components to render.
 * @param v_min Per-component minimum value pointers.
 * @param v_max Per-component maximum value pointers.
 * @param format Per-component printf-style display format strings.
 * @param flags ImGui slider flags.
 * @return True when any component value changed.
 */
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
