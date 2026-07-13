#pragma once

#include "common/Types.h"
#include "logic/app/Settings.h"
#include "ui/GuiData.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace ui::settings
{

struct ScaleChoice
{
  const char* label;          //!< User-visible scale label.
  std::optional<float> scale; //!< Manual UI scale, or nullopt for automatic platform scaling.
};

struct FontChoice
{
  const char* label;   //!< User-visible font family label.
  UiFontFamily family; //!< Font family selected by this choice.
};

struct ScaleBarPositionButton
{
  ScaleBarPosition position; //!< Scale bar position selected by this button.
  const char* label;         //!< Compact button label for the visual 3x3 position selector.
};

struct SettingsPageChoice
{
  GuiData::SettingsTab page; //!< Settings page selected by this navigation choice.
  const char* label;         //!< User-visible page label.
};

/**
 * @brief Return the user-visible label for a scale bar position.
 * @param position Scale bar position.
 * @return Stable label shown in the Settings UI.
 */
const char* scaleBarPositionName(ScaleBarPosition position);

/**
 * @brief Return the orientation forced by an edge-only scale bar position.
 * @param position Scale bar position.
 * @return Horizontal for top/bottom, vertical for left/right, or nullopt for corners.
 */
std::optional<ScaleBarOrientation> forcedScaleBarOrientation(ScaleBarPosition position);

/**
 * @brief Return the orientation to store after a position change.
 * @param position New scale bar position.
 * @param currentOrientation Existing orientation, preserved for corner positions.
 * @return Forced orientation for edge positions, otherwise currentOrientation.
 */
ScaleBarOrientation normalizedScaleBarOrientation(ScaleBarPosition position, ScaleBarOrientation currentOrientation);

/**
 * @brief Return true when the Settings UI should show explicit orientation controls.
 * @param position Scale bar position.
 * @return True for corner positions, false for edge-only positions.
 */
bool canChooseScaleBarOrientation(ScaleBarPosition position);

/** @brief Return scale bar positions in the order shown in Settings. */
const std::array<ScaleBarPosition, 8>& orderedScaleBarPositions();

/** @brief Return scale bar position buttons in visual 3x3 selector order, excluding the center. */
const std::array<ScaleBarPositionButton, 8>& scaleBarPositionButtons();

/**
 * @brief Convert a stored target fraction to the nearest displayed 5-percent integer.
 * @param fraction Stored target length as a fraction of view size.
 * @return Percent in the Settings slider range.
 */
int targetLengthPercentFromFraction(float fraction);

/**
 * @brief Convert a displayed target percent to a clamped stored fraction.
 * @param percent Displayed target length percentage.
 * @return Stored target length fraction.
 */
float targetLengthFractionFromPercent(int percent);

/**
 * @brief Round and clamp the configured scale bar margin in pixels.
 * @param marginPx Stored margin value.
 * @return Displayed integer margin in pixels.
 */
int marginPixelsFromFloat(float marginPx);

/**
 * @brief Convert a displayed margin value to a clamped stored pixel value.
 * @param marginPx Displayed integer margin in pixels.
 * @return Stored margin value.
 */
float marginFloatFromPixels(int marginPx);

/**
 * @brief Clamp a UI precision value to the supported range.
 * @param precision Requested precision.
 * @return Precision clamped to the Settings-supported range.
 */
std::uint32_t clampPrecision(std::uint32_t precision);

/**
 * @brief Return a printf-style fixed precision format string such as "%0.3f".
 * @param precision Requested precision.
 * @return Fixed precision format string using clamped precision.
 */
std::string precisionFormat(std::uint32_t precision);

/** @brief Return UI scale choices in the order shown in Settings. */
const std::array<ScaleChoice, 12>& uiScaleChoices();

/** @brief Return the visible font choices in the order shown in Settings. */
const std::array<FontChoice, 3>& visibleFontChoices();

/** @brief Return UI color presets in the order shown in Settings. */
const std::array<UiColorPreset, 9>& uiColorPresets();

/** @brief Return UI density presets in the order shown in Settings. */
const std::array<UiDensityPreset, 3>& uiDensityPresets();

/** @brief Return settings pages in the order shown in the side navigation. */
const std::array<SettingsPageChoice, 9>& settingsPageChoices();

/**
 * @brief Return the user-visible label for a settings page.
 * @param page Settings page.
 * @return Stable label shown in the Settings side navigation.
 */
const char* settingsPageLabel(GuiData::SettingsTab page);

} // namespace ui::settings
