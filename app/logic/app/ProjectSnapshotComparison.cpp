#include "logic/app/ProjectSnapshotComparison.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace
{
bool imageSettingsEqual(
  const std::optional<serialize::ImageSettings>& a,
  const std::optional<serialize::ImageSettings>& b)
{
  if (a.has_value() != b.has_value()) {
    return false;
  }
  if (!a) {
    return true;
  }

  return a->m_displayName == b->m_displayName && a->m_globalVisibility == b->m_globalVisibility &&
         a->m_globalOpacity == b->m_globalOpacity && a->m_borderColor == b->m_borderColor && a->m_level == b->m_level &&
         a->m_lockedToReference == b->m_lockedToReference && a->m_warpEnabled == b->m_warpEnabled &&
         a->m_warpStrength == b->m_warpStrength && a->m_window == b->m_window &&
         a->m_thresholdLow == b->m_thresholdLow && a->m_thresholdHigh == b->m_thresholdHigh &&
         a->m_opacity == b->m_opacity && a->m_activeComponent == b->m_activeComponent &&
         a->m_activeTimePoint == b->m_activeTimePoint && a->m_timePlaybackLoop == b->m_timePlaybackLoop &&
         a->m_timePlaybackPlaying == b->m_timePlaybackPlaying && a->m_timePlaybackSpeed == b->m_timePlaybackSpeed &&
         a->m_componentRenderMode == b->m_componentRenderMode && a->m_complexPhaseUnit == b->m_complexPhaseUnit &&
         a->m_complexPhaseRange == b->m_complexPhaseRange &&
         a->m_vectorArrowOverlayVisible == b->m_vectorArrowOverlayVisible &&
         a->m_vectorArrowOverlayOnImage == b->m_vectorArrowOverlayOnImage &&
         a->m_vectorArrowOverlayDensity == b->m_vectorArrowOverlayDensity &&
         a->m_vectorArrowOverlayVoxelSpacing == b->m_vectorArrowOverlayVoxelSpacing &&
         a->m_vectorArrowOverlayMillimeterSpacing == b->m_vectorArrowOverlayMillimeterSpacing &&
         a->m_vectorArrowOverlaySpacingMode == b->m_vectorArrowOverlaySpacingMode &&
         a->m_vectorArrowOverlayColor == b->m_vectorArrowOverlayColor &&
         a->m_vectorArrowOverlayUseDirectionColor == b->m_vectorArrowOverlayUseDirectionColor &&
         a->m_vectorArrowOverlayLineThickness == b->m_vectorArrowOverlayLineThickness &&
         a->m_vectorArrowOverlayOpacity == b->m_vectorArrowOverlayOpacity &&
         a->m_vectorArrowOverlayScaleByMagnitude == b->m_vectorArrowOverlayScaleByMagnitude &&
         a->m_vectorArrowOverlayScaleFactor == b->m_vectorArrowOverlayScaleFactor &&
         a->m_vectorWarpedGridVisible == b->m_vectorWarpedGridVisible &&
         a->m_vectorWarpedGridOverlayOnImage == b->m_vectorWarpedGridOverlayOnImage &&
         a->m_vectorWarpedGridConvention == b->m_vectorWarpedGridConvention &&
         a->m_vectorWarpedGridPixelSpacing == b->m_vectorWarpedGridPixelSpacing &&
         a->m_vectorWarpedGridVoxelSpacing == b->m_vectorWarpedGridVoxelSpacing &&
         a->m_vectorWarpedGridMillimeterSpacing == b->m_vectorWarpedGridMillimeterSpacing &&
         a->m_vectorWarpedGridSpacingMode == b->m_vectorWarpedGridSpacingMode &&
         a->m_vectorWarpedGridLineThickness == b->m_vectorWarpedGridLineThickness &&
         a->m_vectorWarpedGridScaleFactor == b->m_vectorWarpedGridScaleFactor &&
         a->m_vectorWarpedGridForegroundColor == b->m_vectorWarpedGridForegroundColor &&
         a->m_vectorWarpedGridBackgroundColor == b->m_vectorWarpedGridBackgroundColor &&
         a->m_vectorPlanarProjectionSignedColors == b->m_vectorPlanarProjectionSignedColors &&
         a->m_vectorLogJacobianDeterminant == b->m_vectorLogJacobianDeterminant &&
         a->m_ignoreAlpha == b->m_ignoreAlpha && a->m_colorInterpolationMode == b->m_colorInterpolationMode &&
         a->m_componentLevels == b->m_componentLevels && a->m_componentWindows == b->m_componentWindows &&
         a->m_componentThresholdLows == b->m_componentThresholdLows &&
         a->m_componentThresholdHighs == b->m_componentThresholdHighs &&
         a->m_componentVisibility == b->m_componentVisibility && a->m_componentOpacities == b->m_componentOpacities &&
         a->m_colorMapIndices == b->m_colorMapIndices && a->m_colorMapInverted == b->m_colorMapInverted &&
         a->m_colorMapContinuous == b->m_colorMapContinuous && a->m_colorMapLevels == b->m_colorMapLevels &&
         a->m_colorMapHsvModifiers == b->m_colorMapHsvModifiers && a->m_interpolationModes == b->m_interpolationModes &&
         a->m_foregroundThresholdLows == b->m_foregroundThresholdLows &&
         a->m_foregroundThresholdHighs == b->m_foregroundThresholdHighs &&
         a->m_edgeDetectionMethod == b->m_edgeDetectionMethod && a->m_showEdges == b->m_showEdges &&
         a->m_thresholdEdges == b->m_thresholdEdges && a->m_thinPixelEdges == b->m_thinPixelEdges &&
         a->m_overlayEdges == b->m_overlayEdges && a->m_colormapEdges == b->m_colormapEdges &&
         a->m_edgeMagnitude == b->m_edgeMagnitude && a->m_pixelEdgeScale == b->m_pixelEdgeScale &&
         a->m_pixelEdgeThreshold == b->m_pixelEdgeThreshold && a->m_edgeColor == b->m_edgeColor &&
         a->m_edgeOpacity == b->m_edgeOpacity && a->m_useDistanceMapForRaycasting == b->m_useDistanceMapForRaycasting &&
         a->m_isosurfacesVisible == b->m_isosurfacesVisible &&
         a->m_applyImageColormapToIsosurfaces == b->m_applyImageColormapToIsosurfaces &&
         a->m_showIsocontoursIn2D == b->m_showIsocontoursIn2D &&
         a->m_isocontourLineWidthIn2D == b->m_isocontourLineWidthIn2D &&
         a->m_isosurfaceOpacityModulator == b->m_isosurfaceOpacityModulator;
}

bool segSettingsEqual(const std::optional<serialize::SegSettings>& a, const std::optional<serialize::SegSettings>& b)
{
  if (a.has_value() != b.has_value()) {
    return false;
  }
  return !a ||
         (a->m_displayName == b->m_displayName && a->m_visibility == b->m_visibility && a->m_opacity == b->m_opacity &&
          a->m_activeComponent == b->m_activeComponent && a->m_componentVisibility == b->m_componentVisibility &&
          a->m_componentOpacities == b->m_componentOpacities && a->m_labelTableIndices == b->m_labelTableIndices &&
          a->m_interpolationModes == b->m_interpolationModes);
}

bool matricesEqual(const std::optional<glm::mat4>& a, const std::optional<glm::mat4>& b)
{
  if (a.has_value() != b.has_value()) {
    return false;
  }
  if (!a) {
    return true;
  }

  for (glm::length_t c = 0; c < 4; ++c) {
    for (glm::length_t r = 0; r < 4; ++r) {
      if ((*a)[c][r] != (*b)[c][r]) {
        return false;
      }
    }
  }
  return true;
}

bool segmentationsEqual(const serialize::Segmentation& a, const serialize::Segmentation& b)
{
  return a.m_segFileName == b.m_segFileName && segSettingsEqual(a.m_settings, b.m_settings);
}

bool landmarkGroupsEqual(const serialize::LandmarkGroup& a, const serialize::LandmarkGroup& b)
{
  return a.m_csvFileName == b.m_csvFileName && a.m_inVoxelSpace == b.m_inVoxelSpace;
}

template<typename T, typename Equal>
bool vectorsEqual(const std::vector<T>& a, const std::vector<T>& b, Equal equal)
{
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), equal);
}

bool imageSelectionsEqual(const layout::ImageSelectionSpec& a, const layout::ImageSelectionSpec& b)
{
  return a.m_renderedImageIndices == b.m_renderedImageIndices && a.m_metricImageIndices == b.m_metricImageIndices;
}

bool viewLayoutsEqual(const layout::ViewSpec& a, const layout::ViewSpec& b)
{
  return a.m_left == b.m_left && a.m_bottom == b.m_bottom && a.m_width == b.m_width && a.m_height == b.m_height &&
         a.m_viewType == b.m_viewType && a.m_renderMode == b.m_renderMode &&
         a.m_intensityProjectionMode == b.m_intensityProjectionMode && a.m_offsetMode == b.m_offsetMode &&
         a.m_absoluteOffset == b.m_absoluteOffset && a.m_relativeOffsetSteps == b.m_relativeOffsetSteps &&
         a.m_offsetImageIndex == b.m_offsetImageIndex && a.m_rotationSyncGroup == b.m_rotationSyncGroup &&
         a.m_translationSyncGroup == b.m_translationSyncGroup && a.m_zoomSyncGroup == b.m_zoomSyncGroup &&
         a.m_preferredDefaultRenderedImages == b.m_preferredDefaultRenderedImages &&
         a.m_defaultRenderAllImages == b.m_defaultRenderAllImages &&
         imageSelectionsEqual(a.m_imageSelection, b.m_imageSelection);
}

bool projectLayoutsEqual(const layout::LayoutSpec& a, const layout::LayoutSpec& b)
{
  return a.m_isLightbox == b.m_isLightbox && a.m_viewType == b.m_viewType && a.m_renderMode == b.m_renderMode &&
         a.m_intensityProjectionMode == b.m_intensityProjectionMode &&
         a.m_preferredDefaultRenderedImages == b.m_preferredDefaultRenderedImages &&
         a.m_defaultRenderAllImages == b.m_defaultRenderAllImages &&
         imageSelectionsEqual(a.m_imageSelection, b.m_imageSelection) &&
         vectorsEqual(a.m_views, b.m_views, viewLayoutsEqual);
}

bool dicomSourcesEqual(const std::optional<serialize::DicomSource>& a, const std::optional<serialize::DicomSource>& b)
{
  if (a.has_value() != b.has_value()) {
    return false;
  }
  if (!a) {
    return true;
  }
  return a->m_rootPath == b->m_rootPath && a->m_studyInstanceUid == b->m_studyInstanceUid &&
         a->m_seriesInstanceUid == b->m_seriesInstanceUid && a->m_files == b->m_files;
}

bool imagesEqual(const serialize::Image& a, const serialize::Image& b)
{
  return a.m_imageFileName == b.m_imageFileName && dicomSourcesEqual(a.m_dicomSource, b.m_dicomSource) &&
         a.m_affineTxFileName == b.m_affineTxFileName && a.m_inverseWarpFileName == b.m_inverseWarpFileName &&
         a.m_forwardWarpFileName == b.m_forwardWarpFileName && matricesEqual(a.m_worldDefTx, b.m_worldDefTx) &&
         a.m_annotationsFileName == b.m_annotationsFileName && imageSettingsEqual(a.m_settings, b.m_settings) &&
         vectorsEqual(a.m_segmentations, b.m_segmentations, segmentationsEqual) &&
         vectorsEqual(a.m_landmarkGroups, b.m_landmarkGroups, landmarkGroupsEqual);
}

bool projectInterfaceSettingsEqual(
  const serialize::ProjectInterfaceSettings& a,
  const serialize::ProjectInterfaceSettings& b)
{
  return a.m_showLayoutTabs == b.m_showLayoutTabs && a.m_layoutTabPlacement == b.m_layoutTabPlacement &&
         a.m_showGlobalTimeControls == b.m_showGlobalTimeControls &&
         a.m_synchronizeTimeSeries == b.m_synchronizeTimeSeries && a.m_imageValuePrecision == b.m_imageValuePrecision &&
         a.m_coordsPrecision == b.m_coordsPrecision && a.m_txPrecision == b.m_txPrecision &&
         a.m_percentilePrecision == b.m_percentilePrecision;
}
} // namespace

namespace project_snapshot
{
bool equivalent(const serialize::EntropyProject& a, const serialize::EntropyProject& b)
{
  return imagesEqual(a.m_referenceImage, b.m_referenceImage) && a.m_layoutsFileName == b.m_layoutsFileName &&
         vectorsEqual(a.m_additionalImages, b.m_additionalImages, imagesEqual) &&
         vectorsEqual(a.m_layouts, b.m_layouts, projectLayoutsEqual) &&
         a.m_currentLayoutIndex == b.m_currentLayoutIndex &&
         projectInterfaceSettingsEqual(a.m_interface, b.m_interface);
}
} // namespace project_snapshot
