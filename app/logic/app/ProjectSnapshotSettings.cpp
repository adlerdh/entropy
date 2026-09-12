#include "logic/app/ProjectSnapshotSettings.h"

#include "common/Types.h"
#include "image/Image.h"
#include "image/ImageDerivedData.h"
#include "image/ImageHeader.h"
#include "image/ImageTimeAxis.h"
#include "logic/app/Data.h"
#include "logic/app/ParcellationLabelTable.h"
#include "logic/app/Settings.h"
#include "rendering/RenderSettings.h"
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
         table.getShowMesh(index) == baseline.getShowMesh(index) &&
         table.getIncludeInCutaway(index) == baseline.getIncludeInCutaway(index);
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
      .m_showMesh = table->getShowMesh(index),
      .m_includeInCutaway = table->getIncludeInCutaway(index)});
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
    table->setIncludeInCutaway(label.m_index, label.m_includeInCutaway);
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
    .m_showImageBorders = appData.renderSettings().m_globalSliceIntersectionParams.renderInactiveImageViewIntersections,
    .m_showImageBordersInLightboxViews =
      appData.renderSettings().m_globalSliceIntersectionParams.renderInactiveImageViewIntersectionsInLightboxViews,
    .m_showCrosshairs = appData.renderSettings().m_showCrosshairs,
    .m_showCrosshairsInLightboxViews = appData.renderSettings().m_showCrosshairsInLightboxViews,
    .m_showAnatomicalLabels = appData.renderSettings().m_showAnatomicalLabels,
    .m_showAnatomicalLabelsInLightboxViews = appData.renderSettings().m_showAnatomicalLabelsInLightboxViews,
    .m_showScaleBars = appData.renderSettings().m_showScaleBars,
    .m_showScaleBarsInLightboxViews = appData.renderSettings().m_showScaleBarsInLightboxViews,
    .m_annotationsOnTop = appData.renderSettings().m_globalAnnotationParams.renderOnTopOfAllImagePlanes,
    .m_landmarksOnTop = appData.renderSettings().m_globalLandmarkParams.renderOnTopOfAllImagePlanes,
    .m_hideAnnotationVertices = appData.renderSettings().m_globalAnnotationParams.hidePolygonVertices,
    .m_anatomicalLabelType = appData.renderSettings().m_anatomicalLabelType,
    .m_quadrupedBodyRegion = appData.renderSettings().m_quadrupedBodyRegion,
    .m_viewConvention = appData.windowData().getViewOrientationConvention(),
    .m_lockAnatomicalDirectionsToReferenceImage = appData.settings().lockAnatomicalCoordinateAxesWithReferenceImage(),
    .m_crosshairsSnapping = appData.renderSettings().m_snapCrosshairs};
}

void applyViewSettings(AppData& appData, const serialize::ProjectViewSettings& settings)
{
  appData.renderSettings().m_globalSliceIntersectionParams.renderInactiveImageViewIntersections =
    settings.m_showImageBorders;
  appData.renderSettings().m_globalSliceIntersectionParams.renderInactiveImageViewIntersectionsInLightboxViews =
    settings.m_showImageBorders && settings.m_showImageBordersInLightboxViews;
  appData.renderSettings().m_showCrosshairs = settings.m_showCrosshairs;
  appData.renderSettings().m_showCrosshairsInLightboxViews =
    settings.m_showCrosshairs && settings.m_showCrosshairsInLightboxViews;
  appData.renderSettings().m_showAnatomicalLabels = settings.m_showAnatomicalLabels;
  appData.renderSettings().m_showAnatomicalLabelsInLightboxViews =
    settings.m_showAnatomicalLabels && settings.m_showAnatomicalLabelsInLightboxViews;
  appData.renderSettings().m_showScaleBars = settings.m_showScaleBars;
  appData.renderSettings().m_showScaleBarsInLightboxViews =
    settings.m_showScaleBars && settings.m_showScaleBarsInLightboxViews;
  appData.renderSettings().m_globalAnnotationParams.renderOnTopOfAllImagePlanes = settings.m_annotationsOnTop;
  appData.renderSettings().m_globalLandmarkParams.renderOnTopOfAllImagePlanes = settings.m_landmarksOnTop;
  appData.renderSettings().m_globalAnnotationParams.hidePolygonVertices = settings.m_hideAnnotationVertices;
  appData.renderSettings().m_anatomicalLabelType = settings.m_anatomicalLabelType;
  appData.renderSettings().m_quadrupedBodyRegion = settings.m_quadrupedBodyRegion;
  appData.windowData().setViewOrientationConvention(settings.m_viewConvention);
  appData.settings().setLockAnatomicalCoordinateAxesWithReferenceImage(
    settings.m_lockAnatomicalDirectionsToReferenceImage);
  appData.renderSettings().m_snapCrosshairs = settings.m_crosshairsSnapping;
}

// Project-owned comparison metric and mode settings.

serialize::ProjectMetricSettings metricSettings(const rendering::RenderSettings::MetricParams& params)
{
  return serialize::ProjectMetricSettings{
    .m_colorMapIndex = params.m_colorMapIndex,
    .m_slopeIntercept = params.m_slopeIntercept,
    .m_invertColormap = params.m_invertCmap,
    .m_continuousColormap = params.m_cmapContinuous,
    .m_colormapLevels = params.m_cmapQuantizationLevels};
}

void applyMetricSettings(
  rendering::RenderSettings::MetricParams& params,
  const serialize::ProjectMetricSettings& settings)
{
  params.m_colorMapIndex = settings.m_colorMapIndex;
  params.m_slopeIntercept = settings.m_slopeIntercept;
  params.m_invertCmap = settings.m_invertColormap;
  params.m_cmapContinuous = settings.m_continuousColormap;
  params.m_cmapQuantizationLevels = settings.m_colormapLevels;
}

serialize::ProjectLocalNccPresentation localNccPresentation(
  rendering::RenderSettings::LocalNccPresentation presentation)
{
  return rendering::RenderSettings::LocalNccPresentation::Correlation == presentation
           ? serialize::ProjectLocalNccPresentation::Correlation
           : serialize::ProjectLocalNccPresentation::Dissimilarity;
}

rendering::RenderSettings::LocalNccPresentation localNccPresentation(
  serialize::ProjectLocalNccPresentation presentation)
{
  return serialize::ProjectLocalNccPresentation::Correlation == presentation
           ? rendering::RenderSettings::LocalNccPresentation::Correlation
           : rendering::RenderSettings::LocalNccPresentation::Dissimilarity;
}

serialize::ProjectLocalMetricInvalidStyle localMetricInvalidStyle(rendering::RenderSettings::LocalNccInvalidStyle style)
{
  return rendering::RenderSettings::LocalNccInvalidStyle::Gray == style
           ? serialize::ProjectLocalMetricInvalidStyle::Gray
           : serialize::ProjectLocalMetricInvalidStyle::Transparent;
}

rendering::RenderSettings::LocalNccInvalidStyle localMetricInvalidStyle(serialize::ProjectLocalMetricInvalidStyle style)
{
  return serialize::ProjectLocalMetricInvalidStyle::Gray == style
           ? rendering::RenderSettings::LocalNccInvalidStyle::Gray
           : rendering::RenderSettings::LocalNccInvalidStyle::Transparent;
}

serialize::ProjectSegmentationRaycastMasking raycastSegmentationMasking(
  rendering::RenderSettings::SegMaskingForRaycasting masking)
{
  switch (masking) {
    case rendering::RenderSettings::SegMaskingForRaycasting::SegMasksIn:
      return serialize::ProjectSegmentationRaycastMasking::MaskIn;
    case rendering::RenderSettings::SegMaskingForRaycasting::SegMasksOut:
      return serialize::ProjectSegmentationRaycastMasking::MaskOut;
    case rendering::RenderSettings::SegMaskingForRaycasting::Disabled:
      return serialize::ProjectSegmentationRaycastMasking::Disabled;
  }

  return serialize::ProjectSegmentationRaycastMasking::Disabled;
}

rendering::RenderSettings::SegMaskingForRaycasting raycastSegmentationMasking(
  serialize::ProjectSegmentationRaycastMasking masking)
{
  switch (masking) {
    case serialize::ProjectSegmentationRaycastMasking::MaskIn:
      return rendering::RenderSettings::SegMaskingForRaycasting::SegMasksIn;
    case serialize::ProjectSegmentationRaycastMasking::MaskOut:
      return rendering::RenderSettings::SegMaskingForRaycasting::SegMasksOut;
    case serialize::ProjectSegmentationRaycastMasking::Disabled:
      return rendering::RenderSettings::SegMaskingForRaycasting::Disabled;
  }

  return rendering::RenderSettings::SegMaskingForRaycasting::Disabled;
}

serialize::ProjectComparisonSettings comparisonSettings(const AppData& appData)
{
  const auto& renderSettings = appData.renderSettings();
  return serialize::ProjectComparisonSettings{
    .m_difference =
      serialize::ProjectDifferenceMetricSettings{
        .m_squared = renderSettings.m_useSquare,
        .m_metric = metricSettings(renderSettings.m_squaredDifferenceParams)},
    .m_localNcc =
      serialize::ProjectLocalNccMetricSettings{
        .m_metric = metricSettings(renderSettings.m_localNccParams),
        .m_presentation = localNccPresentation(renderSettings.m_localNccPresentation),
        .m_negativeCorrelationAsMismatch = renderSettings.m_localNccIgnoreNegativeCorrelation,
        .m_patchRadius = renderSettings.m_localNccPatchRadius,
        .m_sampleSpacing = renderSettings.m_localNccSampleSpacing,
        .m_minimumValidFraction = renderSettings.m_localNccMinValidFraction,
        .m_varianceEpsilon = renderSettings.m_localNccVarianceEpsilon,
        .m_invalidStyle = localMetricInvalidStyle(renderSettings.m_localNccInvalidStyle)},
    .m_localLinearResidual =
      serialize::ProjectLocalLinearResidualMetricSettings{
        .m_metric = metricSettings(renderSettings.m_localLinearResidualParams),
        .m_patchRadius = renderSettings.m_localLinearResidualPatchRadius,
        .m_sampleSpacing = renderSettings.m_localLinearResidualSampleSpacing,
        .m_minimumValidFraction = renderSettings.m_localLinearResidualMinValidFraction,
        .m_varianceEpsilon = renderSettings.m_localLinearResidualVarianceEpsilon,
        .m_invalidStyle = localMetricInvalidStyle(renderSettings.m_localLinearResidualInvalidStyle)},
    .m_overlayMagentaCyan = renderSettings.m_overlayMagentaCyan,
    .m_quadrants = renderSettings.m_quadrants,
    .m_checkerboardSquares = renderSettings.m_numCheckerboardSquares,
    .m_flashlightRadiusFraction = renderSettings.m_flashlightRadius,
    .m_flashlightOverlayMovingImage = renderSettings.m_flashlightOverlays};
}

void applyComparisonSettings(AppData& appData, const serialize::ProjectComparisonSettings& settings)
{
  auto& renderSettings = appData.renderSettings();
  renderSettings.m_useSquare = settings.m_difference.m_squared;
  applyMetricSettings(renderSettings.m_squaredDifferenceParams, settings.m_difference.m_metric);

  applyMetricSettings(renderSettings.m_localNccParams, settings.m_localNcc.m_metric);
  renderSettings.m_localNccPresentation = localNccPresentation(settings.m_localNcc.m_presentation);
  renderSettings.m_localNccIgnoreNegativeCorrelation = settings.m_localNcc.m_negativeCorrelationAsMismatch;
  renderSettings.m_localNccPatchRadius = settings.m_localNcc.m_patchRadius;
  renderSettings.m_localNccSampleSpacing = settings.m_localNcc.m_sampleSpacing;
  renderSettings.m_localNccMinValidFraction = settings.m_localNcc.m_minimumValidFraction;
  renderSettings.m_localNccVarianceEpsilon = settings.m_localNcc.m_varianceEpsilon;
  renderSettings.m_localNccInvalidStyle = localMetricInvalidStyle(settings.m_localNcc.m_invalidStyle);

  applyMetricSettings(renderSettings.m_localLinearResidualParams, settings.m_localLinearResidual.m_metric);
  renderSettings.m_localLinearResidualPatchRadius = settings.m_localLinearResidual.m_patchRadius;
  renderSettings.m_localLinearResidualSampleSpacing = settings.m_localLinearResidual.m_sampleSpacing;
  renderSettings.m_localLinearResidualMinValidFraction = settings.m_localLinearResidual.m_minimumValidFraction;
  renderSettings.m_localLinearResidualVarianceEpsilon = settings.m_localLinearResidual.m_varianceEpsilon;
  renderSettings.m_localLinearResidualInvalidStyle =
    localMetricInvalidStyle(settings.m_localLinearResidual.m_invalidStyle);

  renderSettings.m_overlayMagentaCyan = settings.m_overlayMagentaCyan;
  renderSettings.m_quadrants = settings.m_quadrants;
  renderSettings.m_numCheckerboardSquares = settings.m_checkerboardSquares;
  renderSettings.m_flashlightRadius = settings.m_flashlightRadiusFraction;
  renderSettings.m_flashlightOverlays = settings.m_flashlightOverlayMovingImage;
}

// Project-owned rendering presentation settings.

serialize::ProjectThreeDRenderingSettings threeDRenderingSettings(const AppData& appData)
{
  const auto& renderSettings = appData.renderSettings();
  return serialize::ProjectThreeDRenderingSettings{
    .m_transparentBackground = renderSettings.m_3dTransparentIfNoHit,
    .m_imageBoxVisible = renderSettings.m_raycastBackgroundEdgeBrighteningEnabled,
    .m_imagePlanesVisible = renderSettings.m_showImagePlanesIn3D,
    .m_imagePlaneSegmentationsVisible = renderSettings.m_showSegmentationsOnImagePlanesIn3D,
    .m_imagePlaneIsocontoursVisible = renderSettings.m_showIsocontoursOnImagePlanesIn3D,
    .m_imagePlaneOpacity = renderSettings.m_imagePlaneOpacity,
    .m_imagePlaneViewAngleOpacity = renderSettings.m_modulateImagePlaneOpacityWithViewAngle,
    .m_imagePlaneShading = renderSettings.m_shadeImagePlanesIn3D,
    .m_imagePlaneLightingAmbient = renderSettings.m_imagePlaneLightingAmbient,
    .m_imagePlaneLightingDiffuse = renderSettings.m_imagePlaneLightingDiffuse,
    .m_imagePlaneLightingSpecular = renderSettings.m_imagePlaneLightingSpecular,
    .m_imagePlaneLightingSpecularPower = renderSettings.m_imagePlaneLightingSpecularPower,
    .m_lightingAmbient = renderSettings.m_lightingAmbient,
    .m_lightingDiffuse = renderSettings.m_lightingDiffuse,
    .m_lightingSpecular = renderSettings.m_lightingSpecular,
    .m_lightingSpecularPower = renderSettings.m_lightingSpecularPower,
    .m_showCrosshairsIn3D = renderSettings.m_showCrosshairsIn3D,
    .m_crosshairs3DGlyphDiameterScenePercent = renderSettings.m_crosshairs3DGlyphDiameterScenePercent,
    .m_crosshairs3DGlyphLengthScenePercent = renderSettings.m_crosshairs3DGlyphLengthScenePercent,
    .m_showThreeDCameraFrustumIn2DViews = renderSettings.m_showThreeDCameraFrustumIn2DViews,
    .m_reverseThreeDRotateAboutEye = renderSettings.m_reverseThreeDRotateAboutEye,
    .m_threeDCameraFrustumColor = renderSettings.m_threeDCameraFrustumColor};
}

void applyThreeDRenderingSettings(AppData& appData, const serialize::ProjectThreeDRenderingSettings& settings)
{
  auto& renderSettings = appData.renderSettings();
  renderSettings.m_3dTransparentIfNoHit = settings.m_transparentBackground;
  renderSettings.m_raycastBackgroundEdgeBrighteningEnabled = settings.m_imageBoxVisible;
  renderSettings.m_showImagePlanesIn3D = settings.m_imagePlanesVisible;
  renderSettings.m_imagePlaneOpacity = settings.m_imagePlaneOpacity;
  renderSettings.m_modulateImagePlaneOpacityWithViewAngle = settings.m_imagePlaneViewAngleOpacity;
  renderSettings.m_showSegmentationsOnImagePlanesIn3D = settings.m_imagePlaneSegmentationsVisible;
  renderSettings.m_showIsocontoursOnImagePlanesIn3D = settings.m_imagePlaneIsocontoursVisible;
  renderSettings.m_shadeImagePlanesIn3D = settings.m_imagePlaneShading;
  renderSettings.m_imagePlaneLightingAmbient = settings.m_imagePlaneLightingAmbient;
  renderSettings.m_imagePlaneLightingDiffuse = settings.m_imagePlaneLightingDiffuse;
  renderSettings.m_imagePlaneLightingSpecular = settings.m_imagePlaneLightingSpecular;
  renderSettings.m_imagePlaneLightingSpecularPower = settings.m_imagePlaneLightingSpecularPower;
  renderSettings.m_lightingAmbient = settings.m_lightingAmbient;
  renderSettings.m_lightingDiffuse = settings.m_lightingDiffuse;
  renderSettings.m_lightingSpecular = settings.m_lightingSpecular;
  renderSettings.m_lightingSpecularPower = settings.m_lightingSpecularPower;
  renderSettings.m_showCrosshairsIn3D = settings.m_showCrosshairsIn3D;
  renderSettings.m_crosshairs3DGlyphDiameterScenePercent = settings.m_crosshairs3DGlyphDiameterScenePercent;
  renderSettings.m_crosshairs3DGlyphLengthScenePercent = settings.m_crosshairs3DGlyphLengthScenePercent;
  renderSettings.m_showThreeDCameraFrustumIn2DViews = settings.m_showThreeDCameraFrustumIn2DViews;
  renderSettings.m_reverseThreeDRotateAboutEye = settings.m_reverseThreeDRotateAboutEye;
  renderSettings.m_threeDCameraFrustumColor = settings.m_threeDCameraFrustumColor;
}

serialize::ProjectRaycastingSettings raycastingSettings(const AppData& appData)
{
  const auto& renderSettings = appData.renderSettings();
  return serialize::ProjectRaycastingSettings{
    .m_samplingFactor = renderSettings.m_raycastSamplingFactor,
    .m_useDistanceMap = renderSettings.m_useDistanceMapForRaycasting,
    .m_distanceMapForegroundLowerPercentile = renderSettings.m_distanceMapForegroundLowerPercentile,
    .m_distanceMapForegroundUpperPercentile = renderSettings.m_distanceMapForegroundUpperPercentile,
    .m_renderFrontFaces = renderSettings.m_renderFrontFaces,
    .m_renderBackFaces = renderSettings.m_renderBackFaces,
    .m_segmentationMasking = raycastSegmentationMasking(renderSettings.m_segMasking)};
}

void applyRaycastingSettings(AppData& appData, const serialize::ProjectRaycastingSettings& settings)
{
  auto& renderSettings = appData.renderSettings();
  renderSettings.m_raycastSamplingFactor = std::clamp(settings.m_samplingFactor, 0.5f, 2.0f);
  renderSettings.m_useDistanceMapForRaycasting = settings.m_useDistanceMap;
  renderSettings.m_distanceMapForegroundLowerPercentile =
    std::clamp(settings.m_distanceMapForegroundLowerPercentile, 0.0f, 1.0f);
  renderSettings.m_distanceMapForegroundUpperPercentile =
    std::clamp(settings.m_distanceMapForegroundUpperPercentile, 0.0f, 1.0f);
  renderSettings.m_adaptiveRaycastSamplingEnabled = false;
  renderSettings.m_adaptiveRaycastTargetFrameRate = 30.0f;
  renderSettings.m_adaptiveRaycastEffectiveSamplingFactor = std::clamp(settings.m_samplingFactor, 0.5f, 2.0f);
  renderSettings.m_renderFrontFaces = settings.m_renderFrontFaces;
  renderSettings.m_renderBackFaces = settings.m_renderBackFaces;
  renderSettings.m_segMasking = raycastSegmentationMasking(settings.m_segmentationMasking);
}

serialize::ProjectMeshRenderingSettings meshRenderingSettings(const AppData& appData)
{
  const auto& renderSettings = appData.renderSettings();
  return serialize::ProjectMeshRenderingSettings{
    .m_renderingEnabled = renderSettings.m_isosurfaceMeshRenderingEnabled,
    .m_flatShadingEnabled = renderSettings.m_meshSurfaceMaterialSettings.flatShadingEnabled,
    .m_triangleEdgesEnabled = renderSettings.m_meshSurfaceMaterialSettings.triangleEdgesEnabled,
    .m_triangleEdgeColor = renderSettings.m_meshSurfaceMaterialSettings.triangleEdgeColor,
    .m_pbrShadingEnabled = renderSettings.m_meshSurfaceMaterialSettings.pbrShadingEnabled,
    .m_pbrMetallic = renderSettings.m_meshSurfaceMaterialSettings.metallic,
    .m_pbrRoughness = renderSettings.m_meshSurfaceMaterialSettings.roughness,
    .m_pbrAmbientOcclusion = renderSettings.m_meshSurfaceMaterialSettings.ambientOcclusion,
    .m_smoothSegmentationMeshes = renderSettings.m_smoothSegmentationMeshes,
    .m_smoothIsosurfaceMeshes = renderSettings.m_smoothIsosurfaceMeshes,
    .m_meshSmoothingIterations = renderSettings.m_meshSmoothingIterations,
    .m_meshSmoothingPassBand = renderSettings.m_meshSmoothingPassBand,
    .m_ddpMaxPeelPasses = renderSettings.m_meshDdpSettings.maxPeelPasses,
    .m_pickingEnabled = renderSettings.m_meshPickingEnabled,
    .m_cutawayEnabled = renderSettings.m_meshCutawayEnabled,
    .m_shadowsEnabled = renderSettings.m_meshAdvancedLightingSettings.shadows.enabled,
    .m_shadowMapSizePixels = renderSettings.m_meshAdvancedLightingSettings.shadows.mapSizePixels,
    .m_shadowStrength = renderSettings.m_meshAdvancedLightingSettings.shadows.strength,
    .m_shadowDepthBias = renderSettings.m_meshAdvancedLightingSettings.shadows.depthBias,
    .m_ambientOcclusionEnabled = renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.enabled,
    .m_ambientOcclusionRadiusMm = renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.radiusMm,
    .m_ambientOcclusionStrength = renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.strength,
    .m_ambientOcclusionPower = renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.power,
    .m_ambientOcclusionContrast = renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.contrast,
    .m_ambientOcclusionSampleCount = renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.sampleCount,
    .m_rimLightingEnabled = renderSettings.m_meshSurfaceMaterialSettings.rimLightingEnabled,
    .m_rimOpacityStrength = renderSettings.m_meshSurfaceMaterialSettings.rimOpacityStrength,
    .m_rimEmissionStrength = renderSettings.m_meshSurfaceMaterialSettings.rimEmissionStrength,
    .m_rimPower = renderSettings.m_meshSurfaceMaterialSettings.rimPower};
}

void applyMeshRenderingSettings(AppData& appData, const serialize::ProjectMeshRenderingSettings& settings)
{
  auto& renderSettings = appData.renderSettings();
  renderSettings.m_isosurfaceMeshRenderingEnabled = settings.m_renderingEnabled;
  renderSettings.m_meshSurfaceMaterialSettings.flatShadingEnabled =
    settings.m_flatShadingEnabled || settings.m_triangleEdgesEnabled;
  renderSettings.m_meshSurfaceMaterialSettings.triangleEdgesEnabled = settings.m_triangleEdgesEnabled;
  renderSettings.m_meshSurfaceMaterialSettings.triangleEdgeColor = settings.m_triangleEdgeColor;
  renderSettings.m_meshSurfaceMaterialSettings.pbrShadingEnabled = settings.m_pbrShadingEnabled;
  renderSettings.m_meshSurfaceMaterialSettings.metallic = settings.m_pbrMetallic;
  renderSettings.m_meshSurfaceMaterialSettings.roughness = settings.m_pbrRoughness;
  renderSettings.m_meshSurfaceMaterialSettings.ambientOcclusion = settings.m_pbrAmbientOcclusion;
  renderSettings.m_smoothSegmentationMeshes = settings.m_smoothSegmentationMeshes;
  renderSettings.m_smoothIsosurfaceMeshes = settings.m_smoothIsosurfaceMeshes;
  renderSettings.m_meshSmoothingIterations = std::clamp(settings.m_meshSmoothingIterations, 1u, 1000u);
  renderSettings.m_meshSmoothingPassBand = std::clamp(settings.m_meshSmoothingPassBand, 0.001f, 2.0f);
  renderSettings.m_meshDdpSettings.maxPeelPasses = std::clamp(settings.m_ddpMaxPeelPasses, 1u, 32u);
  renderSettings.m_meshPickingEnabled = settings.m_pickingEnabled;
  renderSettings.m_meshCutawayEnabled = settings.m_cutawayEnabled;
  renderSettings.m_meshAdvancedLightingSettings.shadows.enabled = settings.m_shadowsEnabled;
  renderSettings.m_meshAdvancedLightingSettings.shadows.mapSizePixels = settings.m_shadowMapSizePixels;
  renderSettings.m_meshAdvancedLightingSettings.shadows.strength = settings.m_shadowStrength;
  renderSettings.m_meshAdvancedLightingSettings.shadows.depthBias = settings.m_shadowDepthBias;
  renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.enabled = settings.m_ambientOcclusionEnabled;
  renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.radiusMm = settings.m_ambientOcclusionRadiusMm;
  renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.strength = settings.m_ambientOcclusionStrength;
  renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.power = settings.m_ambientOcclusionPower;
  renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.contrast = settings.m_ambientOcclusionContrast;
  renderSettings.m_meshAdvancedLightingSettings.ambientOcclusion.sampleCount = settings.m_ambientOcclusionSampleCount;
  renderSettings.m_meshSurfaceMaterialSettings.rimLightingEnabled = settings.m_rimLightingEnabled;
  renderSettings.m_meshSurfaceMaterialSettings.rimOpacityStrength = settings.m_rimOpacityStrength;
  renderSettings.m_meshSurfaceMaterialSettings.rimEmissionStrength = settings.m_rimEmissionStrength;
  renderSettings.m_meshSurfaceMaterialSettings.rimPower = settings.m_rimPower;
}

serialize::ProjectIntensityProjectionSettings intensityProjectionSettings(const AppData& appData)
{
  const auto& renderSettings = appData.renderSettings();
  return serialize::ProjectIntensityProjectionSettings{
    .m_useMaximumImageExtent = renderSettings.m_doMaxExtentIntensityProjection,
    .m_slabThicknessMm = renderSettings.m_intensityProjectionSlabThickness,
    .m_xrayEnergyKeV = renderSettings.m_xrayEnergyKeV,
    .m_xrayWindow = renderSettings.m_xrayIntensityWindow,
    .m_xrayLevel = renderSettings.m_xrayIntensityLevel};
}

void applyIntensityProjectionSettings(AppData& appData, const serialize::ProjectIntensityProjectionSettings& settings)
{
  auto& renderSettings = appData.renderSettings();
  renderSettings.m_doMaxExtentIntensityProjection = settings.m_useMaximumImageExtent;
  renderSettings.m_intensityProjectionSlabThickness = settings.m_slabThicknessMm;
  renderSettings.setXrayEnergy(settings.m_xrayEnergyKeV);
  renderSettings.m_xrayIntensityWindow = settings.m_xrayWindow;
  renderSettings.m_xrayIntensityLevel = settings.m_xrayLevel;
}

serialize::ProjectSegmentationDisplaySettings segmentationDisplaySettings(const AppData& appData)
{
  const auto& renderSettings = appData.renderSettings();
  return serialize::ProjectSegmentationDisplaySettings{
    .m_modulateOpacityWithImageOpacity2d = renderSettings.m_modulateSegmentationOpacityWithImageOpacity2d,
    .m_modulateOpacityWithImageOpacity3d = renderSettings.m_modulateSegmentationOpacityWithImageOpacity3d,
    .m_outlineStyle = renderSettings.m_segOutlineStyle,
    .m_interiorOpacity = renderSettings.m_segInteriorOpacity,
    .m_erosionFactor = renderSettings.m_segInterpCutoff};
}

void applySegmentationDisplaySettings(AppData& appData, const serialize::ProjectSegmentationDisplaySettings& settings)
{
  auto& renderSettings = appData.renderSettings();
  renderSettings.m_modulateSegmentationOpacityWithImageOpacity2d = settings.m_modulateOpacityWithImageOpacity2d;
  renderSettings.m_modulateSegmentationOpacityWithImageOpacity3d = settings.m_modulateOpacityWithImageOpacity3d;
  renderSettings.m_segOutlineStyle = settings.m_outlineStyle;
  renderSettings.m_segInteriorOpacity = settings.m_interiorOpacity;
  renderSettings.m_segInterpCutoff = settings.m_erosionFactor;
}

serialize::ProjectIsocontourDisplaySettings isocontourDisplaySettings(const AppData& appData)
{
  const auto& renderSettings = appData.renderSettings();
  return serialize::ProjectIsocontourDisplaySettings{
    .m_floatingPointInterpolationPolicy = renderSettings.m_isocontourFloatingPointInterpolationPolicy};
}

void applyIsocontourDisplaySettings(AppData& appData, const serialize::ProjectIsocontourDisplaySettings& settings)
{
  auto& renderSettings = appData.renderSettings();
  renderSettings.m_isocontourFloatingPointInterpolationPolicy = settings.m_floatingPointInterpolationPolicy;
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
    const double windowCenter = imageSettings.windowCenter(component);
    const double windowWidth = imageSettings.windowWidth(component);
    if (
      windowCenter != defaultSettings.windowCenter(component) || windowWidth != defaultSettings.windowWidth(component))
    {
      // Center and width form one display window. Persist both whenever either differs so that
      // applying an out-of-range clinical preset can restore them atomically.
      setDiffValue(settings.m_componentLevels, settings.m_componentLevelIndices, component, windowCenter, 0.0);
      setDiffValue(settings.m_componentWindows, settings.m_componentWindowIndices, component, windowWidth, 1.0);
    }
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
  settings.m_edgeDetectionMethod = EdgeDetectionMethod::ScreenPixel == imageSettings.edgeDetectionMethod()
                                     ? serialize::ProjectEdgeDetectionMethod::ScreenPixel
                                     : serialize::ProjectEdgeDetectionMethod::Voxel;
  settings.m_showEdges = imageSettings.edgesVisible();
  settings.m_hardEdges = imageSettings.hardEdges();
  settings.m_thinPixelEdges = imageSettings.thinPixelEdges();
  settings.m_overlayEdges = imageSettings.overlayEdges();
  settings.m_colormapEdges = imageSettings.colormapEdges();
  settings.m_voxelEdgeScale = imageSettings.voxelEdgeScale();
  settings.m_voxelEdgeThreshold = imageSettings.voxelEdgeThreshold();
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
  const std::size_t numWindowingComponents = std::min<std::size_t>(
    std::max(settings.m_componentLevels.size(), settings.m_componentWindows.size()),
    imageSettingsLocal.numComponents());
  for (std::size_t component = 0; component < numWindowingComponents; ++component) {
    const bool applyLevel = component < settings.m_componentLevels.size() &&
                            shouldApplySparseComponentValue(settings.m_componentLevelIndices, component);
    const bool applyWidth = component < settings.m_componentWindows.size() &&
                            shouldApplySparseComponentValue(settings.m_componentWindowIndices, component) &&
                            settings.m_componentWindows.at(component) > 0.0;
    const auto componentIndex = static_cast<uint32_t>(component);
    if (applyLevel && applyWidth) {
      imageSettingsLocal.setWindowCenterAndWidth(
        componentIndex,
        settings.m_componentLevels.at(component),
        settings.m_componentWindows.at(component));
    }
    else if (applyLevel) {
      imageSettingsLocal.setWindowCenter(componentIndex, settings.m_componentLevels.at(component));
    }
    else if (applyWidth) {
      imageSettingsLocal.setWindowWidth(componentIndex, settings.m_componentWindows.at(component));
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
    serialize::ProjectEdgeDetectionMethod::ScreenPixel == settings.m_edgeDetectionMethod
      ? EdgeDetectionMethod::ScreenPixel
      : EdgeDetectionMethod::Voxel);
  imageSettingsLocal.setEdgesVisible(settings.m_showEdges);
  imageSettingsLocal.setHardEdges(settings.m_hardEdges);
  imageSettingsLocal.setThinPixelEdges(settings.m_thinPixelEdges);
  imageSettingsLocal.setOverlayEdges(settings.m_overlayEdges);
  imageSettingsLocal.setColormapEdges(settings.m_colormapEdges);
  imageSettingsLocal.setVoxelEdgeScale(settings.m_voxelEdgeScale);
  imageSettingsLocal.setVoxelEdgeThreshold(settings.m_voxelEdgeThreshold);
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
