#include "logic/app/UserPreferences.h"

#include "common/LoggingSettings.h"
#include "rendering/RenderData.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{

using json = nlohmann::json;

template<typename Enum>
struct EnumName
{
  Enum value;
  std::string_view name;
};

template<typename Enum, std::size_t N>
const char* enumToName(Enum value, const std::array<EnumName<Enum>, N>& names)
{
  const auto it =
    std::find_if(names.begin(), names.end(), [value](const EnumName<Enum>& entry) { return entry.value == value; });
  return it == names.end() ? names.front().name.data() : it->name.data();
}

template<typename Enum, std::size_t N>
std::optional<Enum> enumFromName(std::string_view name, const std::array<EnumName<Enum>, N>& names)
{
  const auto it =
    std::find_if(names.begin(), names.end(), [name](const EnumName<Enum>& entry) { return entry.name == name; });
  return it == names.end() ? std::nullopt : std::optional<Enum>{it->value};
}

template<typename Enum, std::size_t N>
void setEnumFromJson(Enum& value, const json& object, const char* key, const std::array<EnumName<Enum>, N>& names)
{
  const auto it = object.find(key);
  if (it == object.end() || !it->is_string()) {
    return;
  }

  if (const auto parsed = enumFromName<Enum>(it->get<std::string>(), names)) {
    value = *parsed;
  }
}

template<typename Value>
void setFromJson(Value& value, const json& object, const char* key)
{
  const auto it = object.find(key);
  if (it == object.end() || it->is_null()) {
    return;
  }

  try {
    value = it->get<Value>();
  }
  catch (const json::exception&) {
  }
}

void setFloatFromJson(float& value, const json& object, const char* key, float minValue, float maxValue)
{
  const auto it = object.find(key);
  if (it == object.end() || !it->is_number()) {
    return;
  }

  value = std::clamp(it->get<float>(), minValue, maxValue);
}

void setDoubleFromJson(double& value, const json& object, const char* key, double minValue, double maxValue)
{
  const auto it = object.find(key);
  if (it == object.end() || !it->is_number()) {
    return;
  }

  value = std::clamp(it->get<double>(), minValue, maxValue);
}

json vec2ToJson(const glm::vec2& value)
{
  return json::array({value.x, value.y});
}

json vec3ToJson(const glm::vec3& value)
{
  return json::array({value.x, value.y, value.z});
}

json vec4ToJson(const glm::vec4& value)
{
  return json::array({value.x, value.y, value.z, value.w});
}

void setVec2FromJson(glm::vec2& value, const json& object, const char* key)
{
  const auto it = object.find(key);
  if (it == object.end() || !it->is_array() || it->size() != 2) {
    return;
  }

  value = glm::vec2{it->at(0).get<float>(), it->at(1).get<float>()};
}

void setVec3FromJson(glm::vec3& value, const json& object, const char* key)
{
  const auto it = object.find(key);
  if (it == object.end() || !it->is_array() || it->size() != 3) {
    return;
  }

  value = glm::vec3{it->at(0).get<float>(), it->at(1).get<float>(), it->at(2).get<float>()};
}

void setVec4FromJson(glm::vec4& value, const json& object, const char* key)
{
  const auto it = object.find(key);
  if (it == object.end() || !it->is_array() || it->size() != 4) {
    return;
  }

  value = glm::vec4{it->at(0).get<float>(), it->at(1).get<float>(), it->at(2).get<float>(), it->at(3).get<float>()};
}

constexpr std::array sk_uiFontNames{
  EnumName{UiFontFamily::SpaceGrotesk, "spaceGrotesk"},
  EnumName{UiFontFamily::Inter, "inter"},
  EnumName{UiFontFamily::NotoSans, "notoSans"},
  EnumName{UiFontFamily::Roboto, "roboto"},
  EnumName{UiFontFamily::SourceSans3, "sourceSans3"},
  EnumName{UiFontFamily::IBMPlexSans, "ibmPlexSans"},
  EnumName{UiFontFamily::Cousine, "cousine"}};

constexpr std::array sk_uiColorPresetNames{
  EnumName{UiColorPreset::EntropyDark, "entropyDark"},
  EnumName{UiColorPreset::ImGuiDark, "imguiDark"},
  EnumName{UiColorPreset::ImGuiClassic, "imguiClassic"},
  EnumName{UiColorPreset::ImGuiLight, "imguiLight"},
  EnumName{UiColorPreset::SlateBlue, "slateBlue"},
  EnumName{UiColorPreset::Graphite, "graphite"},
  EnumName{UiColorPreset::DeepTeal, "deepTeal"},
  EnumName{UiColorPreset::Midnight, "midnight"},
  EnumName{UiColorPreset::SoftLight, "softLight"}};

constexpr std::array sk_uiDensityPresetNames{
  EnumName{UiDensityPreset::Compact, "compact"},
  EnumName{UiDensityPreset::Default, "default"},
  EnumName{UiDensityPreset::Comfortable, "comfortable"}};

constexpr std::array sk_crosshairsSnappingNames{
  EnumName{CrosshairsSnapping::Disabled, "disabled"},
  EnumName{CrosshairsSnapping::ReferenceImage, "referenceImage"},
  EnumName{CrosshairsSnapping::ActiveImage, "activeImage"}};

constexpr std::array sk_anatomicalLabelNames{
  EnumName{AnatomicalLabelType::Human, "human"},
  EnumName{AnatomicalLabelType::Cartesian, "cartesian"},
  EnumName{AnatomicalLabelType::Rodent, "rodent"},
  EnumName{AnatomicalLabelType::Disabled, "disabled"}};

constexpr std::array sk_scaleBarPositionNames{
  EnumName{ScaleBarPosition::BottomRight, "rightBottom"},
  EnumName{ScaleBarPosition::Right, "right"},
  EnumName{ScaleBarPosition::TopRight, "rightTop"},
  EnumName{ScaleBarPosition::Top, "top"},
  EnumName{ScaleBarPosition::TopLeft, "leftTop"},
  EnumName{ScaleBarPosition::Left, "left"},
  EnumName{ScaleBarPosition::BottomLeft, "leftBottom"},
  EnumName{ScaleBarPosition::Bottom, "bottom"}};

constexpr std::array sk_scaleBarOrientationNames{
  EnumName{ScaleBarOrientation::Horizontal, "horizontal"},
  EnumName{ScaleBarOrientation::Vertical, "vertical"}};

constexpr std::array sk_scaleBarTicksNames{
  EnumName{ScaleBarTicks::Endpoints, "endpoints"},
  EnumName{ScaleBarTicks::Automatic, "automatic"}};

constexpr std::array sk_segmentationOutlineNames{
  EnumName{SegmentationOutlineStyle::Disabled, "disabled"},
  EnumName{SegmentationOutlineStyle::ViewPixel, "pixel"},
  EnumName{SegmentationOutlineStyle::ImageVoxel, "voxel"}};

constexpr std::array sk_brushPreviewModeNames{
  EnumName{BrushPreviewMode::Disabled, "disabled"},
  EnumName{BrushPreviewMode::Hover, "hover"}};

constexpr std::array sk_brushPreviewVoxelsNames{
  EnumName{BrushPreviewVoxels::Changed, "changed"},
  EnumName{BrushPreviewVoxels::All, "all"}};

constexpr std::array sk_brushPreviewStyleNames{
  EnumName{BrushPreviewStyle::Outline, "outline"},
  EnumName{BrushPreviewStyle::OutlineAndFill, "outlineAndFill"}};

constexpr std::array sk_raycastSegMaskingNames{
  EnumName{RenderData::SegMaskingForRaycasting::Disabled, "disabled"},
  EnumName{RenderData::SegMaskingForRaycasting::SegMasksIn, "maskIn"},
  EnumName{RenderData::SegMaskingForRaycasting::SegMasksOut, "maskOut"}};

json metricParamsToJson(const RenderData::MetricParams& params)
{
  return {
    {"windowSlopeIntercept", vec2ToJson(params.m_slopeIntercept)},
    {"colormapIndex", params.m_colorMapIndex},
    {"invertColormap", params.m_invertCmap},
    {"continuousColormap", params.m_cmapContinuous},
    {"colormapLevels", params.m_cmapQuantizationLevels}};
}

void applyMetricParamsFromJson(RenderData::MetricParams& params, const json& object)
{
  setVec2FromJson(params.m_slopeIntercept, object, "windowSlopeIntercept");
  setFromJson(params.m_colorMapIndex, object, "colormapIndex");
  setFromJson(params.m_invertCmap, object, "invertColormap");
  setFromJson(params.m_cmapContinuous, object, "continuousColormap");
  setFromJson(params.m_cmapQuantizationLevels, object, "colormapLevels");
  params.m_cmapQuantizationLevels = std::max(params.m_cmapQuantizationLevels, 2);
}

json toJson(const AppSettings& settings, const RenderData& renderData)
{
  json uiScale = "auto";
  if (settings.uiScaleOverride()) {
    uiScale = *settings.uiScaleOverride();
  }

  return {
    {"format", "entropy.userSettings"},
    {"version", 1},
    {"interface",
     {{"uiScale", uiScale},
      {"font", enumToName(settings.uiFontFamily(), sk_uiFontNames)},
      {"colorScheme", enumToName(settings.uiColorPreset(), sk_uiColorPresetNames)},
      {"density", enumToName(settings.uiDensityPreset(), sk_uiDensityPresetNames)},
      {"windowBackgroundOpacity", settings.uiWindowBgOpacity()}}},
    {"views",
     {{"showImageBorders", renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections},
      {"lockAnatomicalDirectionsToReferenceImage", settings.lockAnatomicalCoordinateAxesWithReferenceImage()},
      {"crosshairs",
       {{"color", vec4ToJson(renderData.m_crosshairsColor)},
        {"snapping", enumToName(renderData.m_snapCrosshairs, sk_crosshairsSnappingNames)}}},
      {"synchronizeViewZooms", settings.synchronizeZooms()},
      {"backgrounds",
       {{"2d", vec3ToJson(renderData.m_2dBackgroundColor)}, {"3d", vec4ToJson(renderData.m_3dBackgroundColor)}}},
      {"anatomicalLabels",
       {{"color", vec4ToJson(renderData.m_anatomicalLabelColor)},
        {"type", enumToName(renderData.m_anatomicalLabelType, sk_anatomicalLabelNames)}}},
      {"scaleBars",
       {{"show", renderData.m_showScaleBars},
        {"showInLightboxViews", renderData.m_showScaleBarsInLightboxViews},
        {"color", vec4ToJson(renderData.m_scaleBarColor)},
        {"position", enumToName(renderData.m_scaleBarPosition, sk_scaleBarPositionNames)},
        {"orientation", enumToName(renderData.m_scaleBarOrientation, sk_scaleBarOrientationNames)},
        {"targetLengthFraction", renderData.m_scaleBarTargetFraction},
        {"marginPixels", renderData.m_scaleBarMarginPx},
        {"ticks", enumToName(renderData.m_scaleBarTicks, sk_scaleBarTicksNames)}}},
      {"lightbox",
       {{"showOffsetLabels", renderData.m_showLightboxOffsetLabels},
        {"offsetLabelColor", vec4ToJson(renderData.m_lightboxOffsetLabelColor)}}}}},
    {"images",
     {{"floatingPointLinearInterpolation", renderData.m_imageGrayFloatingPointInterpolation},
      {"intensityProjectionDefaults",
       {{"useMaximumImageExtent", renderData.m_doMaxExtentIntensityProjection},
        {"slabThicknessMm", renderData.m_intensityProjectionSlabThickness},
        {"xrayEnergyKeV", renderData.m_xrayEnergyKeV},
        {"xrayWindow", renderData.m_xrayIntensityWindow},
        {"xrayLevel", renderData.m_xrayIntensityLevel}}}}},
    {"segmentation",
     {{"display",
       {{"modulateWithImageOpacity", renderData.m_modulateSegOpacityWithImageOpacity},
        {"outlineStyle", enumToName(renderData.m_segOutlineStyle, sk_segmentationOutlineNames)},
        {"interiorOpacity", renderData.m_segInteriorOpacity},
        {"erosionFactor", renderData.m_segInterpCutoff}}},
      {"brush",
       {{"replaceBackgroundWithForeground", settings.replaceBackgroundWithForeground()},
        {"use3d", settings.use3dBrush()},
        {"useIsotropic", settings.useIsotropicBrush()},
        {"useVoxelSize", settings.useVoxelBrushSize()},
        {"useRound", settings.useRoundBrush()},
        {"crosshairsMoveWithBrush", settings.crosshairsMoveWithBrush()},
        {"sizeVoxels", settings.brushSizeInVoxels()},
        {"sizeMm", settings.brushSizeInMm()}}},
      {"brushPreview",
       {{"mode", enumToName(settings.brushPreviewMode(), sk_brushPreviewModeNames)},
        {"voxels", enumToName(settings.brushPreviewVoxels(), sk_brushPreviewVoxelsNames)},
        {"style", enumToName(settings.brushPreviewStyle(), sk_brushPreviewStyleNames)},
        {"fillOpacity", settings.brushPreviewFillOpacity()},
        {"showWhilePainting", settings.brushPreviewWhilePainting()},
        {"outlineStyle", enumToName(settings.brushPreviewOutlineStyle(), sk_segmentationOutlineNames)}}}}},
    {"comparison",
     {{"difference",
       {{"squared", renderData.m_useSquare}, {"metric", metricParamsToJson(renderData.m_squaredDifferenceParams)}}},
      {"overlay", {{"magentaCyan", renderData.m_overlayMagentaCyan}}},
      {"quadrants",
       {{"x", static_cast<bool>(renderData.m_quadrants.x)}, {"y", static_cast<bool>(renderData.m_quadrants.y)}}},
      {"checkerboard", {{"squares", renderData.m_numCheckerboardSquares}}},
      {"flashlight",
       {{"radiusFraction", renderData.m_flashlightRadius}, {"overlayMovingImage", renderData.m_flashlightOverlays}}}}},
    {"rendering",
     {{"frameRate",
       {{"limit", renderData.m_manualFramerateLimiter},
        {"targetFrameTimeSeconds", renderData.m_targetFrameTimeSeconds}}},
      {"raycasting",
       {{"samplingFactor", renderData.m_raycastSamplingFactor},
        {"transparentBackgroundWhenNoHit", renderData.m_3dTransparentIfNoHit},
        {"renderFrontFaces", renderData.m_renderFrontFaces},
        {"renderBackFaces", renderData.m_renderBackFaces},
        {"segmentationMasking", enumToName(renderData.m_segMasking, sk_raycastSegMaskingNames)}}},
      {"asciiShading",
       {{"enabled", renderData.m_asciiEnabled},
        {"cellSizePx", vec2ToJson(renderData.m_asciiCellSizePx)},
        {"charsetIndex", renderData.m_asciiCharsetIndex},
        {"foregroundColor", vec3ToJson(renderData.m_asciiFgColor)},
        {"backgroundColor", vec3ToJson(renderData.m_asciiBgColor)},
        {"backgroundAlpha", renderData.m_asciiBgAlpha},
        {"useColormapAsForeground", renderData.m_asciiUseColormap},
        {"spatialMatching", renderData.m_asciiSpatialMode},
        {"spatialExponent", renderData.m_asciiSpatialExponent}}}}},
    {"annotations",
     {{"annotationsOnTop", renderData.m_globalAnnotationParams.renderOnTopOfAllImagePlanes},
      {"landmarksOnTop", renderData.m_globalLandmarkParams.renderOnTopOfAllImagePlanes},
      {"hideAnnotationVertices", renderData.m_globalAnnotationParams.hidePolygonVertices},
      {"crosshairsMoveWhileAnnotating", settings.crosshairsMoveWhileAnnotating()}}},
    {"synchronization",
     {{"itkSnap",
       {{"enabled", settings.cursorSyncEnabled()},
        {"sendCursor", settings.sendCursorSync()},
        {"receiveCursor", settings.receiveCursorSync()},
        {"sendZoom", settings.sendZoomSync()},
        {"receiveZoom", settings.receiveZoomSync()},
        {"sendPan", settings.sendPanSync()},
        {"receivePan", settings.receivePanSync()}}},
      {"entropyInstances", {{"enabled", settings.entropyInstanceSyncEnabled()}}}}},
    {"system",
     {{"diagnostics",
       {{"logVerbosity", std::string{entropy::logging::logLevelLabel(entropy::logging::defaultLoggerSinkLevel())}}}}}}};
}

void applyJson(AppSettings& settings, RenderData& renderData, const json& root)
{
  if (const auto interface = root.find("interface"); interface != root.end() && interface->is_object()) {
    if (const auto scale = interface->find("uiScale"); scale != interface->end()) {
      if (scale->is_string() && scale->get<std::string>() == "auto") {
        settings.setUiScaleOverride(std::nullopt);
      }
      else if (scale->is_number()) {
        settings.setUiScaleOverride(scale->get<float>());
      }
    }

    if (const auto parsed = enumFromName<UiFontFamily>(interface->value("font", ""), sk_uiFontNames)) {
      settings.setUiFontFamily(*parsed);
    }
    if (const auto parsed = enumFromName<UiColorPreset>(interface->value("colorScheme", ""), sk_uiColorPresetNames)) {
      settings.setUiColorPreset(*parsed);
    }
    if (const auto parsed = enumFromName<UiDensityPreset>(interface->value("density", ""), sk_uiDensityPresetNames)) {
      settings.setUiDensityPreset(*parsed);
    }
    if (const auto opacity = interface->find("windowBackgroundOpacity");
        opacity != interface->end() && opacity->is_number())
    {
      settings.setUiWindowBgOpacity(opacity->get<float>());
    }
  }

  if (const auto views = root.find("views"); views != root.end() && views->is_object()) {
    setFromJson(
      renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections,
      *views,
      "showImageBorders");
    if (const auto lock = views->find("lockAnatomicalDirectionsToReferenceImage");
        lock != views->end() && lock->is_boolean())
    {
      settings.setLockAnatomicalCoordinateAxesWithReferenceImage(lock->get<bool>());
    }

    if (const auto crosshairs = views->find("crosshairs"); crosshairs != views->end() && crosshairs->is_object()) {
      setVec4FromJson(renderData.m_crosshairsColor, *crosshairs, "color");
      setEnumFromJson(renderData.m_snapCrosshairs, *crosshairs, "snapping", sk_crosshairsSnappingNames);
    }
    if (const auto syncZooms = views->find("synchronizeViewZooms");
        syncZooms != views->end() && syncZooms->is_boolean())
    {
      settings.setSynchronizeZooms(syncZooms->get<bool>());
    }
    if (const auto backgrounds = views->find("backgrounds"); backgrounds != views->end() && backgrounds->is_object()) {
      setVec3FromJson(renderData.m_2dBackgroundColor, *backgrounds, "2d");
      setVec4FromJson(renderData.m_3dBackgroundColor, *backgrounds, "3d");
    }
    if (const auto labels = views->find("anatomicalLabels"); labels != views->end() && labels->is_object()) {
      setVec4FromJson(renderData.m_anatomicalLabelColor, *labels, "color");
      setEnumFromJson(renderData.m_anatomicalLabelType, *labels, "type", sk_anatomicalLabelNames);
    }
    if (const auto scaleBars = views->find("scaleBars"); scaleBars != views->end() && scaleBars->is_object()) {
      setFromJson(renderData.m_showScaleBars, *scaleBars, "show");
      setFromJson(renderData.m_showScaleBarsInLightboxViews, *scaleBars, "showInLightboxViews");
      setVec4FromJson(renderData.m_scaleBarColor, *scaleBars, "color");
      setEnumFromJson(renderData.m_scaleBarPosition, *scaleBars, "position", sk_scaleBarPositionNames);
      setEnumFromJson(renderData.m_scaleBarOrientation, *scaleBars, "orientation", sk_scaleBarOrientationNames);
      setFloatFromJson(renderData.m_scaleBarTargetFraction, *scaleBars, "targetLengthFraction", 0.05f, 1.0f);
      setFloatFromJson(renderData.m_scaleBarMarginPx, *scaleBars, "marginPixels", 12.0f, 96.0f);
      setEnumFromJson(renderData.m_scaleBarTicks, *scaleBars, "ticks", sk_scaleBarTicksNames);
    }
    if (const auto lightbox = views->find("lightbox"); lightbox != views->end() && lightbox->is_object()) {
      setFromJson(renderData.m_showLightboxOffsetLabels, *lightbox, "showOffsetLabels");
      setVec4FromJson(renderData.m_lightboxOffsetLabelColor, *lightbox, "offsetLabelColor");
    }
  }

  if (const auto images = root.find("images"); images != root.end() && images->is_object()) {
    setFromJson(renderData.m_imageGrayFloatingPointInterpolation, *images, "floatingPointLinearInterpolation");
    if (const auto projection = images->find("intensityProjectionDefaults");
        projection != images->end() && projection->is_object())
    {
      setFromJson(renderData.m_doMaxExtentIntensityProjection, *projection, "useMaximumImageExtent");
      setFloatFromJson(renderData.m_intensityProjectionSlabThickness, *projection, "slabThicknessMm", 0.0f, 1.0e6f);
      if (const auto energy = projection->find("xrayEnergyKeV"); energy != projection->end() && energy->is_number()) {
        renderData.setXrayEnergy(energy->get<float>());
      }
      setFloatFromJson(renderData.m_xrayIntensityWindow, *projection, "xrayWindow", 1.0e-3f, 1.0f);
      setFloatFromJson(renderData.m_xrayIntensityLevel, *projection, "xrayLevel", 0.0f, 1.0f);
    }
  }

  if (const auto segmentation = root.find("segmentation"); segmentation != root.end() && segmentation->is_object()) {
    if (const auto display = segmentation->find("display"); display != segmentation->end() && display->is_object()) {
      setFromJson(renderData.m_modulateSegOpacityWithImageOpacity, *display, "modulateWithImageOpacity");
      setEnumFromJson(renderData.m_segOutlineStyle, *display, "outlineStyle", sk_segmentationOutlineNames);
      setFloatFromJson(renderData.m_segInteriorOpacity, *display, "interiorOpacity", 0.0f, 1.0f);
      setFloatFromJson(renderData.m_segInterpCutoff, *display, "erosionFactor", 0.5f, 1.0f);
    }
    if (const auto brush = segmentation->find("brush"); brush != segmentation->end() && brush->is_object()) {
      if (const auto value = brush->find("replaceBackgroundWithForeground");
          value != brush->end() && value->is_boolean())
      {
        settings.setReplaceBackgroundWithForeground(value->get<bool>());
      }
      if (const auto value = brush->find("use3d"); value != brush->end() && value->is_boolean()) {
        settings.setUse3dBrush(value->get<bool>());
      }
      if (const auto value = brush->find("useIsotropic"); value != brush->end() && value->is_boolean()) {
        settings.setUseIsotropicBrush(value->get<bool>());
      }
      if (const auto value = brush->find("useVoxelSize"); value != brush->end() && value->is_boolean()) {
        settings.setUseVoxelBrushSize(value->get<bool>());
      }
      if (const auto value = brush->find("useRound"); value != brush->end() && value->is_boolean()) {
        settings.setUseRoundBrush(value->get<bool>());
      }
      if (const auto value = brush->find("crosshairsMoveWithBrush"); value != brush->end() && value->is_boolean()) {
        settings.setCrosshairsMoveWithBrush(value->get<bool>());
      }
      if (const auto size = brush->find("sizeVoxels"); size != brush->end() && size->is_number_unsigned()) {
        settings.setBrushSizeInVoxels(size->get<uint32_t>());
      }
      if (const auto size = brush->find("sizeMm"); size != brush->end() && size->is_number()) {
        settings.setBrushSizeInMm(size->get<float>());
      }
    }
    if (const auto preview = segmentation->find("brushPreview"); preview != segmentation->end() && preview->is_object())
    {
      if (const auto parsed = enumFromName<BrushPreviewMode>(preview->value("mode", ""), sk_brushPreviewModeNames)) {
        settings.setBrushPreviewMode(*parsed);
      }
      if (
        const auto parsed = enumFromName<BrushPreviewVoxels>(preview->value("voxels", ""), sk_brushPreviewVoxelsNames))
      {
        settings.setBrushPreviewVoxels(*parsed);
      }
      if (const auto parsed = enumFromName<BrushPreviewStyle>(preview->value("style", ""), sk_brushPreviewStyleNames)) {
        settings.setBrushPreviewStyle(*parsed);
      }
      if (const auto opacity = preview->find("fillOpacity"); opacity != preview->end() && opacity->is_number()) {
        settings.setBrushPreviewFillOpacity(opacity->get<float>());
      }
      if (const auto value = preview->find("showWhilePainting"); value != preview->end() && value->is_boolean()) {
        settings.setBrushPreviewWhilePainting(value->get<bool>());
      }
      if (
        const auto parsed =
          enumFromName<SegmentationOutlineStyle>(preview->value("outlineStyle", ""), sk_segmentationOutlineNames))
      {
        settings.setBrushPreviewOutlineStyle(*parsed);
      }
    }
  }

  if (const auto comparison = root.find("comparison"); comparison != root.end() && comparison->is_object()) {
    if (const auto difference = comparison->find("difference");
        difference != comparison->end() && difference->is_object())
    {
      setFromJson(renderData.m_useSquare, *difference, "squared");
      if (const auto metric = difference->find("metric"); metric != difference->end() && metric->is_object()) {
        applyMetricParamsFromJson(renderData.m_squaredDifferenceParams, *metric);
      }
    }
    if (const auto overlay = comparison->find("overlay"); overlay != comparison->end() && overlay->is_object()) {
      setFromJson(renderData.m_overlayMagentaCyan, *overlay, "magentaCyan");
    }
    if (const auto quadrants = comparison->find("quadrants"); quadrants != comparison->end() && quadrants->is_object())
    {
      bool x = static_cast<bool>(renderData.m_quadrants.x);
      bool y = static_cast<bool>(renderData.m_quadrants.y);
      setFromJson(x, *quadrants, "x");
      setFromJson(y, *quadrants, "y");
      renderData.m_quadrants = glm::ivec2{x, y};
    }
    if (const auto checker = comparison->find("checkerboard"); checker != comparison->end() && checker->is_object()) {
      setFromJson(renderData.m_numCheckerboardSquares, *checker, "squares");
      renderData.m_numCheckerboardSquares = std::clamp(renderData.m_numCheckerboardSquares, 2, 2048);
    }
    if (const auto flashlight = comparison->find("flashlight");
        flashlight != comparison->end() && flashlight->is_object())
    {
      setFloatFromJson(renderData.m_flashlightRadius, *flashlight, "radiusFraction", 0.01f, 1.0f);
      setFromJson(renderData.m_flashlightOverlays, *flashlight, "overlayMovingImage");
    }
  }

  if (const auto rendering = root.find("rendering"); rendering != root.end() && rendering->is_object()) {
    if (const auto frameRate = rendering->find("frameRate"); frameRate != rendering->end() && frameRate->is_object()) {
      setFromJson(renderData.m_manualFramerateLimiter, *frameRate, "limit");
      setDoubleFromJson(renderData.m_targetFrameTimeSeconds, *frameRate, "targetFrameTimeSeconds", 1.0 / 240.0, 1.0);
    }
    if (const auto raycasting = rendering->find("raycasting");
        raycasting != rendering->end() && raycasting->is_object())
    {
      setFloatFromJson(renderData.m_raycastSamplingFactor, *raycasting, "samplingFactor", 0.1f, 5.0f);
      setFromJson(renderData.m_3dTransparentIfNoHit, *raycasting, "transparentBackgroundWhenNoHit");
      setFromJson(renderData.m_renderFrontFaces, *raycasting, "renderFrontFaces");
      setFromJson(renderData.m_renderBackFaces, *raycasting, "renderBackFaces");
      setEnumFromJson(renderData.m_segMasking, *raycasting, "segmentationMasking", sk_raycastSegMaskingNames);
    }
    if (const auto ascii = rendering->find("asciiShading"); ascii != rendering->end() && ascii->is_object()) {
      setFromJson(renderData.m_asciiEnabled, *ascii, "enabled");
      setVec2FromJson(renderData.m_asciiCellSizePx, *ascii, "cellSizePx");
      setFromJson(renderData.m_asciiCharsetIndex, *ascii, "charsetIndex");
      renderData.m_asciiCharsetIndex = std::clamp(renderData.m_asciiCharsetIndex, 0, 2);
      setVec3FromJson(renderData.m_asciiFgColor, *ascii, "foregroundColor");
      setVec3FromJson(renderData.m_asciiBgColor, *ascii, "backgroundColor");
      setFloatFromJson(renderData.m_asciiBgAlpha, *ascii, "backgroundAlpha", 0.0f, 1.0f);
      setFromJson(renderData.m_asciiUseColormap, *ascii, "useColormapAsForeground");
      setFromJson(renderData.m_asciiSpatialMode, *ascii, "spatialMatching");
      setFloatFromJson(renderData.m_asciiSpatialExponent, *ascii, "spatialExponent", 0.25f, 4.0f);
      renderData.m_asciiAtlasNeedsRebuild = true;
    }
  }

  if (const auto annotations = root.find("annotations"); annotations != root.end() && annotations->is_object()) {
    setFromJson(renderData.m_globalAnnotationParams.renderOnTopOfAllImagePlanes, *annotations, "annotationsOnTop");
    setFromJson(renderData.m_globalLandmarkParams.renderOnTopOfAllImagePlanes, *annotations, "landmarksOnTop");
    setFromJson(renderData.m_globalAnnotationParams.hidePolygonVertices, *annotations, "hideAnnotationVertices");
    if (const auto value = annotations->find("crosshairsMoveWhileAnnotating");
        value != annotations->end() && value->is_boolean())
    {
      settings.setCrosshairsMoveWhileAnnotating(value->get<bool>());
    }
  }

  if (const auto sync = root.find("synchronization"); sync != root.end() && sync->is_object()) {
    const auto applyItkSnapSync = [&settings](const json& object) {
      if (const auto value = object.find("enabled"); value != object.end() && value->is_boolean()) {
        settings.setCursorSyncEnabled(value->get<bool>());
      }
      if (const auto value = object.find("sendCursor"); value != object.end() && value->is_boolean()) {
        settings.setSendCursorSync(value->get<bool>());
      }
      if (const auto value = object.find("receiveCursor"); value != object.end() && value->is_boolean()) {
        settings.setReceiveCursorSync(value->get<bool>());
      }
      if (const auto value = object.find("sendZoom"); value != object.end() && value->is_boolean()) {
        settings.setSendZoomSync(value->get<bool>());
      }
      if (const auto value = object.find("receiveZoom"); value != object.end() && value->is_boolean()) {
        settings.setReceiveZoomSync(value->get<bool>());
      }
      if (const auto value = object.find("sendPan"); value != object.end() && value->is_boolean()) {
        settings.setSendPanSync(value->get<bool>());
      }
      if (const auto value = object.find("receivePan"); value != object.end() && value->is_boolean()) {
        settings.setReceivePanSync(value->get<bool>());
      }
    };

    // Backward-compatible reader for settings files written before ITK-SNAP and
    // Entropy-instance synchronization had separate sections.
    applyItkSnapSync(*sync);
    if (const auto itkSnap = sync->find("itkSnap"); itkSnap != sync->end() && itkSnap->is_object()) {
      applyItkSnapSync(*itkSnap);
    }

    if (const auto entropyInstances = sync->find("entropyInstances");
        entropyInstances != sync->end() && entropyInstances->is_object())
    {
      if (const auto value = entropyInstances->find("enabled"); value != entropyInstances->end() && value->is_boolean())
      {
        settings.setEntropyInstanceSyncEnabled(value->get<bool>());
      }
    }
  }

  if (const auto system = root.find("system"); system != root.end() && system->is_object()) {
    if (const auto diagnostics = system->find("diagnostics"); diagnostics != system->end() && diagnostics->is_object())
    {
      if (const auto level = diagnostics->find("logVerbosity"); level != diagnostics->end() && level->is_string()) {
        const std::string levelName = level->get<std::string>();
        for (const auto& choice : entropy::logging::allLogLevelChoices()) {
          if (choice.label == levelName && entropy::logging::isLogLevelChoiceAvailable(choice)) {
            entropy::logging::setDefaultLoggerSinkLevel(choice.level);
            break;
          }
        }
      }
    }
  }
}

void applyRenderDefaults(RenderData& renderData)
{
  renderData.m_snapCrosshairs = CrosshairsSnapping::Disabled;
  renderData.m_modulateSegOpacityWithImageOpacity = true;
  renderData.m_imageGrayFloatingPointInterpolation = false;
  renderData.m_intensityProjectionSlabThickness = 10.0f;
  renderData.m_doMaxExtentIntensityProjection = false;
  renderData.m_xrayIntensityWindow = 1.0f;
  renderData.m_xrayIntensityLevel = 0.5f;
  renderData.setXrayEnergy(80.0f);
  renderData.m_2dBackgroundColor = glm::vec3{0.1f, 0.1f, 0.1f};
  renderData.m_3dBackgroundColor = glm::vec4{0.0f, 0.0f, 0.0f, 0.5f};
  renderData.m_3dTransparentIfNoHit = true;
  renderData.m_crosshairsColor = glm::vec4{0.05f, 0.6f, 1.0f, 1.0f};
  renderData.m_anatomicalLabelColor = glm::vec4{0.695f, 0.870f, 0.090f, 1.0f};
  renderData.m_anatomicalLabelType = AnatomicalLabelType::Human;
  renderData.m_showScaleBars = true;
  renderData.m_showScaleBarsInLightboxViews = false;
  renderData.m_scaleBarColor = glm::vec4{0.380392f, 0.858824f, 0.250980f, 1.0f};
  renderData.m_scaleBarPosition = ScaleBarPosition::BottomRight;
  renderData.m_scaleBarOrientation = ScaleBarOrientation::Horizontal;
  renderData.m_scaleBarTicks = ScaleBarTicks::Automatic;
  renderData.m_scaleBarTargetFraction = 0.2f;
  renderData.m_scaleBarMarginPx = 12.0f;
  renderData.m_showLightboxOffsetLabels = false;
  renderData.m_lightboxOffsetLabelColor = glm::vec4{0.75f, 0.75f, 0.75f, 0.8f};
  renderData.m_renderFrontFaces = true;
  renderData.m_renderBackFaces = true;
  renderData.m_raycastSamplingFactor = 0.5f;
  renderData.m_segMasking = RenderData::SegMaskingForRaycasting::Disabled;
  renderData.m_segOutlineStyle = SegmentationOutlineStyle::Disabled;
  renderData.m_segInteriorOpacity = 0.2f;
  renderData.m_segInterpCutoff = 0.5f;
  renderData.m_squaredDifferenceParams = {};
  renderData.m_numCheckerboardSquares = 10;
  renderData.m_overlayMagentaCyan = true;
  renderData.m_quadrants = glm::ivec2{true, true};
  renderData.m_useSquare = true;
  renderData.m_flashlightRadius = 0.15f;
  renderData.m_flashlightOverlays = true;
  renderData.m_manualFramerateLimiter = false;
  renderData.m_targetFrameTimeSeconds = 1.0 / 60.0;
  renderData.m_asciiEnabled = false;
  renderData.m_asciiCellSizePx = glm::vec2{8.0f, 16.0f};
  renderData.m_asciiCharsetIndex = 0;
  renderData.m_asciiFgColor = glm::vec3{1.0f, 1.0f, 1.0f};
  renderData.m_asciiBgColor = glm::vec3{0.0f, 0.0f, 0.0f};
  renderData.m_asciiBgAlpha = 1.0f;
  renderData.m_asciiUseColormap = false;
  renderData.m_asciiSpatialMode = false;
  renderData.m_asciiSpatialExponent = 1.0f;
  renderData.m_asciiAtlasNeedsRebuild = true;
  renderData.m_globalAnnotationParams.renderOnTopOfAllImagePlanes = false;
  renderData.m_globalAnnotationParams.hidePolygonVertices = false;
  renderData.m_globalLandmarkParams.renderOnTopOfAllImagePlanes = false;
  renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections = true;
}

} // namespace

namespace user_preferences
{

std::string toJsonString(const AppSettings& settings, const RenderData& renderData)
{
  return toJson(settings, renderData).dump(2);
}

bool applyJsonString(AppSettings& settings, RenderData& renderData, const std::string& text, std::string* error)
{
  try {
    const json root = json::parse(text);
    applyJson(settings, renderData, root);
    return true;
  }
  catch (const std::exception& e) {
    if (error != nullptr) {
      *error = e.what();
    }
    return false;
  }
}

bool save(
  const AppSettings& settings,
  const RenderData& renderData,
  const std::filesystem::path& fileName,
  std::string* error)
{
  try {
    if (!fileName.parent_path().empty()) {
      std::filesystem::create_directories(fileName.parent_path());
    }
    std::ofstream out(fileName, std::ios::out | std::ios::trunc);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << toJsonString(settings, renderData) << '\n';
    spdlog::info("Saved user settings to {}", fileName);
    return true;
  }
  catch (const std::exception& e) {
    if (error != nullptr) {
      *error = e.what();
    }
    spdlog::error("Failed to save user settings to {}: {}", fileName, e.what());
    return false;
  }
}

bool load(AppSettings& settings, RenderData& renderData, const std::filesystem::path& fileName, std::string* error)
{
  std::error_code ec;
  if (!std::filesystem::exists(fileName, ec)) {
    return true;
  }

  try {
    std::ifstream in(fileName);
    in.exceptions(std::ios::badbit | std::ios::failbit);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    if (!applyJsonString(settings, renderData, buffer.str(), error)) {
      return false;
    }
    spdlog::info("Loaded user settings from {}", fileName);
    return true;
  }
  catch (const std::exception& e) {
    if (error != nullptr) {
      *error = e.what();
    }
    spdlog::error("Failed to load user settings from {}: {}", fileName, e.what());
    return false;
  }
}

void applyDefaults(AppSettings& settings, RenderData& renderData)
{
  settings = AppSettings{};
  applyRenderDefaults(renderData);
  entropy::logging::setDefaultLoggerSinkLevel(entropy::logging::defaultLogLevel());
}

} // namespace user_preferences
