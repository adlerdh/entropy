#pragma once

#include "image/DicomSeries.h"
#include "image/ImageHeader.h"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
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
 * @brief Transient user-interface state shared by windows, popups, menus, and toolbars.
 */
struct GuiData
{
  /**
   * @brief Settings pages that can be requested programmatically.
   */
  enum class SettingsTab : std::uint8_t
  {
    Views,           //!< View, overlay, crosshair, lightbox, and scale bar settings page.
    Interface,       //!< Interface appearance and behavior settings page.
    Images,          //!< Global image display default settings page.
    Segmentation,    //!< Segmentation display and brush settings page.
    Comparison,      //!< Image comparison and metric settings page.
    Rendering,       //!< Rendering, projection, raycasting, and ASCII settings page.
    Annotations,     //!< Annotation and landmark display settings page.
    Synchronization, //!< ITK-SNAP synchronization settings page.
    System           //!< System, runtime, and diagnostics settings page.
  };

  enum class LayoutTabPlacement : std::uint8_t
  {
    Top,   //!< Place layout tabs above the rendered views.
    Bottom //!< Place layout tabs below the rendered views.
  };

  /// Global setting to turn on/off rendering of the UI windows.
  bool m_renderUiWindows = false;

  /// Global setting to turn on/off rendering of the UI overlays (crosshairs, anatomical labels).
  bool m_renderUiOverlays = false;

  bool m_showImagePropertiesWindow = true;                          //!< Show the image properties window.
  bool m_showSegmentationsWindow = false;                           //!< Show the segmentations window.
  bool m_showLandmarksWindow = false;                               //!< Show the landmarks window.
  bool m_showAnnotationsWindow = false;                             //!< Show the annotations window.
  bool m_showIsosurfacesWindow = false;                             //!< Show the isosurfaces window.
  bool m_showSettingsWindow = false;                                //!< Show the settings window.
  std::optional<SettingsTab> m_requestedSettingsTab = std::nullopt; //!< One-shot requested settings page.
  bool m_showInspectionWindow = true;                               //!< Show the cursor inspection window.
  bool m_showOpacityBlenderWindow = false;                          //!< Show the opacity mixer window.
  bool m_showImGuiDemoWindow = false;                               //!< Show the ImGui demo window.
  bool m_showImPlotDemoWindow = false;                              //!< Show the ImPlot demo window.
  bool m_showAboutDialog = false;                                   //!< Show the About Entropy dialog.
  bool m_showAddLayoutPopup = false;                                //!< Show the Add Layout popup.

  /**
   * @brief Deferred action selected when the unsaved-project confirmation popup resolves.
   */
  enum class UnsavedProjectAction : std::uint8_t
  {
    CloseProject,    //!< Close only the active project.
    OpenImages,      //!< Replace the current project with image files.
    OpenDicomSeries, //!< Replace the current project with DICOM series.
    OpenProject,     //!< Replace the current project with another project file.
    QuitApp          //!< Quit the application.
  };

  /// This is set to false until the user requests to close a dirty project or quit the app.
  bool m_showUnsavedProjectPopup = false;

  /// Action to perform if the user confirms the unsaved-project prompt.
  UnsavedProjectAction m_pendingUnsavedProjectAction = UnsavedProjectAction::CloseProject;

  /// This is set to false until the user requests to quit with no unsaved project changes.
  bool m_showConfirmCloseAppPopup = false;

  /// Flag to show dialog confirming reference image reassignment.
  bool m_showConfirmSetReferenceImagePopup = false;

  /// Image waiting to become the reference image after confirmation.
  std::optional<uuids::uuid> m_pendingReferenceImageUid = std::nullopt;

  /// Flag to show dialog confirming image removal.
  bool m_showConfirmRemoveImagePopup = false;

  /// Image waiting to be removed after confirmation.
  std::optional<uuids::uuid> m_pendingRemoveImageUid = std::nullopt;

  /**
   * @brief State for the large-image loading preflight popup.
   */
  struct LargeImageLoadPrompt
  {
    std::filesystem::path fileName;  //!< Image file being considered for loading.
    ImageHeader header;              //!< Parsed image header used to display size and type estimates.
    bool allowCancelProject = false; //!< True when the prompt can cancel an in-progress project load.
    bool allowSkipImage = true;      //!< True when the user can skip this image and continue loading.
  };

  /**
   * @brief User decisions available from the large-image loading preflight popup.
   */
  enum class LargeImageLoadDecision : std::uint8_t
  {
    LoadOriginal, //!< Load the image at its original size and type.
    SkipImage,    //!< Skip this image and continue the larger load operation.
    CancelProject //!< Cancel the current project load operation.
  };

  /// Show the large-image loading preflight popup.
  bool m_showLargeImageLoadPrompt = false;

  /// Skip the next large-image preflight check after the user has explicitly chosen to load the original.
  bool m_bypassNextImageLoadPreflight = false;

  /// Pending large-image loading prompt state.
  std::optional<LargeImageLoadPrompt> m_pendingLargeImageLoadPrompt = std::nullopt;

  /**
   * @brief State for selecting one or more discovered DICOM series to load.
   */
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

    /**
     * @brief Visibility state for the DICOM series table columns.
     *
     * The initializer is ordered to match kSeriesColumnNames in DicomSeriesSelectionPopup.cpp. Common identifiers and
     * geometry columns are visible by default, while the more verbose orientation and spacing diagnostics start hidden.
     */
    std::array<bool, 16> seriesColumnVisible{
      true,  //!< Load column.
      true,  //!< Reference column.
      true,  //!< Description column.
      true,  //!< Modality column.
      true,  //!< Series number column.
      true,  //!< Slice count column.
      true,  //!< Slice orientation column.
      true,  //!< Image dimensions column.
      true,  //!< Voxel spacing column.
      true,  //!< Field-of-view column.
      false, //!< Image origin column.
      false, //!< Direction matrix column.
      true,  //!< Series UID column.
      true,  //!< Study UID column.
      true,  //!< Metadata action column.
      true}; //!< Warning summary column.
  };

  bool m_showDicomFolderPathPopup = false; //!< Show the fallback DICOM folder path entry popup.
  std::string m_dicomFolderPathText;       //!< Editable folder path text for the fallback DICOM folder popup.

  /// True while the background DICOM discovery task is scanning folders/files.
  bool m_dicomSeriesScanInProgress = false;

  /// Root path displayed while a DICOM discovery task is pending.
  std::filesystem::path m_pendingDicomScanRoot;

  /// Opens the modal that lets the user choose which discovered DICOM series to load.
  bool m_showDicomSeriesSelectionPopup = false;

  /// Pending DICOM series selection dialog state, cleared when the dialog is accepted or canceled.
  std::optional<DicomSeriesSelectionPrompt> m_pendingDicomSeriesSelectionPrompt = std::nullopt;

  /// Map from image UID to whether its image color map popup is shown from the Image Properties window.
  std::unordered_map<uuids::uuid, bool> m_showImageColormapWindow;

  bool m_showDifferenceColormapWindow = false;     //!< Show the difference metric color map popup.
  bool m_showCorrelationColormapWindow = false;    //!< Show the correlation metric color map popup.
  bool m_showJointHistogramColormapWindow = false; //!< Show the joint histogram metric color map popup.

  /// Update m_coordsPrecisionFormat from m_coordsPrecision.
  void setCoordsPrecisionFormat();

  /// Update m_txPrecisionFormat from m_txPrecision.
  void setTxPrecisionFormat();

  /// printf-style precision format string used for spatial coordinates.
  std::string m_coordsPrecisionFormat = "%0.3f";

  /// Number of digits after the decimal point for spatial coordinates.
  uint32_t m_coordsPrecision = 3;

  /// printf-style precision format string used for image transformations.
  std::string m_txPrecisionFormat = "%0.3f";

  /// Number of digits after the decimal point for image transformations.
  uint32_t m_txPrecision = 3;

  /// printf-style precision format string used for image values.
  std::string m_imageValuePrecisionFormat = "%0.3f";

  /// Number of digits after the decimal point for image values.
  uint32_t m_imageValuePrecision = 3;

  /// printf-style precision format string used for percentiles.
  std::string m_percentilePrecisionFormat = "%0.2f";

  /// Number of digits after the decimal point for percentiles.
  uint32_t m_percentilePrecision = 2;

  /**
   * @brief Visibility state for the Voxel Inspector table columns.
   *
   * The initializer is ordered to match the inspector table column declarations in InspectionTableWindow.cpp.
   * Interpolated values and region names start hidden, matching the previous table defaults.
   */
  std::array<bool, 8> m_inspectionColumnVisible{
    true,  //!< Image column.
    true,  //!< Nearest-neighbor image value column.
    false, //!< Linearly interpolated image value column.
    false, //!< Nearest-neighbor active component percentile column.
    true,  //!< Active segmentation label column.
    false, //!< Active segmentation region name column.
    true,  //!< Voxel index column.
    true}; //!< Physical subject coordinate column.

  /// ImGui font pointers keyed by UI font name. ImGui owns the pointed-to font data.
  std::unordered_map<std::string, ImFont*> m_fonts;

  /**
   * @brief Show the ImGui main menu bar.
   *
   * Native-menu platforms generally keep this false after the native menu is installed.
   */
  bool m_showMainMenuBar = false;

  /**
   * @brief Last measured dimensions of the ImGui main menu bar, in display coordinates.
   *
   * These dimensions are included in the computed UI margins when the ImGui menu bar is visible.
   */
  glm::vec2 m_mainMenuBarDims{0.0f, 0.0f};

  /**
   * @brief Show the interaction mode toolbar.
   */
  bool m_showModeToolbar = true;

  /**
   * @brief True when the interaction mode toolbar is docked horizontally.
   */
  bool m_isModeToolbarHorizontal = false;

  /**
   * @brief Dock corner for the interaction mode toolbar.
   *
   * Values are -1 for custom placement, 0 for top-left, 1 for top-right, 2 for bottom-left, and 3 for bottom-right.
   */
  int m_modeToolbarCorner = 1;

  /**
   * @brief Last measured docked dimensions of the interaction mode toolbar.
   *
   * These dimensions are used to reserve space around the image views when the toolbar is docked.
   */
  glm::vec2 m_modeToolbarDockDims{0.0f, 0.0f};

  /**
   * @brief Show the segmentation toolbar.
   */
  bool m_showSegToolbar = false;

  /**
   * @brief True when the segmentation toolbar is docked horizontally.
   */
  bool m_isSegToolbarHorizontal = false;

  /**
   * @brief Dock corner for the segmentation toolbar.
   *
   * Values are -1 for custom placement, 0 for top-left, 1 for top-right, 2 for bottom-left, and 3 for bottom-right.
   */
  int m_segToolbarCorner = 0;

  /**
   * @brief Last measured docked dimensions of the segmentation toolbar.
   *
   * These dimensions are used to reserve space around the image views when the toolbar is docked.
   */
  glm::vec2 m_segToolbarDockDims{0.0f, 0.0f};

  bool m_showLayoutTabs = true;                                      //!< Show the layout tab strip.
  LayoutTabPlacement m_layoutTabPlacement = LayoutTabPlacement::Top; //!< Layout tab strip edge.
  float m_layoutTabBarHeight = 0.0f;                                 //!< Current visible layout tab strip height.
  float m_layoutTabInnerGap = 0.0f;                                  //!< Gap between layout tabs and the dockspace.
  std::optional<glm::vec4> m_renderViewport = std::nullopt;          //!< Image rendering bounds in window coordinates.
  bool m_showConfirmRemoveLayoutPopup = false;                       //!< Confirm deletion of a layout tab.
  std::optional<std::size_t> m_pendingRemoveLayoutIndex = std::nullopt; //!< Layout waiting for deletion.

  /**
   * @brief Reserved screen margins occupied by visible UI chrome.
   *
   * The values are in display coordinates and are subtracted from the drawable image-view area.
   */
  struct Margins
  {
    float left = 0.0f;   //!< Reserved margin on the left edge of the application window.
    float right = 0.0f;  //!< Reserved margin on the right edge of the application window.
    float bottom = 0.0f; //!< Reserved margin on the bottom edge of the application window.
    float top = 0.0f;    //!< Reserved margin on the top edge of the application window.
  };

  /** @brief Compute UI margins based on visible menu, toolbars, and layout tabs. */
  Margins computeMargins() const;

  /** @brief Compute only margins reserved by visible toolbars. */
  Margins computeToolbarMargins() const;

  /** @brief Offset docked UI from top chrome such as the menu bar and layout tabs. */
  float topDockOffset() const;

  /** @brief Offset docked UI from bottom chrome such as bottom layout tabs. */
  float bottomDockOffset() const;
};
