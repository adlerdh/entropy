#pragma once

#include <imgui/imgui.h>

#include <glm/fwd.hpp>

#include <vector>

namespace ImGui
{

/**
 * @brief paletteButton
 * @param label Button label
 * @param colors RGBA colors to render
 * @param inverted Flag to invert the color map
 * @param quantize Flag to quantize the color map
 * @param quantizationLevels Number of color map quantization levels
 * @param hsvModFactors HSV modification factors
 * @param size Widget size
 * @return True iff success
 *
 * @copyright Copyright (c) 2018-2020 Michele Morrone
 * All rights reserved.
 * https://michelemorrone.eu - https://BrutPitt.com
 * twitter: https://twitter.com/BrutPitt - github: https://github.com/BrutPitt
 * mailto:brutpitt@gmail.com - mailto:me@michelemorrone.eu
 * This software is distributed under the terms of the BSD 2-Clause license
 *
 * @note Minor modifications have been made to this function for Entropy.
 */
IMGUI_API bool paletteButton(
  const char* label,
  const std::vector<glm::vec4>& colors,
  bool inverted,
  bool quantize,
  int quantizationLevels,
  const glm::vec3& hsvModFactors,
  const ImVec2& size);

} // namespace ImGui
