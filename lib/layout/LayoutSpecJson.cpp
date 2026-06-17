#include "layout/LayoutSpecJson.h"

#include "viewer_types/ViewModes.h"
#include "viewer_types/ViewTypes.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace layout
{
namespace
{

template<typename Enum>
int enumToInt(Enum value)
{
  return static_cast<int>(value);
}

template<typename Enum>
int enumValueFromJson(const nlohmann::json& j, const std::vector<std::pair<Enum, const char*>>& names)
{
  if (j.is_number_integer()) {
    return j.get<int>();
  }

  const std::string value = j.get<std::string>();
  for (const auto& [enumValue, name] : names) {
    if (value == name) {
      return enumToInt(enumValue);
    }
  }

  throw nlohmann::json::type_error::create(302, "unsupported enum value: " + value, &j);
}

template<typename Enum>
nlohmann::json enumValueToJson(int value, const std::vector<std::pair<Enum, const char*>>& names)
{
  for (const auto& [enumValue, name] : names) {
    if (value == enumToInt(enumValue)) {
      return name;
    }
  }
  return value;
}

const std::vector<std::pair<int, const char*>>& layoutKindNames()
{
  // Keep this mapping in sync with LayoutKind without pulling Layout.h into the JSON-only test target.
  static const std::vector<std::pair<int, const char*>> names{
    {0, "Custom"},
    {1, "FourUp"},
    {2, "Tri"},
    {3, "SingleAxial"},
    {4, "MultiImageAxialGrid"},
    {5, "AxCorSagByImage"},
    {6, "AxialLightbox"},
    {7, "CoronalLightbox"},
    {8, "SagittalLightbox"},
    {9, "SingleCoronal"},
    {10, "SingleSagittal"},
    {11, "MultiImageCoronalGrid"},
    {12, "MultiImageSagittalGrid"}};
  return names;
}

const std::vector<std::pair<ViewType, const char*>>& viewTypeNames()
{
  static const std::vector<std::pair<ViewType, const char*>> names{
    {ViewType::Axial, "Axial"},
    {ViewType::Coronal, "Coronal"},
    {ViewType::Sagittal, "Sagittal"},
    {ViewType::Oblique, "Oblique"},
    {ViewType::ThreeD, "ThreeD"}};
  return names;
}

const std::vector<std::pair<ViewRenderMode, const char*>>& renderModeNames()
{
  static const std::vector<std::pair<ViewRenderMode, const char*>> names{
    {ViewRenderMode::Image, "Image"},
    {ViewRenderMode::Checkerboard, "Checkerboard"},
    {ViewRenderMode::Quadrants, "Quadrants"},
    {ViewRenderMode::Flashlight, "Flashlight"},
    {ViewRenderMode::Overlay, "Overlay"},
    {ViewRenderMode::Difference, "Difference"},
    {ViewRenderMode::JointHistogram, "JointHistogram"},
    {ViewRenderMode::VolumeRender, "VolumeRender"},
    {ViewRenderMode::Disabled, "Disabled"}};
  return names;
}

const std::vector<std::pair<IntensityProjectionMode, const char*>>& intensityProjectionModeNames()
{
  static const std::vector<std::pair<IntensityProjectionMode, const char*>> names{
    {IntensityProjectionMode::None, "None"},
    {IntensityProjectionMode::Maximum, "Maximum"},
    {IntensityProjectionMode::Mean, "Mean"},
    {IntensityProjectionMode::Minimum, "Minimum"},
    {IntensityProjectionMode::Xray, "Xray"}};
  return names;
}

const std::vector<std::pair<int, const char*>>& offsetModeNames()
{
  // Keep this mapping in sync with ViewOffsetMode without pulling common runtime headers into this test target.
  static const std::vector<std::pair<int, const char*>> names{
    {0, "RelativeToRefImageScrolls"},
    {1, "RelativeToImageScrolls"},
    {2, "Absolute"},
    {3, "None"}};
  return names;
}

nlohmann::json optionalSizeToJson(const std::optional<std::size_t>& value)
{
  return value ? nlohmann::json(*value) : nlohmann::json(nullptr);
}

std::optional<std::size_t> optionalSizeFromJson(const nlohmann::json& j)
{
  if (j.is_null()) {
    return std::nullopt;
  }
  return j.get<std::size_t>();
}

} // namespace

void to_json(nlohmann::json& j, const ImageSelectionSpec& selection)
{
  j = nlohmann::json::object();
  if (!selection.m_renderedImageIndices.empty()) {
    j["rendered"] = selection.m_renderedImageIndices;
  }
  if (!selection.m_metricImageIndices.empty()) {
    j["metric"] = selection.m_metricImageIndices;
  }
}

void from_json(const nlohmann::json& j, ImageSelectionSpec& selection)
{
  if (j.count("rendered")) {
    j.at("rendered").get_to(selection.m_renderedImageIndices);
  }
  if (j.count("metric")) {
    j.at("metric").get_to(selection.m_metricImageIndices);
  }
}

void to_json(nlohmann::json& j, const ViewSpec& view)
{
  j = nlohmann::json{
    {"viewport",
     {{"left", view.m_left}, {"bottom", view.m_bottom}, {"width", view.m_width}, {"height", view.m_height}}},
    {"viewType", enumValueToJson(view.m_viewType, viewTypeNames())},
    {"renderMode", enumValueToJson(view.m_renderMode, renderModeNames())},
    {"intensityProjectionMode", enumValueToJson(view.m_intensityProjectionMode, intensityProjectionModeNames())},
    {"offset",
     {{"mode", enumValueToJson(view.m_offsetMode, offsetModeNames())},
      {"absolute", view.m_absoluteOffset},
      {"relativeSteps", view.m_relativeOffsetSteps},
      {"imageIndex", optionalSizeToJson(view.m_offsetImageIndex)}}},
    {"sync",
     {{"rotation", optionalSizeToJson(view.m_rotationSyncGroup)},
      {"translation", optionalSizeToJson(view.m_translationSyncGroup)},
      {"zoom", optionalSizeToJson(view.m_zoomSyncGroup)}}},
    {"syncMembership",
     {{"rotation", optionalSizeToJson(view.m_rotationSyncMembershipGroup)},
      {"translation", optionalSizeToJson(view.m_translationSyncMembershipGroup)},
      {"zoom", optionalSizeToJson(view.m_zoomSyncMembershipGroup)}}},
    {"preferredDefaultRenderedImages", view.m_preferredDefaultRenderedImages},
    {"defaultRenderAllImages", view.m_defaultRenderAllImages},
    {"imageSelection", view.m_imageSelection}};
}

void from_json(const nlohmann::json& j, ViewSpec& view)
{
  if (j.count("viewport")) {
    const auto& v = j.at("viewport");
    v.at("left").get_to(view.m_left);
    v.at("bottom").get_to(view.m_bottom);
    v.at("width").get_to(view.m_width);
    v.at("height").get_to(view.m_height);
  }
  if (j.count("viewType")) {
    view.m_viewType = enumValueFromJson(j.at("viewType"), viewTypeNames());
  }
  if (j.count("renderMode")) {
    view.m_renderMode = enumValueFromJson(j.at("renderMode"), renderModeNames());
  }
  if (j.count("intensityProjectionMode")) {
    view.m_intensityProjectionMode = enumValueFromJson(j.at("intensityProjectionMode"), intensityProjectionModeNames());
  }
  if (j.count("offset")) {
    const auto& o = j.at("offset");
    if (o.count("mode")) {
      view.m_offsetMode = enumValueFromJson(o.at("mode"), offsetModeNames());
    }
    if (o.count("absolute")) {
      o.at("absolute").get_to(view.m_absoluteOffset);
    }
    if (o.count("relativeSteps")) {
      o.at("relativeSteps").get_to(view.m_relativeOffsetSteps);
    }
    if (o.count("imageIndex")) {
      view.m_offsetImageIndex = optionalSizeFromJson(o.at("imageIndex"));
    }
  }
  if (j.count("sync")) {
    const auto& s = j.at("sync");
    if (s.count("rotation")) {
      view.m_rotationSyncGroup = optionalSizeFromJson(s.at("rotation"));
    }
    if (s.count("translation")) {
      view.m_translationSyncGroup = optionalSizeFromJson(s.at("translation"));
    }
    if (s.count("zoom")) {
      view.m_zoomSyncGroup = optionalSizeFromJson(s.at("zoom"));
    }
  }
  if (j.count("syncMembership")) {
    const auto& s = j.at("syncMembership");
    if (s.count("rotation")) {
      view.m_rotationSyncMembershipGroup = optionalSizeFromJson(s.at("rotation"));
    }
    if (s.count("translation")) {
      view.m_translationSyncMembershipGroup = optionalSizeFromJson(s.at("translation"));
    }
    if (s.count("zoom")) {
      view.m_zoomSyncMembershipGroup = optionalSizeFromJson(s.at("zoom"));
    }
  }
  if (j.count("preferredDefaultRenderedImages")) {
    j.at("preferredDefaultRenderedImages").get_to(view.m_preferredDefaultRenderedImages);
  }
  if (j.count("defaultRenderAllImages")) {
    j.at("defaultRenderAllImages").get_to(view.m_defaultRenderAllImages);
  }
  if (j.count("imageSelection")) {
    j.at("imageSelection").get_to(view.m_imageSelection);
  }
}

void to_json(nlohmann::json& j, const LayoutSpec& layout)
{
  j = nlohmann::json{
    {"kind", enumValueToJson(layout.m_kind, layoutKindNames())},
    {"isLightbox", layout.m_isLightbox},
    {"viewType", enumValueToJson(layout.m_viewType, viewTypeNames())},
    {"renderMode", enumValueToJson(layout.m_renderMode, renderModeNames())},
    {"intensityProjectionMode", enumValueToJson(layout.m_intensityProjectionMode, intensityProjectionModeNames())},
    {"preferredDefaultRenderedImages", layout.m_preferredDefaultRenderedImages},
    {"defaultRenderAllImages", layout.m_defaultRenderAllImages},
    {"imageSelection", layout.m_imageSelection},
    {"views", layout.m_views}};
}

void from_json(const nlohmann::json& j, LayoutSpec& layout)
{
  if (j.count("kind")) {
    layout.m_kind = enumValueFromJson(j.at("kind"), layoutKindNames());
  }
  if (j.count("isLightbox")) {
    j.at("isLightbox").get_to(layout.m_isLightbox);
  }
  if (j.count("viewType")) {
    layout.m_viewType = enumValueFromJson(j.at("viewType"), viewTypeNames());
  }
  if (j.count("renderMode")) {
    layout.m_renderMode = enumValueFromJson(j.at("renderMode"), renderModeNames());
  }
  if (j.count("intensityProjectionMode")) {
    layout.m_intensityProjectionMode =
      enumValueFromJson(j.at("intensityProjectionMode"), intensityProjectionModeNames());
  }
  if (j.count("preferredDefaultRenderedImages")) {
    j.at("preferredDefaultRenderedImages").get_to(layout.m_preferredDefaultRenderedImages);
  }
  if (j.count("defaultRenderAllImages")) {
    j.at("defaultRenderAllImages").get_to(layout.m_defaultRenderAllImages);
  }
  if (j.count("imageSelection")) {
    j.at("imageSelection").get_to(layout.m_imageSelection);
  }
  if (j.count("views")) {
    j.at("views").get_to(layout.m_views);
  }
}

} // namespace layout
