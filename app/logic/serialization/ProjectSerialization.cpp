#include "logic/serialization/ProjectSerialization.h"
#include "logic/annotation/SerializeAnnot.h"
#include "logic/serialization/JsonSerializers.h"
#include "logic/serialization/SerializationHelpers.h"

#include "common/Exception.hpp"
#include "layout/LayoutSpecJson.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using json = nlohmann::json;

namespace fs = std::filesystem;

namespace
{
constexpr int k_projectFormatMajorVersion = 1;
constexpr int k_projectFormatMinorVersion = 0;

struct JsonVersion
{
  int major = 0;
  int minor = 0;
};

json versionToJson(const int major, const int minor)
{
  return json{{"major", major}, {"minor", minor}};
}

JsonVersion versionFromJson(const json& value)
{
  if (!value.is_object()) {
    throwDebug("JSON version must be an object");
  }

  return JsonVersion{.major = value.at("major").get<int>(), .minor = value.at("minor").get<int>()};
}
} // namespace

namespace serialize
{

void to_json(json& j, const Image& image);
void from_json(const json& j, Image& image);

void to_json(json& j, const ProjectInterfaceSettings& settings)
{
  const ProjectInterfaceSettings defaults;
  j = json::object();
  addIfChanged(j, "synchronizeTimeSeries", settings.m_synchronizeTimeSeries, defaults.m_synchronizeTimeSeries);
}

void from_json(const json& j, ProjectInterfaceSettings& settings)
{
  if (const auto syncTime = j.find("synchronizeTimeSeries"); syncTime != j.end() && syncTime->is_boolean()) {
    settings.m_synchronizeTimeSeries = syncTime->get<bool>();
  }
}

void to_json(json& j, const ProjectViewSettings& settings)
{
  const ProjectViewSettings defaults;
  j = json::object();

  json imageBorders = json::object();
  addIfChanged(imageBorders, "visible", settings.m_showImageBorders, defaults.m_showImageBorders);
  addIfChanged(
    imageBorders,
    "visibleInLightboxes",
    settings.m_showImageBordersInLightboxViews,
    defaults.m_showImageBordersInLightboxViews);
  addIfNotEmpty(j, "imageBorders", std::move(imageBorders));

  json crosshairs = json::object();
  addIfChanged(crosshairs, "visible", settings.m_showCrosshairs, defaults.m_showCrosshairs);
  addIfChanged(
    crosshairs,
    "visibleInLightboxes",
    settings.m_showCrosshairsInLightboxViews,
    defaults.m_showCrosshairsInLightboxViews);
  addIfChanged(
    crosshairs,
    "snapping",
    enumToName(settings.m_crosshairsSnapping, k_crosshairsSnappingNames),
    enumToName(defaults.m_crosshairsSnapping, k_crosshairsSnappingNames));
  addIfNotEmpty(j, "crosshairs", std::move(crosshairs));

  json anatomicalLabels = json::object();
  addIfChanged(anatomicalLabels, "visible", settings.m_showAnatomicalLabels, defaults.m_showAnatomicalLabels);
  addIfChanged(
    anatomicalLabels,
    "visibleInLightboxes",
    settings.m_showAnatomicalLabelsInLightboxViews,
    defaults.m_showAnatomicalLabelsInLightboxViews);
  addIfChanged(
    anatomicalLabels,
    "type",
    enumToName(settings.m_anatomicalLabelType, k_anatomicalLabelNames),
    enumToName(defaults.m_anatomicalLabelType, k_anatomicalLabelNames));
  addIfChanged(
    anatomicalLabels,
    "lockDirectionsToReferenceImage",
    settings.m_lockAnatomicalDirectionsToReferenceImage,
    defaults.m_lockAnatomicalDirectionsToReferenceImage);
  addIfNotEmpty(j, "anatomicalLabels", std::move(anatomicalLabels));

  json scaleBars = json::object();
  addIfChanged(scaleBars, "visible", settings.m_showScaleBars, defaults.m_showScaleBars);
  addIfChanged(
    scaleBars,
    "visibleInLightboxes",
    settings.m_showScaleBarsInLightboxViews,
    defaults.m_showScaleBarsInLightboxViews);
  addIfNotEmpty(j, "scaleBars", std::move(scaleBars));
}

void from_json(const json& j, ProjectViewSettings& settings)
{
  if (const auto imageBorders = j.find("imageBorders"); imageBorders != j.end() && imageBorders->is_object()) {
    if (const auto value = imageBorders->find("visible"); value != imageBorders->end() && value->is_boolean()) {
      settings.m_showImageBorders = value->get<bool>();
    }
    if (const auto value = imageBorders->find("visibleInLightboxes");
        value != imageBorders->end() && value->is_boolean())
    {
      settings.m_showImageBordersInLightboxViews = value->get<bool>();
    }
  }
  if (const auto crosshairs = j.find("crosshairs"); crosshairs != j.end() && crosshairs->is_object()) {
    if (const auto value = crosshairs->find("visible"); value != crosshairs->end() && value->is_boolean()) {
      settings.m_showCrosshairs = value->get<bool>();
    }
    if (const auto value = crosshairs->find("visibleInLightboxes"); value != crosshairs->end() && value->is_boolean()) {
      settings.m_showCrosshairsInLightboxViews = value->get<bool>();
    }
    if (
      const auto parsed =
        enumFromName<CrosshairsSnapping>(crosshairs->value("snapping", ""), k_crosshairsSnappingNames))
    {
      settings.m_crosshairsSnapping = *parsed;
    }
  }
  if (const auto anatomicalLabels = j.find("anatomicalLabels");
      anatomicalLabels != j.end() && anatomicalLabels->is_object())
  {
    if (const auto value = anatomicalLabels->find("visible"); value != anatomicalLabels->end() && value->is_boolean()) {
      settings.m_showAnatomicalLabels = value->get<bool>();
    }
    if (const auto value = anatomicalLabels->find("visibleInLightboxes");
        value != anatomicalLabels->end() && value->is_boolean())
    {
      settings.m_showAnatomicalLabelsInLightboxViews = value->get<bool>();
    }
    if (
      const auto parsed =
        enumFromName<AnatomicalLabelType>(anatomicalLabels->value("type", ""), k_anatomicalLabelNames))
    {
      settings.m_anatomicalLabelType = *parsed;
      if (AnatomicalLabelType::Disabled == settings.m_anatomicalLabelType) {
        settings.m_showAnatomicalLabels = false;
        settings.m_anatomicalLabelType = AnatomicalLabelType::Human;
      }
    }
    if (const auto value = anatomicalLabels->find("lockDirectionsToReferenceImage");
        value != anatomicalLabels->end() && value->is_boolean())
    {
      settings.m_lockAnatomicalDirectionsToReferenceImage = value->get<bool>();
    }
  }
  if (const auto scaleBars = j.find("scaleBars"); scaleBars != j.end() && scaleBars->is_object()) {
    if (const auto value = scaleBars->find("visible"); value != scaleBars->end() && value->is_boolean()) {
      settings.m_showScaleBars = value->get<bool>();
    }
    if (const auto value = scaleBars->find("visibleInLightboxes"); value != scaleBars->end() && value->is_boolean()) {
      settings.m_showScaleBarsInLightboxViews = value->get<bool>();
    }
  }

  if (!settings.m_showImageBorders) {
    settings.m_showImageBordersInLightboxViews = false;
  }
  if (!settings.m_showCrosshairs) {
    settings.m_showCrosshairsInLightboxViews = false;
  }
  if (!settings.m_showAnatomicalLabels) {
    settings.m_showAnatomicalLabelsInLightboxViews = false;
  }
  if (!settings.m_showScaleBars) {
    settings.m_showScaleBarsInLightboxViews = false;
  }
}

void to_json(json& j, const ProjectMetricSettings& settings)
{
  const ProjectMetricSettings defaults;
  j = json::object();
  addIfChanged(j, "colormapIndex", settings.m_colorMapIndex, defaults.m_colorMapIndex);
  addIfChanged(j, "windowSlopeIntercept", vec2ToJson(settings.m_slopeIntercept), vec2ToJson(defaults.m_slopeIntercept));
  addIfChanged(j, "invertColormap", settings.m_invertColormap, defaults.m_invertColormap);
  addIfChanged(j, "continuousColormap", settings.m_continuousColormap, defaults.m_continuousColormap);
  addIfChanged(j, "colormapLevels", settings.m_colormapLevels, defaults.m_colormapLevels);
}

void from_json(const json& j, ProjectMetricSettings& settings)
{
  if (const auto value = j.find("colormapIndex"); value != j.end() && value->is_number_unsigned()) {
    settings.m_colorMapIndex = value->get<std::size_t>();
  }
  if (const auto value = j.find("windowSlopeIntercept"); value != j.end() && value->is_array() && value->size() == 2) {
    settings.m_slopeIntercept = vec2FromJson(*value);
  }
  if (const auto value = j.find("invertColormap"); value != j.end() && value->is_boolean()) {
    settings.m_invertColormap = value->get<bool>();
  }
  if (const auto value = j.find("continuousColormap"); value != j.end() && value->is_boolean()) {
    settings.m_continuousColormap = value->get<bool>();
  }
  if (const auto value = j.find("colormapLevels"); value != j.end() && value->is_number_integer()) {
    settings.m_colormapLevels = std::max(value->get<int>(), 2);
  }
}

void to_json(json& j, const ProjectDifferenceMetricSettings& settings)
{
  const ProjectDifferenceMetricSettings defaults;
  j = json::object();
  addIfChanged(j, "squared", settings.m_squared, defaults.m_squared);

  json metric = settings.m_metric;
  addIfNotEmpty(j, "metric", std::move(metric));
}

void from_json(const json& j, ProjectDifferenceMetricSettings& settings)
{
  if (const auto value = j.find("squared"); value != j.end() && value->is_boolean()) {
    settings.m_squared = value->get<bool>();
  }
  if (const auto metric = j.find("metric"); metric != j.end() && metric->is_object()) {
    metric->get_to(settings.m_metric);
  }
}

void to_json(json& j, const ProjectLocalNccMetricSettings& settings)
{
  const ProjectLocalNccMetricSettings defaults;
  j = json::object();

  json metric = settings.m_metric;
  addIfNotEmpty(j, "metric", std::move(metric));
  addIfChanged(
    j,
    "presentation",
    enumToName(settings.m_presentation, k_localNccPresentationNames),
    enumToName(defaults.m_presentation, k_localNccPresentationNames));
  addIfChanged(
    j,
    "negativeCorrelationAsMismatch",
    settings.m_negativeCorrelationAsMismatch,
    defaults.m_negativeCorrelationAsMismatch);
  addIfChanged(j, "patchRadius", settings.m_patchRadius, defaults.m_patchRadius);
  addIfChanged(j, "sampleSpacing", settings.m_sampleSpacing, defaults.m_sampleSpacing);
  addIfChanged(j, "minimumValidFraction", settings.m_minimumValidFraction, defaults.m_minimumValidFraction);
  addIfChanged(j, "varianceEpsilon", settings.m_varianceEpsilon, defaults.m_varianceEpsilon);
  addIfChanged(
    j,
    "invalidStyle",
    enumToName(settings.m_invalidStyle, k_localMetricInvalidStyleNames),
    enumToName(defaults.m_invalidStyle, k_localMetricInvalidStyleNames));
}

void from_json(const json& j, ProjectLocalNccMetricSettings& settings)
{
  if (const auto metric = j.find("metric"); metric != j.end() && metric->is_object()) {
    metric->get_to(settings.m_metric);
  }
  if (
    const auto parsed =
      enumFromName<ProjectLocalNccPresentation>(j.value("presentation", ""), k_localNccPresentationNames))
  {
    settings.m_presentation = *parsed;
  }
  if (
    const auto parsed =
      enumFromName<ProjectLocalMetricInvalidStyle>(j.value("invalidStyle", ""), k_localMetricInvalidStyleNames))
  {
    settings.m_invalidStyle = *parsed;
  }
  if (const auto value = j.find("negativeCorrelationAsMismatch"); value != j.end() && value->is_boolean()) {
    settings.m_negativeCorrelationAsMismatch = value->get<bool>();
  }
  if (const auto value = j.find("patchRadius"); value != j.end() && value->is_number_integer()) {
    settings.m_patchRadius = std::clamp(value->get<int>(), 1, 5);
  }
  if (const auto value = j.find("sampleSpacing"); value != j.end() && value->is_number()) {
    settings.m_sampleSpacing = std::clamp(value->get<float>(), 0.5f, 4.0f);
  }
  if (const auto value = j.find("minimumValidFraction"); value != j.end() && value->is_number()) {
    settings.m_minimumValidFraction = std::clamp(value->get<float>(), 0.1f, 1.0f);
  }
  if (const auto value = j.find("varianceEpsilon"); value != j.end() && value->is_number()) {
    settings.m_varianceEpsilon = std::max(value->get<float>(), 0.0f);
  }
}

void to_json(json& j, const ProjectLocalLinearResidualMetricSettings& settings)
{
  const ProjectLocalLinearResidualMetricSettings defaults;
  j = json::object();

  json metric = settings.m_metric;
  addIfNotEmpty(j, "metric", std::move(metric));
  addIfChanged(j, "patchRadius", settings.m_patchRadius, defaults.m_patchRadius);
  addIfChanged(j, "sampleSpacing", settings.m_sampleSpacing, defaults.m_sampleSpacing);
  addIfChanged(j, "minimumValidFraction", settings.m_minimumValidFraction, defaults.m_minimumValidFraction);
  addIfChanged(j, "varianceEpsilon", settings.m_varianceEpsilon, defaults.m_varianceEpsilon);
  addIfChanged(
    j,
    "invalidStyle",
    enumToName(settings.m_invalidStyle, k_localMetricInvalidStyleNames),
    enumToName(defaults.m_invalidStyle, k_localMetricInvalidStyleNames));
}

void from_json(const json& j, ProjectLocalLinearResidualMetricSettings& settings)
{
  if (const auto metric = j.find("metric"); metric != j.end() && metric->is_object()) {
    metric->get_to(settings.m_metric);
  }
  if (
    const auto parsed =
      enumFromName<ProjectLocalMetricInvalidStyle>(j.value("invalidStyle", ""), k_localMetricInvalidStyleNames))
  {
    settings.m_invalidStyle = *parsed;
  }
  if (const auto value = j.find("patchRadius"); value != j.end() && value->is_number_integer()) {
    settings.m_patchRadius = std::clamp(value->get<int>(), 1, 5);
  }
  if (const auto value = j.find("sampleSpacing"); value != j.end() && value->is_number()) {
    settings.m_sampleSpacing = std::clamp(value->get<float>(), 0.5f, 4.0f);
  }
  if (const auto value = j.find("minimumValidFraction"); value != j.end() && value->is_number()) {
    settings.m_minimumValidFraction = std::clamp(value->get<float>(), 0.1f, 1.0f);
  }
  if (const auto value = j.find("varianceEpsilon"); value != j.end() && value->is_number()) {
    settings.m_varianceEpsilon = std::max(value->get<float>(), 0.0f);
  }
}

void to_json(json& j, const ProjectComparisonSettings& settings)
{
  const ProjectComparisonSettings defaults;
  j = json::object();

  json difference = settings.m_difference;
  addIfNotEmpty(j, "difference", std::move(difference));

  json localNcc = settings.m_localNcc;
  addIfNotEmpty(j, "localNormalizedCrossCorrelation", std::move(localNcc));

  json localLinearResidual = settings.m_localLinearResidual;
  addIfNotEmpty(j, "localLinearResidual", std::move(localLinearResidual));

  json overlay = json::object();
  addIfChanged(overlay, "magentaCyan", settings.m_overlayMagentaCyan, defaults.m_overlayMagentaCyan);
  addIfNotEmpty(j, "overlay", std::move(overlay));

  json quadrants = json::object();
  addIfChanged(quadrants, "x", static_cast<bool>(settings.m_quadrants.x), static_cast<bool>(defaults.m_quadrants.x));
  addIfChanged(quadrants, "y", static_cast<bool>(settings.m_quadrants.y), static_cast<bool>(defaults.m_quadrants.y));
  addIfNotEmpty(j, "quadrants", std::move(quadrants));

  json checkerboard = json::object();
  addIfChanged(checkerboard, "squares", settings.m_checkerboardSquares, defaults.m_checkerboardSquares);
  addIfNotEmpty(j, "checkerboard", std::move(checkerboard));

  json flashlight = json::object();
  addIfChanged(flashlight, "radiusFraction", settings.m_flashlightRadiusFraction, defaults.m_flashlightRadiusFraction);
  addIfChanged(
    flashlight,
    "overlayMovingImage",
    settings.m_flashlightOverlayMovingImage,
    defaults.m_flashlightOverlayMovingImage);
  addIfNotEmpty(j, "flashlight", std::move(flashlight));
}

void from_json(const json& j, ProjectComparisonSettings& settings)
{
  if (const auto difference = j.find("difference"); difference != j.end() && difference->is_object()) {
    difference->get_to(settings.m_difference);
  }
  if (const auto localNcc = j.find("localNormalizedCrossCorrelation"); localNcc != j.end() && localNcc->is_object()) {
    localNcc->get_to(settings.m_localNcc);
  }
  if (const auto localResidual = j.find("localLinearResidual"); localResidual != j.end() && localResidual->is_object())
  {
    localResidual->get_to(settings.m_localLinearResidual);
  }
  if (const auto overlay = j.find("overlay"); overlay != j.end() && overlay->is_object()) {
    if (const auto value = overlay->find("magentaCyan"); value != overlay->end() && value->is_boolean()) {
      settings.m_overlayMagentaCyan = value->get<bool>();
    }
  }
  if (const auto quadrants = j.find("quadrants"); quadrants != j.end() && quadrants->is_object()) {
    bool x = static_cast<bool>(settings.m_quadrants.x);
    bool y = static_cast<bool>(settings.m_quadrants.y);
    if (const auto value = quadrants->find("x"); value != quadrants->end() && value->is_boolean()) {
      x = value->get<bool>();
    }
    if (const auto value = quadrants->find("y"); value != quadrants->end() && value->is_boolean()) {
      y = value->get<bool>();
    }
    settings.m_quadrants = glm::ivec2{x, y};
  }
  if (const auto checker = j.find("checkerboard"); checker != j.end() && checker->is_object()) {
    if (const auto value = checker->find("squares"); value != checker->end() && value->is_number_integer()) {
      settings.m_checkerboardSquares = std::clamp(value->get<int>(), 2, 2048);
    }
  }
  if (const auto flashlight = j.find("flashlight"); flashlight != j.end() && flashlight->is_object()) {
    if (const auto value = flashlight->find("radiusFraction"); value != flashlight->end() && value->is_number()) {
      settings.m_flashlightRadiusFraction = std::clamp(value->get<float>(), 0.01f, 1.0f);
    }
    if (const auto value = flashlight->find("overlayMovingImage"); value != flashlight->end() && value->is_boolean()) {
      settings.m_flashlightOverlayMovingImage = value->get<bool>();
    }
  }
}

void to_json(json& j, const ProjectRaycastingSettings& settings)
{
  const ProjectRaycastingSettings defaults;
  j = json::object();
  addIfChanged(j, "samplingFactor", settings.m_samplingFactor, defaults.m_samplingFactor);
  addIfChanged(
    j,
    "transparentBackgroundWhenNoHit",
    settings.m_transparentBackgroundWhenNoHit,
    defaults.m_transparentBackgroundWhenNoHit);
  addIfChanged(
    j,
    "showImageBox",
    settings.m_backgroundEdgeBrighteningEnabled,
    defaults.m_backgroundEdgeBrighteningEnabled);
  addIfChanged(j, "showCrosshairsGlyph", settings.m_showCrosshairsIn3D, defaults.m_showCrosshairsIn3D);
  addIfChanged(
    j,
    "crosshairsGlyphDiameterVoxels",
    settings.m_crosshairs3DGlyphDiameterVoxelDiagonals,
    defaults.m_crosshairs3DGlyphDiameterVoxelDiagonals);
  addIfChanged(
    j,
    "showCameraFrustumIn2DViews",
    settings.m_showThreeDCameraFrustumIn2DViews,
    defaults.m_showThreeDCameraFrustumIn2DViews);
  addIfChanged(
    j,
    "reverseRotateAboutEye",
    settings.m_reverseThreeDRotateAboutEye,
    defaults.m_reverseThreeDRotateAboutEye);
  addIfChanged(
    j,
    "cameraFrustumColor",
    vec4ToJson(settings.m_threeDCameraFrustumColor),
    vec4ToJson(defaults.m_threeDCameraFrustumColor));
  addIfChanged(j, "renderFrontFaces", settings.m_renderFrontFaces, defaults.m_renderFrontFaces);
  addIfChanged(j, "renderBackFaces", settings.m_renderBackFaces, defaults.m_renderBackFaces);
  addIfChanged(
    j,
    "segmentationMasking",
    enumToName(settings.m_segmentationMasking, k_raycastSegmentationMaskingNames),
    enumToName(defaults.m_segmentationMasking, k_raycastSegmentationMaskingNames));
}

void from_json(const json& j, ProjectRaycastingSettings& settings)
{
  if (const auto value = j.find("samplingFactor"); value != j.end() && value->is_number()) {
    settings.m_samplingFactor = std::clamp(value->get<float>(), 0.5f, 2.0f);
  }
  if (const auto value = j.find("transparentBackgroundWhenNoHit"); value != j.end() && value->is_boolean()) {
    settings.m_transparentBackgroundWhenNoHit = value->get<bool>();
  }
  if (const auto value = j.find("showImageBox"); value != j.end() && value->is_boolean()) {
    settings.m_backgroundEdgeBrighteningEnabled = value->get<bool>();
  }
  if (const auto value = j.find("showCrosshairsGlyph"); value != j.end() && value->is_boolean()) {
    settings.m_showCrosshairsIn3D = value->get<bool>();
  }
  if (const auto value = j.find("crosshairsGlyphDiameterVoxels"); value != j.end() && value->is_number()) {
    settings.m_crosshairs3DGlyphDiameterVoxelDiagonals = std::clamp(value->get<float>(), 0.1f, 10.0f);
  }
  if (const auto value = j.find("showCameraFrustumIn2DViews"); value != j.end() && value->is_boolean()) {
    settings.m_showThreeDCameraFrustumIn2DViews = value->get<bool>();
  }
  if (const auto value = j.find("reverseRotateAboutEye"); value != j.end() && value->is_boolean()) {
    settings.m_reverseThreeDRotateAboutEye = value->get<bool>();
  }
  if (const auto value = j.find("cameraFrustumColor"); value != j.end() && value->is_array() && value->size() == 4) {
    settings.m_threeDCameraFrustumColor = glm::clamp(vec4FromJson(*value), glm::vec4{0.0f}, glm::vec4{1.0f});
  }
  if (const auto value = j.find("renderFrontFaces"); value != j.end() && value->is_boolean()) {
    settings.m_renderFrontFaces = value->get<bool>();
  }
  if (const auto value = j.find("renderBackFaces"); value != j.end() && value->is_boolean()) {
    settings.m_renderBackFaces = value->get<bool>();
  }
  if (
    const auto parsed = enumFromName<ProjectSegmentationRaycastMasking>(
      j.value("segmentationMasking", ""),
      k_raycastSegmentationMaskingNames))
  {
    settings.m_segmentationMasking = *parsed;
  }
}

void to_json(json& j, const ProjectIntensityProjectionSettings& settings)
{
  const ProjectIntensityProjectionSettings defaults;
  j = json::object();
  addIfChanged(j, "useMaximumImageExtent", settings.m_useMaximumImageExtent, defaults.m_useMaximumImageExtent);
  addIfChanged(j, "slabThicknessMm", settings.m_slabThicknessMm, defaults.m_slabThicknessMm);
  addIfChanged(j, "xrayEnergyKeV", settings.m_xrayEnergyKeV, defaults.m_xrayEnergyKeV);
  addIfChanged(j, "xrayWindow", settings.m_xrayWindow, defaults.m_xrayWindow);
  addIfChanged(j, "xrayLevel", settings.m_xrayLevel, defaults.m_xrayLevel);
}

void from_json(const json& j, ProjectIntensityProjectionSettings& settings)
{
  if (const auto value = j.find("useMaximumImageExtent"); value != j.end() && value->is_boolean()) {
    settings.m_useMaximumImageExtent = value->get<bool>();
  }
  if (const auto value = j.find("slabThicknessMm"); value != j.end() && value->is_number()) {
    settings.m_slabThicknessMm = std::max(value->get<float>(), 0.0f);
  }
  if (const auto value = j.find("xrayEnergyKeV"); value != j.end() && value->is_number()) {
    settings.m_xrayEnergyKeV = value->get<float>();
  }
  if (const auto value = j.find("xrayWindow"); value != j.end() && value->is_number()) {
    settings.m_xrayWindow = std::clamp(value->get<float>(), 1.0e-3f, 1.0f);
  }
  if (const auto value = j.find("xrayLevel"); value != j.end() && value->is_number()) {
    settings.m_xrayLevel = std::clamp(value->get<float>(), 0.0f, 1.0f);
  }
}

void to_json(json& j, const ProjectSegmentationDisplaySettings& settings)
{
  const ProjectSegmentationDisplaySettings defaults;
  j = json::object();
  addIfChanged(
    j,
    "modulateOpacityWithImageOpacity",
    settings.m_modulateOpacityWithImageOpacity,
    defaults.m_modulateOpacityWithImageOpacity);
  addIfChanged(
    j,
    "outlineStyle",
    enumToName(settings.m_outlineStyle, k_segmentationOutlineNames),
    enumToName(defaults.m_outlineStyle, k_segmentationOutlineNames));
  addIfChanged(j, "interiorOpacity", settings.m_interiorOpacity, defaults.m_interiorOpacity);
  addIfChanged(j, "erosionFactor", settings.m_erosionFactor, defaults.m_erosionFactor);
}

void from_json(const json& j, ProjectSegmentationDisplaySettings& settings)
{
  if (const auto value = j.find("modulateOpacityWithImageOpacity"); value != j.end() && value->is_boolean()) {
    settings.m_modulateOpacityWithImageOpacity = value->get<bool>();
  }
  if (
    const auto parsed = enumFromName<SegmentationOutlineStyle>(j.value("outlineStyle", ""), k_segmentationOutlineNames))
  {
    settings.m_outlineStyle = *parsed;
  }
  if (const auto value = j.find("interiorOpacity"); value != j.end() && value->is_number()) {
    settings.m_interiorOpacity = std::clamp(value->get<float>(), 0.0f, 1.0f);
  }
  if (const auto value = j.find("erosionFactor"); value != j.end() && value->is_number()) {
    settings.m_erosionFactor = std::clamp(value->get<float>(), 0.5f, 1.0f);
  }
}

void to_json(json& j, const ProjectIsosurfaceDisplaySettings& settings)
{
  const ProjectIsosurfaceDisplaySettings defaults;
  j = json::object();
  addIfChanged(
    j,
    "floatingPointInterpolationPolicy",
    enumToName(settings.m_floatingPointInterpolationPolicy, k_floatingPointInterpolationPolicyNames),
    enumToName(defaults.m_floatingPointInterpolationPolicy, k_floatingPointInterpolationPolicyNames));
  addIfChanged(
    j,
    "modulateOpacityWithImageOpacity",
    settings.m_modulateOpacityWithImageOpacity,
    defaults.m_modulateOpacityWithImageOpacity);
}

void from_json(const json& j, ProjectIsosurfaceDisplaySettings& settings)
{
  if (const auto policy = j.find("floatingPointInterpolationPolicy"); policy != j.end() && policy->is_string()) {
    if (
      const auto parsed = enumFromName<FloatingPointLinearInterpolationPolicy>(
        policy->get<std::string>(),
        k_floatingPointInterpolationPolicyNames))
    {
      settings.m_floatingPointInterpolationPolicy = *parsed;
    }
  }
  if (const auto value = j.find("modulateOpacityWithImageOpacity"); value != j.end() && value->is_boolean()) {
    settings.m_modulateOpacityWithImageOpacity = value->get<bool>();
  }
}

void to_json(json& j, const ProjectAnnotationDisplaySettings& settings)
{
  const ProjectAnnotationDisplaySettings defaults;
  j = json::object();

  json rendering = json::object();
  addIfChanged(rendering, "annotationsOnTop", settings.m_annotationsOnTop, defaults.m_annotationsOnTop);
  addIfChanged(rendering, "landmarksOnTop", settings.m_landmarksOnTop, defaults.m_landmarksOnTop);
  addIfChanged(
    rendering,
    "hideAnnotationVertices",
    settings.m_hideAnnotationVertices,
    defaults.m_hideAnnotationVertices);
  addIfNotEmpty(j, "rendering", std::move(rendering));
}

void from_json(const json& j, ProjectAnnotationDisplaySettings& settings)
{
  const auto rendering = j.find("rendering");
  if (rendering == j.end() || !rendering->is_object()) {
    return;
  }

  if (const auto value = rendering->find("annotationsOnTop"); value != rendering->end() && value->is_boolean()) {
    settings.m_annotationsOnTop = value->get<bool>();
  }
  if (const auto value = rendering->find("landmarksOnTop"); value != rendering->end() && value->is_boolean()) {
    settings.m_landmarksOnTop = value->get<bool>();
  }
  if (const auto value = rendering->find("hideAnnotationVertices"); value != rendering->end() && value->is_boolean()) {
    settings.m_hideAnnotationVertices = value->get<bool>();
  }
}

void to_json(json& j, const RegistrationResult& result)
{
  j = json::object();

  if (!result.m_backend.empty()) {
    j["backend"] = result.m_backend;
  }

  optionalPathObjectToJson(j, "fixedImage", result.m_fixedImage);
  optionalPathObjectToJson(j, "movingImage", result.m_movingImage);
  optionalPathObjectToJson(j, "manifest", result.m_manifestFileName);
  optionalPathObjectToJson(j, "warpedImage", result.m_warpedImage);
  optionalPathObjectToJson(j, "inverseWarpField", result.m_inverseWarpField);
  optionalPathObjectToJson(j, "forwardWarpField", result.m_forwardWarpField);
  optionalPathObjectToJson(j, "affine", result.m_affineTransform);
  if (!result.m_warpedSegmentations.empty()) {
    j["warpedSegmentations"] = pathObjectVectorToJson(result.m_warpedSegmentations);
  }
  if (!result.m_transformedSurfaces.empty()) {
    j["transformedSurfaces"] = pathObjectVectorToJson(result.m_transformedSurfaces);
  }
  if (!result.m_transformedLandmarks.empty()) {
    j["transformedLandmarks"] = pathObjectVectorToJson(result.m_transformedLandmarks);
  }
  if (!result.m_warnings.empty()) {
    j["warnings"] = result.m_warnings;
  }
}

void from_json(const json& j, RegistrationResult& result)
{
  if (const auto value = j.find("backend"); value != j.end() && value->is_string()) {
    result.m_backend = value->get<std::string>();
  }
  optionalPathObjectFromJson(j, "fixedImage", result.m_fixedImage);
  optionalPathObjectFromJson(j, "movingImage", result.m_movingImage);
  optionalPathObjectFromJson(j, "manifest", result.m_manifestFileName);
  optionalPathObjectFromJson(j, "warpedImage", result.m_warpedImage);
  optionalPathObjectFromJson(j, "inverseWarpField", result.m_inverseWarpField);
  optionalPathObjectFromJson(j, "forwardWarpField", result.m_forwardWarpField);
  optionalPathObjectFromJson(j, "affine", result.m_affineTransform);
  if (const auto value = j.find("warpedSegmentations"); value != j.end()) {
    result.m_warpedSegmentations = pathObjectVectorFromJson(*value, "warpedSegmentations");
  }
  if (const auto value = j.find("transformedSurfaces"); value != j.end()) {
    result.m_transformedSurfaces = pathObjectVectorFromJson(*value, "transformedSurfaces");
  }
  if (const auto value = j.find("transformedLandmarks"); value != j.end()) {
    result.m_transformedLandmarks = pathObjectVectorFromJson(*value, "transformedLandmarks");
  }
  if (const auto value = j.find("warnings"); value != j.end() && value->is_array()) {
    result.m_warnings = value->get<std::vector<std::string>>();
  }
}

void to_json(json& j, const DefaultLayoutOverride& override)
{
  j = json{{"index", override.m_index}, {"layout", override.m_layout}};
}

void from_json(const json& j, DefaultLayoutOverride& override)
{
  override.m_index = j.at("index").get<std::size_t>();
  override.m_layout = j.at("layout").get<layout::LayoutSpec>();
}

void to_json(json& j, const EntropyProject& project)
{
  std::vector<Image> images;
  images.reserve(project.m_additionalImages.size() + 1);
  images.push_back(project.m_referenceImage);
  images.insert(images.end(), project.m_additionalImages.begin(), project.m_additionalImages.end());

  j = json{{"version", versionToJson(k_projectFormatMajorVersion, k_projectFormatMinorVersion)}, {"images", images}};
  if (
    project.m_layoutsFileName || !project.m_layouts.empty() || !project.m_removedDefaultLayoutIndices.empty() ||
    !project.m_modifiedDefaultLayouts.empty() || project.m_currentLayoutIndex)
  {
    json layouts = json::object();
    if (project.m_layoutsFileName) {
      layouts["path"] = pathToString(*project.m_layoutsFileName);
    }
    else if (!project.m_layouts.empty()) {
      layouts["embedded"] = project.m_layouts;
    }
    if (!project.m_removedDefaultLayoutIndices.empty()) {
      layouts["removedDefaults"] = project.m_removedDefaultLayoutIndices;
    }
    if (!project.m_modifiedDefaultLayouts.empty()) {
      layouts["modifiedDefaults"] = project.m_modifiedDefaultLayouts;
    }
    if (project.m_currentLayoutIndex) {
      layouts["current"] = *project.m_currentLayoutIndex;
    }
    j["layouts"] = std::move(layouts);
  }

  json settings = json::object();
  json interface = project.m_interface;
  addIfNotEmpty(settings, "interface", std::move(interface));

  json view = project.m_view;
  addIfNotEmpty(settings, "view", std::move(view));

  json rendering = json::object();
  json comparison = project.m_comparison;
  addIfNotEmpty(rendering, "comparison", std::move(comparison));
  json raycasting = project.m_raycasting;
  addIfNotEmpty(rendering, "volumeRaycasting", std::move(raycasting));
  json intensityProjection = project.m_intensityProjection;
  addIfNotEmpty(rendering, "intensityProjection", std::move(intensityProjection));
  json segmentation = project.m_segmentationDisplay;
  addIfNotEmpty(rendering, "segmentation", std::move(segmentation));
  json isosurfaces = project.m_isosurfaces;
  addIfNotEmpty(rendering, "isosurfaces", std::move(isosurfaces));
  addIfNotEmpty(settings, "rendering", std::move(rendering));

  json annotations = project.m_annotationDisplay;
  addIfNotEmpty(settings, "annotations", std::move(annotations));
  addIfNotEmpty(j, "settings", std::move(settings));

  if (!project.m_registrationResults.empty()) {
    j["registrationResults"] = project.m_registrationResults;
  }
}

void from_json(const json& j, EntropyProject& project)
{
  if (const auto versionValue = j.find("version"); versionValue != j.end()) {
    const JsonVersion version = versionFromJson(*versionValue);
    if (version.major != k_projectFormatMajorVersion || version.minor != k_projectFormatMinorVersion) {
      throwDebug(
        "Unsupported Entropy project JSON version " + std::to_string(version.major) + "." +
        std::to_string(version.minor));
    }
  }

  std::vector<Image> images = j.at("images").get<std::vector<Image>>();
  if (images.empty()) {
    throwDebug("Entropy project JSON must contain at least one image");
  }

  project.m_referenceImage = std::move(images.front());
  project.m_additionalImages.clear();
  project.m_additionalImages.reserve(images.size() - 1);
  for (std::size_t i = 1; i < images.size(); ++i) {
    project.m_additionalImages.push_back(std::move(images.at(i)));
  }
  if (const auto layouts = j.find("layouts"); layouts != j.end()) {
    if (!layouts->is_object()) {
      throwDebug("Project layouts entry must be an object");
    }
    if (const auto path = layouts->find("path"); path != layouts->end() && path->is_string()) {
      project.m_layoutsFileName = path->get<std::string>();
    }
    if (const auto embedded = layouts->find("embedded"); embedded != layouts->end()) {
      embedded->get_to(project.m_layouts);
    }
    if (const auto removedDefaults = layouts->find("removedDefaults"); removedDefaults != layouts->end()) {
      removedDefaults->get_to(project.m_removedDefaultLayoutIndices);
    }
    if (const auto modifiedDefaults = layouts->find("modifiedDefaults"); modifiedDefaults != layouts->end()) {
      modifiedDefaults->get_to(project.m_modifiedDefaultLayouts);
    }
    if (const auto current = layouts->find("current"); current != layouts->end() && !current->is_null()) {
      project.m_currentLayoutIndex = current->get<std::size_t>();
    }
  }
  if (const auto settings = j.find("settings"); settings != j.end() && settings->is_object()) {
    if (const auto interface = settings->find("interface"); interface != settings->end() && interface->is_object()) {
      interface->get_to(project.m_interface);
    }
    if (const auto view = settings->find("view"); view != settings->end() && view->is_object()) {
      view->get_to(project.m_view);
    }
    if (const auto rendering = settings->find("rendering"); rendering != settings->end() && rendering->is_object()) {
      if (const auto comparison = rendering->find("comparison");
          comparison != rendering->end() && comparison->is_object())
      {
        comparison->get_to(project.m_comparison);
      }
      if (const auto raycasting = rendering->find("volumeRaycasting");
          raycasting != rendering->end() && raycasting->is_object())
      {
        raycasting->get_to(project.m_raycasting);
      }
      if (const auto intensityProjection = rendering->find("intensityProjection");
          intensityProjection != rendering->end() && intensityProjection->is_object())
      {
        intensityProjection->get_to(project.m_intensityProjection);
      }
      if (const auto segmentation = rendering->find("segmentation");
          segmentation != rendering->end() && segmentation->is_object())
      {
        segmentation->get_to(project.m_segmentationDisplay);
      }
      if (const auto isosurfaces = rendering->find("isosurfaces");
          isosurfaces != rendering->end() && isosurfaces->is_object())
      {
        isosurfaces->get_to(project.m_isosurfaces);
      }
    }
    if (const auto annotations = settings->find("annotations");
        annotations != settings->end() && annotations->is_object())
    {
      annotations->get_to(project.m_annotationDisplay);
    }
  }
  if (const auto registrationResults = j.find("registrationResults");
      registrationResults != j.end() && registrationResults->is_array())
  {
    registrationResults->get_to(project.m_registrationResults);
  }
}

} // namespace serialize
