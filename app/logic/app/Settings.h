#pragma once

#include "logic/app/ParcellationLabelTable.h"
#include "common/SegmentationTypes.h"
#include "common/Types.h"
#include "registration/Config.h"

#include <glm/vec3.hpp>

#include <cstdint>
#include <optional>

/**
 * @brief Font families that can be used for the main ImGui interface.
 *
 * Some families are intentionally hidden from the Settings UI while they are
 * being evaluated, but they remain available to the implementation.
 */
enum class UiFontFamily : std::uint8_t
{
  SpaceGrotesk,
  Inter,
  NotoSans,
  Roboto,
  SourceSans3,
  IBMPlexSans,
  Cousine
};

/**
 *  Color presets for the main ImGui interface.
 */
enum class UiColorPreset : std::uint8_t
{
  EntropyDark,
  ImGuiDark,
  ImGuiClassic,
  ImGuiLight,
  SlateBlue,
  Graphite,
  DeepTeal,
  Midnight,
  SoftLight
};

/**
 *  Density and shape presets for the main ImGui interface.
 */
enum class UiDensityPreset : std::uint8_t
{
  Compact,
  Default,
  Comfortable
};

/**
 * @brief Placement options for the layout tab strip.
 */
enum class UiLayoutTabPlacement : std::uint8_t
{
  Top,
  Bottom
};

/**
 * @brief Holds all application settings
 */
class AppSettings
{
public:
  AppSettings() = default;
  ~AppSettings() = default;

  bool synchronizeZooms() const;
  void setSynchronizeZooms(bool);

  bool cursorSyncEnabled() const;
  void setCursorSyncEnabled(bool);

  bool sendCursorSync() const;
  void setSendCursorSync(bool);

  bool receiveCursorSync() const;
  void setReceiveCursorSync(bool);

  bool sendZoomSync() const;
  void setSendZoomSync(bool);

  bool receiveZoomSync() const;
  void setReceiveZoomSync(bool);

  bool sendPanSync() const;
  void setSendPanSync(bool);

  bool receivePanSync() const;
  void setReceivePanSync(bool);

  bool entropyInstanceSyncEnabled() const;
  void setEntropyInstanceSyncEnabled(bool);

  bool overlays() const;
  void setOverlays(bool);

  /**
   * @brief Return the user-selected ImGui UI scale override.
   *
   * A value of \c std::nullopt means platform auto scaling is active.
   */
  std::optional<float> uiScaleOverride() const;

  /**
   * @brief Set the user-selected ImGui UI scale override.
   *
   * A value of \c std::nullopt means that Entropy should use the platform auto
   * scale. Explicit values are clamped to the supported UI scaling range.
   */
  void setUiScaleOverride(std::optional<float> scale);

  /**
   * @brief Return the font family used for the main ImGui interface.
   */
  UiFontFamily uiFontFamily() const;

  /**
   * @brief Set the font family used for the main ImGui interface.
   */
  void setUiFontFamily(UiFontFamily family);

  /**
   *  Return the color preset used for the main ImGui interface.
   */
  UiColorPreset uiColorPreset() const;

  /**
   *  Set the color preset used for the main ImGui interface.
   */
  void setUiColorPreset(UiColorPreset preset);

  /**
   *  Return the density and shape preset used for the main ImGui interface.
   */
  UiDensityPreset uiDensityPreset() const;

  /**
   *  Set the density and shape preset used for the main ImGui interface.
   */
  void setUiDensityPreset(UiDensityPreset preset);

  /**
   *  Return the opacity of regular ImGui window backgrounds.
   */
  float uiWindowBgOpacity() const;

  /**
   *  Set the opacity of regular ImGui window backgrounds.
   */
  void setUiWindowBgOpacity(float opacity);

  /**
   * @brief Return whether the layout tab strip is shown.
   */
  bool showLayoutTabs() const;

  /**
   * @brief Set whether the layout tab strip is shown.
   */
  void setShowLayoutTabs(bool show);

  /**
   * @brief Return where the layout tab strip is placed.
   */
  UiLayoutTabPlacement layoutTabPlacement() const;

  /**
   * @brief Set where the layout tab strip is placed.
   */
  void setLayoutTabPlacement(UiLayoutTabPlacement placement);

  /// @brief Return whether the global time-series control window is shown.
  bool showGlobalTimeControls() const;

  /// @brief Set whether the global time-series control window is shown.
  void setShowGlobalTimeControls(bool show);

  /// @brief Return whether time-series images change frames together.
  bool synchronizeTimeSeries() const;

  /// @brief Set whether time-series images change frames together.
  void setSynchronizeTimeSeries(bool synchronize);

  /// @brief Return whether Entropy should check GitHub for updates automatically.
  bool automaticUpdateChecksEnabled() const;

  /// @brief Set whether Entropy should check GitHub for updates automatically.
  void setAutomaticUpdateChecksEnabled(bool enabled);

  std::size_t foregroundLabel() const;
  std::size_t backgroundLabel() const;

  void setForegroundLabel(std::size_t label, const ParcellationLabelTable& activeLabelTable);
  void setBackgroundLabel(std::size_t label, const ParcellationLabelTable& activeLabelTable);

  void adjustActiveSegmentationLabels(const ParcellationLabelTable& activeLabelTable);
  void swapForegroundAndBackgroundLabels(const ParcellationLabelTable& activeLabelTable);

  bool replaceBackgroundWithForeground() const;
  void setReplaceBackgroundWithForeground(bool set);

  bool use3dBrush() const;
  void setUse3dBrush(bool set);

  bool useIsotropicBrush() const;
  void setUseIsotropicBrush(bool set);

  bool useVoxelBrushSize() const;
  void setUseVoxelBrushSize(bool set);

  bool useRoundBrush() const;
  void setUseRoundBrush(bool set);

  bool crosshairsMoveWithBrush() const;
  void setCrosshairsMoveWithBrush(bool set);

  BrushPreviewMode brushPreviewMode() const;
  void setBrushPreviewMode(BrushPreviewMode mode);

  BrushPreviewVoxels brushPreviewVoxels() const;
  void setBrushPreviewVoxels(BrushPreviewVoxels voxels);

  BrushPreviewStyle brushPreviewStyle() const;
  void setBrushPreviewStyle(BrushPreviewStyle style);

  float brushPreviewFillOpacity() const;
  void setBrushPreviewFillOpacity(float opacity);

  bool brushPreviewWhilePainting() const;
  void setBrushPreviewWhilePainting(bool show);

  SegmentationOutlineStyle brushPreviewOutlineStyle() const;
  void setBrushPreviewOutlineStyle(SegmentationOutlineStyle style);

  uint64_t brushPreviewRevision() const;

  uint32_t brushSizeInVoxels() const;
  void setBrushSizeInVoxels(uint32_t size);

  float brushSizeInMm() const;
  void setBrushSizeInMm(float size);

  bool crosshairsMoveWhileAnnotating() const;
  void setCrosshairsMoveWhileAnnotating(bool set);

  bool lockAnatomicalCoordinateAxesWithReferenceImage() const;
  void setLockAnatomicalCoordinateAxesWithReferenceImage(bool lock);

  /**
   * @brief Return immutable registration backend settings.
   */
  const registration::BackendConfig& registrationBackendConfig() const;

  /**
   * @brief Return mutable registration backend settings.
   */
  registration::BackendConfig& registrationBackendConfig();

private:
  void bumpBrushPreviewRevision();

  bool m_synchronizeZoom = true; //!< Synchronize zoom between views
  bool m_overlays = true;        //!< Render UI and vector overlays
  std::optional<float> m_uiScaleOverride = std::nullopt;
  UiFontFamily m_uiFontFamily = UiFontFamily::Inter;
  UiColorPreset m_uiColorPreset = UiColorPreset::EntropyDark;
  UiDensityPreset m_uiDensityPreset = UiDensityPreset::Default;
  float m_uiWindowBgOpacity = 0.95f;
  bool m_showLayoutTabs = true;
  UiLayoutTabPlacement m_layoutTabPlacement = UiLayoutTabPlacement::Top;
  bool m_showGlobalTimeControls = true;
  bool m_synchronizeTimeSeries = true;
  bool m_automaticUpdateChecksEnabled = false;
  registration::BackendConfig m_registrationBackendConfig;

  bool m_cursorSyncEnabled = false;
  bool m_sendCursorSync = true;
  bool m_receiveCursorSync = true;
  bool m_sendZoomSync = true;
  bool m_receiveZoomSync = true;
  bool m_sendPanSync = true;
  bool m_receivePanSync = true;
  bool m_entropyInstanceSyncEnabled = false;

  /// Crosshairs move to the position of every new point added to an annotation
  bool m_crosshairsMoveWhileAnnotating = false;

  /// When the reference image rotates, do the anatomical coordinate axes (LPS, RAI)
  /// and crosshairs rotate, too? When this option is true, the rotation of the
  /// coordinate axes are locked with the reference image.
  bool m_lockAnatomicalCoordinateAxesWithReferenceImage = false;

  /* Begin segmentation drawing variables */
  std::size_t m_foregroundLabel = 1u; //!< Foreground segmentation label
  std::size_t m_backgroundLabel = 0u; //!< Background segmentation label

  bool m_replaceBackgroundWithForeground = false; /// Paint foreground label only over background label
  bool m_use3dBrush = false;                      //!< Paint with a 3D brush
  bool m_useIsotropicBrush = true;                //!< Paint with an isotropic brush
  bool m_useVoxelBrushSize = true;                //!< Measure brush size in voxel units
  bool m_useRoundBrush = true;                    //!< Brush is round (true) or rectangular (false)
  bool m_crosshairsMoveWithBrush = false;         //!< Crosshairs move with the brush
  BrushPreviewMode m_brushPreviewMode = BrushPreviewMode::Hover;
  BrushPreviewVoxels m_brushPreviewVoxels = BrushPreviewVoxels::Changed;
  BrushPreviewStyle m_brushPreviewStyle = BrushPreviewStyle::OutlineAndFill;
  float m_brushPreviewFillOpacity = 0.2f;
  bool m_brushPreviewWhilePainting = true;
  SegmentationOutlineStyle m_brushPreviewOutlineStyle = SegmentationOutlineStyle::ViewPixel;
  uint64_t m_brushPreviewRevision = 0;
  uint32_t m_brushSizeInVoxels = 1u; //!< Brush size (diameter) in voxels
  float m_brushSizeInMm = 1.0f;      //!< Brush size (diameter) in millimeters
  /* End segmentation drawing variables */
};
