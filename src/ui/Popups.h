#pragma once

#include "common/Filesystem.h"
#include "ui/GuiData.h"

#include <functional>
#include <uuid.h>

class AppData;

/**
 * @brief Modal popup window for adding a new layout
 * @param appData
 * @param openAddLayoutPopup
 * @param recenterViews
 */
void renderAddLayoutModalPopup(
  AppData& appData,
  bool openAddLayoutPopup,
  const std::function<void(void)>& recenterViews);

void renderAboutDialogModalPopup(bool open);

void renderUnsavedProjectPopup(
  AppData& appData,
  const std::function<bool(void)>& saveProject,
  const std::function<bool(const fs::path& fileName)>& saveProjectAs,
  const std::function<void(void)>& closeProjectWithoutPrompt,
  const std::function<void(void)>& quitAppWithoutPrompt,
  const std::function<fs::path(void)>& defaultProjectSaveDirectory,
  const std::function<std::string(void)>& defaultProjectSaveName);

void renderConfirmSetReferenceImagePopup(
  AppData& appData,
  const std::function<bool(const uuids::uuid& imageUid)>& setReferenceImage);

void renderConfirmRemoveImagePopup(
  AppData& appData,
  const std::function<bool(const uuids::uuid& imageUid)>& removeImage);

void renderLargeImageLoadPromptPopup(
  AppData& appData,
  const std::function<void(GuiData::LargeImageLoadDecision decision)>& handleDecision);
