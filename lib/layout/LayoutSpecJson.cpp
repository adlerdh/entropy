#include "layout/LayoutSpecJson.h"

#include "viewer/ViewModes.h"
#include "viewer/ViewTypes.h"

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
    {0, "custom"},
    {1, "fourUp"},
    {2, "threeUp"},
    {3, "oneUp"},
    {4, "multiImageGrid"},
    {5, "axCorSagByImage"},
    {6, "lightbox"}};
  return names;
}

const std::vector<std::pair<ViewType, const char*>>& viewTypeNames()
{
  static const std::vector<std::pair<ViewType, const char*>> names{
    {ViewType::Axial, "axial"},
    {ViewType::Coronal, "coronal"},
    {ViewType::Sagittal, "sagittal"},
    {ViewType::Oblique, "oblique"},
    {ViewType::ThreeD, "threeD"}};
  return names;
}

const std::vector<std::pair<ViewRenderMode, const char*>>& renderModeNames()
{
  static const std::vector<std::pair<ViewRenderMode, const char*>> names{
    {ViewRenderMode::Image, "image"},
    {ViewRenderMode::Checkerboard, "checkerboard"},
    {ViewRenderMode::Quadrants, "quadrants"},
    {ViewRenderMode::Flashlight, "flashlight"},
    {ViewRenderMode::Overlay, "overlay"},
    {ViewRenderMode::Difference, "difference"},
    {ViewRenderMode::JointHistogram, "jointHistogram"},
    {ViewRenderMode::VolumeRender, "volumeRender"},
    {ViewRenderMode::Disabled, "disabled"},
    {ViewRenderMode::LocalNcc, "localNcc"},
    {ViewRenderMode::LocalLinearResidual, "localLinearResidual"}};
  return names;
}

const std::vector<std::pair<IntensityProjectionMode, const char*>>& intensityProjectionModeNames()
{
  static const std::vector<std::pair<IntensityProjectionMode, const char*>> names{
    {IntensityProjectionMode::None, "none"},
    {IntensityProjectionMode::Maximum, "maximum"},
    {IntensityProjectionMode::Mean, "mean"},
    {IntensityProjectionMode::Minimum, "minimum"},
    {IntensityProjectionMode::Xray, "xray"}};
  return names;
}

const std::vector<std::pair<int, const char*>>& threeDProjectionTypeNames()
{
  // Keep this mapping in sync with ProjectionType without pulling app camera headers into layout JSON.
  static const std::vector<std::pair<int, const char*>> names{{0, "orthographic"}, {1, "perspective"}};
  return names;
}

const std::vector<std::pair<int, const char*>>& threeDOrbitTargetModeNames()
{
  // Keep this mapping in sync with camera3d::OrbitTargetMode without pulling app camera headers into layout JSON.
  static const std::vector<std::pair<int, const char*>> names{{0, "imageCenter"}, {1, "crosshairs"}};
  return names;
}

const std::vector<std::pair<int, const char*>>& offsetModeNames()
{
  // Keep this mapping in sync with ViewOffsetMode without pulling common runtime headers into this test target.
  static const std::vector<std::pair<int, const char*>> names{
    {0, "relativeToRefImageScrolls"},
    {1, "relativeToImageScrolls"},
    {2, "absolute"},
    {3, "none"}};
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

std::optional<float> optionalFloatFromJson(const nlohmann::json& j)
{
  if (j.is_null()) {
    return std::nullopt;
  }
  return j.get<float>();
}

template<typename T>
void addIfChanged(nlohmann::json& j, const char* key, const T& value, const T& defaultValue)
{
  if (value != defaultValue) {
    j[key] = value;
  }
}

void addIfNotEmpty(nlohmann::json& j, const char* key, nlohmann::json value)
{
  if (!value.empty()) {
    j[key] = std::move(value);
  }
}

} // namespace

void to_json(nlohmann::json& j, const ImageSelectionSpec& selection)
{
  j = nlohmann::json::object();
  if (!selection.m_renderedImageIndices.empty()) {
    j["rendered"] = selection.m_renderedImageIndices;
  }
  if (!selection.m_volumeRenderedImageIndices.empty()) {
    j["volumeRendered"] = selection.m_volumeRenderedImageIndices;
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
  if (j.count("volumeRendered")) {
    j.at("volumeRendered").get_to(selection.m_volumeRenderedImageIndices);
    if (selection.m_volumeRenderedImageIndices.size() > 1) {
      selection.m_volumeRenderedImageIndices.resize(1);
    }
  }
  if (j.count("metric")) {
    j.at("metric").get_to(selection.m_metricImageIndices);
  }
}

void to_json(nlohmann::json& j, const GridSpec& grid)
{
  const GridSpec defaults;
  j = nlohmann::json::object();
  addIfChanged(j, "columns", grid.m_columns, defaults.m_columns);
  addIfChanged(j, "rows", grid.m_rows, defaults.m_rows);
  addIfChanged(j, "offsetViews", grid.m_offsetViews, defaults.m_offsetViews);
  addIfChanged(j, "imageIndex", optionalSizeToJson(grid.m_imageIndex), optionalSizeToJson(defaults.m_imageIndex));
  addIfChanged(j, "absoluteOffsetStep", grid.m_absoluteOffsetStep, defaults.m_absoluteOffsetStep);
}

void from_json(const nlohmann::json& j, GridSpec& grid)
{
  if (j.count("columns")) {
    j.at("columns").get_to(grid.m_columns);
  }
  if (j.count("rows")) {
    j.at("rows").get_to(grid.m_rows);
  }
  if (j.count("offsetViews")) {
    j.at("offsetViews").get_to(grid.m_offsetViews);
  }
  if (j.count("imageIndex")) {
    grid.m_imageIndex = optionalSizeFromJson(j.at("imageIndex"));
  }
  if (j.count("absoluteOffsetStep")) {
    grid.m_absoluteOffsetStep = optionalFloatFromJson(j.at("absoluteOffsetStep"));
  }
}

void to_json(nlohmann::json& j, const ViewSpec& view)
{
  const ViewSpec defaults;
  j = nlohmann::json::object();

  addIfChanged(j, "index", optionalSizeToJson(view.m_index), optionalSizeToJson(defaults.m_index));

  nlohmann::json viewport = nlohmann::json::object();
  addIfChanged(viewport, "left", view.m_left, defaults.m_left);
  addIfChanged(viewport, "bottom", view.m_bottom, defaults.m_bottom);
  addIfChanged(viewport, "width", view.m_width, defaults.m_width);
  addIfChanged(viewport, "height", view.m_height, defaults.m_height);
  addIfNotEmpty(j, "viewport", std::move(viewport));

  addIfChanged(
    j,
    "viewType",
    enumValueToJson(view.m_viewType, viewTypeNames()),
    enumValueToJson(defaults.m_viewType, viewTypeNames()));
  addIfChanged(
    j,
    "renderMode",
    enumValueToJson(view.m_renderMode, renderModeNames()),
    enumValueToJson(defaults.m_renderMode, renderModeNames()));
  addIfChanged(
    j,
    "intensityProjectionMode",
    enumValueToJson(view.m_intensityProjectionMode, intensityProjectionModeNames()),
    enumValueToJson(defaults.m_intensityProjectionMode, intensityProjectionModeNames()));

  nlohmann::json offset = nlohmann::json::object();
  addIfChanged(
    offset,
    "mode",
    enumValueToJson(view.m_offsetMode, offsetModeNames()),
    enumValueToJson(defaults.m_offsetMode, offsetModeNames()));
  addIfChanged(offset, "absolute", view.m_absoluteOffset, defaults.m_absoluteOffset);
  addIfChanged(offset, "relativeSteps", view.m_relativeOffsetSteps, defaults.m_relativeOffsetSteps);
  addIfChanged(
    offset,
    "imageIndex",
    optionalSizeToJson(view.m_offsetImageIndex),
    optionalSizeToJson(defaults.m_offsetImageIndex));
  addIfNotEmpty(j, "offset", std::move(offset));

  nlohmann::json source = nlohmann::json::object();
  addIfChanged(
    source,
    "rotation",
    optionalSizeToJson(view.m_rotationSyncGroup),
    optionalSizeToJson(defaults.m_rotationSyncGroup));
  addIfChanged(
    source,
    "translation",
    optionalSizeToJson(view.m_translationSyncGroup),
    optionalSizeToJson(defaults.m_translationSyncGroup));
  addIfChanged(source, "zoom", optionalSizeToJson(view.m_zoomSyncGroup), optionalSizeToJson(defaults.m_zoomSyncGroup));

  nlohmann::json membership = nlohmann::json::object();
  addIfChanged(
    membership,
    "rotation",
    optionalSizeToJson(view.m_rotationSyncMembershipGroup),
    optionalSizeToJson(defaults.m_rotationSyncMembershipGroup));
  addIfChanged(
    membership,
    "translation",
    optionalSizeToJson(view.m_translationSyncMembershipGroup),
    optionalSizeToJson(defaults.m_translationSyncMembershipGroup));
  addIfChanged(
    membership,
    "zoom",
    optionalSizeToJson(view.m_zoomSyncMembershipGroup),
    optionalSizeToJson(defaults.m_zoomSyncMembershipGroup));

  nlohmann::json sync = nlohmann::json::object();
  addIfNotEmpty(sync, "source", std::move(source));
  addIfNotEmpty(sync, "membership", std::move(membership));
  addIfNotEmpty(j, "sync", std::move(sync));

  nlohmann::json defaultImages = nlohmann::json::object();
  addIfChanged(
    defaultImages,
    "preferred",
    view.m_preferredDefaultRenderedImages,
    defaults.m_preferredDefaultRenderedImages);
  addIfChanged(defaultImages, "renderAll", view.m_defaultRenderAllImages, defaults.m_defaultRenderAllImages);
  addIfNotEmpty(j, "defaultImages", std::move(defaultImages));

  nlohmann::json images = view.m_imageSelection;
  addIfNotEmpty(j, "images", std::move(images));

  nlohmann::json threeD = nlohmann::json::object();
  addIfChanged(
    threeD,
    "projection",
    enumValueToJson(view.m_threeDProjectionType, threeDProjectionTypeNames()),
    enumValueToJson(defaults.m_threeDProjectionType, threeDProjectionTypeNames()));
  addIfChanged(
    threeD,
    "orbitTarget",
    enumValueToJson(view.m_threeDOrbitTargetMode, threeDOrbitTargetModeNames()),
    enumValueToJson(defaults.m_threeDOrbitTargetMode, threeDOrbitTargetModeNames()));
  addIfChanged(
    threeD,
    "cameraFollowsCrosshairs",
    view.m_threeDCameraFollowsCrosshairs,
    defaults.m_threeDCameraFollowsCrosshairs);
  addIfChanged(threeD, "perspectiveZoom", view.m_threeDPerspectiveZoom, defaults.m_threeDPerspectiveZoom);
  addIfChanged(threeD, "orthographicZoom", view.m_threeDOrthographicZoom, defaults.m_threeDOrthographicZoom);
  addIfNotEmpty(j, "threeD", std::move(threeD));
}

void from_json(const nlohmann::json& j, ViewSpec& view)
{
  if (j.count("index")) {
    view.m_index = optionalSizeFromJson(j.at("index"));
  }
  if (j.count("viewport")) {
    const auto& v = j.at("viewport");
    if (v.count("left")) {
      v.at("left").get_to(view.m_left);
    }
    if (v.count("bottom")) {
      v.at("bottom").get_to(view.m_bottom);
    }
    if (v.count("width")) {
      v.at("width").get_to(view.m_width);
    }
    if (v.count("height")) {
      v.at("height").get_to(view.m_height);
    }
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
    if (s.count("source")) {
      const auto& source = s.at("source");
      if (source.count("rotation")) {
        view.m_rotationSyncGroup = optionalSizeFromJson(source.at("rotation"));
      }
      if (source.count("translation")) {
        view.m_translationSyncGroup = optionalSizeFromJson(source.at("translation"));
      }
      if (source.count("zoom")) {
        view.m_zoomSyncGroup = optionalSizeFromJson(source.at("zoom"));
      }
    }
    if (s.count("membership")) {
      const auto& membership = s.at("membership");
      if (membership.count("rotation")) {
        view.m_rotationSyncMembershipGroup = optionalSizeFromJson(membership.at("rotation"));
      }
      if (membership.count("translation")) {
        view.m_translationSyncMembershipGroup = optionalSizeFromJson(membership.at("translation"));
      }
      if (membership.count("zoom")) {
        view.m_zoomSyncMembershipGroup = optionalSizeFromJson(membership.at("zoom"));
      }
    }
  }
  if (j.count("defaultImages")) {
    const auto& defaultImages = j.at("defaultImages");
    if (defaultImages.count("preferred")) {
      defaultImages.at("preferred").get_to(view.m_preferredDefaultRenderedImages);
    }
    if (defaultImages.count("renderAll")) {
      defaultImages.at("renderAll").get_to(view.m_defaultRenderAllImages);
    }
  }
  if (j.count("images")) {
    j.at("images").get_to(view.m_imageSelection);
  }
  if (j.count("threeD")) {
    const auto& t = j.at("threeD");
    if (t.count("projection")) {
      view.m_threeDProjectionType = enumValueFromJson(t.at("projection"), threeDProjectionTypeNames());
    }
    if (t.count("orbitTarget")) {
      view.m_threeDOrbitTargetMode = enumValueFromJson(t.at("orbitTarget"), threeDOrbitTargetModeNames());
    }
    if (t.count("cameraFollowsCrosshairs")) {
      t.at("cameraFollowsCrosshairs").get_to(view.m_threeDCameraFollowsCrosshairs);
    }
    if (t.count("perspectiveZoom")) {
      t.at("perspectiveZoom").get_to(view.m_threeDPerspectiveZoom);
    }
    if (t.count("orthographicZoom")) {
      t.at("orthographicZoom").get_to(view.m_threeDOrthographicZoom);
    }
  }
}

void to_json(nlohmann::json& j, const LayoutSpec& layout)
{
  const LayoutSpec defaults;
  j = nlohmann::json::object();

  addIfChanged(
    j,
    "kind",
    enumValueToJson(layout.m_kind, layoutKindNames()),
    enumValueToJson(defaults.m_kind, layoutKindNames()));
  addIfChanged(j, "displayName", layout.m_displayName, defaults.m_displayName);
  addIfChanged(j, "isLightbox", layout.m_isLightbox, defaults.m_isLightbox);
  addIfChanged(
    j,
    "viewType",
    enumValueToJson(layout.m_viewType, viewTypeNames()),
    enumValueToJson(defaults.m_viewType, viewTypeNames()));
  addIfChanged(
    j,
    "renderMode",
    enumValueToJson(layout.m_renderMode, renderModeNames()),
    enumValueToJson(defaults.m_renderMode, renderModeNames()));
  addIfChanged(
    j,
    "intensityProjectionMode",
    enumValueToJson(layout.m_intensityProjectionMode, intensityProjectionModeNames()),
    enumValueToJson(defaults.m_intensityProjectionMode, intensityProjectionModeNames()));

  nlohmann::json defaultImages = nlohmann::json::object();
  addIfChanged(
    defaultImages,
    "preferred",
    layout.m_preferredDefaultRenderedImages,
    defaults.m_preferredDefaultRenderedImages);
  addIfChanged(defaultImages, "renderAll", layout.m_defaultRenderAllImages, defaults.m_defaultRenderAllImages);
  addIfNotEmpty(j, "defaultImages", std::move(defaultImages));

  nlohmann::json images = layout.m_imageSelection;
  addIfNotEmpty(j, "images", std::move(images));

  if (layout.m_grid) {
    nlohmann::json grid = *layout.m_grid;
    j["grid"] = std::move(grid);
  }

  if (!layout.m_views.empty()) {
    j["views"] = layout.m_views;
  }
}

void from_json(const nlohmann::json& j, LayoutSpec& layout)
{
  if (j.count("kind")) {
    layout.m_kind = enumValueFromJson(j.at("kind"), layoutKindNames());
  }
  if (j.count("displayName")) {
    j.at("displayName").get_to(layout.m_displayName);
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
  if (j.count("defaultImages")) {
    const auto& defaultImages = j.at("defaultImages");
    if (defaultImages.count("preferred")) {
      defaultImages.at("preferred").get_to(layout.m_preferredDefaultRenderedImages);
    }
    if (defaultImages.count("renderAll")) {
      defaultImages.at("renderAll").get_to(layout.m_defaultRenderAllImages);
    }
  }
  if (j.count("images")) {
    j.at("images").get_to(layout.m_imageSelection);
  }
  if (j.count("grid")) {
    layout.m_grid = j.at("grid").get<GridSpec>();
  }
  if (j.count("views")) {
    j.at("views").get_to(layout.m_views);
  }
}

} // namespace layout
