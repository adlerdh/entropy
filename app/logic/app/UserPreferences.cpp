#include "logic/app/UserPreferences.h"

#include "common/LoggingSettings.h"
#include "registration/Json.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <filesystem>
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

template<typename Enum>
EnumName(Enum, std::string_view) -> EnumName<Enum>;

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
  catch (const json::exception& e) {
    spdlog::debug("Ignoring invalid user preference '{}': {}", key, e.what());
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

json pathGroupToJson(const RecentPathGroup& group)
{
  json paths = json::array();
  for (const auto& path : group.paths) {
    if (!path.empty()) {
      paths.push_back(path.generic_string());
    }
  }
  return paths;
}

json pathGroupsToJson(const std::vector<RecentPathGroup>& groups)
{
  json values = json::array();
  for (const RecentPathGroup& group : groups) {
    if (!group.paths.empty()) {
      values.push_back(pathGroupToJson(group));
    }
  }
  return values;
}

json pathsToJson(const std::vector<std::filesystem::path>& paths)
{
  json values = json::array();
  for (const auto& path : paths) {
    if (!path.empty()) {
      values.push_back(path.generic_string());
    }
  }
  return values;
}

std::vector<RecentPathGroup> pathGroupsFromJson(const json& value)
{
  std::vector<RecentPathGroup> groups;
  if (!value.is_array()) {
    return groups;
  }

  for (const json& item : value) {
    if (item.is_string()) {
      groups.push_back(RecentPathGroup{{item.get<std::string>()}});
      continue;
    }
    if (!item.is_array()) {
      continue;
    }

    RecentPathGroup group;
    for (const json& path : item) {
      if (path.is_string()) {
        group.paths.emplace_back(path.get<std::string>());
      }
    }
    if (!group.paths.empty()) {
      groups.push_back(std::move(group));
    }
  }

  return groups;
}

std::vector<std::filesystem::path> pathsFromJson(const json& value)
{
  std::vector<std::filesystem::path> paths;
  if (!value.is_array()) {
    return paths;
  }
  for (const json& path : value) {
    if (path.is_string()) {
      paths.emplace_back(path.get<std::string>());
    }
  }
  return paths;
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
  catch (const json::exception& e) {
    spdlog::debug("Ignoring invalid vec2 user preference '{}': {}", key, e.what());
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
  catch (const json::exception& e) {
    spdlog::debug("Ignoring invalid vec3 user preference '{}': {}", key, e.what());
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
  catch (const json::exception& e) {
    spdlog::debug("Ignoring invalid vec4 user preference '{}': {}", key, e.what());
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

constexpr std::array sk_floatingPointInterpolationPolicyNames{
  EnumName{FloatingPointLinearInterpolationPolicy::Automatic, "automatic"},
  EnumName{FloatingPointLinearInterpolationPolicy::FixedFunction, "fixedFunction"},
  EnumName{FloatingPointLinearInterpolationPolicy::FloatingPoint, "floatingPoint"}};

constexpr std::uint32_t kMaxPrecision = 9;

std::uint32_t precisionFromJson(const json& root, std::string_view key, std::uint32_t fallback)
{
  const auto value = root.find(key);
  if (value == root.end() || !value->is_number_unsigned()) {
    return fallback;
  }
  return std::min(value->get<std::uint32_t>(), kMaxPrecision);
}

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

json toJson(
  const AppSettings& settings,
  const user_preferences::RenderPreferences& renderPreferences,
  const user_preferences::PrecisionPreferences& precisionPreferences)
{
  json uiScale = "auto";
  if (settings.uiScaleOverride()) {
    uiScale = *settings.uiScaleOverride();
  }

  return {
    {"format", "entropy.userSettings"},
    {"version", {{"major", 1}, {"minor", 0}}},
    {"interface",
     {{"uiScale", uiScale},
      {"font", enumToName(settings.uiFontFamily(), sk_uiFontNames)},
      {"colorScheme", enumToName(settings.uiColorPreset(), sk_uiColorPresetNames)},
      {"density", enumToName(settings.uiDensityPreset(), sk_uiDensityPresetNames)},
      {"windowBackgroundOpacity", settings.uiWindowBgOpacity()},
      {"showLayoutTabs", settings.showLayoutTabs()},
      {"layoutTabsPosition", enumToName(settings.layoutTabPlacement(), sk_layoutTabPlacementNames)},
      {"showGlobalTimeControls", settings.showGlobalTimeControls()},
      {"precision",
       {{"imageValues", precisionPreferences.imageValuePrecision},
        {"coordinates", precisionPreferences.coordsPrecision},
        {"transformations", precisionPreferences.txPrecision},
        {"percentiles", precisionPreferences.percentilePrecision},
        {"timeValues", precisionPreferences.timeValuePrecision}}}}},
    {"views",
     {{"showImageBorders", renderPreferences.showImageBorders},
      {"showOverlays", settings.overlays()},
      {"crosshairs",
       {{"show", renderPreferences.showCrosshairs},
        {"showInLightboxViews", renderPreferences.showCrosshairsInLightboxViews},
        {"color", vec4ToJson(renderPreferences.crosshairsColor)}}},
      {"synchronizeViewZooms", settings.synchronizeZooms()},
      {"backgrounds",
       {{"2d", vec3ToJson(renderPreferences.background2dColor)},
        {"3d", vec4ToJson(renderPreferences.background3dColor)}}},
      {"anatomicalLabels",
       {{"color", vec4ToJson(renderPreferences.anatomicalLabelColor)},
        {"scale", renderPreferences.anatomicalLabelScale}}},
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
       {{"showImageBorders", renderPreferences.showImageBordersInLightboxViews},
        {"showOffsetLabels", renderPreferences.showLightboxOffsetLabels},
        {"offsetLabelColor", vec4ToJson(renderPreferences.lightboxOffsetLabelColor)}}}}},
    {"images",
     {{"floatingPointLinearInterpolationPolicy",
       enumToName(renderPreferences.floatingPointLinearInterpolationPolicy, sk_floatingPointInterpolationPolicyNames)},
      {"isocontourFloatingPointInterpolationPolicy",
       enumToName(
         renderPreferences.isocontourFloatingPointInterpolationPolicy,
         sk_floatingPointInterpolationPolicyNames)}}},
    {"segmentation",
     {{"brush",
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
    {"rendering",
     {{"frameRate",
       {{"limit", renderPreferences.limitFrameRate},
        {"targetFrameTimeSeconds", renderPreferences.targetFrameTimeSeconds}}},
      {"camera", {{"reversePovRotation", renderPreferences.reversePovRotation}}},
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
    {"annotations", {{"crosshairsMoveWhileAnnotating", settings.crosshairsMoveWhileAnnotating()}}},
    {"registration", settings.registrationBackendConfig()},
    {"recent",
     {{"images", pathGroupsToJson(settings.recentImageGroups())},
      {"dicom", pathGroupsToJson(settings.recentDicomGroups())},
      {"projects", pathsToJson(settings.recentProjectFiles())}}},
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
     {{"diagnostics", {{"logVerbosity", std::string{logging::logLevelLabel(logging::defaultLoggerSinkLevel())}}}},
      {"updates", {{"automaticChecks", settings.automaticUpdateChecksEnabled()}}}}}};
}

void applyJson(
  AppSettings& settings,
  user_preferences::RenderPreferences& renderPreferences,
  user_preferences::PrecisionPreferences& precisionPreferences,
  const json& root)
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
    if (const auto showTimeControls = interface->find("showGlobalTimeControls");
        showTimeControls != interface->end() && showTimeControls->is_boolean())
    {
      settings.setShowGlobalTimeControls(showTimeControls->get<bool>());
    }
    if (const auto precision = interface->find("precision"); precision != interface->end() && precision->is_object()) {
      precisionPreferences.imageValuePrecision =
        precisionFromJson(*precision, "imageValues", precisionPreferences.imageValuePrecision);
      precisionPreferences.coordsPrecision =
        precisionFromJson(*precision, "coordinates", precisionPreferences.coordsPrecision);
      precisionPreferences.txPrecision =
        precisionFromJson(*precision, "transformations", precisionPreferences.txPrecision);
      precisionPreferences.percentilePrecision =
        precisionFromJson(*precision, "percentiles", precisionPreferences.percentilePrecision);
      precisionPreferences.timeValuePrecision =
        precisionFromJson(*precision, "timeValues", precisionPreferences.timeValuePrecision);
    }
  }

  if (const auto views = root.find("views"); views != root.end() && views->is_object()) {
    setFromJson(renderPreferences.showImageBorders, *views, "showImageBorders");
    if (const auto overlays = views->find("showOverlays"); overlays != views->end() && overlays->is_boolean()) {
      settings.setOverlays(overlays->get<bool>());
    }
    if (const auto crosshairs = views->find("crosshairs"); crosshairs != views->end() && crosshairs->is_object()) {
      setFromJson(renderPreferences.showCrosshairs, *crosshairs, "show");
      setFromJson(renderPreferences.showCrosshairsInLightboxViews, *crosshairs, "showInLightboxViews");
      setVec4FromJson(renderPreferences.crosshairsColor, *crosshairs, "color");
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
      setFloatFromJson(renderPreferences.anatomicalLabelScale, *labels, "scale", 0.5f, 2.0f);
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
      setFromJson(renderPreferences.showImageBordersInLightboxViews, *lightbox, "showImageBorders");
      setFromJson(renderPreferences.showLightboxOffsetLabels, *lightbox, "showOffsetLabels");
      setVec4FromJson(renderPreferences.lightboxOffsetLabelColor, *lightbox, "offsetLabelColor");
    }
  }

  if (const auto images = root.find("images"); images != root.end() && images->is_object()) {
    setEnumFromJson(
      renderPreferences.floatingPointLinearInterpolationPolicy,
      *images,
      "floatingPointLinearInterpolationPolicy",
      sk_floatingPointInterpolationPolicyNames);
    setEnumFromJson(
      renderPreferences.isocontourFloatingPointInterpolationPolicy,
      *images,
      "isocontourFloatingPointInterpolationPolicy",
      sk_floatingPointInterpolationPolicyNames);
  }

  if (const auto segmentation = root.find("segmentation"); segmentation != root.end() && segmentation->is_object()) {
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
    if (const auto camera = rendering->find("camera"); camera != rendering->end() && camera->is_object()) {
      setFromJson(renderPreferences.reversePovRotation, *camera, "reversePovRotation");
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
    if (const auto value = annotations->find("crosshairsMoveWhileAnnotating");
        value != annotations->end() && value->is_boolean())
    {
      settings.setCrosshairsMoveWhileAnnotating(value->get<bool>());
    }
  }

  if (const auto registrationSettings = root.find("registration");
      registrationSettings != root.end() && registrationSettings->is_object())
  {
    settings.registrationBackendConfig() = registrationSettings->get<registration::BackendConfig>();
  }

  if (const auto recent = root.find("recent"); recent != root.end() && recent->is_object()) {
    if (const auto images = recent->find("images"); images != recent->end()) {
      settings.setRecentImageGroups(pathGroupsFromJson(*images));
    }
    if (const auto dicom = recent->find("dicom"); dicom != recent->end()) {
      settings.setRecentDicomGroups(pathGroupsFromJson(*dicom));
    }
    if (const auto projects = recent->find("projects"); projects != recent->end()) {
      settings.setRecentProjectFiles(pathsFromJson(*projects));
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
        for (const auto& choice : logging::allLogLevelChoices()) {
          if (choice.label == levelName && logging::isLogLevelChoiceAvailable(choice)) {
            logging::setDefaultLoggerSinkLevel(choice.level);
            break;
          }
        }
      }
    }
    if (const auto updates = system->find("updates"); updates != system->end() && updates->is_object()) {
      if (const auto value = updates->find("automaticChecks"); value != updates->end() && value->is_boolean()) {
        settings.setAutomaticUpdateChecksEnabled(value->get<bool>());
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

void preserveProjectOwnedRenderPreferences(RenderPreferences& preferences, const RenderPreferences& currentPreferences)
{
  // Application defaults must not reset presentation state that belongs to the open project.
  preferences.showImageBorders = currentPreferences.showImageBorders;
  preferences.showImageBordersInLightboxViews = currentPreferences.showImageBordersInLightboxViews;
  preferences.showCrosshairs = currentPreferences.showCrosshairs;
  preferences.showCrosshairsInLightboxViews = currentPreferences.showCrosshairsInLightboxViews;
  preferences.crosshairsSnapping = currentPreferences.crosshairsSnapping;
  preferences.showAnatomicalLabels = currentPreferences.showAnatomicalLabels;
  preferences.showAnatomicalLabelsInLightboxViews = currentPreferences.showAnatomicalLabelsInLightboxViews;
  preferences.anatomicalLabelType = currentPreferences.anatomicalLabelType;
  preferences.showScaleBars = currentPreferences.showScaleBars;
  preferences.showScaleBarsInLightboxViews = currentPreferences.showScaleBarsInLightboxViews;
  preferences.useMaximumIntensityProjectionExtent = currentPreferences.useMaximumIntensityProjectionExtent;
  preferences.intensityProjectionSlabThicknessMm = currentPreferences.intensityProjectionSlabThicknessMm;
  preferences.xrayEnergyKeV = currentPreferences.xrayEnergyKeV;
  preferences.xrayWindow = currentPreferences.xrayWindow;
  preferences.xrayLevel = currentPreferences.xrayLevel;
  preferences.modulateIsocontourOpacityWithImageOpacity = currentPreferences.modulateIsocontourOpacityWithImageOpacity;
  preferences.modulateSegmentationOpacityWithImageOpacity =
    currentPreferences.modulateSegmentationOpacityWithImageOpacity;
  preferences.segmentationOutlineStyle = currentPreferences.segmentationOutlineStyle;
  preferences.segmentationInteriorOpacity = currentPreferences.segmentationInteriorOpacity;
  preferences.segmentationErosionFactor = currentPreferences.segmentationErosionFactor;
  preferences.squaredDifference = currentPreferences.squaredDifference;
  preferences.squaredDifferenceMetric = currentPreferences.squaredDifferenceMetric;
  preferences.localNccMetric = currentPreferences.localNccMetric;
  preferences.localNccPatchRadius = currentPreferences.localNccPatchRadius;
  preferences.localNccSampleSpacing = currentPreferences.localNccSampleSpacing;
  preferences.localNccMinValidFraction = currentPreferences.localNccMinValidFraction;
  preferences.localNccVarianceEpsilon = currentPreferences.localNccVarianceEpsilon;
  preferences.localNccIgnoreNegativeCorrelation = currentPreferences.localNccIgnoreNegativeCorrelation;
  preferences.localNccPresentation = currentPreferences.localNccPresentation;
  preferences.localNccInvalidStyle = currentPreferences.localNccInvalidStyle;
  preferences.localLinearResidualMetric = currentPreferences.localLinearResidualMetric;
  preferences.localLinearResidualPatchRadius = currentPreferences.localLinearResidualPatchRadius;
  preferences.localLinearResidualSampleSpacing = currentPreferences.localLinearResidualSampleSpacing;
  preferences.localLinearResidualMinValidFraction = currentPreferences.localLinearResidualMinValidFraction;
  preferences.localLinearResidualVarianceEpsilon = currentPreferences.localLinearResidualVarianceEpsilon;
  preferences.localLinearResidualInvalidStyle = currentPreferences.localLinearResidualInvalidStyle;
  preferences.overlayMagentaCyan = currentPreferences.overlayMagentaCyan;
  preferences.quadrants = currentPreferences.quadrants;
  preferences.checkerboardSquares = currentPreferences.checkerboardSquares;
  preferences.flashlightRadiusFraction = currentPreferences.flashlightRadiusFraction;
  preferences.flashlightOverlayMovingImage = currentPreferences.flashlightOverlayMovingImage;
  preferences.raycastSamplingFactor = currentPreferences.raycastSamplingFactor;
  preferences.transparentBackgroundWhenNoHit = currentPreferences.transparentBackgroundWhenNoHit;
  preferences.renderFrontFaces = currentPreferences.renderFrontFaces;
  preferences.renderBackFaces = currentPreferences.renderBackFaces;
  preferences.segmentationMasking = currentPreferences.segmentationMasking;
  preferences.annotationsOnTop = currentPreferences.annotationsOnTop;
  preferences.landmarksOnTop = currentPreferences.landmarksOnTop;
  preferences.hideAnnotationVertices = currentPreferences.hideAnnotationVertices;
}

RenderPreferences applicationRenderPreferences(RenderPreferences preferences)
{
  preserveProjectOwnedRenderPreferences(preferences, RenderPreferences{});
  return preferences;
}

std::string toJsonString(
  const AppSettings& settings,
  const RenderPreferences& renderPreferences,
  const PrecisionPreferences& precisionPreferences)
{
  return toJson(settings, renderPreferences, precisionPreferences).dump(2);
}

bool applyJsonString(
  AppSettings& settings,
  RenderPreferences& renderPreferences,
  PrecisionPreferences& precisionPreferences,
  const std::string& text,
  std::string* error)
{
  try {
    const json root = json::parse(text);
    applyJson(settings, renderPreferences, precisionPreferences, root);
    return true;
  }
  catch (const std::exception& e) {
    if (error != nullptr) {
      *error = e.what();
    }
    return false;
  }
}

bool applyJsonString(
  AppSettings& settings,
  RenderPreferences& renderPreferences,
  const std::string& text,
  std::string* error)
{
  PrecisionPreferences precisionPreferences;
  return applyJsonString(settings, renderPreferences, precisionPreferences, text, error);
}

bool save(
  const AppSettings& settings,
  const RenderPreferences& renderPreferences,
  const PrecisionPreferences& precisionPreferences,
  const std::filesystem::path& fileName,
  std::string* error)
{
  try {
    if (!fileName.parent_path().empty()) {
      std::filesystem::create_directories(fileName.parent_path());
    }
    std::ofstream out(fileName, std::ios::out | std::ios::trunc);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << toJsonString(settings, renderPreferences, precisionPreferences) << '\n';
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

bool save(
  const AppSettings& settings,
  const RenderPreferences& renderPreferences,
  const std::filesystem::path& fileName,
  std::string* error)
{
  return save(settings, renderPreferences, PrecisionPreferences{}, fileName, error);
}

bool load(
  AppSettings& settings,
  RenderPreferences& renderPreferences,
  PrecisionPreferences& precisionPreferences,
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
    if (!applyJsonString(settings, renderPreferences, precisionPreferences, buffer.str(), error)) {
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

bool load(
  AppSettings& settings,
  RenderPreferences& renderPreferences,
  const std::filesystem::path& fileName,
  std::string* error)
{
  PrecisionPreferences precisionPreferences;
  return load(settings, renderPreferences, precisionPreferences, fileName, error);
}

} // namespace user_preferences
