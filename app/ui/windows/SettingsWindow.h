#pragma once

#include "common/PublicTypes.h"
#include "logic/app/Settings.h"

#include <cstddef>
#include <functional>
#include <optional>

class AppData;
class ImageColorMap;

/**
 * @brief Render the application settings window.
 * @param appData Application data containing mutable settings and window visibility.
 * @param getNumImageColorMaps Callback returning the number of color maps.
 * @param getImageColorMap Callback returning a color map by index.
 * @param updateMetricUniforms Callback that refreshes metric rendering uniforms.
 * @param setUiScaleOverride Callback that applies or clears a manual UI scale override.
 * @param requestFontReload Callback that rebuilds ImGui/NanoVG font resources.
 * @param applyUiColorPreset Callback that applies a color preset immediately.
 * @param applyUiDensityPreset Callback that applies a density preset immediately.
 * @param applyUiWindowBgOpacity Callback that applies window background opacity immediately.
 * @param recenterAllViews Callback used by settings that reposition views.
 */
void renderSettingsWindow(
  AppData& appData,
  const std::function<std::size_t(void)>& getNumImageColorMaps,
  const std::function<const ImageColorMap*(std::size_t cmapIndex)>& getImageColorMap,
  const std::function<void(void)>& updateMetricUniforms,
  const std::function<void(std::optional<float> scale)>& setUiScaleOverride,
  const std::function<void()>& requestFontReload,
  const std::function<void(UiColorPreset preset)>& applyUiColorPreset,
  const std::function<void(UiDensityPreset preset)>& applyUiDensityPreset,
  const std::function<void(float opacity)>& applyUiWindowBgOpacity,
  const AllViewsRecenterType& recenterAllViews);
