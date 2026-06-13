#pragma once

#include "image/DicomSeries.h"
#include "image/ImageHeader.h"

#include <glm/vec2.hpp>
#include <imgui/imgui.h>
#include <uuid.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Data for the user interface
 */
struct GuiData
{
  enum class SettingsTab : std::uint8_t
  {
    Synchronize
  };

  /// Global setting to turn on/off rendering of the UI windows
  bool m_renderUiWindows = false;

  /// Global setting to turn on/off rendering of the UI overlays (crosshairs, anatomical labels)
  bool m_renderUiOverlays = false;

  // Flags to show specific UI windows
  bool m_showImagePropertiesWindow = true;                          //!< Show image properties window
  bool m_showSegmentationsWindow = false;                           //!< Show segmentations window
  bool m_showLandmarksWindow = false;                               //!< Show landmarks window
  bool m_showAnnotationsWindow = false;                             //!< Show annotations window
  bool m_showIsosurfacesWindow = false;                             //!< Show isosurfaces window
  bool m_showSettingsWindow = false;                                //!< Show settings window
  std::optional<SettingsTab> m_requestedSettingsTab = std::nullopt; //!< One-shot requested Settings tab
  bool m_showInspectionWindow = true;                               //!< Show cursor inspection window
  bool m_showOpacityBlenderWindow = false;                          //!< Show opacity blender window
  bool m_showImGuiDemoWindow = false;                               //!< Show ImGui demo window
  bool m_showImPlotDemoWindow = false;                              //!< Show ImPlot demo window
  bool m_showAboutDialog = false;                                   //!< Show About Entropy dialog
  bool m_showAddLayoutPopup = false;                                //!< Show the Add Layout popup

  /// Flag to show dialog confirming closing of the application window.
  enum class UnsavedProjectAction : std::uint8_t
  {
    CloseProject,
    QuitApp
  };

  /// This is set to false until the user requests to close a dirty project or quit the app.
  bool m_showUnsavedProjectPopup = false;
  UnsavedProjectAction m_pendingUnsavedProjectAction = UnsavedProjectAction::CloseProject;

  /// This is set to false until the user requests to quit with no unsaved project changes.
  bool m_showConfirmCloseAppPopup = false;

  /// Flag to show dialog confirming reference image reassignment.
  bool m_showConfirmSetReferenceImagePopup = false;
  std::optional<uuids::uuid> m_pendingReferenceImageUid = std::nullopt;

  /// Flag to show dialog confirming image removal.
  bool m_showConfirmRemoveImagePopup = false;
  std::optional<uuids::uuid> m_pendingRemoveImageUid = std::nullopt;

  struct LargeImageLoadPrompt
  {
    std::filesystem::path fileName;
    ImageHeader header;
    bool allowCancelProject = false;
    bool allowSkipImage = true;
  };

  enum class LargeImageLoadDecision : std::uint8_t
  {
    LoadOriginal,
    SkipImage,
    CancelProject
  };

  bool m_showLargeImageLoadPrompt = false;
  bool m_bypassNextImageLoadPreflight = false;
  std::optional<LargeImageLoadPrompt> m_pendingLargeImageLoadPrompt = std::nullopt;

  struct DicomSeriesSelectionPrompt
  {
    std::vector<dicom::SeriesInfo> series; //!< Discovered DICOM series offered to the user for loading.
    std::vector<bool> selected;            //!< Per-series load selection state, parallel to series.
    std::vector<std::string> warnings;     //!< Per-series warning text, parallel to series.

    /// Index into series for the image that should become reference when reference selection is allowed.
    int referenceSeriesIndex = 0;

    bool addToExistingProject = false;   //!< True when selected series should be appended to the current project.
    bool allowReferenceSelection = true; //!< False when appending images or otherwise preserving the current reference.

    /// Series whose full DICOM metadata modal is currently open.
    std::optional<std::size_t> metadataSeriesIndex = std::nullopt;

    /// Series whose middle-slice previews are currently shown in the preview panel.
    std::optional<std::size_t> previewSeriesIndex = std::nullopt;

    /// Cached slice previews, indexed by series, to avoid reloading preview images every frame.
    std::vector<std::vector<dicom::SlicePreview>> previewCache;

    /// Per-series preview loading errors, parallel to series.
    std::vector<std::string> previewErrors;

    int previewSliceCount = 10; //!< Number of representative slices requested for each series preview.

    /// Visibility state for the DICOM series table columns, ordered to match kSeriesColumnNames in Popups.cpp.
    std::array<bool, 16> seriesColumnVisible{
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      false,
      false,
      true,
      true,
      true,
      true};
  };

  bool m_showDicomFolderPathPopup = false;
  std::string m_dicomFolderPathText;

  /// True while the background DICOM discovery task is scanning folders/files.
  bool m_dicomSeriesScanInProgress = false;

  /// Root path displayed while a DICOM discovery task is pending.
  std::filesystem::path m_pendingDicomScanRoot;

  /// Opens the modal that lets the user choose which discovered DICOM series to load.
  bool m_showDicomSeriesSelectionPopup = false;

  /// Pending DICOM series selection dialog state, cleared when the dialog is accepted or canceled.
  std::optional<DicomSeriesSelectionPrompt> m_pendingDicomSeriesSelectionPrompt = std::nullopt;

  /// Map of imageUid to boolean of whether its image color map window is shown.
  /// (The color map window is shown as a popup from the Image Properties window)
  std::unordered_map<uuids::uuid, bool> m_showImageColormapWindow;

  // Show the color map windows for the difference, cross-correlation, and histogram metric views
  bool m_showDifferenceColormapWindow = false;     //!< Show difference colormap window
  bool m_showCorrelationColormapWindow = false;    //!< Show correlation colormap window
  bool m_showJointHistogramColormapWindow = false; //!< Show joint histogram colormap window

  void setCoordsPrecisionFormat();
  void setTxPrecisionFormat();

  /// Precision format string used for spatial coordinates
  std::string m_coordsPrecisionFormat = "%0.3f";
  uint32_t m_coordsPrecision = 3;

  /// Precision format string used for image transformations
  std::string m_txPrecisionFormat = "%0.3f";
  uint32_t m_txPrecision = 3;

  /// Precision format string used for image values
  std::string m_imageValuePrecisionFormat = "%0.3f";
  uint32_t m_imageValuePrecision = 3;

  /// Precision format string used for percentiles
  std::string m_percentilePrecisionFormat = "%0.2f";
  uint32_t m_percentilePrecision = 2;

  // Pointers to fonts allocated by ImGui.
  // Font data uses raw pointers; ImGui takes ownership and deletes them.
  std::unordered_map<std::string, ImFont*> m_fonts;

  bool m_showMainMenuBar = false;
  glm::vec2 m_mainMenuBarDims{0.0f, 0.0f};

  bool m_showModeToolbar = true;
  bool m_isModeToolbarHorizontal = false;
  int m_modeToolbarCorner = 1;
  glm::vec2 m_modeToolbarDockDims{0.0f, 0.0f};

  bool m_showSegToolbar = false;
  bool m_isSegToolbarHorizontal = false;
  int m_segToolbarCorner = 0;
  glm::vec2 m_segToolbarDockDims{0.0f, 0.0f};

  // Corners: -1 custom, 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right

  struct Margins
  {
    float left = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float top = 0.0f;
  };

  /// Compute UI margins based on visibility of the menu, toolbars, and status bar
  Margins computeMargins() const;
};
