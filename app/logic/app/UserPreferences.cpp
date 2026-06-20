#include "logic/app/UserPreferences.h"

#include "common/LoggingSettings.h"
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

void setNonnegativeFloatFromJson(float& value, const json& object, const char* key)
{
  const auto it = object.find(key);
  if (it == object.end() || !it->is_number()) {
    return;
  }

  value = std::max(it->get<float>(), 0.0f);
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

  try {
    value = glm::vec2{it->at(0).get<float>(), it->at(1).get<float>()};
  }
  catch (const json::exception&) {
  }
}

void setVec3FromJson(glm::vec3& value, const json& object, const char* key)
{
  const auto it = object.find(key);
  if (it == object.end() || !it->is_array() || it->size() != 3) {
    return;
  }

  try {
    value = glm::vec3{it->at(0).get<float>(), it->at(1).get<float>(), it->at(2).get<float>()};
  }
  catch (const json::exception&) {
  }
}

void setVec4FromJson(glm::vec4& value, const json& object, const char* key)
{
  const auto it = object.find(key);
  if (it == object.end() || !it->is_array() || it->size() != 4) {
    return;
  }

  try {
    value = glm::vec4{it->at(0).get<float>(), it->at(1).get<float>(), it->at(2).get<float>(), it->at(3).get<float>()};
  }
  catch (const json::exception&) {
  }
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

constexpr std::array sk_layoutTabPlacementNames{
  EnumName{UiLayoutTabPlacement::Top, "top"},
  EnumName{UiLayoutTabPlacement::Bottom, "bottom"}};

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
  EnumName{user_preferences::RenderPreferences::SegMaskingForRaycasting::Disabled, "disabled"},
  EnumName{user_preferences::RenderPreferences::SegMaskingForRaycasting::SegMasksIn, "maskIn"},
  EnumName{user_preferences::RenderPreferences::SegMaskingForRaycasting::SegMasksOut, "maskOut"}};

constexpr std::array sk_localNccPresentationNames{
  EnumName{user_preferences::RenderPreferences::LocalNccPresentation::Dissimilarity, "dissimilarity"},
  EnumName{user_preferences::RenderPreferences::LocalNccPresentation::Correlation, "correlation"}};

constexpr std::array sk_localNccInvalidStyleNames{
  EnumName{user_preferences::RenderPreferences::LocalNccInvalidStyle::Transparent, "transparent"},
  EnumName{user_preferences::RenderPreferences::LocalNccInvalidStyle::Gray, "gray"}};

json metricParamsToJson(const user_preferences::RenderPreferences::MetricParams& params)
{
  return {
    {"windowSlopeIntercept", vec2ToJson(params.slopeIntercept)},
    {"colormapIndex", params.colorMapIndex},
    {"invertColormap", params.invertColormap},
    {"continuousColormap", params.continuousColormap},
    {"colormapLevels", params.colormapLevels}};
}

void applyMetricParamsFromJson(user_preferences::RenderPreferences::MetricParams& params, const json& object)
{
  setVec2FromJson(params.slopeIntercept, object, "windowSlopeIntercept");
  setFromJson(params.colorMapIndex, object, "colormapIndex");
  setFromJson(params.invertColormap, object, "invertColormap");
  setFromJson(params.continuousColormap, object, "continuousColormap");
  setFromJson(params.colormapLevels, object, "colormapLevels");
  params.colormapLevels = std::max(params.colormapLevels, 2);
}

json toJson(const AppSettings& settings, const user_preferences::RenderPreferences& renderPreferences)
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
      {"windowBackgroundOpacity", settings.uiWindowBgOpacity()},
      {"showLayoutTabs", settings.showLayoutTabs()},
      {"layoutTabsPosition", enumToName(settings.layoutTabPlacement(), sk_layoutTabPlacementNames)}}},
    {"views",
     {{"showImageBorders", renderPreferences.showImageBorders},
      {"showOverlays", settings.overlays()},
      {"lockAnatomicalDirectionsToReferenceImage", settings.lockAnatomicalCoordinateAxesWithReferenceImage()},
      {"crosshairs",
       {{"color", vec4ToJson(renderPreferences.crosshairsColor)},
        {"snapping", enumToName(renderPreferences.crosshairsSnapping, sk_crosshairsSnappingNames)}}},
      {"synchronizeViewZooms", settings.synchronizeZooms()},
      {"backgrounds",
       {{"2d", vec3ToJson(renderPreferences.background2dColor)},
        {"3d", vec4ToJson(renderPreferences.background3dColor)}}},
      {"anatomicalLabels",
       {{"color", vec4ToJson(renderPreferences.anatomicalLabelColor)},
        {"type", enumToName(renderPreferences.anatomicalLabelType, sk_anatomicalLabelNames)}}},
      {"scaleBars",
       {{"show", renderPreferences.showScaleBars},
        {"showInLightboxViews", renderPreferences.showScaleBarsInLightboxViews},
        {"color", vec4ToJson(renderPreferences.scaleBarColor)},
        {"position", enumToName(renderPreferences.scaleBarPosition, sk_scaleBarPositionNames)},
        {"orientation", enumToName(renderPreferences.scaleBarOrientation, sk_scaleBarOrientationNames)},
        {"targetLengthFraction", renderPreferences.scaleBarTargetFraction},
        {"marginPixels", renderPreferences.scaleBarMarginPx},
        {"ticks", enumToName(renderPreferences.scaleBarTicks, sk_scaleBarTicksNames)}}},
      {"lightbox",
       {{"showOffsetLabels", renderPreferences.showLightboxOffsetLabels},
        {"offsetLabelColor", vec4ToJson(renderPreferences.lightboxOffsetLabelColor)}}}}},
    {"images",
     {{"floatingPointLinearInterpolation", renderPreferences.floatingPointLinearInterpolation},
      {"intensityProjectionDefaults",
       {{"useMaximumImageExtent", renderPreferences.useMaximumIntensityProjectionExtent},
        {"slabThicknessMm", renderPreferences.intensityProjectionSlabThicknessMm},
        {"xrayEnergyKeV", renderPreferences.xrayEnergyKeV},
        {"xrayWindow", renderPreferences.xrayWindow},
        {"xrayLevel", renderPreferences.xrayLevel}}}}},
    {"segmentation",
     {{"display",
       {{"modulateWithImageOpacity", renderPreferences.modulateSegmentationOpacityWithImageOpacity},
        {"outlineStyle", enumToName(renderPreferences.segmentationOutlineStyle, sk_segmentationOutlineNames)},
        {"interiorOpacity", renderPreferences.segmentationInteriorOpacity},
        {"erosionFactor", renderPreferences.segmentationErosionFactor}}},
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
       {{"squared", renderPreferences.squaredDifference},
        {"metric", metricParamsToJson(renderPreferences.squaredDifferenceMetric)}}},
      {"localNormalizedCrossCorrelation",
       {{"metric", metricParamsToJson(renderPreferences.localNccMetric)},
        {"presentation", enumToName(renderPreferences.localNccPresentation, sk_localNccPresentationNames)},
        {"negativeCorrelationAsMismatch", renderPreferences.localNccIgnoreNegativeCorrelation},
        {"patchRadius", renderPreferences.localNccPatchRadius},
        {"sampleSpacing", renderPreferences.localNccSampleSpacing},
        {"minimumValidFraction", renderPreferences.localNccMinValidFraction},
        {"varianceEpsilon", renderPreferences.localNccVarianceEpsilon},
        {"invalidStyle", enumToName(renderPreferences.localNccInvalidStyle, sk_localNccInvalidStyleNames)}}},
      {"localLinearResidual",
       {{"metric", metricParamsToJson(renderPreferences.localLinearResidualMetric)},
        {"patchRadius", renderPreferences.localLinearResidualPatchRadius},
        {"sampleSpacing", renderPreferences.localLinearResidualSampleSpacing},
        {"minimumValidFraction", renderPreferences.localLinearResidualMinValidFraction},
        {"varianceEpsilon", renderPreferences.localLinearResidualVarianceEpsilon},
        {"invalidStyle", enumToName(renderPreferences.localLinearResidualInvalidStyle, sk_localNccInvalidStyleNames)}}},
      {"overlay", {{"magentaCyan", renderPreferences.overlayMagentaCyan}}},
      {"quadrants",
       {{"x", static_cast<bool>(renderPreferences.quadrants.x)},
        {"y", static_cast<bool>(renderPreferences.quadrants.y)}}},
      {"checkerboard", {{"squares", renderPreferences.checkerboardSquares}}},
      {"flashlight",
       {{"radiusFraction", renderPreferences.flashlightRadiusFraction},
        {"overlayMovingImage", renderPreferences.flashlightOverlayMovingImage}}}}},
    {"rendering",
     {{"frameRate",
       {{"limit", renderPreferences.limitFrameRate},
        {"targetFrameTimeSeconds", renderPreferences.targetFrameTimeSeconds}}},
      {"raycasting",
       {{"samplingFactor", renderPreferences.raycastSamplingFactor},
        {"transparentBackgroundWhenNoHit", renderPreferences.transparentBackgroundWhenNoHit},
        {"renderFrontFaces", renderPreferences.renderFrontFaces},
        {"renderBackFaces", renderPreferences.renderBackFaces},
        {"segmentationMasking", enumToName(renderPreferences.segmentationMasking, sk_raycastSegMaskingNames)}}},
      {"asciiShading",
       {{"enabled", renderPreferences.asciiEnabled},
        {"cellSizePx", vec2ToJson(renderPreferences.asciiCellSizePx)},
        {"charsetIndex", renderPreferences.asciiCharsetIndex},
        {"foregroundColor", vec3ToJson(renderPreferences.asciiForegroundColor)},
        {"backgroundColor", vec3ToJson(renderPreferences.asciiBackgroundColor)},
        {"backgroundAlpha", renderPreferences.asciiBackgroundAlpha},
        {"useColormapAsForeground", renderPreferences.asciiUseColormapAsForeground},
        {"spatialMatching", renderPreferences.asciiSpatialMatching},
        {"spatialExponent", renderPreferences.asciiSpatialExponent}}}}},
    {"annotations",
     {{"annotationsOnTop", renderPreferences.annotationsOnTop},
      {"landmarksOnTop", renderPreferences.landmarksOnTop},
      {"hideAnnotationVertices", renderPreferences.hideAnnotationVertices},
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

void applyJson(AppSettings& settings, user_preferences::RenderPreferences& renderPreferences, const json& root)
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
    if (const auto showTabs = interface->find("showLayoutTabs"); showTabs != interface->end() && showTabs->is_boolean())
    {
      settings.setShowLayoutTabs(showTabs->get<bool>());
    }
    if (
      const auto parsed =
        enumFromName<UiLayoutTabPlacement>(interface->value("layoutTabsPosition", ""), sk_layoutTabPlacementNames))
    {
      settings.setLayoutTabPlacement(*parsed);
    }
  }

  if (const auto views = root.find("views"); views != root.end() && views->is_object()) {
    setFromJson(renderPreferences.showImageBorders, *views, "showImageBorders");
    if (const auto overlays = views->find("showOverlays"); overlays != views->end() && overlays->is_boolean()) {
      settings.setOverlays(overlays->get<bool>());
    }
    if (const auto lock = views->find("lockAnatomicalDirectionsToReferenceImage");
        lock != views->end() && lock->is_boolean())
    {
      settings.setLockAnatomicalCoordinateAxesWithReferenceImage(lock->get<bool>());
    }

    if (const auto crosshairs = views->find("crosshairs"); crosshairs != views->end() && crosshairs->is_object()) {
      setVec4FromJson(renderPreferences.crosshairsColor, *crosshairs, "color");
      setEnumFromJson(renderPreferences.crosshairsSnapping, *crosshairs, "snapping", sk_crosshairsSnappingNames);
    }
    if (const auto syncZooms = views->find("synchronizeViewZooms");
        syncZooms != views->end() && syncZooms->is_boolean())
    {
      settings.setSynchronizeZooms(syncZooms->get<bool>());
    }
    if (const auto backgrounds = views->find("backgrounds"); backgrounds != views->end() && backgrounds->is_object()) {
      setVec3FromJson(renderPreferences.background2dColor, *backgrounds, "2d");
      setVec4FromJson(renderPreferences.background3dColor, *backgrounds, "3d");
    }
    if (const auto labels = views->find("anatomicalLabels"); labels != views->end() && labels->is_object()) {
      setVec4FromJson(renderPreferences.anatomicalLabelColor, *labels, "color");
      setEnumFromJson(renderPreferences.anatomicalLabelType, *labels, "type", sk_anatomicalLabelNames);
    }
    if (const auto scaleBars = views->find("scaleBars"); scaleBars != views->end() && scaleBars->is_object()) {
      setFromJson(renderPreferences.showScaleBars, *scaleBars, "show");
      setFromJson(renderPreferences.showScaleBarsInLightboxViews, *scaleBars, "showInLightboxViews");
      setVec4FromJson(renderPreferences.scaleBarColor, *scaleBars, "color");
      setEnumFromJson(renderPreferences.scaleBarPosition, *scaleBars, "position", sk_scaleBarPositionNames);
      setEnumFromJson(renderPreferences.scaleBarOrientation, *scaleBars, "orientation", sk_scaleBarOrientationNames);
      setFloatFromJson(renderPreferences.scaleBarTargetFraction, *scaleBars, "targetLengthFraction", 0.05f, 1.0f);
      setFloatFromJson(renderPreferences.scaleBarMarginPx, *scaleBars, "marginPixels", 12.0f, 96.0f);
      setEnumFromJson(renderPreferences.scaleBarTicks, *scaleBars, "ticks", sk_scaleBarTicksNames);
    }
    if (const auto lightbox = views->find("lightbox"); lightbox != views->end() && lightbox->is_object()) {
      setFromJson(renderPreferences.showLightboxOffsetLabels, *lightbox, "showOffsetLabels");
      setVec4FromJson(renderPreferences.lightboxOffsetLabelColor, *lightbox, "offsetLabelColor");
    }
  }

  if (const auto images = root.find("images"); images != root.end() && images->is_object()) {
    setFromJson(renderPreferences.floatingPointLinearInterpolation, *images, "floatingPointLinearInterpolation");
    if (const auto projection = images->find("intensityProjectionDefaults");
        projection != images->end() && projection->is_object())
    {
      setFromJson(renderPreferences.useMaximumIntensityProjectionExtent, *projection, "useMaximumImageExtent");
      setFloatFromJson(
        renderPreferences.intensityProjectionSlabThicknessMm,
        *projection,
        "slabThicknessMm",
        0.0f,
        1.0e6f);
      if (const auto energy = projection->find("xrayEnergyKeV"); energy != projection->end() && energy->is_number()) {
        renderPreferences.xrayEnergyKeV = energy->get<float>();
      }
      setFloatFromJson(renderPreferences.xrayWindow, *projection, "xrayWindow", 1.0e-3f, 1.0f);
      setFloatFromJson(renderPreferences.xrayLevel, *projection, "xrayLevel", 0.0f, 1.0f);
    }
  }

  if (const auto segmentation = root.find("segmentation"); segmentation != root.end() && segmentation->is_object()) {
    if (const auto display = segmentation->find("display"); display != segmentation->end() && display->is_object()) {
      setFromJson(renderPreferences.modulateSegmentationOpacityWithImageOpacity, *display, "modulateWithImageOpacity");
      setEnumFromJson(
        renderPreferences.segmentationOutlineStyle,
        *display,
        "outlineStyle",
        sk_segmentationOutlineNames);
      setFloatFromJson(renderPreferences.segmentationInteriorOpacity, *display, "interiorOpacity", 0.0f, 1.0f);
      setFloatFromJson(renderPreferences.segmentationErosionFactor, *display, "erosionFactor", 0.5f, 1.0f);
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
      setFromJson(renderPreferences.squaredDifference, *difference, "squared");
      if (const auto metric = difference->find("metric"); metric != difference->end() && metric->is_object()) {
        applyMetricParamsFromJson(renderPreferences.squaredDifferenceMetric, *metric);
      }
    }
    if (const auto localNcc = comparison->find("localNormalizedCrossCorrelation");
        localNcc != comparison->end() && localNcc->is_object())
    {
      if (const auto metric = localNcc->find("metric"); metric != localNcc->end() && metric->is_object()) {
        applyMetricParamsFromJson(renderPreferences.localNccMetric, *metric);
      }
      if (
        const auto parsed = enumFromName<user_preferences::RenderPreferences::LocalNccPresentation>(
          localNcc->value("presentation", ""),
          sk_localNccPresentationNames))
      {
        renderPreferences.localNccPresentation = *parsed;
      }
      if (
        const auto parsed = enumFromName<user_preferences::RenderPreferences::LocalNccInvalidStyle>(
          localNcc->value("invalidStyle", ""),
          sk_localNccInvalidStyleNames))
      {
        renderPreferences.localNccInvalidStyle = *parsed;
      }
      setFromJson(renderPreferences.localNccIgnoreNegativeCorrelation, *localNcc, "negativeCorrelationAsMismatch");
      setFromJson(renderPreferences.localNccPatchRadius, *localNcc, "patchRadius");
      setFloatFromJson(renderPreferences.localNccSampleSpacing, *localNcc, "sampleSpacing", 0.5f, 4.0f);
      setFloatFromJson(renderPreferences.localNccMinValidFraction, *localNcc, "minimumValidFraction", 0.1f, 1.0f);
      setNonnegativeFloatFromJson(renderPreferences.localNccVarianceEpsilon, *localNcc, "varianceEpsilon");
      renderPreferences.localNccPatchRadius = std::clamp(renderPreferences.localNccPatchRadius, 1, 5);
    }
    if (const auto localLinearResidual = comparison->find("localLinearResidual");
        localLinearResidual != comparison->end() && localLinearResidual->is_object())
    {
      if (const auto metric = localLinearResidual->find("metric");
          metric != localLinearResidual->end() && metric->is_object())
      {
        applyMetricParamsFromJson(renderPreferences.localLinearResidualMetric, *metric);
      }
      if (
        const auto parsed = enumFromName<user_preferences::RenderPreferences::LocalNccInvalidStyle>(
          localLinearResidual->value("invalidStyle", ""),
          sk_localNccInvalidStyleNames))
      {
        renderPreferences.localLinearResidualInvalidStyle = *parsed;
      }
      setFromJson(renderPreferences.localLinearResidualPatchRadius, *localLinearResidual, "patchRadius");
      setFloatFromJson(
        renderPreferences.localLinearResidualSampleSpacing,
        *localLinearResidual,
        "sampleSpacing",
        0.5f,
        4.0f);
      setFloatFromJson(
        renderPreferences.localLinearResidualMinValidFraction,
        *localLinearResidual,
        "minimumValidFraction",
        0.1f,
        1.0f);
      setNonnegativeFloatFromJson(
        renderPreferences.localLinearResidualVarianceEpsilon,
        *localLinearResidual,
        "varianceEpsilon");
      renderPreferences.localLinearResidualPatchRadius =
        std::clamp(renderPreferences.localLinearResidualPatchRadius, 1, 5);
    }
    if (const auto overlay = comparison->find("overlay"); overlay != comparison->end() && overlay->is_object()) {
      setFromJson(renderPreferences.overlayMagentaCyan, *overlay, "magentaCyan");
    }
    if (const auto quadrants = comparison->find("quadrants"); quadrants != comparison->end() && quadrants->is_object())
    {
      bool x = static_cast<bool>(renderPreferences.quadrants.x);
      bool y = static_cast<bool>(renderPreferences.quadrants.y);
      setFromJson(x, *quadrants, "x");
      setFromJson(y, *quadrants, "y");
      renderPreferences.quadrants = glm::ivec2{x, y};
    }
    if (const auto checker = comparison->find("checkerboard"); checker != comparison->end() && checker->is_object()) {
      setFromJson(renderPreferences.checkerboardSquares, *checker, "squares");
      renderPreferences.checkerboardSquares = std::clamp(renderPreferences.checkerboardSquares, 2, 2048);
    }
    if (const auto flashlight = comparison->find("flashlight");
        flashlight != comparison->end() && flashlight->is_object())
    {
      setFloatFromJson(renderPreferences.flashlightRadiusFraction, *flashlight, "radiusFraction", 0.01f, 1.0f);
      setFromJson(renderPreferences.flashlightOverlayMovingImage, *flashlight, "overlayMovingImage");
    }
  }

  if (const auto rendering = root.find("rendering"); rendering != root.end() && rendering->is_object()) {
    if (const auto frameRate = rendering->find("frameRate"); frameRate != rendering->end() && frameRate->is_object()) {
      setFromJson(renderPreferences.limitFrameRate, *frameRate, "limit");
      setDoubleFromJson(
        renderPreferences.targetFrameTimeSeconds,
        *frameRate,
        "targetFrameTimeSeconds",
        1.0 / 240.0,
        1.0);
    }
    if (const auto raycasting = rendering->find("raycasting");
        raycasting != rendering->end() && raycasting->is_object())
    {
      setFloatFromJson(renderPreferences.raycastSamplingFactor, *raycasting, "samplingFactor", 0.1f, 5.0f);
      setFromJson(renderPreferences.transparentBackgroundWhenNoHit, *raycasting, "transparentBackgroundWhenNoHit");
      setFromJson(renderPreferences.renderFrontFaces, *raycasting, "renderFrontFaces");
      setFromJson(renderPreferences.renderBackFaces, *raycasting, "renderBackFaces");
      setEnumFromJson(
        renderPreferences.segmentationMasking,
        *raycasting,
        "segmentationMasking",
        sk_raycastSegMaskingNames);
    }
    if (const auto ascii = rendering->find("asciiShading"); ascii != rendering->end() && ascii->is_object()) {
      setFromJson(renderPreferences.asciiEnabled, *ascii, "enabled");
      setVec2FromJson(renderPreferences.asciiCellSizePx, *ascii, "cellSizePx");
      setFromJson(renderPreferences.asciiCharsetIndex, *ascii, "charsetIndex");
      renderPreferences.asciiCharsetIndex = std::clamp(renderPreferences.asciiCharsetIndex, 0, 2);
      setVec3FromJson(renderPreferences.asciiForegroundColor, *ascii, "foregroundColor");
      setVec3FromJson(renderPreferences.asciiBackgroundColor, *ascii, "backgroundColor");
      setFloatFromJson(renderPreferences.asciiBackgroundAlpha, *ascii, "backgroundAlpha", 0.0f, 1.0f);
      setFromJson(renderPreferences.asciiUseColormapAsForeground, *ascii, "useColormapAsForeground");
      setFromJson(renderPreferences.asciiSpatialMatching, *ascii, "spatialMatching");
      setFloatFromJson(renderPreferences.asciiSpatialExponent, *ascii, "spatialExponent", 0.25f, 4.0f);
    }
  }

  if (const auto annotations = root.find("annotations"); annotations != root.end() && annotations->is_object()) {
    setFromJson(renderPreferences.annotationsOnTop, *annotations, "annotationsOnTop");
    setFromJson(renderPreferences.landmarksOnTop, *annotations, "landmarksOnTop");
    setFromJson(renderPreferences.hideAnnotationVertices, *annotations, "hideAnnotationVertices");
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

} // namespace

namespace user_preferences
{

RenderPreferences defaultRenderPreferences()
{
  return {};
}

std::string toJsonString(const AppSettings& settings, const RenderPreferences& renderPreferences)
{
  return toJson(settings, renderPreferences).dump(2);
}

bool applyJsonString(
  AppSettings& settings,
  RenderPreferences& renderPreferences,
  const std::string& text,
  std::string* error)
{
  try {
    const json root = json::parse(text);
    applyJson(settings, renderPreferences, root);
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
  const RenderPreferences& renderPreferences,
  const std::filesystem::path& fileName,
  std::string* error)
{
  try {
    if (!fileName.parent_path().empty()) {
      std::filesystem::create_directories(fileName.parent_path());
    }
    std::ofstream out(fileName, std::ios::out | std::ios::trunc);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << toJsonString(settings, renderPreferences) << '\n';
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

bool load(
  AppSettings& settings,
  RenderPreferences& renderPreferences,
  const std::filesystem::path& fileName,
  std::string* error)
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
    if (!applyJsonString(settings, renderPreferences, buffer.str(), error)) {
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

} // namespace user_preferences
