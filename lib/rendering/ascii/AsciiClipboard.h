#pragma once

#include "common/ClipboardFormats.h"

#include <glm/vec3.hpp>

namespace rendering::ascii_clipboard
{

clipboard::Rgb8 toRgb8(const glm::vec3& rgb);

/**
 * @brief Foreground color used by ASCII export for one source RGB cell.
 *
 * This mirrors the ASCII shader's foreground choice: either the fixed foreground color, or source hue/saturation with
 * value forced to one when colormap foreground mode is enabled.
 */
glm::vec3 foregroundColorForCell(const glm::vec3& srcRgb, bool useColormap, const glm::vec3& fixedForeground);

std::string colorSpanOpen(clipboard::Rgb8 color);

} // namespace rendering::ascii_clipboard
