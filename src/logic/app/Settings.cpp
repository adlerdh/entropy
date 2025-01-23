#include "logic/app/Settings.h"

#include <glm/glm.hpp>

#include <algorithm>

AppSettings::AppSettings()
  : m_synchronizeZoom(true)
  , m_overlays(true)
  ,

  m_foregroundLabel(1u)
  , m_backgroundLabel(0u)
  , m_replaceBackgroundWithForeground(false)
  , m_use3dBrush(false)
  , m_useIsotropicBrush(true)
  , m_useVoxelBrushSize(true)
  , m_useRoundBrush(true)
  , m_crosshairsMoveWithBrush(false)
  , m_brushSizeInVoxels(1)
  , m_brushSizeInMm(1.0f)
  ,

  m_graphCutsWeightsAmplitude(1.0)
  , m_graphCutsWeightsSigma(0.01)
  , m_graphCutsNeighborhood(GraphNeighborhoodType::Neighbors6)
  ,

  m_crosshairsMoveWhileAnnotating(false)
  , m_lockAnatomicalCoordinateAxesWithReferenceImage(false)
{
}

void AppSettings::adjustActiveSegmentationLabels(const ParcellationLabelTable& activeLabelTable)
{
  m_foregroundLabel = std::min(m_foregroundLabel, activeLabelTable.numLabels() - 1);
  m_backgroundLabel = std::min(m_backgroundLabel, activeLabelTable.numLabels() - 1);
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

bool AppSettings::overlays() const
{
  return m_overlays;
}
void AppSettings::setOverlays(bool set)
{
  m_overlays = set;
}

void AppSettings::setForegroundLabel(size_t label, const ParcellationLabelTable& activeLabelTable)
{
  m_foregroundLabel = label;
  adjustActiveSegmentationLabels(activeLabelTable);
}

void AppSettings::setBackgroundLabel(size_t label, const ParcellationLabelTable& activeLabelTable)
{
  m_backgroundLabel = label;
  adjustActiveSegmentationLabels(activeLabelTable);
}

size_t AppSettings::foregroundLabel() const
{
  return m_foregroundLabel;
}
size_t AppSettings::backgroundLabel() const
{
  return m_backgroundLabel;
}

bool AppSettings::replaceBackgroundWithForeground() const
{
  return m_replaceBackgroundWithForeground;
}
void AppSettings::setReplaceBackgroundWithForeground(bool set)
{
  m_replaceBackgroundWithForeground = set;
}

bool AppSettings::use3dBrush() const
{
  return m_use3dBrush;
}
void AppSettings::setUse3dBrush(bool set)
{
  m_use3dBrush = set;
}

bool AppSettings::useIsotropicBrush() const
{
  return m_useIsotropicBrush;
}
void AppSettings::setUseIsotropicBrush(bool set)
{
  m_useIsotropicBrush = set;
}

bool AppSettings::useVoxelBrushSize() const
{
  return m_useVoxelBrushSize;
}
void AppSettings::setUseVoxelBrushSize(bool set)
{
  m_useVoxelBrushSize = set;
}

bool AppSettings::useRoundBrush() const
{
  return m_useRoundBrush;
}
void AppSettings::setUseRoundBrush(bool set)
{
  m_useRoundBrush = set;
}

bool AppSettings::crosshairsMoveWithBrush() const
{
  return m_crosshairsMoveWithBrush;
}
void AppSettings::setCrosshairsMoveWithBrush(bool set)
{
  m_crosshairsMoveWithBrush = set;
}

uint32_t AppSettings::brushSizeInVoxels() const
{
  return m_brushSizeInVoxels;
}

void AppSettings::setBrushSizeInVoxels(uint32_t size)
{
  static constexpr uint32_t sk_minBrushVox = 1;
  static constexpr uint32_t sk_maxBrushVox = 511;

  m_brushSizeInVoxels = std::min(std::max(size, sk_minBrushVox), sk_maxBrushVox);
}

float AppSettings::brushSizeInMm() const
{
  return m_brushSizeInMm;
}
void AppSettings::setBrushSizeInMm(float size)
{
  m_brushSizeInMm = size;
}

double AppSettings::graphCutsWeightsAmplitude() const
{
  return m_graphCutsWeightsAmplitude;
}
void AppSettings::setGraphCutsWeightsAmplitude(double amplitude)
{
  m_graphCutsWeightsAmplitude = amplitude;
}

double AppSettings::graphCutsWeightsSigma() const
{
  return m_graphCutsWeightsSigma;
}
void AppSettings::setGraphCutsWeightsSigma(double sigma)
{
  m_graphCutsWeightsSigma = sigma;
}

GraphNeighborhoodType AppSettings::graphCutsNeighborhood() const
{
  return m_graphCutsNeighborhood;
}
void AppSettings::setGraphCutsNeighborhood(const GraphNeighborhoodType& hood)
{
  m_graphCutsNeighborhood = hood;
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
