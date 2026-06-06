#include "windowing/LayoutSpecJson.h"

#include <nlohmann/json.hpp>

namespace layout
{
namespace
{

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
    {"viewType", view.m_viewType},
    {"renderMode", view.m_renderMode},
    {"intensityProjectionMode", view.m_intensityProjectionMode},
    {"offset",
     {{"mode", view.m_offsetMode},
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
    j.at("viewType").get_to(view.m_viewType);
  }
  if (j.count("renderMode")) {
    j.at("renderMode").get_to(view.m_renderMode);
  }
  if (j.count("intensityProjectionMode")) {
    j.at("intensityProjectionMode").get_to(view.m_intensityProjectionMode);
  }
  if (j.count("offset")) {
    const auto& o = j.at("offset");
    if (o.count("mode")) {
      o.at("mode").get_to(view.m_offsetMode);
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
    {"kind", layout.m_kind},
    {"isLightbox", layout.m_isLightbox},
    {"viewType", layout.m_viewType},
    {"renderMode", layout.m_renderMode},
    {"intensityProjectionMode", layout.m_intensityProjectionMode},
    {"preferredDefaultRenderedImages", layout.m_preferredDefaultRenderedImages},
    {"defaultRenderAllImages", layout.m_defaultRenderAllImages},
    {"imageSelection", layout.m_imageSelection},
    {"views", layout.m_views}};
}

void from_json(const nlohmann::json& j, LayoutSpec& layout)
{
  if (j.count("kind")) {
    j.at("kind").get_to(layout.m_kind);
  }
  if (j.count("isLightbox")) {
    j.at("isLightbox").get_to(layout.m_isLightbox);
  }
  if (j.count("viewType")) {
    j.at("viewType").get_to(layout.m_viewType);
  }
  if (j.count("renderMode")) {
    j.at("renderMode").get_to(layout.m_renderMode);
  }
  if (j.count("intensityProjectionMode")) {
    j.at("intensityProjectionMode").get_to(layout.m_intensityProjectionMode);
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
