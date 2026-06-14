#include "logic/app/UserPreferences.h"

#include "common/LoggingSettings.h"
#include "rendering/RenderData.h"

#include <filesystem>
#include <system_error>

namespace
{

user_preferences::RenderPreferences renderPreferencesFromRenderData(const RenderData& renderData)
{
  user_preferences::RenderPreferences preferences;
  preferences.showImageBorders = renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections;
  preferences.crosshairsSnapping = renderData.m_snapCrosshairs;
  preferences.crosshairsColor = renderData.m_crosshairsColor;
  preferences.background2dColor = renderData.m_2dBackgroundColor;
  preferences.background3dColor = renderData.m_3dBackgroundColor;
  preferences.anatomicalLabelColor = renderData.m_anatomicalLabelColor;
  preferences.anatomicalLabelType = renderData.m_anatomicalLabelType;
  preferences.showScaleBars = renderData.m_showScaleBars;
  preferences.showScaleBarsInLightboxViews = renderData.m_showScaleBarsInLightboxViews;
  preferences.scaleBarColor = renderData.m_scaleBarColor;
  preferences.scaleBarPosition = renderData.m_scaleBarPosition;
  preferences.scaleBarOrientation = renderData.m_scaleBarOrientation;
  preferences.scaleBarTicks = renderData.m_scaleBarTicks;
  preferences.scaleBarTargetFraction = renderData.m_scaleBarTargetFraction;
  preferences.scaleBarMarginPx = renderData.m_scaleBarMarginPx;
  preferences.showLightboxOffsetLabels = renderData.m_showLightboxOffsetLabels;
  preferences.lightboxOffsetLabelColor = renderData.m_lightboxOffsetLabelColor;
  preferences.floatingPointLinearInterpolation = renderData.m_imageGrayFloatingPointInterpolation;
  preferences.useMaximumIntensityProjectionExtent = renderData.m_doMaxExtentIntensityProjection;
  preferences.intensityProjectionSlabThicknessMm = renderData.m_intensityProjectionSlabThickness;
  preferences.xrayEnergyKeV = renderData.m_xrayEnergyKeV;
  preferences.xrayWindow = renderData.m_xrayIntensityWindow;
  preferences.xrayLevel = renderData.m_xrayIntensityLevel;
  preferences.modulateSegmentationOpacityWithImageOpacity = renderData.m_modulateSegOpacityWithImageOpacity;
  preferences.segmentationOutlineStyle = renderData.m_segOutlineStyle;
  preferences.segmentationInteriorOpacity = renderData.m_segInteriorOpacity;
  preferences.segmentationErosionFactor = renderData.m_segInterpCutoff;
  preferences.squaredDifference = renderData.m_useSquare;
  preferences.squaredDifferenceMetric.colorMapIndex = renderData.m_squaredDifferenceParams.m_colorMapIndex;
  preferences.squaredDifferenceMetric.slopeIntercept = renderData.m_squaredDifferenceParams.m_slopeIntercept;
  preferences.squaredDifferenceMetric.invertColormap = renderData.m_squaredDifferenceParams.m_invertCmap;
  preferences.squaredDifferenceMetric.continuousColormap = renderData.m_squaredDifferenceParams.m_cmapContinuous;
  preferences.squaredDifferenceMetric.colormapLevels = renderData.m_squaredDifferenceParams.m_cmapQuantizationLevels;
  preferences.overlayMagentaCyan = renderData.m_overlayMagentaCyan;
  preferences.quadrants = renderData.m_quadrants;
  preferences.checkerboardSquares = renderData.m_numCheckerboardSquares;
  preferences.flashlightRadiusFraction = renderData.m_flashlightRadius;
  preferences.flashlightOverlayMovingImage = renderData.m_flashlightOverlays;
  preferences.limitFrameRate = renderData.m_manualFramerateLimiter;
  preferences.targetFrameTimeSeconds = renderData.m_targetFrameTimeSeconds;
  preferences.raycastSamplingFactor = renderData.m_raycastSamplingFactor;
  preferences.transparentBackgroundWhenNoHit = renderData.m_3dTransparentIfNoHit;
  preferences.renderFrontFaces = renderData.m_renderFrontFaces;
  preferences.renderBackFaces = renderData.m_renderBackFaces;
  preferences.segmentationMasking =
    static_cast<user_preferences::RenderPreferences::SegMaskingForRaycasting>(renderData.m_segMasking);
  preferences.asciiEnabled = renderData.m_asciiEnabled;
  preferences.asciiCellSizePx = renderData.m_asciiCellSizePx;
  preferences.asciiCharsetIndex = renderData.m_asciiCharsetIndex;
  preferences.asciiForegroundColor = renderData.m_asciiFgColor;
  preferences.asciiBackgroundColor = renderData.m_asciiBgColor;
  preferences.asciiBackgroundAlpha = renderData.m_asciiBgAlpha;
  preferences.asciiUseColormapAsForeground = renderData.m_asciiUseColormap;
  preferences.asciiSpatialMatching = renderData.m_asciiSpatialMode;
  preferences.asciiSpatialExponent = renderData.m_asciiSpatialExponent;
  preferences.annotationsOnTop = renderData.m_globalAnnotationParams.renderOnTopOfAllImagePlanes;
  preferences.landmarksOnTop = renderData.m_globalLandmarkParams.renderOnTopOfAllImagePlanes;
  preferences.hideAnnotationVertices = renderData.m_globalAnnotationParams.hidePolygonVertices;
  return preferences;
}

void applyRenderPreferences(RenderData& renderData, const user_preferences::RenderPreferences& preferences)
{
  renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections = preferences.showImageBorders;
  renderData.m_snapCrosshairs = preferences.crosshairsSnapping;
  renderData.m_crosshairsColor = preferences.crosshairsColor;
  renderData.m_2dBackgroundColor = preferences.background2dColor;
  renderData.m_3dBackgroundColor = preferences.background3dColor;
  renderData.m_anatomicalLabelColor = preferences.anatomicalLabelColor;
  renderData.m_anatomicalLabelType = preferences.anatomicalLabelType;
  renderData.m_showScaleBars = preferences.showScaleBars;
  renderData.m_showScaleBarsInLightboxViews = preferences.showScaleBarsInLightboxViews;
  renderData.m_scaleBarColor = preferences.scaleBarColor;
  renderData.m_scaleBarPosition = preferences.scaleBarPosition;
  renderData.m_scaleBarOrientation = preferences.scaleBarOrientation;
  renderData.m_scaleBarTicks = preferences.scaleBarTicks;
  renderData.m_scaleBarTargetFraction = preferences.scaleBarTargetFraction;
  renderData.m_scaleBarMarginPx = preferences.scaleBarMarginPx;
  renderData.m_showLightboxOffsetLabels = preferences.showLightboxOffsetLabels;
  renderData.m_lightboxOffsetLabelColor = preferences.lightboxOffsetLabelColor;
  renderData.m_imageGrayFloatingPointInterpolation = preferences.floatingPointLinearInterpolation;
  renderData.m_doMaxExtentIntensityProjection = preferences.useMaximumIntensityProjectionExtent;
  renderData.m_intensityProjectionSlabThickness = preferences.intensityProjectionSlabThicknessMm;
  renderData.setXrayEnergy(preferences.xrayEnergyKeV);
  renderData.m_xrayIntensityWindow = preferences.xrayWindow;
  renderData.m_xrayIntensityLevel = preferences.xrayLevel;
  renderData.m_modulateSegOpacityWithImageOpacity = preferences.modulateSegmentationOpacityWithImageOpacity;
  renderData.m_segOutlineStyle = preferences.segmentationOutlineStyle;
  renderData.m_segInteriorOpacity = preferences.segmentationInteriorOpacity;
  renderData.m_segInterpCutoff = preferences.segmentationErosionFactor;
  renderData.m_useSquare = preferences.squaredDifference;
  renderData.m_squaredDifferenceParams.m_colorMapIndex = preferences.squaredDifferenceMetric.colorMapIndex;
  renderData.m_squaredDifferenceParams.m_slopeIntercept = preferences.squaredDifferenceMetric.slopeIntercept;
  renderData.m_squaredDifferenceParams.m_invertCmap = preferences.squaredDifferenceMetric.invertColormap;
  renderData.m_squaredDifferenceParams.m_cmapContinuous = preferences.squaredDifferenceMetric.continuousColormap;
  renderData.m_squaredDifferenceParams.m_cmapQuantizationLevels = preferences.squaredDifferenceMetric.colormapLevels;
  renderData.m_overlayMagentaCyan = preferences.overlayMagentaCyan;
  renderData.m_quadrants = preferences.quadrants;
  renderData.m_numCheckerboardSquares = preferences.checkerboardSquares;
  renderData.m_flashlightRadius = preferences.flashlightRadiusFraction;
  renderData.m_flashlightOverlays = preferences.flashlightOverlayMovingImage;
  renderData.m_manualFramerateLimiter = preferences.limitFrameRate;
  renderData.m_targetFrameTimeSeconds = preferences.targetFrameTimeSeconds;
  renderData.m_raycastSamplingFactor = preferences.raycastSamplingFactor;
  renderData.m_3dTransparentIfNoHit = preferences.transparentBackgroundWhenNoHit;
  renderData.m_renderFrontFaces = preferences.renderFrontFaces;
  renderData.m_renderBackFaces = preferences.renderBackFaces;
  renderData.m_segMasking = static_cast<RenderData::SegMaskingForRaycasting>(preferences.segmentationMasking);
  renderData.m_asciiEnabled = preferences.asciiEnabled;
  renderData.m_asciiCellSizePx = preferences.asciiCellSizePx;
  renderData.m_asciiCharsetIndex = preferences.asciiCharsetIndex;
  renderData.m_asciiFgColor = preferences.asciiForegroundColor;
  renderData.m_asciiBgColor = preferences.asciiBackgroundColor;
  renderData.m_asciiBgAlpha = preferences.asciiBackgroundAlpha;
  renderData.m_asciiUseColormap = preferences.asciiUseColormapAsForeground;
  renderData.m_asciiSpatialMode = preferences.asciiSpatialMatching;
  renderData.m_asciiSpatialExponent = preferences.asciiSpatialExponent;
  renderData.m_asciiAtlasNeedsRebuild = true;
  renderData.m_globalAnnotationParams.renderOnTopOfAllImagePlanes = preferences.annotationsOnTop;
  renderData.m_globalLandmarkParams.renderOnTopOfAllImagePlanes = preferences.landmarksOnTop;
  renderData.m_globalAnnotationParams.hidePolygonVertices = preferences.hideAnnotationVertices;
}

} // namespace

namespace user_preferences
{

std::string toJsonString(const AppSettings& settings, const RenderData& renderData)
{
  return toJsonString(settings, renderPreferencesFromRenderData(renderData));
}

bool applyJsonString(AppSettings& settings, RenderData& renderData, const std::string& text, std::string* error)
{
  RenderPreferences renderPreferences = renderPreferencesFromRenderData(renderData);
  if (!applyJsonString(settings, renderPreferences, text, error)) {
    return false;
  }

  applyRenderPreferences(renderData, renderPreferences);
  return true;
}

bool save(
  const AppSettings& settings,
  const RenderData& renderData,
  const std::filesystem::path& fileName,
  std::string* error)
{
  return save(settings, renderPreferencesFromRenderData(renderData), fileName, error);
}

bool load(AppSettings& settings, RenderData& renderData, const std::filesystem::path& fileName, std::string* error)
{
  std::error_code ec;
  if (!std::filesystem::exists(fileName, ec)) {
    return true;
  }

  RenderPreferences renderPreferences = renderPreferencesFromRenderData(renderData);
  const bool loaded = load(settings, renderPreferences, fileName, error);
  if (loaded) {
    applyRenderPreferences(renderData, renderPreferences);
  }
  return loaded;
}

void applyDefaults(AppSettings& settings, RenderData& renderData)
{
  settings = AppSettings{};
  applyRenderPreferences(renderData, defaultRenderPreferences());
  entropy::logging::setDefaultLoggerSinkLevel(entropy::logging::defaultLogLevel());
}

} // namespace user_preferences
