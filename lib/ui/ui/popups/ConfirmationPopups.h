#pragma once

#include "ui/GuiData.h"

#include <uuid.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class AppData;

/**
 * @brief Render the quit confirmation popup shown when no dirty project prompt is needed.
 */
void renderConfirmCloseAppPopup(AppData& appData, const std::function<void(void)>& quitAppWithoutPrompt);

/**
 *  Render the dirty-application-settings save/discard/cancel popup.
 */
void renderUnsavedAppSettingsPopup(
  AppData& appData,
  const std::function<bool(void)>& saveSettings,
  const std::function<void(void)>& quitAppWithoutPrompt);

/**
 * @brief Render the dirty-project save/discard/cancel popup.
 */
void renderUnsavedProjectPopup(
  AppData& appData,
  const std::function<bool(void)>& saveProject,
  const std::function<bool(const std::filesystem::path& fileName)>& saveProjectAs,
  const std::function<void(void)>& closeProjectWithoutPrompt,
  const std::function<void(void)>& quitAppWithoutPrompt,
  const std::function<std::filesystem::path(void)>& defaultProjectSaveDirectory,
  const std::function<std::string(void)>& defaultProjectSaveName);

/**
 * @brief Render the confirmation popup for reassigning the reference image.
 */
void renderConfirmSetReferenceImagePopup(
  AppData& appData,
  const std::function<bool(const uuids::uuid& imageUid)>& setReferenceImage);

/**
 * @brief Render the confirmation popup for removing an image.
 */
void renderConfirmRemoveImagePopup(
  AppData& appData,
  const std::function<bool(const uuids::uuid& imageUid)>& removeImage);
