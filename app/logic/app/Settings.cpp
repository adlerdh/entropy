#include "logic/app/Settings.h"

#include <glm/glm.hpp>

#include <algorithm>

void AppSettings::bumpBrushPreviewRevision()
{
  ++m_brushPreviewRevision;
}

void AppSettings::adjustActiveSegmentationLabels(const ParcellationLabelTable& activeLabelTable)
{
  const auto oldForegroundLabel = m_foregroundLabel;
  const auto oldBackgroundLabel = m_backgroundLabel;

  m_foregroundLabel = std::min(m_foregroundLabel, activeLabelTable.numLabels() - 1);
  m_backgroundLabel = std::min(m_backgroundLabel, activeLabelTable.numLabels() - 1);

  if (oldForegroundLabel != m_foregroundLabel || oldBackgroundLabel != m_backgroundLabel) {
    bumpBrushPreviewRevision();
  }
}

void AppSettings::swapForegroundAndBackgroundLabels(const ParcellationLabelTable& activeLabelTable)
{
  const auto fg = foregroundLabel();
  const auto bg = backgroundLabel();

  setForegroundLabel(bg, activeLabelTable);
  setBackgroundLabel(fg, activeLabelTable);
}

bool AppSettings::synchronizeZooms() const
{
  return m_synchronizeZoom;
}
void AppSettings::setSynchronizeZooms(bool sync)
{
  m_synchronizeZoom = sync;
}

bool AppSettings::cursorSyncEnabled() const
{
  return m_cursorSyncEnabled;
}
void AppSettings::setCursorSyncEnabled(bool set)
{
  m_cursorSyncEnabled = set;
}

bool AppSettings::sendCursorSync() const
{
  return m_sendCursorSync;
}
void AppSettings::setSendCursorSync(bool set)
{
  m_sendCursorSync = set;
}

bool AppSettings::receiveCursorSync() const
{
  return m_receiveCursorSync;
}
void AppSettings::setReceiveCursorSync(bool set)
{
  m_receiveCursorSync = set;
}

bool AppSettings::sendZoomSync() const
{
  return m_sendZoomSync;
}
void AppSettings::setSendZoomSync(bool set)
{
  m_sendZoomSync = set;
}

bool AppSettings::receiveZoomSync() const
{
  return m_receiveZoomSync;
}
void AppSettings::setReceiveZoomSync(bool set)
{
  m_receiveZoomSync = set;
}

bool AppSettings::sendPanSync() const
{
  return m_sendPanSync;
}
void AppSettings::setSendPanSync(bool set)
{
  m_sendPanSync = set;
}

bool AppSettings::receivePanSync() const
{
  return m_receivePanSync;
}
void AppSettings::setReceivePanSync(bool set)
{
  m_receivePanSync = set;
}

bool AppSettings::entropyInstanceSyncEnabled() const
{
  return m_entropyInstanceSyncEnabled;
}
void AppSettings::setEntropyInstanceSyncEnabled(bool set)
{
  m_entropyInstanceSyncEnabled = set;
}

bool AppSettings::overlays() const
{
  return m_overlays;
}
void AppSettings::setOverlays(bool set)
{
  m_overlays = set;
}

std::optional<float> AppSettings::uiScaleOverride() const
{
  return m_uiScaleOverride;
}

void AppSettings::setUiScaleOverride(std::optional<float> scale)
{
  if (scale) {
    scale = std::clamp(*scale, 0.5f, 4.0f);
  }

  m_uiScaleOverride = scale;
}

UiFontFamily AppSettings::uiFontFamily() const
{
  return m_uiFontFamily;
}

void AppSettings::setUiFontFamily(UiFontFamily family)
{
  m_uiFontFamily = family;
}

UiColorPreset AppSettings::uiColorPreset() const
{
  return m_uiColorPreset;
}

void AppSettings::setUiColorPreset(UiColorPreset preset)
{
  m_uiColorPreset = preset;
}

UiDensityPreset AppSettings::uiDensityPreset() const
{
  return m_uiDensityPreset;
}

void AppSettings::setUiDensityPreset(UiDensityPreset preset)
{
  m_uiDensityPreset = preset;
}

float AppSettings::uiWindowBgOpacity() const
{
  return m_uiWindowBgOpacity;
}

void AppSettings::setUiWindowBgOpacity(float opacity)
{
  m_uiWindowBgOpacity = std::clamp(opacity, 0.2f, 1.0f);
}

bool AppSettings::showLayoutTabs() const
{
  return m_showLayoutTabs;
}

void AppSettings::setShowLayoutTabs(bool show)
{
  m_showLayoutTabs = show;
}

UiLayoutTabPlacement AppSettings::layoutTabPlacement() const
{
  return m_layoutTabPlacement;
}

void AppSettings::setLayoutTabPlacement(UiLayoutTabPlacement placement)
{
  m_layoutTabPlacement = placement;
}

bool AppSettings::showGlobalTimeControls() const
{
  return m_showGlobalTimeControls;
}

void AppSettings::setShowGlobalTimeControls(bool show)
{
  m_showGlobalTimeControls = show;
}

bool AppSettings::synchronizeTimeSeries() const
{
  return m_synchronizeTimeSeries;
}

void AppSettings::setSynchronizeTimeSeries(bool synchronize)
{
  m_synchronizeTimeSeries = synchronize;
}

bool AppSettings::automaticUpdateChecksEnabled() const
{
  return m_automaticUpdateChecksEnabled;
}

void AppSettings::setAutomaticUpdateChecksEnabled(bool enabled)
{
  m_automaticUpdateChecksEnabled = enabled;
}

const registration::BackendConfig& AppSettings::registrationBackendConfig() const
{
  return m_registrationBackendConfig;
}

registration::BackendConfig& AppSettings::registrationBackendConfig()
{
  return m_registrationBackendConfig;
}

void AppSettings::setForegroundLabel(std::size_t label, const ParcellationLabelTable& activeLabelTable)
{
  const auto oldForegroundLabel = m_foregroundLabel;
  const auto oldBackgroundLabel = m_backgroundLabel;
  const auto oldBrushPreviewRevision = m_brushPreviewRevision;

  m_foregroundLabel = label;
  adjustActiveSegmentationLabels(activeLabelTable);

  if (
    oldBrushPreviewRevision == m_brushPreviewRevision &&
    (oldForegroundLabel != m_foregroundLabel || oldBackgroundLabel != m_backgroundLabel))
  {
    bumpBrushPreviewRevision();
  }
}

void AppSettings::setBackgroundLabel(std::size_t label, const ParcellationLabelTable& activeLabelTable)
{
  const auto oldForegroundLabel = m_foregroundLabel;
  const auto oldBackgroundLabel = m_backgroundLabel;
  const auto oldBrushPreviewRevision = m_brushPreviewRevision;

  m_backgroundLabel = label;
  adjustActiveSegmentationLabels(activeLabelTable);

  if (
    oldBrushPreviewRevision == m_brushPreviewRevision &&
    (oldForegroundLabel != m_foregroundLabel || oldBackgroundLabel != m_backgroundLabel))
  {
    bumpBrushPreviewRevision();
  }
}

std::size_t AppSettings::foregroundLabel() const
{
  return m_foregroundLabel;
}
std::size_t AppSettings::backgroundLabel() const
{
  return m_backgroundLabel;
}

bool AppSettings::replaceBackgroundWithForeground() const
{
  return m_replaceBackgroundWithForeground;
}
void AppSettings::setReplaceBackgroundWithForeground(bool set)
{
  if (m_replaceBackgroundWithForeground == set) {
    return;
  }

  m_replaceBackgroundWithForeground = set;
  bumpBrushPreviewRevision();
}

bool AppSettings::use3dBrush() const
{
  return m_use3dBrush;
}
void AppSettings::setUse3dBrush(bool set)
{
  if (m_use3dBrush == set) {
    return;
  }

  m_use3dBrush = set;
  bumpBrushPreviewRevision();
}

bool AppSettings::useIsotropicBrush() const
{
  return m_useIsotropicBrush;
}
void AppSettings::setUseIsotropicBrush(bool set)
{
  if (m_useIsotropicBrush == set) {
    return;
  }

  m_useIsotropicBrush = set;
  bumpBrushPreviewRevision();
}

bool AppSettings::useVoxelBrushSize() const
{
  return m_useVoxelBrushSize;
}
void AppSettings::setUseVoxelBrushSize(bool set)
{
  if (m_useVoxelBrushSize == set) {
    return;
  }

  m_useVoxelBrushSize = set;
  bumpBrushPreviewRevision();
}

bool AppSettings::useRoundBrush() const
{
  return m_useRoundBrush;
}
void AppSettings::setUseRoundBrush(bool set)
{
  if (m_useRoundBrush == set) {
    return;
  }

  m_useRoundBrush = set;
  bumpBrushPreviewRevision();
}

bool AppSettings::crosshairsMoveWithBrush() const
{
  return m_crosshairsMoveWithBrush;
}
void AppSettings::setCrosshairsMoveWithBrush(bool set)
{
  m_crosshairsMoveWithBrush = set;
}

BrushPreviewMode AppSettings::brushPreviewMode() const
{
  return m_brushPreviewMode;
}

void AppSettings::setBrushPreviewMode(BrushPreviewMode mode)
{
  if (m_brushPreviewMode == mode) {
    return;
  }

  m_brushPreviewMode = mode;
  bumpBrushPreviewRevision();
}

BrushPreviewVoxels AppSettings::brushPreviewVoxels() const
{
  return m_brushPreviewVoxels;
}

void AppSettings::setBrushPreviewVoxels(BrushPreviewVoxels voxels)
{
  if (m_brushPreviewVoxels == voxels) {
    return;
  }

  m_brushPreviewVoxels = voxels;
  bumpBrushPreviewRevision();
}

BrushPreviewStyle AppSettings::brushPreviewStyle() const
{
  return m_brushPreviewStyle;
}

void AppSettings::setBrushPreviewStyle(BrushPreviewStyle style)
{
  if (m_brushPreviewStyle == style) {
    return;
  }

  m_brushPreviewStyle = style;
  bumpBrushPreviewRevision();
}

float AppSettings::brushPreviewFillOpacity() const
{
  return m_brushPreviewFillOpacity;
}

void AppSettings::setBrushPreviewFillOpacity(float opacity)
{
  const float clampedOpacity = std::clamp(opacity, 0.0f, 1.0f);
  if (m_brushPreviewFillOpacity == clampedOpacity) {
    return;
  }

  m_brushPreviewFillOpacity = clampedOpacity;
  bumpBrushPreviewRevision();
}

bool AppSettings::brushPreviewWhilePainting() const
{
  return m_brushPreviewWhilePainting;
}

void AppSettings::setBrushPreviewWhilePainting(bool show)
{
  if (m_brushPreviewWhilePainting == show) {
    return;
  }

  m_brushPreviewWhilePainting = show;
  bumpBrushPreviewRevision();
}

SegmentationOutlineStyle AppSettings::brushPreviewOutlineStyle() const
{
  return m_brushPreviewOutlineStyle;
}

void AppSettings::setBrushPreviewOutlineStyle(SegmentationOutlineStyle style)
{
  if (m_brushPreviewOutlineStyle == style) {
    return;
  }

  m_brushPreviewOutlineStyle = style;
  bumpBrushPreviewRevision();
}

uint64_t AppSettings::brushPreviewRevision() const
{
  return m_brushPreviewRevision;
}

uint32_t AppSettings::brushSizeInVoxels() const
{
  return m_brushSizeInVoxels;
}

void AppSettings::setBrushSizeInVoxels(uint32_t size)
{
  static constexpr uint32_t sk_minBrushVox = 1;
  static constexpr uint32_t sk_maxBrushVox = 511;

  const uint32_t clampedSize = std::min(std::max(size, sk_minBrushVox), sk_maxBrushVox);
  if (m_brushSizeInVoxels == clampedSize) {
    return;
  }

  m_brushSizeInVoxels = clampedSize;
  bumpBrushPreviewRevision();
}

float AppSettings::brushSizeInMm() const
{
  return m_brushSizeInMm;
}
void AppSettings::setBrushSizeInMm(float size)
{
  if (m_brushSizeInMm == size) {
    return;
  }

  m_brushSizeInMm = size;
  bumpBrushPreviewRevision();
}

bool AppSettings::crosshairsMoveWhileAnnotating() const
{
  return m_crosshairsMoveWhileAnnotating;
}
void AppSettings::setCrosshairsMoveWhileAnnotating(bool set)
{
  m_crosshairsMoveWhileAnnotating = set;
}

bool AppSettings::lockAnatomicalCoordinateAxesWithReferenceImage() const
{
  return m_lockAnatomicalCoordinateAxesWithReferenceImage;
}
void AppSettings::setLockAnatomicalCoordinateAxesWithReferenceImage(bool lock)
{
  m_lockAnatomicalCoordinateAxesWithReferenceImage = lock;
}
