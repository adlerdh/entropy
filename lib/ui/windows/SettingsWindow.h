#pragma once

#include "common/PublicTypes.h"
#include "logic/app/Settings.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

class AppData;
class ImageColorMap;

/**
 * @brief File-backed user settings actions exposed to the Settings window.
 */
struct SettingsPersistenceCallbacks
{
  std::filesystem::path settingsFile; //!< Platform default JSON settings file shown to the user.
  std::function<bool()> saveSettings; //!< Save current app preferences to the settings file.
  std::function<bool(const std::filesystem::path& fileName)> saveSettingsAs; //!< Save preferences to a selected file.
  std::function<void()> restoreDefaults;        //!< Restore built-in app preference defaults.
  std::function<void()> resetInterfaceSettings; //!< Reset ImGui window, docking, table, and panel state.
  std::function<std::string()> statusText;      //!< Return the latest persistence status message.
};

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
 * @param readjustViewport Callback that reapplies viewport margins after placement changes.
 * @param persistenceCallbacks File-backed user settings callbacks.
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
  const std::function<void()>& readjustViewport,
  const SettingsPersistenceCallbacks& persistenceCallbacks,
  const AllViewsRecenterType& recenterAllViews);
