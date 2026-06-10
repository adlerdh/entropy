#pragma once

#include <filesystem>
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

void renderConfirmCloseAppPopup(AppData& appData, const std::function<void(void)>& quitAppWithoutPrompt);

void renderUnsavedProjectPopup(
  AppData& appData,
  const std::function<bool(void)>& saveProject,
  const std::function<bool(const std::filesystem::path& fileName)>& saveProjectAs,
  const std::function<void(void)>& closeProjectWithoutPrompt,
  const std::function<void(void)>& quitAppWithoutPrompt,
  const std::function<std::filesystem::path(void)>& defaultProjectSaveDirectory,
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

/**
 * @brief Render fallback DICOM folder-entry UI when native folder picking is unavailable.
 * @param appData Application data holding popup state and path text.
 * @param openDicomFolders Callback invoked with folders to scan.
 */
void renderDicomFolderPathPopup(
  AppData& appData,
  const std::function<void(const std::vector<std::filesystem::path>& folderNames)>& openDicomFolders);

/**
 * @brief Render DICOM scan progress, series selection, and metadata inspection popups.
 * @param appData Application data holding DICOM selection state.
 * @param loadSelectedSeries Callback invoked with selected series and reference choice.
 */
void renderDicomSeriesSelectionPopup(
  AppData& appData,
  const std::function<void(
    const std::vector<dicom::SeriesInfo>& series,
    std::optional<std::size_t> referenceSeriesIndex,
    bool addToExistingProject)>& loadSelectedSeries);
