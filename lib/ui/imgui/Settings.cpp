#define IMGUI_DEFINE_MATH_OPERATORS
#include "ui/ImGuiWrapper.h"

#include "common/LoggingDefaults.h"
#include "common/LoggingSettings.h"
#include "logic/app/AppPaths.h"
#include "logic/app/Data.h"
#include "logic/app/UserPreferences.h"
#include "ui/imgui/Layout.h"
#include "ui/windows/SettingsWindow.h"

#include <imgui/imgui_internal.h>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <functional>
#include <utility>

using namespace ui::imgui_detail;

namespace
{
// Keep status shared by the save prompt and settings panel on the UI thread.
// cppcheck-suppress threadsafety-threadsafety
thread_local std::string s_settingsPersistenceStatus;
} // namespace

bool ImGuiWrapper::saveUserSettingsToDefault()
{
  const std::filesystem::path settingsFile = app_paths::userSettingsFile();
  std::string error;
  if (user_preferences::save(
        m_appData.settings(),
        m_appData.applicationRenderPreferences(),
        user_preferences::precisionPreferencesFrom(m_appData.guiData()),
        settingsFile,
        &error))
  {
    user_preferences::markSavedAppSettingsState(
      m_appData.settings(),
      m_appData.applicationRenderPreferences(),
      m_appData.guiData());
    s_settingsPersistenceStatus = "Saved";
    return true;
  }

  s_settingsPersistenceStatus = "Save failed: " + error;
  return false;
}

void ImGuiWrapper::renderSettingsPanel()
{
  const auto getNumImageColorMaps = std::bind_front(&ImGuiWrapper::getNumImageColorMaps, this);
  const auto getImageColorMap = std::bind_front(&ImGuiWrapper::getImageColorMap, this);
  const auto saveUserSettingsToDefault = std::bind_front(&ImGuiWrapper::saveUserSettingsToDefault, this);
  if (m_appData.guiData().m_showSettingsWindow) {
    const auto applyActivePreferences = [this]() {
      setUserScaleOverride(m_appData.settings().uiScaleOverride());
      requestFontReload();
      applyUiColorPreset(m_appData.settings().uiColorPreset());
      applyUiDensityPreset(m_appData.settings().uiDensityPreset());
      applyUiWindowBgOpacity(m_appData.settings().uiWindowBgOpacity());
      if (m_updateMetricUniforms) {
        m_updateMetricUniforms();
      }
    };
    const std::filesystem::path settingsFile = app_paths::userSettingsFile();
    const SettingsPersistenceCallbacks settingsPersistenceCallbacks{
      .settingsFile = settingsFile,
      .saveSettings = saveUserSettingsToDefault,
      .saveSettingsAs =
        [this](const std::filesystem::path& fileName) {
          std::string error;
          if (user_preferences::save(
                m_appData.settings(),
                m_appData.applicationRenderPreferences(),
                user_preferences::precisionPreferencesFrom(m_appData.guiData()),
                fileName,
                &error))
          {
            s_settingsPersistenceStatus = "Saved " + fileName.filename().string();
            return true;
          }
          s_settingsPersistenceStatus = "Save failed: " + error;
          return false;
        },
      .restoreDefaults =
        [this, applyActivePreferences]() {
          auto currentProjectPresentation = user_preferences::renderPreferencesFrom(m_appData.renderSettings());
          const bool projectLoaded = ProjectLoadState::Loaded == m_appData.state().projectLoadState();
          const bool synchronizeTimeSeries = m_appData.settings().synchronizeTimeSeries();
          const bool lockAnatomicalDirections = m_appData.settings().lockAnatomicalCoordinateAxesWithReferenceImage();
          m_appData.applicationRenderPreferences() = user_preferences::defaultRenderPreferences();
          auto liveDefaults = m_appData.applicationRenderPreferences();
          if (projectLoaded) {
            user_preferences::preserveProjectPresentation(liveDefaults, currentProjectPresentation);
          }
          m_appData.settings() = AppSettings{};
          if (projectLoaded) {
            m_appData.settings().setSynchronizeTimeSeries(synchronizeTimeSeries);
            m_appData.settings().setLockAnatomicalCoordinateAxesWithReferenceImage(lockAnatomicalDirections);
          }
          user_preferences::applyRenderPreferencesTo(m_appData.renderSettings(), liveDefaults);
          user_preferences::applyPrecisionPreferencesTo(m_appData.guiData(), user_preferences::PrecisionPreferences{});
          logging::setApplicationLogLevel(logging::defaultLogLevel());
          logging::setLoggingEnabled(true);
          applyActivePreferences();
          syncLayoutTabGuiDataFromSettings(m_appData);
          user_preferences::updateAppSettingsDirtyState(
            m_appData.settings(),
            m_appData.applicationRenderPreferences(),
            m_appData.guiData());
          s_settingsPersistenceStatus = "Defaults restored";
        },
      .resetInterfaceSettings =
        [this]() {
          ImGui::ClearIniSettings();
          ImGui::SaveIniSettingsToDisk(m_iniFileName.c_str());
          m_appData.guiData().m_showImagePropertiesWindow = true;
          m_appData.guiData().m_showSegmentationsWindow = true;
          m_applyDefaultPanelLayout = true;
          if (m_readjustViewport) {
            m_readjustViewport();
          }
          if (m_postEmptyGlfwEvent) {
            m_postEmptyGlfwEvent();
          }
          s_settingsPersistenceStatus = "Interface reset";
        },
      .statusText =
        []() {
          return s_settingsPersistenceStatus;
        }};

    renderSettingsWindow(
      m_appData,
      getNumImageColorMaps,
      getImageColorMap,
      m_updateMetricUniforms,
      [this](std::optional<float> scale) { setUserScaleOverride(scale); },
      [this]() { requestFontReload(); },
      [this](UiColorPreset preset) { applyUiColorPreset(preset); },
      [this](UiDensityPreset preset) { applyUiDensityPreset(preset); },
      [this](float opacity) { applyUiWindowBgOpacity(opacity); },
      m_readjustViewport,
      settingsPersistenceCallbacks,
      m_recenterAllViews);
  }
}
