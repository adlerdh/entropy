#include "logic/app/UserPreferences.h"

#include "common/LoggingDefaults.h"
#include "common/LoggingSettings.h"
#include "logic/app/Settings.h"
#include "rendering/RenderData.h"
#include "rendering/mesh/MeshAdvancedLighting.h"
#include "rendering/mesh/MeshDdpPolicy.h"
#include "ui/GuiData.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>

namespace
{

user_preferences::PrecisionPreferences precisionPreferencesFromGuiData(const GuiData& guiData)
{
  return user_preferences::PrecisionPreferences{
    .imageValuePrecision = guiData.m_imageValuePrecision,
    .coordsPrecision = guiData.m_coordsPrecision,
    .txPrecision = guiData.m_txPrecision,
    .percentilePrecision = guiData.m_percentilePrecision,
    .timeValuePrecision = guiData.m_timeValuePrecision};
}

void applyPrecisionPreferences(GuiData& guiData, const user_preferences::PrecisionPreferences& preferences)
{
  guiData.m_imageValuePrecision = preferences.imageValuePrecision;
  guiData.m_imageValuePrecisionFormat = std::string{"%0."} + std::to_string(preferences.imageValuePrecision) + "f";
  guiData.m_coordsPrecision = preferences.coordsPrecision;
  guiData.setCoordsPrecisionFormat();
  guiData.m_txPrecision = preferences.txPrecision;
  guiData.setTxPrecisionFormat();
  guiData.m_percentilePrecision = preferences.percentilePrecision;
  guiData.m_percentilePrecisionFormat = std::string{"%0."} + std::to_string(preferences.percentilePrecision) + "f";
  guiData.m_timeValuePrecision = preferences.timeValuePrecision;
  guiData.setTimeValuePrecisionFormat();
}

user_preferences::RenderPreferences::LocalNccPresentation localNccPresentationFromRenderData(
  RenderData::LocalNccPresentation presentation)
{
  return RenderData::LocalNccPresentation::Correlation == presentation
           ? user_preferences::RenderPreferences::LocalNccPresentation::Correlation
           : user_preferences::RenderPreferences::LocalNccPresentation::Dissimilarity;
}

RenderData::LocalNccPresentation localNccPresentationToRenderData(
  user_preferences::RenderPreferences::LocalNccPresentation presentation)
{
  return user_preferences::RenderPreferences::LocalNccPresentation::Correlation == presentation
           ? RenderData::LocalNccPresentation::Correlation
           : RenderData::LocalNccPresentation::Dissimilarity;
}

user_preferences::RenderPreferences::LocalNccInvalidStyle localNccInvalidStyleFromRenderData(
  RenderData::LocalNccInvalidStyle style)
{
  return RenderData::LocalNccInvalidStyle::Gray == style
           ? user_preferences::RenderPreferences::LocalNccInvalidStyle::Gray
           : user_preferences::RenderPreferences::LocalNccInvalidStyle::Transparent;
}

RenderData::LocalNccInvalidStyle localNccInvalidStyleToRenderData(
  user_preferences::RenderPreferences::LocalNccInvalidStyle style)
{
  return user_preferences::RenderPreferences::LocalNccInvalidStyle::Gray == style
           ? RenderData::LocalNccInvalidStyle::Gray
           : RenderData::LocalNccInvalidStyle::Transparent;
}

user_preferences::RenderPreferences renderPreferencesFromRenderData(const RenderData& renderData)
{
  user_preferences::RenderPreferences preferences;
  preferences.showImageBorders = renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections;
  preferences.showImageBordersInLightboxViews =
    renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersectionsInLightboxViews;
  preferences.crosshairsSnapping = renderData.m_snapCrosshairs;
  preferences.crosshairsColor = renderData.m_crosshairsColor;
  preferences.showCrosshairs = renderData.m_showCrosshairs;
  preferences.showCrosshairsInLightboxViews = renderData.m_showCrosshairsInLightboxViews;
  preferences.background2dColor = renderData.m_2dBackgroundColor;
  preferences.background3dColor = renderData.m_3dBackgroundColor;
  preferences.anatomicalLabelColor = renderData.m_anatomicalLabelColor;
  preferences.showAnatomicalLabels = renderData.m_showAnatomicalLabels;
  preferences.showAnatomicalLabelsInLightboxViews = renderData.m_showAnatomicalLabelsInLightboxViews;
  preferences.anatomicalLabelType = renderData.m_anatomicalLabelType;
  preferences.anatomicalLabelScale = renderData.m_anatomicalLabelScale;
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
  preferences.floatingPointLinearInterpolationPolicy = renderData.m_imageGrayFloatingPointInterpolationPolicy;
  preferences.useMaximumIntensityProjectionExtent = renderData.m_doMaxExtentIntensityProjection;
  preferences.intensityProjectionSlabThicknessMm = renderData.m_intensityProjectionSlabThickness;
  preferences.xrayEnergyKeV = renderData.m_xrayEnergyKeV;
  preferences.xrayWindow = renderData.m_xrayIntensityWindow;
  preferences.xrayLevel = renderData.m_xrayIntensityLevel;
  preferences.isocontourFloatingPointInterpolationPolicy = renderData.m_isocontourFloatingPointInterpolationPolicy;
  preferences.modulateSegmentationOpacityWithImageOpacity2d =
    renderData.m_modulateSegmentationOpacityWithImageOpacity2d;
  preferences.modulateSegmentationOpacityWithImageOpacity3d =
    renderData.m_modulateSegmentationOpacityWithImageOpacity3d;
  preferences.segmentationOutlineStyle = renderData.m_segOutlineStyle;
  preferences.segmentationInteriorOpacity = renderData.m_segInteriorOpacity;
  preferences.segmentationErosionFactor = renderData.m_segInterpCutoff;
  preferences.squaredDifference = renderData.m_useSquare;
  preferences.squaredDifferenceMetric.colorMapIndex = renderData.m_squaredDifferenceParams.m_colorMapIndex;
  preferences.squaredDifferenceMetric.slopeIntercept = renderData.m_squaredDifferenceParams.m_slopeIntercept;
  preferences.squaredDifferenceMetric.invertColormap = renderData.m_squaredDifferenceParams.m_invertCmap;
  preferences.squaredDifferenceMetric.continuousColormap = renderData.m_squaredDifferenceParams.m_cmapContinuous;
  preferences.squaredDifferenceMetric.colormapLevels = renderData.m_squaredDifferenceParams.m_cmapQuantizationLevels;
  preferences.localNccMetric.colorMapIndex = renderData.m_localNccParams.m_colorMapIndex;
  preferences.localNccMetric.slopeIntercept = renderData.m_localNccParams.m_slopeIntercept;
  preferences.localNccMetric.invertColormap = renderData.m_localNccParams.m_invertCmap;
  preferences.localNccMetric.continuousColormap = renderData.m_localNccParams.m_cmapContinuous;
  preferences.localNccMetric.colormapLevels = renderData.m_localNccParams.m_cmapQuantizationLevels;
  preferences.localNccPatchRadius = renderData.m_localNccPatchRadius;
  preferences.localNccSampleSpacing = renderData.m_localNccSampleSpacing;
  preferences.localNccMinValidFraction = renderData.m_localNccMinValidFraction;
  preferences.localNccVarianceEpsilon = renderData.m_localNccVarianceEpsilon;
  preferences.localNccIgnoreNegativeCorrelation = renderData.m_localNccIgnoreNegativeCorrelation;
  preferences.localNccPresentation = localNccPresentationFromRenderData(renderData.m_localNccPresentation);
  preferences.localNccInvalidStyle = localNccInvalidStyleFromRenderData(renderData.m_localNccInvalidStyle);
  preferences.localLinearResidualMetric.colorMapIndex = renderData.m_localLinearResidualParams.m_colorMapIndex;
  preferences.localLinearResidualMetric.slopeIntercept = renderData.m_localLinearResidualParams.m_slopeIntercept;
  preferences.localLinearResidualMetric.invertColormap = renderData.m_localLinearResidualParams.m_invertCmap;
  preferences.localLinearResidualMetric.continuousColormap = renderData.m_localLinearResidualParams.m_cmapContinuous;
  preferences.localLinearResidualMetric.colormapLevels =
    renderData.m_localLinearResidualParams.m_cmapQuantizationLevels;
  preferences.localLinearResidualPatchRadius = renderData.m_localLinearResidualPatchRadius;
  preferences.localLinearResidualSampleSpacing = renderData.m_localLinearResidualSampleSpacing;
  preferences.localLinearResidualMinValidFraction = renderData.m_localLinearResidualMinValidFraction;
  preferences.localLinearResidualVarianceEpsilon = renderData.m_localLinearResidualVarianceEpsilon;
  preferences.localLinearResidualInvalidStyle =
    localNccInvalidStyleFromRenderData(renderData.m_localLinearResidualInvalidStyle);
  preferences.overlayMagentaCyan = renderData.m_overlayMagentaCyan;
  preferences.quadrants = renderData.m_quadrants;
  preferences.checkerboardSquares = renderData.m_numCheckerboardSquares;
  preferences.flashlightRadiusFraction = renderData.m_flashlightRadius;
  preferences.flashlightOverlayMovingImage = renderData.m_flashlightOverlays;
  preferences.limitFrameRate = renderData.m_manualFramerateLimiter;
  preferences.targetFrameTimeSeconds = renderData.m_targetFrameTimeSeconds;
  preferences.raycastSamplingFactor = renderData.m_raycastSamplingFactor;
  preferences.useDistanceMapForRaycasting = renderData.m_useDistanceMapForRaycasting;
  preferences.distanceMapForegroundLowerPercentile = renderData.m_distanceMapForegroundLowerPercentile;
  preferences.distanceMapForegroundUpperPercentile = renderData.m_distanceMapForegroundUpperPercentile;
  preferences.transparent3DBackground = renderData.m_3dTransparentIfNoHit;
  preferences.imageBoxVisible = renderData.m_raycastBackgroundEdgeBrighteningEnabled;
  preferences.showImagePlanesIn3D = renderData.m_showImagePlanesIn3D;
  preferences.modulateImagePlaneOpacityWithViewAngle = renderData.m_modulateImagePlaneOpacityWithViewAngle;
  preferences.showSegmentationsOnImagePlanesIn3D = renderData.m_showSegmentationsOnImagePlanesIn3D;
  preferences.shadeImagePlanesIn3D = renderData.m_shadeImagePlanesIn3D;
  preferences.imagePlaneLightingAmbient = renderData.m_imagePlaneLightingAmbient;
  preferences.imagePlaneLightingDiffuse = renderData.m_imagePlaneLightingDiffuse;
  preferences.imagePlaneLightingSpecular = renderData.m_imagePlaneLightingSpecular;
  preferences.imagePlaneLightingSpecularPower = renderData.m_imagePlaneLightingSpecularPower;
  preferences.lightingAmbient = renderData.m_lightingAmbient;
  preferences.lightingDiffuse = renderData.m_lightingDiffuse;
  preferences.lightingSpecular = renderData.m_lightingSpecular;
  preferences.lightingSpecularPower = renderData.m_lightingSpecularPower;
  preferences.meshFlatShadingEnabled = renderData.m_meshSurfaceMaterialSettings.flatShadingEnabled;
  preferences.meshPbrShadingEnabled = renderData.m_meshSurfaceMaterialSettings.pbrShadingEnabled;
  preferences.meshPbrMetallic = renderData.m_meshSurfaceMaterialSettings.metallic;
  preferences.meshPbrRoughness = renderData.m_meshSurfaceMaterialSettings.roughness;
  preferences.meshPbrAmbientOcclusion = renderData.m_meshSurfaceMaterialSettings.ambientOcclusion;
  preferences.renderFrontFaces = renderData.m_renderFrontFaces;
  preferences.renderBackFaces = renderData.m_renderBackFaces;
  preferences.reversePovRotation = renderData.m_reverseThreeDRotateAboutEye;
  preferences.showCrosshairsIn3D = renderData.m_showCrosshairsIn3D;
  preferences.crosshairs3DGlyphDiameterVoxelDiagonals = renderData.m_crosshairs3DGlyphDiameterVoxelDiagonals;
  preferences.crosshairs3DGlyphLengthVoxelDiagonals = renderData.m_crosshairs3DGlyphLengthVoxelDiagonals;
  preferences.showThreeDCameraFrustumIn2DViews = renderData.m_showThreeDCameraFrustumIn2DViews;
  preferences.threeDCameraFrustumColor = renderData.m_threeDCameraFrustumColor;
  preferences.smoothSegmentationMeshes = renderData.m_smoothSegmentationMeshes;
  preferences.smoothIsosurfaceMeshes = renderData.m_smoothIsosurfaceMeshes;
  preferences.meshSmoothingIterations = renderData.m_meshSmoothingIterations;
  preferences.meshSmoothingPassBand = renderData.m_meshSmoothingPassBand;
  preferences.meshPickingEnabled = renderData.m_meshPickingEnabled;
  preferences.meshClipPlaneEnabled = renderData.m_meshClipPlaneEnabled;
  preferences.meshClipPlaneWorld = renderData.m_meshClipPlaneWorld;
  preferences.meshShadowsEnabled = renderData.m_meshAdvancedLightingSettings.shadows.enabled;
  preferences.meshShadowMapSizePixels = renderData.m_meshAdvancedLightingSettings.shadows.mapSizePixels;
  preferences.meshShadowStrength = renderData.m_meshAdvancedLightingSettings.shadows.strength;
  preferences.meshShadowDepthBias = renderData.m_meshAdvancedLightingSettings.shadows.depthBias;
  preferences.meshAmbientOcclusionEnabled = renderData.m_meshAdvancedLightingSettings.ambientOcclusion.enabled;
  preferences.meshAmbientOcclusionRadiusMm = renderData.m_meshAdvancedLightingSettings.ambientOcclusion.radiusMm;
  preferences.meshAmbientOcclusionStrength = renderData.m_meshAdvancedLightingSettings.ambientOcclusion.strength;
  preferences.meshAmbientOcclusionPower = renderData.m_meshAdvancedLightingSettings.ambientOcclusion.power;
  preferences.meshAmbientOcclusionContrast = renderData.m_meshAdvancedLightingSettings.ambientOcclusion.contrast;
  preferences.meshAmbientOcclusionSampleCount = renderData.m_meshAdvancedLightingSettings.ambientOcclusion.sampleCount;
  preferences.meshRimLightingEnabled = renderData.m_meshSurfaceMaterialSettings.rimLightingEnabled;
  preferences.meshRimOpacityStrength = renderData.m_meshSurfaceMaterialSettings.rimOpacityStrength;
  preferences.meshRimEmissionStrength = renderData.m_meshSurfaceMaterialSettings.rimEmissionStrength;
  preferences.meshRimPower = renderData.m_meshSurfaceMaterialSettings.rimPower;
  preferences.ddpMaxPeelPasses = renderData.m_meshDdpSettings.maxPeelPasses;
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
  renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersectionsInLightboxViews =
    preferences.showImageBorders && preferences.showImageBordersInLightboxViews;
  renderData.m_snapCrosshairs = preferences.crosshairsSnapping;
  renderData.m_crosshairsColor = preferences.crosshairsColor;
  renderData.m_showCrosshairs = preferences.showCrosshairs;
  renderData.m_showCrosshairsInLightboxViews = preferences.showCrosshairs && preferences.showCrosshairsInLightboxViews;
  renderData.m_2dBackgroundColor = preferences.background2dColor;
  renderData.m_3dBackgroundColor = preferences.background3dColor;
  renderData.m_anatomicalLabelColor = preferences.anatomicalLabelColor;
  renderData.m_showAnatomicalLabels = preferences.showAnatomicalLabels;
  renderData.m_showAnatomicalLabelsInLightboxViews =
    preferences.showAnatomicalLabels && preferences.showAnatomicalLabelsInLightboxViews;
  renderData.m_anatomicalLabelType = preferences.anatomicalLabelType;
  renderData.m_anatomicalLabelScale = std::clamp(preferences.anatomicalLabelScale, 0.5f, 2.0f);
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
  renderData.m_imageGrayFloatingPointInterpolationPolicy = preferences.floatingPointLinearInterpolationPolicy;
  renderData.m_doMaxExtentIntensityProjection = preferences.useMaximumIntensityProjectionExtent;
  renderData.m_intensityProjectionSlabThickness = preferences.intensityProjectionSlabThicknessMm;
  renderData.setXrayEnergy(preferences.xrayEnergyKeV);
  renderData.m_xrayIntensityWindow = preferences.xrayWindow;
  renderData.m_xrayIntensityLevel = preferences.xrayLevel;
  renderData.m_isocontourFloatingPointInterpolationPolicy = preferences.isocontourFloatingPointInterpolationPolicy;
  renderData.m_modulateSegmentationOpacityWithImageOpacity2d =
    preferences.modulateSegmentationOpacityWithImageOpacity2d;
  renderData.m_modulateSegmentationOpacityWithImageOpacity3d =
    preferences.modulateSegmentationOpacityWithImageOpacity3d;
  renderData.m_segOutlineStyle = preferences.segmentationOutlineStyle;
  renderData.m_segInteriorOpacity = preferences.segmentationInteriorOpacity;
  renderData.m_segInterpCutoff = preferences.segmentationErosionFactor;
  renderData.m_useSquare = preferences.squaredDifference;
  renderData.m_squaredDifferenceParams.m_colorMapIndex = preferences.squaredDifferenceMetric.colorMapIndex;
  renderData.m_squaredDifferenceParams.m_slopeIntercept = preferences.squaredDifferenceMetric.slopeIntercept;
  renderData.m_squaredDifferenceParams.m_invertCmap = preferences.squaredDifferenceMetric.invertColormap;
  renderData.m_squaredDifferenceParams.m_cmapContinuous = preferences.squaredDifferenceMetric.continuousColormap;
  renderData.m_squaredDifferenceParams.m_cmapQuantizationLevels = preferences.squaredDifferenceMetric.colormapLevels;
  renderData.m_localNccParams.m_colorMapIndex = preferences.localNccMetric.colorMapIndex;
  renderData.m_localNccParams.m_slopeIntercept = preferences.localNccMetric.slopeIntercept;
  renderData.m_localNccParams.m_invertCmap = preferences.localNccMetric.invertColormap;
  renderData.m_localNccParams.m_cmapContinuous = preferences.localNccMetric.continuousColormap;
  renderData.m_localNccParams.m_cmapQuantizationLevels = preferences.localNccMetric.colormapLevels;
  renderData.m_localNccPatchRadius = preferences.localNccPatchRadius;
  renderData.m_localNccSampleSpacing = preferences.localNccSampleSpacing;
  renderData.m_localNccMinValidFraction = preferences.localNccMinValidFraction;
  renderData.m_localNccVarianceEpsilon = preferences.localNccVarianceEpsilon;
  renderData.m_localNccIgnoreNegativeCorrelation = preferences.localNccIgnoreNegativeCorrelation;
  renderData.m_localNccPresentation = localNccPresentationToRenderData(preferences.localNccPresentation);
  renderData.m_localNccInvalidStyle = localNccInvalidStyleToRenderData(preferences.localNccInvalidStyle);
  renderData.m_localLinearResidualParams.m_colorMapIndex = preferences.localLinearResidualMetric.colorMapIndex;
  renderData.m_localLinearResidualParams.m_slopeIntercept = preferences.localLinearResidualMetric.slopeIntercept;
  renderData.m_localLinearResidualParams.m_invertCmap = preferences.localLinearResidualMetric.invertColormap;
  renderData.m_localLinearResidualParams.m_cmapContinuous = preferences.localLinearResidualMetric.continuousColormap;
  renderData.m_localLinearResidualParams.m_cmapQuantizationLevels =
    preferences.localLinearResidualMetric.colormapLevels;
  renderData.m_localLinearResidualPatchRadius = preferences.localLinearResidualPatchRadius;
  renderData.m_localLinearResidualSampleSpacing = preferences.localLinearResidualSampleSpacing;
  renderData.m_localLinearResidualMinValidFraction = preferences.localLinearResidualMinValidFraction;
  renderData.m_localLinearResidualVarianceEpsilon = preferences.localLinearResidualVarianceEpsilon;
  renderData.m_localLinearResidualInvalidStyle =
    localNccInvalidStyleToRenderData(preferences.localLinearResidualInvalidStyle);
  renderData.m_overlayMagentaCyan = preferences.overlayMagentaCyan;
  renderData.m_quadrants = preferences.quadrants;
  renderData.m_numCheckerboardSquares = preferences.checkerboardSquares;
  renderData.m_flashlightRadius = preferences.flashlightRadiusFraction;
  renderData.m_flashlightOverlays = preferences.flashlightOverlayMovingImage;
  renderData.m_manualFramerateLimiter = preferences.limitFrameRate;
  renderData.m_targetFrameTimeSeconds = preferences.targetFrameTimeSeconds;
  renderData.m_raycastSamplingFactor = std::clamp(preferences.raycastSamplingFactor, 0.5f, 2.0f);
  renderData.m_useDistanceMapForRaycasting = preferences.useDistanceMapForRaycasting;
  renderData.m_distanceMapForegroundLowerPercentile =
    std::clamp(preferences.distanceMapForegroundLowerPercentile, 0.0f, 1.0f);
  renderData.m_distanceMapForegroundUpperPercentile =
    std::clamp(preferences.distanceMapForegroundUpperPercentile, 0.0f, 1.0f);
  renderData.m_adaptiveRaycastSamplingEnabled = false;
  renderData.m_adaptiveRaycastTargetFrameRate = 30.0f;
  renderData.m_adaptiveRaycastEffectiveSamplingFactor = std::clamp(preferences.raycastSamplingFactor, 0.5f, 2.0f);
  renderData.m_3dTransparentIfNoHit = preferences.transparent3DBackground;
  renderData.m_raycastBackgroundEdgeBrighteningEnabled = preferences.imageBoxVisible;
  renderData.m_showImagePlanesIn3D = preferences.showImagePlanesIn3D;
  renderData.m_modulateImagePlaneOpacityWithViewAngle = preferences.modulateImagePlaneOpacityWithViewAngle;
  renderData.m_showSegmentationsOnImagePlanesIn3D = preferences.showSegmentationsOnImagePlanesIn3D;
  renderData.m_shadeImagePlanesIn3D = preferences.shadeImagePlanesIn3D;
  renderData.m_imagePlaneLightingAmbient = preferences.imagePlaneLightingAmbient;
  renderData.m_imagePlaneLightingDiffuse = preferences.imagePlaneLightingDiffuse;
  renderData.m_imagePlaneLightingSpecular = preferences.imagePlaneLightingSpecular;
  renderData.m_imagePlaneLightingSpecularPower = preferences.imagePlaneLightingSpecularPower;
  renderData.m_lightingAmbient = preferences.lightingAmbient;
  renderData.m_lightingDiffuse = preferences.lightingDiffuse;
  renderData.m_lightingSpecular = preferences.lightingSpecular;
  renderData.m_lightingSpecularPower = preferences.lightingSpecularPower;
  renderData.m_meshSurfaceMaterialSettings.flatShadingEnabled = preferences.meshFlatShadingEnabled;
  renderData.m_meshSurfaceMaterialSettings.pbrShadingEnabled = preferences.meshPbrShadingEnabled;
  renderData.m_meshSurfaceMaterialSettings.metallic = preferences.meshPbrMetallic;
  renderData.m_meshSurfaceMaterialSettings.roughness = preferences.meshPbrRoughness;
  renderData.m_meshSurfaceMaterialSettings.ambientOcclusion = preferences.meshPbrAmbientOcclusion;
  renderData.m_renderFrontFaces = preferences.renderFrontFaces;
  renderData.m_renderBackFaces = preferences.renderBackFaces;
  renderData.m_reverseThreeDRotateAboutEye = preferences.reversePovRotation;
  renderData.m_showCrosshairsIn3D = preferences.showCrosshairsIn3D;
  renderData.m_crosshairs3DGlyphDiameterVoxelDiagonals = preferences.crosshairs3DGlyphDiameterVoxelDiagonals;
  renderData.m_crosshairs3DGlyphLengthVoxelDiagonals = preferences.crosshairs3DGlyphLengthVoxelDiagonals;
  renderData.m_showThreeDCameraFrustumIn2DViews = preferences.showThreeDCameraFrustumIn2DViews;
  renderData.m_threeDCameraFrustumColor = preferences.threeDCameraFrustumColor;
  renderData.m_smoothSegmentationMeshes = preferences.smoothSegmentationMeshes;
  renderData.m_smoothIsosurfaceMeshes = preferences.smoothIsosurfaceMeshes;
  renderData.m_meshSmoothingIterations = std::clamp(preferences.meshSmoothingIterations, 1u, 1000u);
  renderData.m_meshSmoothingPassBand = std::clamp(preferences.meshSmoothingPassBand, 0.001f, 2.0f);
  renderData.m_meshPickingEnabled = preferences.meshPickingEnabled;
  renderData.m_meshClipPlaneEnabled = preferences.meshClipPlaneEnabled;
  renderData.m_meshClipPlaneWorld = preferences.meshClipPlaneWorld;
  renderData.m_meshAdvancedLightingSettings.shadows.enabled = preferences.meshShadowsEnabled;
  renderData.m_meshAdvancedLightingSettings.shadows.mapSizePixels = preferences.meshShadowMapSizePixels;
  renderData.m_meshAdvancedLightingSettings.shadows.strength = preferences.meshShadowStrength;
  renderData.m_meshAdvancedLightingSettings.shadows.depthBias = preferences.meshShadowDepthBias;
  renderData.m_meshAdvancedLightingSettings.ambientOcclusion.enabled = preferences.meshAmbientOcclusionEnabled;
  renderData.m_meshAdvancedLightingSettings.ambientOcclusion.radiusMm = preferences.meshAmbientOcclusionRadiusMm;
  renderData.m_meshAdvancedLightingSettings.ambientOcclusion.strength = preferences.meshAmbientOcclusionStrength;
  renderData.m_meshAdvancedLightingSettings.ambientOcclusion.power = preferences.meshAmbientOcclusionPower;
  renderData.m_meshAdvancedLightingSettings.ambientOcclusion.contrast = preferences.meshAmbientOcclusionContrast;
  renderData.m_meshAdvancedLightingSettings.ambientOcclusion.sampleCount = preferences.meshAmbientOcclusionSampleCount;
  renderData.m_meshSurfaceMaterialSettings.rimLightingEnabled = preferences.meshRimLightingEnabled;
  renderData.m_meshSurfaceMaterialSettings.rimOpacityStrength = preferences.meshRimOpacityStrength;
  renderData.m_meshSurfaceMaterialSettings.rimEmissionStrength = preferences.meshRimEmissionStrength;
  renderData.m_meshSurfaceMaterialSettings.rimPower = preferences.meshRimPower;
  renderData.m_meshDdpSettings.maxPeelPasses = std::clamp<uint32_t>(preferences.ddpMaxPeelPasses, 1u, 32u);
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

void preserveProjectRenderPreferences(
  user_preferences::RenderPreferences& preferences,
  const user_preferences::RenderPreferences& currentPreferences)
{
  user_preferences::preserveProjectOwnedRenderPreferences(preferences, currentPreferences);
}

user_preferences::RenderPreferences applicationRenderPreferencesFromRenderData(const RenderData& renderData)
{
  return user_preferences::applicationRenderPreferences(renderPreferencesFromRenderData(renderData));
}

} // namespace

namespace user_preferences
{

std::string toJsonString(const AppSettings& settings, const RenderData& renderData, const GuiData& guiData)
{
  return toJsonString(
    settings,
    applicationRenderPreferencesFromRenderData(renderData),
    precisionPreferencesFromGuiData(guiData));
}

void markSavedAppSettingsState(const AppSettings& settings, const RenderData& renderData, GuiData& guiData)
{
  guiData.m_savedAppSettingsJson = toJsonString(settings, renderData, guiData);
  guiData.m_appSettingsDirty = false;
}

void updateAppSettingsDirtyState(const AppSettings& settings, const RenderData& renderData, GuiData& guiData)
{
  if (guiData.m_savedAppSettingsJson.empty()) {
    markSavedAppSettingsState(settings, renderData, guiData);
    return;
  }

  guiData.m_appSettingsDirty = toJsonString(settings, renderData, guiData) != guiData.m_savedAppSettingsJson;
}

bool applyJsonString(
  AppSettings& settings,
  RenderData& renderData,
  GuiData& guiData,
  const std::string& text,
  std::string* error)
{
  RenderPreferences renderPreferences = renderPreferencesFromRenderData(renderData);
  PrecisionPreferences precisionPreferences = precisionPreferencesFromGuiData(guiData);
  if (!applyJsonString(settings, renderPreferences, precisionPreferences, text, error)) {
    return false;
  }

  preserveProjectRenderPreferences(renderPreferences, renderPreferencesFromRenderData(renderData));
  applyRenderPreferences(renderData, renderPreferences);
  applyPrecisionPreferences(guiData, precisionPreferences);
  return true;
}

bool save(
  const AppSettings& settings,
  const RenderData& renderData,
  const GuiData& guiData,
  const std::filesystem::path& fileName,
  std::string* error)
{
  return save(
    settings,
    applicationRenderPreferencesFromRenderData(renderData),
    precisionPreferencesFromGuiData(guiData),
    fileName,
    error);
}

bool load(
  AppSettings& settings,
  RenderData& renderData,
  GuiData& guiData,
  const std::filesystem::path& fileName,
  std::string* error)
{
  std::error_code ec;
  if (!std::filesystem::exists(fileName, ec)) {
    return true;
  }

  RenderPreferences renderPreferences = renderPreferencesFromRenderData(renderData);
  PrecisionPreferences precisionPreferences = precisionPreferencesFromGuiData(guiData);
  const bool loaded = load(settings, renderPreferences, precisionPreferences, fileName, error);
  if (loaded) {
    preserveProjectRenderPreferences(renderPreferences, renderPreferencesFromRenderData(renderData));
    applyRenderPreferences(renderData, renderPreferences);
    applyPrecisionPreferences(guiData, precisionPreferences);
  }
  return loaded;
}

void applyDefaults(AppSettings& settings, RenderData& renderData, GuiData& guiData)
{
  const bool synchronizeTimeSeries = settings.synchronizeTimeSeries();
  const bool lockAnatomicalDirections = settings.lockAnatomicalCoordinateAxesWithReferenceImage();
  const user_preferences::RenderPreferences currentRenderPreferences = renderPreferencesFromRenderData(renderData);
  user_preferences::RenderPreferences defaultPreferences = defaultRenderPreferences();
  preserveProjectRenderPreferences(defaultPreferences, currentRenderPreferences);

  settings = AppSettings{};
  settings.setSynchronizeTimeSeries(synchronizeTimeSeries);
  settings.setLockAnatomicalCoordinateAxesWithReferenceImage(lockAnatomicalDirections);
  applyRenderPreferences(renderData, defaultPreferences);
  applyPrecisionPreferences(guiData, PrecisionPreferences{});
  logging::setDefaultLoggerSinkLevel(logging::defaultLogLevel());
}

} // namespace user_preferences
