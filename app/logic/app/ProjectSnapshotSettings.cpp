#include "logic/app/ProjectSnapshotSettings.h"

#include "common/Types.h"
#include "image/Image.h"
#include "image/ImageDerivedData.h"
#include "image/ImageHeader.h"
#include "image/ImageTimeAxis.h"
#include "logic/app/Data.h"
#include "logic/app/ParcellationLabelTable.h"
#include "logic/app/Settings.h"
#include "rendering/RenderData.h"
#include "rendering/mesh/MeshAdvancedLighting.h"
#include "rendering/mesh/MeshDdpPolicy.h"
#include "ui/GuiData.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
GuiData::LayoutTabPlacement guiLayoutTabPlacement(UiLayoutTabPlacement placement)
{
  return UiLayoutTabPlacement::Bottom == placement ? GuiData::LayoutTabPlacement::Bottom
                                                   : GuiData::LayoutTabPlacement::Top;
}

template<typename T>
void setDiffValue(
  std::vector<T>& values,
  std::set<std::size_t>& valueIndices,
  const std::size_t index,
  const T& value,
  const T& fillValue)
{
  if (values.size() <= index) {
    values.resize(index + 1u, fillValue);
  }
  values[index] = value;
  valueIndices.insert(index);
}

template<typename T>
void addDiffValue(
  std::vector<T>& values,
  std::set<std::size_t>& valueIndices,
  const std::size_t index,
  const T& value,
  const T& baseline,
  const T& fillValue)
{
  if (value != baseline) {
    setDiffValue(values, valueIndices, index, value, fillValue);
  }
}

bool shouldApplySparseComponentValue(const std::set<std::size_t>& valueIndices, const std::size_t component)
{
  return valueIndices.empty() || valueIndices.contains(component);
}

std::size_t defaultLabelCountForSegmentation(const Image& seg)
{
  constexpr auto minNumLabels = static_cast<int64_t>(256);
  const int64_t maxLabelInSeg = static_cast<int64_t>(seg.settings().componentStatistics().onlineStats.max);
  const int64_t requiredLabels = maxLabelInSeg + 1;

  return static_cast<std::size_t>(std::min(
    std::max(requiredLabels, minNumLabels),
    static_cast<int64_t>(ParcellationLabelTable::labelCountUpperBound())));
}

glm::vec4 normalizedLabelColor(const ParcellationLabelTable& table, const std::size_t index)
{
  const glm::u8vec3 color = table.getColor(index);
  return glm::vec4{
    static_cast<float>(color.r) / 255.0f,
    static_cast<float>(color.g) / 255.0f,
    static_cast<float>(color.b) / 255.0f,
    static_cast<float>(table.getAlpha(index)) / 255.0f};
}

glm::u8vec3 labelColorFromNormalized(const glm::vec4& color)
{
  const glm::vec4 clamped = glm::clamp(color, glm::vec4{0.0f}, glm::vec4{1.0f});
  return glm::u8vec3{
    static_cast<uint8_t>(std::lround(clamped.r * 255.0f)),
    static_cast<uint8_t>(std::lround(clamped.g * 255.0f)),
    static_cast<uint8_t>(std::lround(clamped.b * 255.0f))};
}

uint8_t labelAlphaFromNormalized(const glm::vec4& color)
{
  return static_cast<uint8_t>(std::lround(glm::clamp(color.a, 0.0f, 1.0f) * 255.0f));
}

bool labelEntryMatches(const ParcellationLabelTable& table, const ParcellationLabelTable& baseline, std::size_t index)
{
  return table.getName(index) == baseline.getName(index) && table.getColor(index) == baseline.getColor(index) &&
         table.getAlpha(index) == baseline.getAlpha(index) && table.getVisible(index) == baseline.getVisible(index) &&
         table.getShowMesh(index) == baseline.getShowMesh(index);
}

std::optional<serialize::SegmentationLabels> segmentationLabels(const Image& seg, const ParcellationLabelTable* table)
{
  if (!table) {
    return std::nullopt;
  }

  serialize::SegmentationLabels labels;
  labels.m_count = table->numLabels();

  const std::size_t defaultCount = defaultLabelCountForSegmentation(seg);
  const ParcellationLabelTable baseline(labels.m_count, table->maxNumLabels());

  for (std::size_t index = 0; index < table->numLabels(); ++index) {
    if (labelEntryMatches(*table, baseline, index)) {
      continue;
    }

    labels.m_values.push_back(serialize::SegmentationLabel{
      .m_index = index,
      .m_name = table->getName(index),
      .m_color = normalizedLabelColor(*table, index),
      .m_visible = table->getVisible(index),
      .m_showMesh = table->getShowMesh(index)});
  }

  if (labels.m_count == defaultCount && labels.m_values.empty()) {
    return std::nullopt;
  }
  return labels;
}

void applySegmentationLabels(AppData& appData, Image& seg, const serialize::SegmentationLabels& labels)
{
  if (seg.settings().numComponents() == 0) {
    return;
  }

  const auto tableUid = appData.labelTableUid(seg.settings().labelTableIndex(0));
  ParcellationLabelTable* table = tableUid ? appData.labelTable(*tableUid) : nullptr;
  if (!table) {
    return;
  }

  if (labels.m_count > table->numLabels()) {
    table->addLabels(labels.m_count - table->numLabels());
  }

  for (const auto& label : labels.m_values) {
    if (label.m_index >= table->numLabels()) {
      continue;
    }

    table->setName(label.m_index, label.m_name);
    table->setColor(label.m_index, labelColorFromNormalized(label.m_color));
    table->setAlpha(label.m_index, labelAlphaFromNormalized(label.m_color));
    table->setVisible(label.m_index, label.m_visible);
    table->setShowMesh(label.m_index, label.m_showMesh);
  }
}

} // namespace

namespace project_snapshot
{
// Project-owned synchronization and view settings.

void syncLayoutTabGuiData(AppData& appData)
{
  appData.guiData().m_showLayoutTabs = appData.settings().showLayoutTabs();
  appData.guiData().m_layoutTabPlacement = guiLayoutTabPlacement(appData.settings().layoutTabPlacement());
}

serialize::ProjectSynchronizationSettings synchronizationSettings(const AppData& appData)
{
  return serialize::ProjectSynchronizationSettings{
    .m_synchronizeTimeSeries = appData.settings().synchronizeTimeSeries()};
}

void applySynchronizationSettings(AppData& appData, const serialize::ProjectSynchronizationSettings& settings)
{
  appData.settings().setSynchronizeTimeSeries(settings.m_synchronizeTimeSeries);
  syncLayoutTabGuiData(appData);
}

serialize::ProjectViewSettings viewSettings(const AppData& appData)
{
  return serialize::ProjectViewSettings{
    .m_showImageBorders = appData.renderData().m_globalSliceIntersectionParams.renderInactiveImageViewIntersections,
    .m_showImageBordersInLightboxViews =
      appData.renderData().m_globalSliceIntersectionParams.renderInactiveImageViewIntersectionsInLightboxViews,
    .m_showCrosshairs = appData.renderData().m_showCrosshairs,
    .m_showCrosshairsInLightboxViews = appData.renderData().m_showCrosshairsInLightboxViews,
    .m_showAnatomicalLabels = appData.renderData().m_showAnatomicalLabels,
    .m_showAnatomicalLabelsInLightboxViews = appData.renderData().m_showAnatomicalLabelsInLightboxViews,
    .m_showScaleBars = appData.renderData().m_showScaleBars,
    .m_showScaleBarsInLightboxViews = appData.renderData().m_showScaleBarsInLightboxViews,
    .m_annotationsOnTop = appData.renderData().m_globalAnnotationParams.renderOnTopOfAllImagePlanes,
    .m_landmarksOnTop = appData.renderData().m_globalLandmarkParams.renderOnTopOfAllImagePlanes,
    .m_hideAnnotationVertices = appData.renderData().m_globalAnnotationParams.hidePolygonVertices,
    .m_anatomicalLabelType = appData.renderData().m_anatomicalLabelType,
    .m_lockAnatomicalDirectionsToReferenceImage = appData.settings().lockAnatomicalCoordinateAxesWithReferenceImage(),
    .m_crosshairsSnapping = appData.renderData().m_snapCrosshairs};
}

void applyViewSettings(AppData& appData, const serialize::ProjectViewSettings& settings)
{
  appData.renderData().m_globalSliceIntersectionParams.renderInactiveImageViewIntersections =
    settings.m_showImageBorders;
  appData.renderData().m_globalSliceIntersectionParams.renderInactiveImageViewIntersectionsInLightboxViews =
    settings.m_showImageBorders && settings.m_showImageBordersInLightboxViews;
  appData.renderData().m_showCrosshairs = settings.m_showCrosshairs;
  appData.renderData().m_showCrosshairsInLightboxViews =
    settings.m_showCrosshairs && settings.m_showCrosshairsInLightboxViews;
  appData.renderData().m_showAnatomicalLabels = settings.m_showAnatomicalLabels;
  appData.renderData().m_showAnatomicalLabelsInLightboxViews =
    settings.m_showAnatomicalLabels && settings.m_showAnatomicalLabelsInLightboxViews;
  appData.renderData().m_showScaleBars = settings.m_showScaleBars;
  appData.renderData().m_showScaleBarsInLightboxViews =
    settings.m_showScaleBars && settings.m_showScaleBarsInLightboxViews;
  appData.renderData().m_globalAnnotationParams.renderOnTopOfAllImagePlanes = settings.m_annotationsOnTop;
  appData.renderData().m_globalLandmarkParams.renderOnTopOfAllImagePlanes = settings.m_landmarksOnTop;
  appData.renderData().m_globalAnnotationParams.hidePolygonVertices = settings.m_hideAnnotationVertices;
  appData.renderData().m_anatomicalLabelType = settings.m_anatomicalLabelType;
  appData.settings().setLockAnatomicalCoordinateAxesWithReferenceImage(
    settings.m_lockAnatomicalDirectionsToReferenceImage);
  appData.renderData().m_snapCrosshairs = settings.m_crosshairsSnapping;
}

// Project-owned comparison metric and mode settings.

serialize::ProjectMetricSettings metricSettings(const RenderData::MetricParams& params)
{
  return serialize::ProjectMetricSettings{
    .m_colorMapIndex = params.m_colorMapIndex,
    .m_slopeIntercept = params.m_slopeIntercept,
    .m_invertColormap = params.m_invertCmap,
    .m_continuousColormap = params.m_cmapContinuous,
    .m_colormapLevels = params.m_cmapQuantizationLevels};
}

void applyMetricSettings(RenderData::MetricParams& params, const serialize::ProjectMetricSettings& settings)
{
  params.m_colorMapIndex = settings.m_colorMapIndex;
  params.m_slopeIntercept = settings.m_slopeIntercept;
  params.m_invertCmap = settings.m_invertColormap;
  params.m_cmapContinuous = settings.m_continuousColormap;
  params.m_cmapQuantizationLevels = settings.m_colormapLevels;
}

serialize::ProjectLocalNccPresentation localNccPresentation(RenderData::LocalNccPresentation presentation)
{
  return RenderData::LocalNccPresentation::Correlation == presentation
           ? serialize::ProjectLocalNccPresentation::Correlation
           : serialize::ProjectLocalNccPresentation::Dissimilarity;
}

RenderData::LocalNccPresentation localNccPresentation(serialize::ProjectLocalNccPresentation presentation)
{
  return serialize::ProjectLocalNccPresentation::Correlation == presentation
           ? RenderData::LocalNccPresentation::Correlation
           : RenderData::LocalNccPresentation::Dissimilarity;
}

serialize::ProjectLocalMetricInvalidStyle localMetricInvalidStyle(RenderData::LocalNccInvalidStyle style)
{
  return RenderData::LocalNccInvalidStyle::Gray == style ? serialize::ProjectLocalMetricInvalidStyle::Gray
                                                         : serialize::ProjectLocalMetricInvalidStyle::Transparent;
}

RenderData::LocalNccInvalidStyle localMetricInvalidStyle(serialize::ProjectLocalMetricInvalidStyle style)
{
  return serialize::ProjectLocalMetricInvalidStyle::Gray == style ? RenderData::LocalNccInvalidStyle::Gray
                                                                  : RenderData::LocalNccInvalidStyle::Transparent;
}

serialize::ProjectSegmentationRaycastMasking raycastSegmentationMasking(RenderData::SegMaskingForRaycasting masking)
{
  switch (masking) {
    case RenderData::SegMaskingForRaycasting::SegMasksIn:
      return serialize::ProjectSegmentationRaycastMasking::MaskIn;
    case RenderData::SegMaskingForRaycasting::SegMasksOut:
      return serialize::ProjectSegmentationRaycastMasking::MaskOut;
    case RenderData::SegMaskingForRaycasting::Disabled:
      return serialize::ProjectSegmentationRaycastMasking::Disabled;
  }

  return serialize::ProjectSegmentationRaycastMasking::Disabled;
}

RenderData::SegMaskingForRaycasting raycastSegmentationMasking(serialize::ProjectSegmentationRaycastMasking masking)
{
  switch (masking) {
    case serialize::ProjectSegmentationRaycastMasking::MaskIn:
      return RenderData::SegMaskingForRaycasting::SegMasksIn;
    case serialize::ProjectSegmentationRaycastMasking::MaskOut:
      return RenderData::SegMaskingForRaycasting::SegMasksOut;
    case serialize::ProjectSegmentationRaycastMasking::Disabled:
      return RenderData::SegMaskingForRaycasting::Disabled;
  }

  return RenderData::SegMaskingForRaycasting::Disabled;
}

serialize::ProjectComparisonSettings comparisonSettings(const AppData& appData)
{
  const auto& renderData = appData.renderData();
  return serialize::ProjectComparisonSettings{
    .m_difference =
      serialize::ProjectDifferenceMetricSettings{
        .m_squared = renderData.m_useSquare,
        .m_metric = metricSettings(renderData.m_squaredDifferenceParams)},
    .m_localNcc =
      serialize::ProjectLocalNccMetricSettings{
        .m_metric = metricSettings(renderData.m_localNccParams),
        .m_presentation = localNccPresentation(renderData.m_localNccPresentation),
        .m_negativeCorrelationAsMismatch = renderData.m_localNccIgnoreNegativeCorrelation,
        .m_patchRadius = renderData.m_localNccPatchRadius,
        .m_sampleSpacing = renderData.m_localNccSampleSpacing,
        .m_minimumValidFraction = renderData.m_localNccMinValidFraction,
        .m_varianceEpsilon = renderData.m_localNccVarianceEpsilon,
        .m_invalidStyle = localMetricInvalidStyle(renderData.m_localNccInvalidStyle)},
    .m_localLinearResidual =
      serialize::ProjectLocalLinearResidualMetricSettings{
        .m_metric = metricSettings(renderData.m_localLinearResidualParams),
        .m_patchRadius = renderData.m_localLinearResidualPatchRadius,
        .m_sampleSpacing = renderData.m_localLinearResidualSampleSpacing,
        .m_minimumValidFraction = renderData.m_localLinearResidualMinValidFraction,
        .m_varianceEpsilon = renderData.m_localLinearResidualVarianceEpsilon,
        .m_invalidStyle = localMetricInvalidStyle(renderData.m_localLinearResidualInvalidStyle)},
    .m_overlayMagentaCyan = renderData.m_overlayMagentaCyan,
    .m_quadrants = renderData.m_quadrants,
    .m_checkerboardSquares = renderData.m_numCheckerboardSquares,
    .m_flashlightRadiusFraction = renderData.m_flashlightRadius,
    .m_flashlightOverlayMovingImage = renderData.m_flashlightOverlays};
}

void applyComparisonSettings(AppData& appData, const serialize::ProjectComparisonSettings& settings)
{
  auto& renderData = appData.renderData();
  renderData.m_useSquare = settings.m_difference.m_squared;
  applyMetricSettings(renderData.m_squaredDifferenceParams, settings.m_difference.m_metric);

  applyMetricSettings(renderData.m_localNccParams, settings.m_localNcc.m_metric);
  renderData.m_localNccPresentation = localNccPresentation(settings.m_localNcc.m_presentation);
  renderData.m_localNccIgnoreNegativeCorrelation = settings.m_localNcc.m_negativeCorrelationAsMismatch;
  renderData.m_localNccPatchRadius = settings.m_localNcc.m_patchRadius;
  renderData.m_localNccSampleSpacing = settings.m_localNcc.m_sampleSpacing;
  renderData.m_localNccMinValidFraction = settings.m_localNcc.m_minimumValidFraction;
  renderData.m_localNccVarianceEpsilon = settings.m_localNcc.m_varianceEpsilon;
  renderData.m_localNccInvalidStyle = localMetricInvalidStyle(settings.m_localNcc.m_invalidStyle);

  applyMetricSettings(renderData.m_localLinearResidualParams, settings.m_localLinearResidual.m_metric);
  renderData.m_localLinearResidualPatchRadius = settings.m_localLinearResidual.m_patchRadius;
  renderData.m_localLinearResidualSampleSpacing = settings.m_localLinearResidual.m_sampleSpacing;
  renderData.m_localLinearResidualMinValidFraction = settings.m_localLinearResidual.m_minimumValidFraction;
  renderData.m_localLinearResidualVarianceEpsilon = settings.m_localLinearResidual.m_varianceEpsilon;
  renderData.m_localLinearResidualInvalidStyle = localMetricInvalidStyle(settings.m_localLinearResidual.m_invalidStyle);

  renderData.m_overlayMagentaCyan = settings.m_overlayMagentaCyan;
  renderData.m_quadrants = settings.m_quadrants;
  renderData.m_numCheckerboardSquares = settings.m_checkerboardSquares;
  renderData.m_flashlightRadius = settings.m_flashlightRadiusFraction;
  renderData.m_flashlightOverlays = settings.m_flashlightOverlayMovingImage;
}

// Project-owned rendering presentation settings.

serialize::ProjectThreeDRenderingSettings threeDRenderingSettings(const AppData& appData)
{
  const auto& renderData = appData.renderData();
  return serialize::ProjectThreeDRenderingSettings{
    .m_transparentBackground = renderData.m_3dTransparentIfNoHit,
    .m_imageBoxVisible = renderData.m_raycastBackgroundEdgeBrighteningEnabled,
    .m_imagePlanesVisible = renderData.m_showImagePlanesIn3D,
    .m_imagePlaneViewAngleOpacity = renderData.m_modulateImagePlaneOpacityWithViewAngle,
    .m_imagePlaneSegmentationsVisible = renderData.m_showSegmentationsOnImagePlanesIn3D,
    .m_imagePlaneShading = renderData.m_shadeImagePlanesIn3D,
    .m_imagePlaneLightingAmbient = renderData.m_imagePlaneLightingAmbient,
    .m_imagePlaneLightingDiffuse = renderData.m_imagePlaneLightingDiffuse,
    .m_imagePlaneLightingSpecular = renderData.m_imagePlaneLightingSpecular,
    .m_imagePlaneLightingSpecularPower = renderData.m_imagePlaneLightingSpecularPower,
    .m_lightingAmbient = renderData.m_lightingAmbient,
    .m_lightingDiffuse = renderData.m_lightingDiffuse,
    .m_lightingSpecular = renderData.m_lightingSpecular,
    .m_lightingSpecularPower = renderData.m_lightingSpecularPower,
    .m_showCrosshairsIn3D = renderData.m_showCrosshairsIn3D,
    .m_crosshairs3DGlyphDiameterVoxelDiagonals = renderData.m_crosshairs3DGlyphDiameterVoxelDiagonals,
    .m_crosshairs3DGlyphLengthVoxelDiagonals = renderData.m_crosshairs3DGlyphLengthVoxelDiagonals,
    .m_showThreeDCameraFrustumIn2DViews = renderData.m_showThreeDCameraFrustumIn2DViews,
    .m_reverseThreeDRotateAboutEye = renderData.m_reverseThreeDRotateAboutEye,
    .m_threeDCameraFrustumColor = renderData.m_threeDCameraFrustumColor};
}

void applyThreeDRenderingSettings(AppData& appData, const serialize::ProjectThreeDRenderingSettings& settings)
{
  auto& renderData = appData.renderData();
  renderData.m_3dTransparentIfNoHit = settings.m_transparentBackground;
  renderData.m_raycastBackgroundEdgeBrighteningEnabled = settings.m_imageBoxVisible;
  renderData.m_showImagePlanesIn3D = settings.m_imagePlanesVisible;
  renderData.m_modulateImagePlaneOpacityWithViewAngle = settings.m_imagePlaneViewAngleOpacity;
  renderData.m_showSegmentationsOnImagePlanesIn3D = settings.m_imagePlaneSegmentationsVisible;
  renderData.m_shadeImagePlanesIn3D = settings.m_imagePlaneShading;
  renderData.m_imagePlaneLightingAmbient = settings.m_imagePlaneLightingAmbient;
  renderData.m_imagePlaneLightingDiffuse = settings.m_imagePlaneLightingDiffuse;
  renderData.m_imagePlaneLightingSpecular = settings.m_imagePlaneLightingSpecular;
  renderData.m_imagePlaneLightingSpecularPower = settings.m_imagePlaneLightingSpecularPower;
  renderData.m_lightingAmbient = settings.m_lightingAmbient;
  renderData.m_lightingDiffuse = settings.m_lightingDiffuse;
  renderData.m_lightingSpecular = settings.m_lightingSpecular;
  renderData.m_lightingSpecularPower = settings.m_lightingSpecularPower;
  renderData.m_showCrosshairsIn3D = settings.m_showCrosshairsIn3D;
  renderData.m_crosshairs3DGlyphDiameterVoxelDiagonals = settings.m_crosshairs3DGlyphDiameterVoxelDiagonals;
  renderData.m_crosshairs3DGlyphLengthVoxelDiagonals = settings.m_crosshairs3DGlyphLengthVoxelDiagonals;
  renderData.m_showThreeDCameraFrustumIn2DViews = settings.m_showThreeDCameraFrustumIn2DViews;
  renderData.m_reverseThreeDRotateAboutEye = settings.m_reverseThreeDRotateAboutEye;
  renderData.m_threeDCameraFrustumColor = settings.m_threeDCameraFrustumColor;
}

serialize::ProjectRaycastingSettings raycastingSettings(const AppData& appData)
{
  const auto& renderData = appData.renderData();
  return serialize::ProjectRaycastingSettings{
    .m_samplingFactor = renderData.m_raycastSamplingFactor,
    .m_useDistanceMap = renderData.m_useDistanceMapForRaycasting,
    .m_distanceMapForegroundLowerPercentile = renderData.m_distanceMapForegroundLowerPercentile,
    .m_distanceMapForegroundUpperPercentile = renderData.m_distanceMapForegroundUpperPercentile,
    .m_renderFrontFaces = renderData.m_renderFrontFaces,
    .m_renderBackFaces = renderData.m_renderBackFaces,
    .m_segmentationMasking = raycastSegmentationMasking(renderData.m_segMasking)};
}

void applyRaycastingSettings(AppData& appData, const serialize::ProjectRaycastingSettings& settings)
{
  auto& renderData = appData.renderData();
  renderData.m_raycastSamplingFactor = std::clamp(settings.m_samplingFactor, 0.5f, 2.0f);
  renderData.m_useDistanceMapForRaycasting = settings.m_useDistanceMap;
  renderData.m_distanceMapForegroundLowerPercentile =
    std::clamp(settings.m_distanceMapForegroundLowerPercentile, 0.0f, 1.0f);
  renderData.m_distanceMapForegroundUpperPercentile =
    std::clamp(settings.m_distanceMapForegroundUpperPercentile, 0.0f, 1.0f);
  renderData.m_adaptiveRaycastSamplingEnabled = false;
  renderData.m_adaptiveRaycastTargetFrameRate = 30.0f;
  renderData.m_adaptiveRaycastEffectiveSamplingFactor = std::clamp(settings.m_samplingFactor, 0.5f, 2.0f);
  renderData.m_renderFrontFaces = settings.m_renderFrontFaces;
  renderData.m_renderBackFaces = settings.m_renderBackFaces;
  renderData.m_segMasking = raycastSegmentationMasking(settings.m_segmentationMasking);
}

serialize::ProjectMeshRenderingSettings meshRenderingSettings(const AppData& appData)
{
  const auto& renderData = appData.renderData();
  return serialize::ProjectMeshRenderingSettings{
    .m_renderingEnabled = renderData.m_isosurfaceMeshRenderingEnabled,
    .m_flatShadingEnabled = renderData.m_meshSurfaceMaterialSettings.flatShadingEnabled,
    .m_triangleEdgesEnabled = renderData.m_meshSurfaceMaterialSettings.triangleEdgesEnabled,
    .m_triangleEdgeColor = renderData.m_meshSurfaceMaterialSettings.triangleEdgeColor,
    .m_pbrShadingEnabled = renderData.m_meshSurfaceMaterialSettings.pbrShadingEnabled,
    .m_pbrMetallic = renderData.m_meshSurfaceMaterialSettings.metallic,
    .m_pbrRoughness = renderData.m_meshSurfaceMaterialSettings.roughness,
    .m_pbrAmbientOcclusion = renderData.m_meshSurfaceMaterialSettings.ambientOcclusion,
    .m_smoothSegmentationMeshes = renderData.m_smoothSegmentationMeshes,
    .m_smoothIsosurfaceMeshes = renderData.m_smoothIsosurfaceMeshes,
    .m_meshSmoothingIterations = renderData.m_meshSmoothingIterations,
    .m_meshSmoothingPassBand = renderData.m_meshSmoothingPassBand,
    .m_ddpMaxPeelPasses = renderData.m_meshDdpSettings.maxPeelPasses,
    .m_pickingEnabled = renderData.m_meshPickingEnabled,
    .m_clipPlaneEnabled = renderData.m_meshClipPlaneEnabled,
    .m_clipPlaneWorld = renderData.m_meshClipPlaneWorld,
    .m_shadowsEnabled = renderData.m_meshAdvancedLightingSettings.shadows.enabled,
    .m_shadowMapSizePixels = renderData.m_meshAdvancedLightingSettings.shadows.mapSizePixels,
    .m_shadowStrength = renderData.m_meshAdvancedLightingSettings.shadows.strength,
    .m_shadowDepthBias = renderData.m_meshAdvancedLightingSettings.shadows.depthBias,
    .m_ambientOcclusionEnabled = renderData.m_meshAdvancedLightingSettings.ambientOcclusion.enabled,
    .m_ambientOcclusionRadiusMm = renderData.m_meshAdvancedLightingSettings.ambientOcclusion.radiusMm,
    .m_ambientOcclusionStrength = renderData.m_meshAdvancedLightingSettings.ambientOcclusion.strength,
    .m_ambientOcclusionPower = renderData.m_meshAdvancedLightingSettings.ambientOcclusion.power,
    .m_ambientOcclusionContrast = renderData.m_meshAdvancedLightingSettings.ambientOcclusion.contrast,
    .m_ambientOcclusionSampleCount = renderData.m_meshAdvancedLightingSettings.ambientOcclusion.sampleCount,
    .m_rimLightingEnabled = renderData.m_meshSurfaceMaterialSettings.rimLightingEnabled,
    .m_rimOpacityStrength = renderData.m_meshSurfaceMaterialSettings.rimOpacityStrength,
    .m_rimEmissionStrength = renderData.m_meshSurfaceMaterialSettings.rimEmissionStrength,
    .m_rimPower = renderData.m_meshSurfaceMaterialSettings.rimPower};
}

void applyMeshRenderingSettings(AppData& appData, const serialize::ProjectMeshRenderingSettings& settings)
{
  auto& renderData = appData.renderData();
  renderData.m_isosurfaceMeshRenderingEnabled = settings.m_renderingEnabled;
  renderData.m_meshSurfaceMaterialSettings.flatShadingEnabled =
    settings.m_flatShadingEnabled || settings.m_triangleEdgesEnabled;
  renderData.m_meshSurfaceMaterialSettings.triangleEdgesEnabled = settings.m_triangleEdgesEnabled;
  renderData.m_meshSurfaceMaterialSettings.triangleEdgeColor = settings.m_triangleEdgeColor;
  renderData.m_meshSurfaceMaterialSettings.pbrShadingEnabled = settings.m_pbrShadingEnabled;
  renderData.m_meshSurfaceMaterialSettings.metallic = settings.m_pbrMetallic;
  renderData.m_meshSurfaceMaterialSettings.roughness = settings.m_pbrRoughness;
  renderData.m_meshSurfaceMaterialSettings.ambientOcclusion = settings.m_pbrAmbientOcclusion;
  renderData.m_smoothSegmentationMeshes = settings.m_smoothSegmentationMeshes;
  renderData.m_smoothIsosurfaceMeshes = settings.m_smoothIsosurfaceMeshes;
  renderData.m_meshSmoothingIterations = std::clamp(settings.m_meshSmoothingIterations, 1u, 1000u);
  renderData.m_meshSmoothingPassBand = std::clamp(settings.m_meshSmoothingPassBand, 0.001f, 2.0f);
  renderData.m_meshDdpSettings.maxPeelPasses = std::clamp(settings.m_ddpMaxPeelPasses, 1u, 32u);
  renderData.m_meshPickingEnabled = settings.m_pickingEnabled;
  renderData.m_meshClipPlaneEnabled = settings.m_clipPlaneEnabled;
  renderData.m_meshClipPlaneWorld = settings.m_clipPlaneWorld;
  renderData.m_meshAdvancedLightingSettings.shadows.enabled = settings.m_shadowsEnabled;
  renderData.m_meshAdvancedLightingSettings.shadows.mapSizePixels = settings.m_shadowMapSizePixels;
  renderData.m_meshAdvancedLightingSettings.shadows.strength = settings.m_shadowStrength;
  renderData.m_meshAdvancedLightingSettings.shadows.depthBias = settings.m_shadowDepthBias;
  renderData.m_meshAdvancedLightingSettings.ambientOcclusion.enabled = settings.m_ambientOcclusionEnabled;
  renderData.m_meshAdvancedLightingSettings.ambientOcclusion.radiusMm = settings.m_ambientOcclusionRadiusMm;
  renderData.m_meshAdvancedLightingSettings.ambientOcclusion.strength = settings.m_ambientOcclusionStrength;
  renderData.m_meshAdvancedLightingSettings.ambientOcclusion.power = settings.m_ambientOcclusionPower;
  renderData.m_meshAdvancedLightingSettings.ambientOcclusion.contrast = settings.m_ambientOcclusionContrast;
  renderData.m_meshAdvancedLightingSettings.ambientOcclusion.sampleCount = settings.m_ambientOcclusionSampleCount;
  renderData.m_meshSurfaceMaterialSettings.rimLightingEnabled = settings.m_rimLightingEnabled;
  renderData.m_meshSurfaceMaterialSettings.rimOpacityStrength = settings.m_rimOpacityStrength;
  renderData.m_meshSurfaceMaterialSettings.rimEmissionStrength = settings.m_rimEmissionStrength;
  renderData.m_meshSurfaceMaterialSettings.rimPower = settings.m_rimPower;
}

serialize::ProjectIntensityProjectionSettings intensityProjectionSettings(const AppData& appData)
{
  const auto& renderData = appData.renderData();
  return serialize::ProjectIntensityProjectionSettings{
    .m_useMaximumImageExtent = renderData.m_doMaxExtentIntensityProjection,
    .m_slabThicknessMm = renderData.m_intensityProjectionSlabThickness,
    .m_xrayEnergyKeV = renderData.m_xrayEnergyKeV,
    .m_xrayWindow = renderData.m_xrayIntensityWindow,
    .m_xrayLevel = renderData.m_xrayIntensityLevel};
}

void applyIntensityProjectionSettings(AppData& appData, const serialize::ProjectIntensityProjectionSettings& settings)
{
  auto& renderData = appData.renderData();
  renderData.m_doMaxExtentIntensityProjection = settings.m_useMaximumImageExtent;
  renderData.m_intensityProjectionSlabThickness = settings.m_slabThicknessMm;
  renderData.setXrayEnergy(settings.m_xrayEnergyKeV);
  renderData.m_xrayIntensityWindow = settings.m_xrayWindow;
  renderData.m_xrayIntensityLevel = settings.m_xrayLevel;
}

serialize::ProjectSegmentationDisplaySettings segmentationDisplaySettings(const AppData& appData)
{
  const auto& renderData = appData.renderData();
  return serialize::ProjectSegmentationDisplaySettings{
    .m_modulateOpacityWithImageOpacity2d = renderData.m_modulateSegmentationOpacityWithImageOpacity2d,
    .m_modulateOpacityWithImageOpacity3d = renderData.m_modulateSegmentationOpacityWithImageOpacity3d,
    .m_outlineStyle = renderData.m_segOutlineStyle,
    .m_interiorOpacity = renderData.m_segInteriorOpacity,
    .m_erosionFactor = renderData.m_segInterpCutoff};
}

void applySegmentationDisplaySettings(AppData& appData, const serialize::ProjectSegmentationDisplaySettings& settings)
{
  auto& renderData = appData.renderData();
  renderData.m_modulateSegmentationOpacityWithImageOpacity2d = settings.m_modulateOpacityWithImageOpacity2d;
  renderData.m_modulateSegmentationOpacityWithImageOpacity3d = settings.m_modulateOpacityWithImageOpacity3d;
  renderData.m_segOutlineStyle = settings.m_outlineStyle;
  renderData.m_segInteriorOpacity = settings.m_interiorOpacity;
  renderData.m_segInterpCutoff = settings.m_erosionFactor;
}

serialize::ProjectIsocontourDisplaySettings isocontourDisplaySettings(const AppData& appData)
{
  const auto& renderData = appData.renderData();
  return serialize::ProjectIsocontourDisplaySettings{
    .m_floatingPointInterpolationPolicy = renderData.m_isocontourFloatingPointInterpolationPolicy};
}

void applyIsocontourDisplaySettings(AppData& appData, const serialize::ProjectIsocontourDisplaySettings& settings)
{
  auto& renderData = appData.renderData();
  renderData.m_isocontourFloatingPointInterpolationPolicy = settings.m_floatingPointInterpolationPolicy;
}

// Project-wide reset.

void applyDefaultProjectSettings(AppData& appData)
{
  applySynchronizationSettings(appData, serialize::ProjectSynchronizationSettings{});
  applyViewSettings(appData, serialize::ProjectViewSettings{});
  applyComparisonSettings(appData, serialize::ProjectComparisonSettings{});
  applyThreeDRenderingSettings(appData, serialize::ProjectThreeDRenderingSettings{});
  applyRaycastingSettings(appData, serialize::ProjectRaycastingSettings{});
  applyMeshRenderingSettings(appData, serialize::ProjectMeshRenderingSettings{});
  applyIntensityProjectionSettings(appData, serialize::ProjectIntensityProjectionSettings{});
  applySegmentationDisplaySettings(appData, serialize::ProjectSegmentationDisplaySettings{});
  applyIsocontourDisplaySettings(appData, serialize::ProjectIsocontourDisplaySettings{});
}

bool componentRenderModeIsValidForImage(ComponentRenderMode mode, const Image& image)
{
  const uint32_t numComponents = image.header().numComponentsPerPixel();
  switch (mode) {
    case ComponentRenderMode::SingleComponent:
      return true;
    case ComponentRenderMode::Color:
      return 3 == numComponents || 4 == numComponents;
    case ComponentRenderMode::Minimum:
    case ComponentRenderMode::Mean:
    case ComponentRenderMode::Maximum:
    case ComponentRenderMode::Magnitude:
      return numComponents >= 2;
    case ComponentRenderMode::ComplexPhase:
    case ComponentRenderMode::ComplexReal:
    case ComponentRenderMode::ComplexImaginary:
      return isComplexValuedImage(image);
    case ComponentRenderMode::VectorDirectionColor:
    case ComponentRenderMode::VectorSignedNormalProjection:
    case ComponentRenderMode::VectorPlanarProjectionColor:
    case ComponentRenderMode::VectorJacobianDeterminant:
    case ComponentRenderMode::VectorGradientMagnitude:
    case ComponentRenderMode::VectorDivergence:
    case ComponentRenderMode::VectorCurlMagnitude:
    case ComponentRenderMode::VectorLaplacianMagnitude:
      return isVectorFieldCandidate(image);
  }

  return false;
}

// Per-image and per-segmentation settings.

serialize::ImageSettings imageSettings(const Image& image, std::optional<glm::vec3> defaultBorderColor)
{
  const ImageSettings& imageSettings = image.settings();
  const ImageSettings defaultSettings = image.defaultSettings();

  serialize::ImageSettings settings;
  if (imageSettings.displayName() != defaultSettings.displayName()) {
    settings.m_displayName = imageSettings.displayName();
  }
  settings.m_globalVisibility = imageSettings.globalVisibility();
  settings.m_globalOpacity = imageSettings.globalOpacity();
  const glm::vec3 baselineBorderColor = defaultBorderColor.value_or(defaultSettings.borderColor());
  if (imageSettings.borderColor() != baselineBorderColor) {
    settings.m_borderColor = imageSettings.borderColor();
    settings.m_hasBorderColor = true;
  }
  settings.m_lockedToReference = imageSettings.isLockedToReference();
  settings.m_warpEnabled = imageSettings.warpEnabled();
  settings.m_warpStrength = imageSettings.warpStrength();
  settings.m_allowExaggeratedWarp = imageSettings.allowExaggeratedWarp();
  settings.m_level = 0.0;
  settings.m_window = 1.0;
  settings.m_thresholdLow = 0.0;
  settings.m_thresholdHigh = 0.0;
  settings.m_opacity = imageSettings.opacity();
  settings.m_activeComponent = imageSettings.activeComponent();
  const serialize::ProjectComponentRenderMode componentRenderMode =
    toSerializedComponentRenderMode(imageSettings.componentRenderMode());
  if (componentRenderMode != toSerializedComponentRenderMode(defaultSettings.componentRenderMode())) {
    settings.m_componentRenderMode = componentRenderMode;
    settings.m_hasComponentRenderMode = true;
  }
  settings.m_complexPhaseUnit = toSerializedComplexPhaseUnit(imageSettings.complexPhaseUnit());
  settings.m_complexPhaseRange = toSerializedComplexPhaseRange(imageSettings.complexPhaseRange());
  settings.m_vectorArrowOverlayVisible = imageSettings.vectorArrowOverlayVisible();
  settings.m_vectorArrowOverlayOnImage = imageSettings.vectorArrowOverlayOnImage();
  settings.m_vectorArrowOverlayDensity = imageSettings.vectorArrowOverlayDensity();
  settings.m_vectorArrowOverlayVoxelSpacing = imageSettings.vectorArrowOverlayVoxelSpacing();
  settings.m_vectorArrowOverlayMillimeterSpacing = imageSettings.vectorArrowOverlayMillimeterSpacing();
  settings.m_vectorArrowOverlaySpacingMode =
    toSerializedVectorArrowOverlaySpacingMode(imageSettings.vectorArrowOverlaySpacingMode());
  settings.m_vectorArrowOverlayColor = imageSettings.vectorArrowOverlayColor();
  settings.m_vectorArrowOverlayUseDirectionColor = imageSettings.vectorArrowOverlayUseDirectionColor();
  settings.m_vectorArrowOverlayLineThickness = imageSettings.vectorArrowOverlayLineThickness();
  settings.m_vectorArrowOverlayOpacity = imageSettings.vectorArrowOverlayOpacity();
  settings.m_vectorArrowOverlayScaleByMagnitude = imageSettings.vectorArrowOverlayScaleByMagnitude();
  settings.m_vectorArrowOverlayScaleFactor = imageSettings.vectorArrowOverlayScaleFactor();
  settings.m_vectorWarpedGridVisible = imageSettings.vectorWarpedGridVisible();
  settings.m_vectorWarpedGridOverlayOnImage = imageSettings.vectorWarpedGridOverlayOnImage();
  settings.m_vectorWarpedGridConvention =
    toSerializedVectorWarpedGridConvention(imageSettings.vectorWarpedGridConvention());
  settings.m_vectorWarpedGridPixelSpacing = imageSettings.vectorWarpedGridPixelSpacing();
  settings.m_vectorWarpedGridVoxelSpacing = imageSettings.vectorWarpedGridVoxelSpacing();
  settings.m_vectorWarpedGridMillimeterSpacing = imageSettings.vectorWarpedGridMillimeterSpacing();
  settings.m_vectorWarpedGridSpacingMode =
    toSerializedVectorArrowOverlaySpacingMode(imageSettings.vectorWarpedGridSpacingMode());
  settings.m_vectorWarpedGridLineThickness = imageSettings.vectorWarpedGridLineThickness();
  settings.m_vectorWarpedGridScaleFactor = imageSettings.vectorWarpedGridScaleFactor();
  settings.m_vectorWarpedGridForegroundColor = imageSettings.vectorWarpedGridForegroundColor();
  settings.m_vectorWarpedGridBackgroundColor = imageSettings.vectorWarpedGridBackgroundColor();
  settings.m_vectorPlanarProjectionSignedColors = imageSettings.vectorPlanarProjectionSignedColors();
  settings.m_vectorLogJacobianDeterminant = imageSettings.vectorLogJacobianDeterminant();
  settings.m_ignoreAlpha = imageSettings.ignoreAlpha();
  settings.m_colorInterpolationMode = imageSettings.colorInterpolationMode();
  settings.m_activeTimePoint = imageSettings.activeTimePoint();
  settings.m_timePlaybackLoop = imageSettings.timePlaybackLoop();
  settings.m_timePlaybackPlaying = imageSettings.timePlaybackPlaying();
  settings.m_timePlaybackSpeed = imageSettings.timePlaybackSpeed();
  settings.m_componentLevels.reserve(imageSettings.numComponents());
  settings.m_componentWindows.reserve(imageSettings.numComponents());
  settings.m_componentThresholdLows.reserve(imageSettings.numComponents());
  settings.m_componentThresholdHighs.reserve(imageSettings.numComponents());
  settings.m_componentVisibility.reserve(imageSettings.numComponents());
  settings.m_componentOpacities.reserve(imageSettings.numComponents());
  settings.m_colorMapIndices.reserve(imageSettings.numComponents());
  settings.m_colorMapInverted.reserve(imageSettings.numComponents());
  settings.m_colorMapContinuous.reserve(imageSettings.numComponents());
  settings.m_colorMapLevels.reserve(imageSettings.numComponents());
  settings.m_colorMapHsvModifiers.reserve(imageSettings.numComponents());
  settings.m_interpolationModes.reserve(imageSettings.numComponents());
  for (uint32_t component = 0; component < imageSettings.numComponents(); ++component) {
    const auto componentThresholds = imageSettings.thresholds(component);
    const auto defaultComponentThresholds = defaultSettings.thresholds(component);
    addDiffValue(
      settings.m_componentLevels,
      settings.m_componentLevelIndices,
      component,
      imageSettings.windowCenter(component),
      defaultSettings.windowCenter(component),
      0.0);
    addDiffValue(
      settings.m_componentWindows,
      settings.m_componentWindowIndices,
      component,
      imageSettings.windowWidth(component),
      defaultSettings.windowWidth(component),
      1.0);
    addDiffValue(
      settings.m_componentThresholdLows,
      settings.m_componentThresholdLowIndices,
      component,
      componentThresholds.first,
      defaultComponentThresholds.first,
      0.0);
    addDiffValue(
      settings.m_componentThresholdHighs,
      settings.m_componentThresholdHighIndices,
      component,
      componentThresholds.second,
      defaultComponentThresholds.second,
      0.0);
    addDiffValue(
      settings.m_componentVisibility,
      settings.m_componentVisibilityIndices,
      component,
      imageSettings.visibility(component),
      defaultSettings.visibility(component),
      true);
    addDiffValue(
      settings.m_componentOpacities,
      settings.m_componentOpacityIndices,
      component,
      imageSettings.opacity(component),
      defaultSettings.opacity(component),
      1.0);
    addDiffValue(
      settings.m_colorMapIndices,
      settings.m_colorMapIndexIndices,
      component,
      imageSettings.colorMapIndex(component),
      defaultSettings.colorMapIndex(component),
      std::size_t{0});
    addDiffValue(
      settings.m_colorMapInverted,
      settings.m_colorMapInvertedIndices,
      component,
      imageSettings.isColorMapInverted(component),
      defaultSettings.isColorMapInverted(component),
      false);
    addDiffValue(
      settings.m_colorMapContinuous,
      settings.m_colorMapContinuousIndices,
      component,
      imageSettings.colorMapContinuous(component),
      defaultSettings.colorMapContinuous(component),
      true);
    addDiffValue(
      settings.m_colorMapLevels,
      settings.m_colorMapLevelIndices,
      component,
      imageSettings.colorMapQuantizationLevels(component),
      defaultSettings.colorMapQuantizationLevels(component),
      std::size_t{8});
    addDiffValue(
      settings.m_colorMapHsvModifiers,
      settings.m_colorMapHsvModifierIndices,
      component,
      imageSettings.colorMapHsvModFactors(component),
      defaultSettings.colorMapHsvModFactors(component),
      glm::vec3{0.0f, 1.0f, 1.0f});
    addDiffValue(
      settings.m_interpolationModes,
      settings.m_interpolationModeIndices,
      component,
      imageSettings.interpolationMode(component),
      defaultSettings.interpolationMode(component),
      InterpolationMode::Linear);
  }
  settings.m_edgeDetectionMethod = EdgeDetectionMethod::Pixel == imageSettings.edgeDetectionMethod()
                                     ? serialize::ProjectEdgeDetectionMethod::Pixel
                                     : serialize::ProjectEdgeDetectionMethod::Voxel;
  settings.m_showEdges = imageSettings.showAnyEdges();
  settings.m_thresholdEdges = EdgeDetectionMethod::Pixel == imageSettings.edgeDetectionMethod()
                                ? imageSettings.thresholdPixelEdges()
                                : imageSettings.thresholdEdges();
  settings.m_thinPixelEdges = imageSettings.thinPixelEdges();
  settings.m_overlayEdges = EdgeDetectionMethod::Pixel == imageSettings.edgeDetectionMethod()
                              ? imageSettings.overlayPixelEdges()
                              : imageSettings.overlayEdges();
  settings.m_colormapEdges =
    EdgeDetectionMethod::Voxel == imageSettings.edgeDetectionMethod() && imageSettings.colormapEdges();
  settings.m_edgeMagnitude = imageSettings.edgeMagnitude();
  settings.m_pixelEdgeScale = imageSettings.pixelEdgeScale();
  settings.m_pixelEdgeThreshold = imageSettings.pixelEdgeThreshold();
  const glm::vec3 baselineEdgeColor = defaultBorderColor.value_or(defaultSettings.edgeColor());
  if (imageSettings.edgeColor() != baselineEdgeColor) {
    settings.m_edgeColor = imageSettings.edgeColor();
    settings.m_hasEdgeColor = true;
  }
  settings.m_edgeOpacity = imageSettings.edgeOpacity();
  settings.m_applyImageColormapToIsosurfaces = imageSettings.applyImageColormapToIsosurfaces();
  settings.m_modulateIsosurfaceOpacityWithImageOpacity = imageSettings.modulateIsosurfaceOpacityWithImageOpacity();
  settings.m_isocontourLineWidthIn2D = imageSettings.isoContourLineWidthIn2D();
  settings.m_isosurfaceOpacityModulator = imageSettings.isosurfaceOpacityModulator();
  return settings;
}

serialize::SegSettings segmentationSettings(const AppData& appData, const Image& seg)
{
  serialize::SegSettings settings;
  const ImageSettings& segSettings = seg.settings();
  settings.m_displayName = segSettings.displayName();
  settings.m_visible = segSettings.visibility();
  settings.m_opacity = seg.settings().opacity();
  if (segSettings.numComponents() > 0) {
    settings.m_labelTableIndex = segSettings.labelTableIndex(0);
    settings.m_interpolationMode = segSettings.interpolationMode(0);
    const auto tableUid = appData.labelTableUid(settings.m_labelTableIndex);
    settings.m_labels = segmentationLabels(seg, tableUid ? appData.labelTable(*tableUid) : nullptr);
  }
  return settings;
}

void applyImageSettings(Image& image, const serialize::ImageSettings& settings)
{
  ImageSettings& imageSettingsLocal = image.settings();
  if (!settings.m_displayName.empty()) {
    imageSettingsLocal.setDisplayName(settings.m_displayName);
  }

  imageSettingsLocal.setGlobalVisibility(settings.m_globalVisibility);
  imageSettingsLocal.setGlobalOpacity(settings.m_globalOpacity);
  if (settings.m_hasBorderColor) {
    imageSettingsLocal.setBorderColor(settings.m_borderColor);
  }
  imageSettingsLocal.setLockedToReference(settings.m_lockedToReference);
  imageSettingsLocal.setWarpEnabled(settings.m_warpEnabled);
  imageSettingsLocal.setAllowExaggeratedWarp(settings.m_allowExaggeratedWarp);
  imageSettingsLocal.setWarpStrength(settings.m_warpStrength);
  if (settings.m_activeComponent < imageSettingsLocal.numComponents()) {
    imageSettingsLocal.setActiveComponent(settings.m_activeComponent);
  }
  imageSettingsLocal.setActiveTimePoint(image.timeAxis().clamp(settings.m_activeTimePoint));
  imageSettingsLocal.setTimePlaybackLoop(settings.m_timePlaybackLoop);
  imageSettingsLocal.setTimePlaybackPlaying(settings.m_timePlaybackPlaying && image.isTimeSeries());
  imageSettingsLocal.setTimePlaybackSpeed(settings.m_timePlaybackSpeed);
  imageSettingsLocal.setOpacity(settings.m_opacity);
  if (settings.m_hasComponentRenderMode) {
    const ComponentRenderMode componentMode = fromSerializedComponentRenderMode(settings.m_componentRenderMode);
    imageSettingsLocal.setComponentRenderMode(
      componentRenderModeIsValidForImage(componentMode, image) ? componentMode : ComponentRenderMode::SingleComponent);
  }
  if (ComponentRenderMode::ComplexReal == imageSettingsLocal.componentRenderMode()) {
    imageSettingsLocal.setActiveComponent(0);
  }
  else if (ComponentRenderMode::ComplexImaginary == imageSettingsLocal.componentRenderMode()) {
    imageSettingsLocal.setActiveComponent(1);
  }
  imageSettingsLocal.setComplexPhaseUnit(fromSerializedComplexPhaseUnit(settings.m_complexPhaseUnit));
  imageSettingsLocal.setComplexPhaseRange(fromSerializedComplexPhaseRange(settings.m_complexPhaseRange));
  imageSettingsLocal.setVectorArrowOverlayVisible(settings.m_vectorArrowOverlayVisible);
  imageSettingsLocal.setVectorArrowOverlayOnImage(settings.m_vectorArrowOverlayOnImage);
  imageSettingsLocal.setVectorArrowOverlayDensity(settings.m_vectorArrowOverlayDensity);
  imageSettingsLocal.setVectorArrowOverlayVoxelSpacing(settings.m_vectorArrowOverlayVoxelSpacing);
  imageSettingsLocal.setVectorArrowOverlayMillimeterSpacing(settings.m_vectorArrowOverlayMillimeterSpacing);
  imageSettingsLocal.setVectorArrowOverlaySpacingMode(
    fromSerializedVectorArrowOverlaySpacingMode(settings.m_vectorArrowOverlaySpacingMode));
  imageSettingsLocal.setVectorArrowOverlayColor(settings.m_vectorArrowOverlayColor);
  imageSettingsLocal.setVectorArrowOverlayUseDirectionColor(settings.m_vectorArrowOverlayUseDirectionColor);
  imageSettingsLocal.setVectorArrowOverlayLineThickness(settings.m_vectorArrowOverlayLineThickness);
  imageSettingsLocal.setVectorArrowOverlayOpacity(settings.m_vectorArrowOverlayOpacity);
  imageSettingsLocal.setVectorArrowOverlayScaleByMagnitude(settings.m_vectorArrowOverlayScaleByMagnitude);
  imageSettingsLocal.setVectorArrowOverlayScaleFactor(settings.m_vectorArrowOverlayScaleFactor);
  imageSettingsLocal.setVectorWarpedGridVisible(settings.m_vectorWarpedGridVisible);
  imageSettingsLocal.setVectorWarpedGridOverlayOnImage(settings.m_vectorWarpedGridOverlayOnImage);
  imageSettingsLocal.setVectorWarpedGridConvention(
    fromSerializedVectorWarpedGridConvention(settings.m_vectorWarpedGridConvention));
  imageSettingsLocal.setVectorWarpedGridPixelSpacing(settings.m_vectorWarpedGridPixelSpacing);
  imageSettingsLocal.setVectorWarpedGridVoxelSpacing(settings.m_vectorWarpedGridVoxelSpacing);
  imageSettingsLocal.setVectorWarpedGridMillimeterSpacing(settings.m_vectorWarpedGridMillimeterSpacing);
  imageSettingsLocal.setVectorWarpedGridSpacingMode(
    fromSerializedVectorArrowOverlaySpacingMode(settings.m_vectorWarpedGridSpacingMode));
  imageSettingsLocal.setVectorWarpedGridLineThickness(settings.m_vectorWarpedGridLineThickness);
  imageSettingsLocal.setVectorWarpedGridScaleFactor(settings.m_vectorWarpedGridScaleFactor);
  imageSettingsLocal.setVectorWarpedGridForegroundColor(settings.m_vectorWarpedGridForegroundColor);
  imageSettingsLocal.setVectorWarpedGridBackgroundColor(settings.m_vectorWarpedGridBackgroundColor);
  imageSettingsLocal.setVectorPlanarProjectionSignedColors(settings.m_vectorPlanarProjectionSignedColors);
  imageSettingsLocal.setVectorLogJacobianDeterminant(settings.m_vectorLogJacobianDeterminant);
  imageSettingsLocal.setIgnoreAlpha(settings.m_ignoreAlpha);
  imageSettingsLocal.setColorInterpolationMode(settings.m_colorInterpolationMode);
  const std::size_t numLevelComponents =
    std::min<std::size_t>(settings.m_componentLevels.size(), imageSettingsLocal.numComponents());
  for (std::size_t component = 0; component < numLevelComponents; ++component) {
    if (!shouldApplySparseComponentValue(settings.m_componentLevelIndices, component)) {
      continue;
    }
    imageSettingsLocal.setWindowCenter(static_cast<uint32_t>(component), settings.m_componentLevels.at(component));
  }
  const std::size_t numWindowComponents =
    std::min<std::size_t>(settings.m_componentWindows.size(), imageSettingsLocal.numComponents());
  for (std::size_t component = 0; component < numWindowComponents; ++component) {
    if (!shouldApplySparseComponentValue(settings.m_componentWindowIndices, component)) {
      continue;
    }
    const double windowWidth = settings.m_componentWindows.at(component);
    if (windowWidth > 0.0) {
      imageSettingsLocal.setWindowWidth(static_cast<uint32_t>(component), windowWidth);
    }
  }
  const std::size_t numThresholdLowComponents =
    std::min<std::size_t>(settings.m_componentThresholdLows.size(), imageSettingsLocal.numComponents());
  for (std::size_t component = 0; component < numThresholdLowComponents; ++component) {
    if (!shouldApplySparseComponentValue(settings.m_componentThresholdLowIndices, component)) {
      continue;
    }
    imageSettingsLocal.setThresholdLow(
      static_cast<uint32_t>(component),
      settings.m_componentThresholdLows.at(component));
  }
  const std::size_t numThresholdHighComponents =
    std::min<std::size_t>(settings.m_componentThresholdHighs.size(), imageSettingsLocal.numComponents());
  for (std::size_t component = 0; component < numThresholdHighComponents; ++component) {
    if (!shouldApplySparseComponentValue(settings.m_componentThresholdHighIndices, component)) {
      continue;
    }
    imageSettingsLocal.setThresholdHigh(
      static_cast<uint32_t>(component),
      settings.m_componentThresholdHighs.at(component));
  }
  const std::size_t numVisibilityComponents =
    std::min<std::size_t>(settings.m_componentVisibility.size(), imageSettingsLocal.numComponents());
  for (std::size_t component = 0; component < numVisibilityComponents; ++component) {
    if (!shouldApplySparseComponentValue(settings.m_componentVisibilityIndices, component)) {
      continue;
    }
    imageSettingsLocal.setVisibility(static_cast<uint32_t>(component), settings.m_componentVisibility.at(component));
  }
  const std::size_t numOpacityComponents =
    std::min<std::size_t>(settings.m_componentOpacities.size(), imageSettingsLocal.numComponents());
  for (std::size_t component = 0; component < numOpacityComponents; ++component) {
    if (!shouldApplySparseComponentValue(settings.m_componentOpacityIndices, component)) {
      continue;
    }
    imageSettingsLocal.setOpacity(static_cast<uint32_t>(component), settings.m_componentOpacities.at(component));
  }
  const std::size_t numColorMapComponents =
    std::min<std::size_t>(settings.m_colorMapIndices.size(), imageSettingsLocal.numComponents());
  for (std::size_t component = 0; component < numColorMapComponents; ++component) {
    if (!shouldApplySparseComponentValue(settings.m_colorMapIndexIndices, component)) {
      continue;
    }
    imageSettingsLocal.setColorMapIndex(static_cast<uint32_t>(component), settings.m_colorMapIndices.at(component));
  }
  const std::size_t numColorMapInvertedComponents =
    std::min<std::size_t>(settings.m_colorMapInverted.size(), imageSettingsLocal.numComponents());
  for (std::size_t component = 0; component < numColorMapInvertedComponents; ++component) {
    if (!shouldApplySparseComponentValue(settings.m_colorMapInvertedIndices, component)) {
      continue;
    }
    imageSettingsLocal.setColorMapInverted(static_cast<uint32_t>(component), settings.m_colorMapInverted.at(component));
  }
  const std::size_t numColorMapContinuousComponents =
    std::min<std::size_t>(settings.m_colorMapContinuous.size(), imageSettingsLocal.numComponents());
  for (std::size_t component = 0; component < numColorMapContinuousComponents; ++component) {
    if (!shouldApplySparseComponentValue(settings.m_colorMapContinuousIndices, component)) {
      continue;
    }
    imageSettingsLocal.setColorMapContinuous(
      static_cast<uint32_t>(component),
      settings.m_colorMapContinuous.at(component));
  }
  const std::size_t numColorMapLevelComponents =
    std::min<std::size_t>(settings.m_colorMapLevels.size(), imageSettingsLocal.numComponents());
  for (std::size_t component = 0; component < numColorMapLevelComponents; ++component) {
    if (!shouldApplySparseComponentValue(settings.m_colorMapLevelIndices, component)) {
      continue;
    }
    imageSettingsLocal.setColorMapQuantization(
      static_cast<uint32_t>(component),
      static_cast<uint32_t>(settings.m_colorMapLevels.at(component)));
  }
  const std::size_t numColorMapHsvComponents =
    std::min<std::size_t>(settings.m_colorMapHsvModifiers.size(), imageSettingsLocal.numComponents());
  for (std::size_t component = 0; component < numColorMapHsvComponents; ++component) {
    if (!shouldApplySparseComponentValue(settings.m_colorMapHsvModifierIndices, component)) {
      continue;
    }
    imageSettingsLocal.setColormapHsvModfactors(
      static_cast<uint32_t>(component),
      settings.m_colorMapHsvModifiers.at(component));
  }
  const std::size_t numInterpolationComponents =
    std::min<std::size_t>(settings.m_interpolationModes.size(), imageSettingsLocal.numComponents());
  for (std::size_t component = 0; component < numInterpolationComponents; ++component) {
    if (!shouldApplySparseComponentValue(settings.m_interpolationModeIndices, component)) {
      continue;
    }
    imageSettingsLocal.setInterpolationMode(
      static_cast<uint32_t>(component),
      settings.m_interpolationModes.at(component));
  }
  imageSettingsLocal.setEdgeDetectionMethod(
    serialize::ProjectEdgeDetectionMethod::Pixel == settings.m_edgeDetectionMethod ? EdgeDetectionMethod::Pixel
                                                                                   : EdgeDetectionMethod::Voxel);
  imageSettingsLocal.setShowAnyEdges(settings.m_showEdges);
  imageSettingsLocal.setThresholdEdges(settings.m_thresholdEdges);
  imageSettingsLocal.setThresholdPixelEdges(settings.m_thresholdEdges);
  imageSettingsLocal.setThinPixelEdges(settings.m_thinPixelEdges);
  imageSettingsLocal.setOverlayEdges(settings.m_overlayEdges);
  imageSettingsLocal.setOverlayPixelEdges(settings.m_overlayEdges);
  imageSettingsLocal.setColormapEdges(
    serialize::ProjectEdgeDetectionMethod::Voxel == settings.m_edgeDetectionMethod && settings.m_colormapEdges);
  imageSettingsLocal.setEdgeMagnitude(settings.m_edgeMagnitude);
  imageSettingsLocal.setPixelEdgeScale(settings.m_pixelEdgeScale);
  imageSettingsLocal.setPixelEdgeThreshold(settings.m_pixelEdgeThreshold);
  if (settings.m_hasEdgeColor) {
    imageSettingsLocal.setEdgeColor(settings.m_edgeColor);
  }
  imageSettingsLocal.setEdgeOpacity(settings.m_edgeOpacity);
  imageSettingsLocal.setApplyImageColormapToIsosurfaces(settings.m_applyImageColormapToIsosurfaces);
  imageSettingsLocal.setModulateIsosurfaceOpacityWithImageOpacity(settings.m_modulateIsosurfaceOpacityWithImageOpacity);
  imageSettingsLocal.setIsosurfaceWidthIn2d(settings.m_isocontourLineWidthIn2D);
  imageSettingsLocal.setIsosurfaceOpacityModulator(settings.m_isosurfaceOpacityModulator);
}

void applySegmentationSettings(AppData& appData, Image& seg, const serialize::SegSettings& settings)
{
  ImageSettings& segSettings = seg.settings();
  if (!settings.m_displayName.empty()) {
    segSettings.setDisplayName(settings.m_displayName);
  }
  segSettings.setVisibility(settings.m_visible);
  segSettings.setOpacity(settings.m_opacity);
  if (segSettings.numComponents() > 0) {
    segSettings.setActiveComponent(0);
    segSettings.setInterpolationMode(0, settings.m_interpolationMode);
  }
  if (settings.m_labels) {
    applySegmentationLabels(appData, seg, *settings.m_labels);
  }
}
} // namespace project_snapshot
