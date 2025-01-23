#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui.h>

#include <glm/fwd.hpp>

#include <optional>
#include <string>
#include <vector>

// Custom controls added to the ImGui system
namespace ImGui
{

/**
 * @brief paletteButton
 * @param label Button label
 * @param numColors Number of colors in the buffer
 * @param buff Float array of RGBA tuplets, with size equal to 4*numCol
 * @param inverted Flag to invert the color map
 * @param size Widget size
 * @return True iff success
 *
 * @copyright Copyright (c) 2018-2020 Michele Morrone
 * All rights reserved.
 * https://michelemorrone.eu - https://BrutPitt.com
 * twitter: https://twitter.com/BrutPitt - github: https://github.com/BrutPitt
 * mailto:brutpitt@gmail.com - mailto:me@michelemorrone.eu
 * This software is distributed under the terms of the BSD 2-Clause license
 */
IMGUI_API bool paletteButton(
  const char* label,
  const std::vector<glm::vec4>& colors,
  bool inverted,
  bool quantize,
  int quantizationLevels,
  const glm::vec3& hsvModFactors,
  const ImVec2& size
);

std::optional<std::string> renderFileButtonDialogAndWindow(
  const char* buttonText, const char* dialogTitle, const std::vector<std::string> dialogFilters
);

bool SliderScalarN_multiComp(
  const char* label,
  ImGuiDataType data_type,
  void* v,
  int components,
  const void** v_min,
  const void** v_max,
  const char** format,
  ImGuiSliderFlags flags
);

} // namespace ImGui
