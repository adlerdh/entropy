#include "logic/app/UserPreferences.h"

#include "common/LoggingDefaults.h"
#include "common/LoggingSettings.h"
#include "logic/app/Settings.h"
#include "rendering/RenderSettings.h"
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

user_preferences::RenderPreferences::LocalNccPresentation localNccPresentationFromRenderSettings(
  rendering::RenderSettings::LocalNccPresentation presentation)
{
  return rendering::RenderSettings::LocalNccPresentation::Correlation == presentation
           ? user_preferences::RenderPreferences::LocalNccPresentation::Correlation
           : user_preferences::RenderPreferences::LocalNccPresentation::Dissimilarity;
}

rendering::RenderSettings::LocalNccPresentation localNccPresentationToRenderSettings(
  user_preferences::RenderPreferences::LocalNccPresentation presentation)
{
  return user_preferences::RenderPreferences::LocalNccPresentation::Correlation == presentation
           ? rendering::RenderSettings::LocalNccPresentation::Correlation
           : rendering::RenderSettings::LocalNccPresentation::Dissimilarity;
}

user_preferences::RenderPreferences::LocalNccInvalidStyle localNccInvalidStyleFromRenderSettings(
  rendering::RenderSettings::LocalNccInvalidStyle style)
{
  return rendering::RenderSettings::LocalNccInvalidStyle::Gray == style
           ? user_preferences::RenderPreferences::LocalNccInvalidStyle::Gray
           : user_preferences::RenderPreferences::LocalNccInvalidStyle::Transparent;
}

rendering::RenderSettings::LocalNccInvalidStyle localNccInvalidStyleToRenderSettings(
  user_preferences::RenderPreferences::LocalNccInvalidStyle style)
{
  return user_preferences::RenderPreferences::LocalNccInvalidStyle::Gray == style
           ? rendering::RenderSettings::LocalNccInvalidStyle::Gray
           : rendering::RenderSettings::LocalNccInvalidStyle::Transparent;
}

user_preferences::RenderPreferences renderPreferencesFromRenderSettings(const rendering::RenderSettings& renderSettings)
{
  user_preferences::RenderPreferences preferences;
  preferences.showImageBorders = renderSettings.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections;
  preferences.showImageBordersInLightboxViews =
    renderSettings.m_globalSliceIntersectionParams.renderInactiveImageViewIntersectionsInLightboxViews;
  preferences.crosshairsSnapping = renderSettings.m_snapCrosshairs;
  preferences.crosshairsColor = renderSettings.m_crosshairsColor;
  preferences.showCrosshairs = renderSettings.m_showCrosshairs;
  preferences.showCrosshairsInLightboxViews = renderSettings.m_showCrosshairsInLightboxViews;
  preferences.showTransformationGuides = renderSettings.m_showTransformationGuides;
  preferences.transformationGuideColor = renderSettings.m_transformationGuideColor;
  preferences.background2dColor = renderSettings.m_2dBackgroundColor;
  preferences.background3dColor = renderSettings.m_3dBackgroundColor;
  preferences.anatomicalLabelColor = renderSettings.m_anatomicalLabelColor;
  preferences.showAnatomicalLabels = renderSettings.m_showAnatomicalLabels;
  preferences.showAnatomicalLabelsInLightboxViews = renderSettings.m_showAnatomicalLabelsInLightboxViews;
  preferences.anatomicalLabelType = renderSettings.m_anatomicalLabelType;
  preferences.quadrupedBodyRegion = renderSettings.m_quadrupedBodyRegion;
  preferences.anatomicalLabelScale = renderSettings.m_anatomicalLabelScale;
  preferences.showScaleBars = renderSettings.m_showScaleBars;
  preferences.showScaleBarsInLightboxViews = renderSettings.m_showScaleBarsInLightboxViews;
  preferences.scaleBarColor = renderSettings.m_scaleBarColor;
  preferences.scaleBarPosition = renderSettings.m_scaleBarPosition;
  preferences.scaleBarOrientation = renderSettings.m_scaleBarOrientation;
  preferences.scaleBarTicks = renderSettings.m_scaleBarTicks;
  preferences.scaleBarTargetFraction = renderSettings.m_scaleBarTargetFraction;
  preferences.scaleBarMarginPx = renderSettings.m_scaleBarMarginPx;
  preferences.showLightboxOffsetLabels = renderSettings.m_showLightboxOffsetLabels;
  preferences.lightboxOffsetLabelColor = renderSettings.m_lightboxOffsetLabelColor;
  preferences.floatingPointLinearInterpolationPolicy = renderSettings.m_imageGrayFloatingPointInterpolationPolicy;
  preferences.useMaximumIntensityProjectionExtent = renderSettings.m_doMaxExtentIntensityProjection;
  preferences.intensityProjectionSlabThicknessMm = renderSettings.m_intensityProjectionSlabThickness;
  preferences.xrayEnergyKeV = renderSettings.m_xrayEnergyKeV;
  preferences.xrayWindow = renderSettings.m_xrayIntensityWindow;
  preferences.xrayLevel = renderSettings.m_xrayIntensityLevel;
  preferences.isocontourFloatingPointInterpolationPolicy = renderSettings.m_isocontourFloatingPointInterpolationPolicy;
  preferences.modulateSegmentationOpacityWithImageOpacity2d =
    renderSettings.m_modulateSegmentationOpacityWithImageOpacity2d;
  preferences.modulateSegmentationOpacityWithImageOpacity3d =
    renderSettings.m_modulateSegmentationOpacityWithImageOpacity3d;
  preferences.segmentationOutlineStyle = renderSettings.m_segOutlineStyle;
  preferences.segmentationInteriorOpacity = renderSettings.m_segInteriorOpacity;
  preferences.segmentationErosionFactor = renderSettings.m_segInterpCutoff;
  preferences.squaredDifference = renderSettings.m_useSquare;
  preferences.squaredDifferenceMetric.colorMapIndex = renderSettings.m_squaredDifferenceParams.m_colorMapIndex;
  preferences.squaredDifferenceMetric.slopeIntercept = renderSettings.m_squaredDifferenceParams.m_slopeIntercept;
  preferences.squaredDifferenceMetric.invertColormap = renderSettings.m_squaredDifferenceParams.m_invertCmap;
  preferences.squaredDifferenceMetric.continuousColormap = renderSettings.m_squaredDifferenceParams.m_cmapContinuous;
  preferences.squaredDifferenceMetric.colormapLevels =
    renderSettings.m_squaredDifferenceParams.m_cmapQuantizationLevels;
  preferences.localNccMetric.colorMapIndex = renderSettings.m_localNccParams.m_colorMapIndex;
  preferences.localNccMetric.slopeIntercept = renderSettings.m_localNccParams.m_slopeIntercept;
  preferences.localNccMetric.invertColormap = renderSettings.m_localNccParams.m_invertCmap;
  preferences.localNccMetric.continuousColormap = renderSettings.m_localNccParams.m_cmapContinuous;
  preferences.localNccMetric.colormapLevels = renderSettings.m_localNccParams.m_cmapQuantizationLevels;
  preferences.localNccPatchRadius = renderSettings.m_localNccPatchRadius;
  preferences.localNccSampleSpacing = renderSettings.m_localNccSampleSpacing;
  preferences.localNccMinValidFraction = renderSettings.m_localNccMinValidFraction;
  preferences.localNccVarianceEpsilon = renderSettings.m_localNccVarianceEpsilon;
  preferences.localNccIgnoreNegativeCorrelation = renderSettings.m_localNccIgnoreNegativeCorrelation;
  preferences.localNccPresentation = localNccPresentationFromRenderSettings(renderSettings.m_localNccPresentation);
  preferences.localNccInvalidStyle = localNccInvalidStyleFromRenderSettings(renderSettings.m_localNccInvalidStyle);
  preferences.localLinearResidualMetric.colorMapIndex = renderSettings.m_localLinearResidualParams.m_colorMapIndex;
  preferences.localLinearResidualMetric.slopeIntercept = renderSettings.m_localLinearResidualParams.m_slopeIntercept;
  preferences.localLinearResidualMetric.invertColormap = renderSettings.m_localLinearResidualParams.m_invertCmap;
  preferences.localLinearResidualMetric.continuousColormap =
    renderSettings.m_localLinearResidualParams.m_cmapContinuous;
  preferences.localLinearResidualMetric.colormapLevels =
    renderSettings.m_localLinearResidualParams.m_cmapQuantizationLevels;
  preferences.localLinearResidualPatchRadius = renderSettings.m_localLinearResidualPatchRadius;
  preferences.localLinearResidualSampleSpacing = renderSettings.m_localLinearResidualSampleSpacing;
  preferences.localLinearResidualMinValidFraction = renderSettings.m_localLinearResidualMinValidFraction;
  preferences.localLinearResidualVarianceEpsilon = renderSettings.m_localLinearResidualVarianceEpsilon;
  preferences.localLinearResidualInvalidStyle =
    localNccInvalidStyleFromRenderSettings(renderSettings.m_localLinearResidualInvalidStyle);
  preferences.overlayMagentaCyan = renderSettings.m_overlayMagentaCyan;
  preferences.quadrants = renderSettings.m_quadrants;
  preferences.checkerboardSquares = renderSettings.m_numCheckerboardSquares;
  preferences.flashlightRadiusFraction = renderSettings.m_flashlightRadius;
  preferences.flashlightOverlayMovingImage = renderSettings.m_flashlightOverlays;
  preferences.limitFrameRate = renderSettings.m_manualFramerateLimiter;
  preferences.targetFrameTimeSeconds = renderSettings.m_targetFrameTimeSeconds;
  preferences.raycastSamplingFactor = renderSettings.m_raycastSamplingFactor;
  preferences.useDistanceMapForRaycasting = renderSettings.m_useDistanceMapForRaycasting;
  preferences.distanceMapForegroundLowerPercentile = renderSettings.m_distanceMapForegroundLowerPercentile;
  preferences.distanceMapForegroundUpperPercentile = renderSettings.m_distanceMapForegroundUpperPercentile;
  preferences.transparent3DBackground = renderSettings.m_3dTransparentIfNoHit;
  preferences.imageBoxVisible = renderSettings.m_raycastBackgroundEdgeBrighteningEnabled;
  preferences.showImagePlanesIn3D = renderSettings.m_showImagePlanesIn3D;
  preferences.imagePlaneOpacity = renderSettings.m_imagePlaneOpacity;
  preferences.modulateImagePlaneOpacityWithViewAngle = renderSettings.m_modulateImagePlaneOpacityWithViewAngle;
  preferences.showSegmentationsOnImagePlanesIn3D = renderSettings.m_showSegmentationsOnImagePlanesIn3D;
  preferences.showIsocontoursOnImagePlanesIn3D = renderSettings.m_showIsocontoursOnImagePlanesIn3D;
  preferences.shadeImagePlanesIn3D = renderSettings.m_shadeImagePlanesIn3D;
  preferences.imagePlaneLightingAmbient = renderSettings.m_imagePlaneLightingAmbient;
  preferences.imagePlaneLightingDiffuse = renderSettings.m_imagePlaneLightingDiffuse;
  preferences.imagePlaneLightingSpecular = renderSettings.m_imagePlaneLightingSpecular;
  preferences.imagePlaneLightingSpecularPower = renderSettings.m_imagePlaneLightingSpecularPower;
  preferences.lightingAmbient = renderSettings.m_lightingAmbient;
  preferences.lightingDiffuse = renderSettings.m_lightingDiffuse;
  preferences.lightingSpecular = renderSettings.m_lightingSpecular;
  preferences.lightingSpecularPower = renderSettings.m_lightingSpecularPower;
  preferences.meshFlatShadingEnabled = renderSettings.m_meshSurfaceMaterialSettings.flatShadingEnabled;
  preferences.meshTriangleEdgesEnabled = renderSettings.m_meshSurfaceMaterialSettings.triangleEdgesEnabled;
  preferences.meshTriangleEdgeColor = renderSettings.m_meshSurfaceMaterialSettings.triangleEdgeColor;
  preferences.meshPbrShadingEnabled = renderSettings.m_meshSurfaceMaterialSettings.pbrShadingEnabled;
  preferences.meshPbrMetallic = renderSettings.m_meshSurfaceMaterialSettings.metallic;
  preferences.meshPbrRoughness = renderSettings.m_meshSurfaceMaterialSettings.roughness;
  preferences.meshPbrAmbientOcclusion = renderSettings.m_meshSurfaceMaterialSettings.ambientOcclusion;
  preferences.renderFrontFaces = renderSettings.m_renderFrontFaces;
  preferences.renderBackFaces = renderSettings.m_renderBackFaces;
  preferences.reversePovRotation = renderSettings.m_reverseThreeDRotateAboutEye;
  preferences.synchronizeThreeDCameras = renderSettings.m_synchronizeThreeDCameras;
  preferences.showCrosshairsIn3D = renderSettings.m_showCrosshairsIn3D;
  preferences.crosshairs3DGlyphDiameterScenePercent = renderSettings.m_crosshairs3DGlyphDiameterScenePercent;
  preferences.crosshairs3DGlyphLengthScenePercent = renderSettings.m_crosshairs3DGlyphLengthScenePercent;
  preferences.showThreeDCameraFrustumIn2DViews = renderSettings.m_showThreeDCameraFrustumIn2DViews;
  preferences.threeDCameraFrustumColor = renderSettings.m_threeDCameraFrustumColor;
  preferences.smoothSegmentationMeshes = renderSettings.m_smoothSegmentationMeshes;
  preferences.smoothIsosurfaceMeshes = renderSettings.m_smoothIsosurfaceMeshes;
  preferences.meshSmoothingIterations = renderSettings.m_meshSmoothingIterations;
  preferences.meshSmoothingPassBand = renderSettings.m_meshSmoothingPassBand;
  preferences.meshPickingEnabled = renderSettings.m_meshPickingEnabled;
  preferences.meshCutawayEnabled = renderSettings.m_meshCutawayEnabled;
  preferences.meshShadowsEnabled = renderSettings.m_meshAdvancedLightingSettings.shadows.enabled;
  preferences.meshShadowMapSizePixels = renderSettings.m_meshAdvancedLightingSettings.shadows.mapSizePixels;
  preferences.meshShadowStrength = renderSettings.m_meshAdvancedLightingSettings.shadows.strength;
  preferences.meshShadowDepthBias = renderSettings.m_meshAdvancedLightingSettings.shadows.depthBias;
  preferences.meshAmbientOcclusionEnabled = renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.enabled;
  preferences.meshAmbientOcclusionRadiusMm = renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.radiusMm;
  preferences.meshAmbientOcclusionStrength = renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.strength;
  preferences.meshAmbientOcclusionPower = renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.power;
  preferences.meshAmbientOcclusionContrast = renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.contrast;
  preferences.meshAmbientOcclusionSampleCount =
    renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.sampleCount;
  preferences.meshRimLightingEnabled = renderSettings.m_meshSurfaceMaterialSettings.rimLightingEnabled;
  preferences.meshRimOpacityStrength = renderSettings.m_meshSurfaceMaterialSettings.rimOpacityStrength;
  preferences.meshRimEmissionStrength = renderSettings.m_meshSurfaceMaterialSettings.rimEmissionStrength;
  preferences.meshRimPower = renderSettings.m_meshSurfaceMaterialSettings.rimPower;
  preferences.ddpMaxPeelPasses = renderSettings.m_meshDdpSettings.maxPeelPasses;
  preferences.segmentationMasking =
    static_cast<user_preferences::RenderPreferences::SegMaskingForRaycasting>(renderSettings.m_segMasking);
  preferences.asciiEnabled = renderSettings.m_asciiEnabled;
  preferences.asciiCellSizePx = renderSettings.m_asciiCellSizePx;
  preferences.asciiCharsetIndex = renderSettings.m_asciiCharsetIndex;
  preferences.asciiForegroundColor = renderSettings.m_asciiFgColor;
  preferences.asciiBackgroundColor = renderSettings.m_asciiBgColor;
  preferences.asciiBackgroundAlpha = renderSettings.m_asciiBgAlpha;
  preferences.asciiUseColormapAsForeground = renderSettings.m_asciiUseColormap;
  preferences.asciiSpatialMatching = renderSettings.m_asciiSpatialMode;
  preferences.asciiSpatialExponent = renderSettings.m_asciiSpatialExponent;
  preferences.annotationsOnTop = renderSettings.m_globalAnnotationParams.renderOnTopOfAllImagePlanes;
  preferences.landmarksOnTop = renderSettings.m_globalLandmarkParams.renderOnTopOfAllImagePlanes;
  preferences.hideAnnotationVertices = renderSettings.m_globalAnnotationParams.hidePolygonVertices;
  return preferences;
}

void applyRenderPreferences(
  rendering::RenderSettings& renderSettings,
  const user_preferences::RenderPreferences& preferences)
{
  renderSettings.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections = preferences.showImageBorders;
  renderSettings.m_globalSliceIntersectionParams.renderInactiveImageViewIntersectionsInLightboxViews =
    preferences.showImageBorders && preferences.showImageBordersInLightboxViews;
  renderSettings.m_snapCrosshairs = preferences.crosshairsSnapping;
  renderSettings.m_crosshairsColor = preferences.crosshairsColor;
  renderSettings.m_showCrosshairs = preferences.showCrosshairs;
  renderSettings.m_showCrosshairsInLightboxViews =
    preferences.showCrosshairs && preferences.showCrosshairsInLightboxViews;
  renderSettings.m_showTransformationGuides = preferences.showTransformationGuides;
  renderSettings.m_transformationGuideColor = preferences.transformationGuideColor;
  renderSettings.m_2dBackgroundColor = preferences.background2dColor;
  renderSettings.m_3dBackgroundColor = preferences.background3dColor;
  renderSettings.m_anatomicalLabelColor = preferences.anatomicalLabelColor;
  renderSettings.m_showAnatomicalLabels = preferences.showAnatomicalLabels;
  renderSettings.m_showAnatomicalLabelsInLightboxViews =
    preferences.showAnatomicalLabels && preferences.showAnatomicalLabelsInLightboxViews;
  renderSettings.m_anatomicalLabelType = preferences.anatomicalLabelType;
  renderSettings.m_quadrupedBodyRegion = preferences.quadrupedBodyRegion;
  renderSettings.m_anatomicalLabelScale = std::clamp(preferences.anatomicalLabelScale, 0.5f, 2.0f);
  renderSettings.m_showScaleBars = preferences.showScaleBars;
  renderSettings.m_showScaleBarsInLightboxViews = preferences.showScaleBarsInLightboxViews;
  renderSettings.m_scaleBarColor = preferences.scaleBarColor;
  renderSettings.m_scaleBarPosition = preferences.scaleBarPosition;
  renderSettings.m_scaleBarOrientation = preferences.scaleBarOrientation;
  renderSettings.m_scaleBarTicks = preferences.scaleBarTicks;
  renderSettings.m_scaleBarTargetFraction = preferences.scaleBarTargetFraction;
  renderSettings.m_scaleBarMarginPx = preferences.scaleBarMarginPx;
  renderSettings.m_showLightboxOffsetLabels = preferences.showLightboxOffsetLabels;
  renderSettings.m_lightboxOffsetLabelColor = preferences.lightboxOffsetLabelColor;
  renderSettings.m_imageGrayFloatingPointInterpolationPolicy = preferences.floatingPointLinearInterpolationPolicy;
  renderSettings.m_doMaxExtentIntensityProjection = preferences.useMaximumIntensityProjectionExtent;
  renderSettings.m_intensityProjectionSlabThickness = preferences.intensityProjectionSlabThicknessMm;
  renderSettings.setXrayEnergy(preferences.xrayEnergyKeV);
  renderSettings.m_xrayIntensityWindow = preferences.xrayWindow;
  renderSettings.m_xrayIntensityLevel = preferences.xrayLevel;
  renderSettings.m_isocontourFloatingPointInterpolationPolicy = preferences.isocontourFloatingPointInterpolationPolicy;
  renderSettings.m_modulateSegmentationOpacityWithImageOpacity2d =
    preferences.modulateSegmentationOpacityWithImageOpacity2d;
  renderSettings.m_modulateSegmentationOpacityWithImageOpacity3d =
    preferences.modulateSegmentationOpacityWithImageOpacity3d;
  renderSettings.m_segOutlineStyle = preferences.segmentationOutlineStyle;
  renderSettings.m_segInteriorOpacity = preferences.segmentationInteriorOpacity;
  renderSettings.m_segInterpCutoff = preferences.segmentationErosionFactor;
  renderSettings.m_useSquare = preferences.squaredDifference;
  renderSettings.m_squaredDifferenceParams.m_colorMapIndex = preferences.squaredDifferenceMetric.colorMapIndex;
  renderSettings.m_squaredDifferenceParams.m_slopeIntercept = preferences.squaredDifferenceMetric.slopeIntercept;
  renderSettings.m_squaredDifferenceParams.m_invertCmap = preferences.squaredDifferenceMetric.invertColormap;
  renderSettings.m_squaredDifferenceParams.m_cmapContinuous = preferences.squaredDifferenceMetric.continuousColormap;
  renderSettings.m_squaredDifferenceParams.m_cmapQuantizationLevels =
    preferences.squaredDifferenceMetric.colormapLevels;
  renderSettings.m_localNccParams.m_colorMapIndex = preferences.localNccMetric.colorMapIndex;
  renderSettings.m_localNccParams.m_slopeIntercept = preferences.localNccMetric.slopeIntercept;
  renderSettings.m_localNccParams.m_invertCmap = preferences.localNccMetric.invertColormap;
  renderSettings.m_localNccParams.m_cmapContinuous = preferences.localNccMetric.continuousColormap;
  renderSettings.m_localNccParams.m_cmapQuantizationLevels = preferences.localNccMetric.colormapLevels;
  renderSettings.m_localNccPatchRadius = preferences.localNccPatchRadius;
  renderSettings.m_localNccSampleSpacing = preferences.localNccSampleSpacing;
  renderSettings.m_localNccMinValidFraction = preferences.localNccMinValidFraction;
  renderSettings.m_localNccVarianceEpsilon = preferences.localNccVarianceEpsilon;
  renderSettings.m_localNccIgnoreNegativeCorrelation = preferences.localNccIgnoreNegativeCorrelation;
  renderSettings.m_localNccPresentation = localNccPresentationToRenderSettings(preferences.localNccPresentation);
  renderSettings.m_localNccInvalidStyle = localNccInvalidStyleToRenderSettings(preferences.localNccInvalidStyle);
  renderSettings.m_localLinearResidualParams.m_colorMapIndex = preferences.localLinearResidualMetric.colorMapIndex;
  renderSettings.m_localLinearResidualParams.m_slopeIntercept = preferences.localLinearResidualMetric.slopeIntercept;
  renderSettings.m_localLinearResidualParams.m_invertCmap = preferences.localLinearResidualMetric.invertColormap;
  renderSettings.m_localLinearResidualParams.m_cmapContinuous =
    preferences.localLinearResidualMetric.continuousColormap;
  renderSettings.m_localLinearResidualParams.m_cmapQuantizationLevels =
    preferences.localLinearResidualMetric.colormapLevels;
  renderSettings.m_localLinearResidualPatchRadius = preferences.localLinearResidualPatchRadius;
  renderSettings.m_localLinearResidualSampleSpacing = preferences.localLinearResidualSampleSpacing;
  renderSettings.m_localLinearResidualMinValidFraction = preferences.localLinearResidualMinValidFraction;
  renderSettings.m_localLinearResidualVarianceEpsilon = preferences.localLinearResidualVarianceEpsilon;
  renderSettings.m_localLinearResidualInvalidStyle =
    localNccInvalidStyleToRenderSettings(preferences.localLinearResidualInvalidStyle);
  renderSettings.m_overlayMagentaCyan = preferences.overlayMagentaCyan;
  renderSettings.m_quadrants = preferences.quadrants;
  renderSettings.m_numCheckerboardSquares = preferences.checkerboardSquares;
  renderSettings.m_flashlightRadius = preferences.flashlightRadiusFraction;
  renderSettings.m_flashlightOverlays = preferences.flashlightOverlayMovingImage;
  renderSettings.m_manualFramerateLimiter = preferences.limitFrameRate;
  renderSettings.m_targetFrameTimeSeconds = preferences.targetFrameTimeSeconds;
  renderSettings.m_raycastSamplingFactor = std::clamp(preferences.raycastSamplingFactor, 0.5f, 2.0f);
  renderSettings.m_useDistanceMapForRaycasting = preferences.useDistanceMapForRaycasting;
  renderSettings.m_distanceMapForegroundLowerPercentile =
    std::clamp(preferences.distanceMapForegroundLowerPercentile, 0.0f, 1.0f);
  renderSettings.m_distanceMapForegroundUpperPercentile =
    std::clamp(preferences.distanceMapForegroundUpperPercentile, 0.0f, 1.0f);
  renderSettings.m_adaptiveRaycastSamplingEnabled = false;
  renderSettings.m_adaptiveRaycastTargetFrameRate = 30.0f;
  renderSettings.m_adaptiveRaycastEffectiveSamplingFactor = std::clamp(preferences.raycastSamplingFactor, 0.5f, 2.0f);
  renderSettings.m_3dTransparentIfNoHit = preferences.transparent3DBackground;
  renderSettings.m_raycastBackgroundEdgeBrighteningEnabled = preferences.imageBoxVisible;
  renderSettings.m_showImagePlanesIn3D = preferences.showImagePlanesIn3D;
  renderSettings.m_imagePlaneOpacity = std::clamp(preferences.imagePlaneOpacity, 0.0f, 1.0f);
  renderSettings.m_modulateImagePlaneOpacityWithViewAngle = preferences.modulateImagePlaneOpacityWithViewAngle;
  renderSettings.m_showSegmentationsOnImagePlanesIn3D = preferences.showSegmentationsOnImagePlanesIn3D;
  renderSettings.m_showIsocontoursOnImagePlanesIn3D = preferences.showIsocontoursOnImagePlanesIn3D;
  renderSettings.m_shadeImagePlanesIn3D = preferences.shadeImagePlanesIn3D;
  renderSettings.m_imagePlaneLightingAmbient = preferences.imagePlaneLightingAmbient;
  renderSettings.m_imagePlaneLightingDiffuse = preferences.imagePlaneLightingDiffuse;
  renderSettings.m_imagePlaneLightingSpecular = preferences.imagePlaneLightingSpecular;
  renderSettings.m_imagePlaneLightingSpecularPower = preferences.imagePlaneLightingSpecularPower;
  renderSettings.m_lightingAmbient = preferences.lightingAmbient;
  renderSettings.m_lightingDiffuse = preferences.lightingDiffuse;
  renderSettings.m_lightingSpecular = preferences.lightingSpecular;
  renderSettings.m_lightingSpecularPower = preferences.lightingSpecularPower;
  renderSettings.m_meshSurfaceMaterialSettings.flatShadingEnabled =
    preferences.meshFlatShadingEnabled || preferences.meshTriangleEdgesEnabled;
  renderSettings.m_meshSurfaceMaterialSettings.triangleEdgesEnabled = preferences.meshTriangleEdgesEnabled;
  renderSettings.m_meshSurfaceMaterialSettings.triangleEdgeColor =
    glm::clamp(preferences.meshTriangleEdgeColor, glm::vec3{0.0f}, glm::vec3{1.0f});
  renderSettings.m_meshSurfaceMaterialSettings.pbrShadingEnabled = preferences.meshPbrShadingEnabled;
  renderSettings.m_meshSurfaceMaterialSettings.metallic = preferences.meshPbrMetallic;
  renderSettings.m_meshSurfaceMaterialSettings.roughness = preferences.meshPbrRoughness;
  renderSettings.m_meshSurfaceMaterialSettings.ambientOcclusion = preferences.meshPbrAmbientOcclusion;
  renderSettings.m_renderFrontFaces = preferences.renderFrontFaces;
  renderSettings.m_renderBackFaces = preferences.renderBackFaces;
  renderSettings.m_reverseThreeDRotateAboutEye = preferences.reversePovRotation;
  renderSettings.m_synchronizeThreeDCameras = preferences.synchronizeThreeDCameras;
  renderSettings.m_showCrosshairsIn3D = preferences.showCrosshairsIn3D;
  renderSettings.m_crosshairs3DGlyphDiameterScenePercent = preferences.crosshairs3DGlyphDiameterScenePercent;
  renderSettings.m_crosshairs3DGlyphLengthScenePercent = preferences.crosshairs3DGlyphLengthScenePercent;
  renderSettings.m_showThreeDCameraFrustumIn2DViews = preferences.showThreeDCameraFrustumIn2DViews;
  renderSettings.m_threeDCameraFrustumColor = preferences.threeDCameraFrustumColor;
  renderSettings.m_smoothSegmentationMeshes = preferences.smoothSegmentationMeshes;
  renderSettings.m_smoothIsosurfaceMeshes = preferences.smoothIsosurfaceMeshes;
  renderSettings.m_meshSmoothingIterations = std::clamp(preferences.meshSmoothingIterations, 1u, 1000u);
  renderSettings.m_meshSmoothingPassBand = std::clamp(preferences.meshSmoothingPassBand, 0.001f, 2.0f);
  renderSettings.m_meshPickingEnabled = preferences.meshPickingEnabled;
  renderSettings.m_meshCutawayEnabled = preferences.meshCutawayEnabled;
  renderSettings.m_meshAdvancedLightingSettings.shadows.enabled = preferences.meshShadowsEnabled;
  renderSettings.m_meshAdvancedLightingSettings.shadows.mapSizePixels = preferences.meshShadowMapSizePixels;
  renderSettings.m_meshAdvancedLightingSettings.shadows.strength = preferences.meshShadowStrength;
  renderSettings.m_meshAdvancedLightingSettings.shadows.depthBias = preferences.meshShadowDepthBias;
  renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.enabled = preferences.meshAmbientOcclusionEnabled;
  renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.radiusMm = preferences.meshAmbientOcclusionRadiusMm;
  renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.strength = preferences.meshAmbientOcclusionStrength;
  renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.power = preferences.meshAmbientOcclusionPower;
  renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.contrast = preferences.meshAmbientOcclusionContrast;
  renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.sampleCount =
    preferences.meshAmbientOcclusionSampleCount;
  renderSettings.m_meshSurfaceMaterialSettings.rimLightingEnabled = preferences.meshRimLightingEnabled;
  renderSettings.m_meshSurfaceMaterialSettings.rimOpacityStrength = preferences.meshRimOpacityStrength;
  renderSettings.m_meshSurfaceMaterialSettings.rimEmissionStrength = preferences.meshRimEmissionStrength;
  renderSettings.m_meshSurfaceMaterialSettings.rimPower = preferences.meshRimPower;
  renderSettings.m_meshDdpSettings.maxPeelPasses = std::clamp<uint32_t>(preferences.ddpMaxPeelPasses, 1u, 32u);
  renderSettings.m_segMasking =
    static_cast<rendering::RenderSettings::SegMaskingForRaycasting>(preferences.segmentationMasking);
  renderSettings.m_asciiEnabled = preferences.asciiEnabled;
  renderSettings.m_asciiCellSizePx = preferences.asciiCellSizePx;
  renderSettings.m_asciiCharsetIndex = preferences.asciiCharsetIndex;
  renderSettings.m_asciiFgColor = preferences.asciiForegroundColor;
  renderSettings.m_asciiBgColor = preferences.asciiBackgroundColor;
  renderSettings.m_asciiBgAlpha = preferences.asciiBackgroundAlpha;
  renderSettings.m_asciiUseColormap = preferences.asciiUseColormapAsForeground;
  renderSettings.m_asciiSpatialMode = preferences.asciiSpatialMatching;
  renderSettings.m_asciiSpatialExponent = preferences.asciiSpatialExponent;
  renderSettings.m_asciiAtlasNeedsRebuild = true;
  renderSettings.m_globalAnnotationParams.renderOnTopOfAllImagePlanes = preferences.annotationsOnTop;
  renderSettings.m_globalLandmarkParams.renderOnTopOfAllImagePlanes = preferences.landmarksOnTop;
  renderSettings.m_globalAnnotationParams.hidePolygonVertices = preferences.hideAnnotationVertices;
}

void preserveProjectRenderPreferences(
  user_preferences::RenderPreferences& preferences,
  const user_preferences::RenderPreferences& currentPreferences)
{
  user_preferences::preserveProjectOwnedRenderPreferences(preferences, currentPreferences);
}

user_preferences::RenderPreferences applicationRenderPreferencesFromRenderSettings(
  const rendering::RenderSettings& renderSettings)
{
  return user_preferences::applicationRenderPreferences(renderPreferencesFromRenderSettings(renderSettings));
}

} // namespace

namespace user_preferences
{

std::string
toJsonString(const AppSettings& settings, const rendering::RenderSettings& renderSettings, const GuiData& guiData)
{
  return toJsonString(
    settings,
    applicationRenderPreferencesFromRenderSettings(renderSettings),
    precisionPreferencesFromGuiData(guiData));
}

void markSavedAppSettingsState(
  const AppSettings& settings,
  const rendering::RenderSettings& renderSettings,
  GuiData& guiData)
{
  guiData.m_savedAppSettingsJson = toJsonString(settings, renderSettings, guiData);
  guiData.m_appSettingsDirty = false;
}

void updateAppSettingsDirtyState(
  const AppSettings& settings,
  const rendering::RenderSettings& renderSettings,
  GuiData& guiData)
{
  if (guiData.m_savedAppSettingsJson.empty()) {
    markSavedAppSettingsState(settings, renderSettings, guiData);
    return;
  }

  guiData.m_appSettingsDirty = toJsonString(settings, renderSettings, guiData) != guiData.m_savedAppSettingsJson;
}

bool applyJsonString(
  AppSettings& settings,
  rendering::RenderSettings& renderSettings,
  GuiData& guiData,
  const std::string& text,
  std::string* error)
{
  RenderPreferences renderPreferences = renderPreferencesFromRenderSettings(renderSettings);
  PrecisionPreferences precisionPreferences = precisionPreferencesFromGuiData(guiData);
  if (!applyJsonString(settings, renderPreferences, precisionPreferences, text, error)) {
    return false;
  }

  preserveProjectRenderPreferences(renderPreferences, renderPreferencesFromRenderSettings(renderSettings));
  applyRenderPreferences(renderSettings, renderPreferences);
  applyPrecisionPreferences(guiData, precisionPreferences);
  return true;
}

bool save(
  const AppSettings& settings,
  const rendering::RenderSettings& renderSettings,
  const GuiData& guiData,
  const std::filesystem::path& fileName,
  std::string* error)
{
  return save(
    settings,
    applicationRenderPreferencesFromRenderSettings(renderSettings),
    precisionPreferencesFromGuiData(guiData),
    fileName,
    error);
}

bool load(
  AppSettings& settings,
  rendering::RenderSettings& renderSettings,
  GuiData& guiData,
  const std::filesystem::path& fileName,
  std::string* error)
{
  std::error_code ec;
  if (!std::filesystem::exists(fileName, ec)) {
    return true;
  }

  RenderPreferences renderPreferences = renderPreferencesFromRenderSettings(renderSettings);
  PrecisionPreferences precisionPreferences = precisionPreferencesFromGuiData(guiData);
  const bool loaded = load(settings, renderPreferences, precisionPreferences, fileName, error);
  if (loaded) {
    preserveProjectRenderPreferences(renderPreferences, renderPreferencesFromRenderSettings(renderSettings));
    applyRenderPreferences(renderSettings, renderPreferences);
    applyPrecisionPreferences(guiData, precisionPreferences);
  }
  return loaded;
}

void applyDefaults(AppSettings& settings, rendering::RenderSettings& renderSettings, GuiData& guiData)
{
  const bool synchronizeTimeSeries = settings.synchronizeTimeSeries();
  const bool lockAnatomicalDirections = settings.lockAnatomicalCoordinateAxesWithReferenceImage();
  const user_preferences::RenderPreferences currentRenderPreferences =
    renderPreferencesFromRenderSettings(renderSettings);
  user_preferences::RenderPreferences defaultPreferences = defaultRenderPreferences();
  preserveProjectRenderPreferences(defaultPreferences, currentRenderPreferences);

  settings = AppSettings{};
  settings.setSynchronizeTimeSeries(synchronizeTimeSeries);
  settings.setLockAnatomicalCoordinateAxesWithReferenceImage(lockAnatomicalDirections);
  applyRenderPreferences(renderSettings, defaultPreferences);
  applyPrecisionPreferences(guiData, PrecisionPreferences{});
  logging::setApplicationLogLevel(logging::defaultLogLevel());
  logging::setLoggingEnabled(true);
}

} // namespace user_preferences
