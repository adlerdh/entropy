#pragma once

namespace ui
{

/**
 * @brief Clamp an inspector window's content-fitting height to a useful workspace range.
 *
 * The maximum is the smaller of @p maximumHeight and 45% of the viewport height. This keeps an
 * inspector with many rows from taking over the main workspace.
 */
float fittedInspectionWindowHeight(float desiredHeight, float viewportHeight, float minimumHeight, float maximumHeight);

} // namespace ui
