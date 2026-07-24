#include "logic/serialization/ProjectSerialization.h"
#include "logic/annotation/SerializeAnnot.h"
#include "logic/serialization/SerializationHelpers.h"

namespace serialize
{

json segmentationLabelToJson(const SegmentationLabel& label);
SegmentationLabel segmentationLabelFromJson(const json& j);
json segmentationLabelsToJson(const SegmentationLabels& labels);
SegmentationLabels segmentationLabelsFromJson(const json& j);

void to_json(json& j, const ImageSettings& settings);
void from_json(const json& j, ImageSettings& settings);

void to_json(json& j, const serialize::SegSettings& settings)
{
  const serialize::SegSettings defaults;
  j = json::object();

  json display = json::object();
  addIfChanged(display, "name", settings.m_displayName, defaults.m_displayName);
  addIfChanged(display, "visible", settings.m_visible, defaults.m_visible);
  addIfChanged(display, "opacity", settings.m_opacity, defaults.m_opacity);
  addIfNotEmpty(j, "display", std::move(display));

  addIfChanged(
    j,
    "interpolationMode",
    enumToName(settings.m_interpolationMode, k_interpolationModeNames),
    enumToName(defaults.m_interpolationMode, k_interpolationModeNames));

  if (settings.m_labels) {
    addIfNotEmpty(j, "labels", segmentationLabelsToJson(*settings.m_labels));
  }
}

void from_json(const json& j, serialize::SegSettings& settings)
{
  if (const auto display = j.find("display"); display != j.end() && display->is_object()) {
    if (const auto name = display->find("name"); name != display->end() && name->is_string()) {
      settings.m_displayName = name->get<std::string>();
    }
    if (const auto visible = display->find("visible"); visible != display->end() && visible->is_boolean()) {
      settings.m_visible = visible->get<bool>();
    }
    if (const auto opacity = display->find("opacity"); opacity != display->end() && opacity->is_number()) {
      settings.m_opacity = opacity->get<double>();
    }
  }
  if (const auto parsed = enumFromName<InterpolationMode>(j.value("interpolationMode", ""), k_interpolationModeNames)) {
    settings.m_interpolationMode = *parsed;
  }
  if (const auto labels = j.find("labels"); labels != j.end() && labels->is_object()) {
    settings.m_labels = segmentationLabelsFromJson(*labels);
  }
}

json segmentationLabelToJson(const serialize::SegmentationLabel& label)
{
  return json{
    {"index", label.m_index},
    {"name", label.m_name},
    {"color", vec4ToJson(label.m_color)},
    {"visible", label.m_visible},
    {"showMesh", label.m_showMesh}};
}

serialize::SegmentationLabel segmentationLabelFromJson(const json& j)
{
  serialize::SegmentationLabel label;
  if (const auto index = unsignedIntFromJson(j.value("index", json{}))) {
    label.m_index = *index;
  }
  if (const auto name = j.find("name"); name != j.end() && name->is_string()) {
    label.m_name = name->get<std::string>();
  }
  if (const auto color = j.find("color"); color != j.end() && color->is_array()) {
    label.m_color = glm::clamp(vec4FromJson(*color), glm::vec4{0.0f}, glm::vec4{1.0f});
  }
  if (const auto visible = j.find("visible"); visible != j.end() && visible->is_boolean()) {
    label.m_visible = visible->get<bool>();
  }
  if (const auto showMesh = j.find("showMesh"); showMesh != j.end() && showMesh->is_boolean()) {
    label.m_showMesh = showMesh->get<bool>();
  }
  return label;
}

json segmentationLabelsToJson(const serialize::SegmentationLabels& labels)
{
  json j = json::object();
  if (labels.m_count > 0) {
    j["count"] = labels.m_count;
  }
  if (!labels.m_values.empty()) {
    json values = json::array();
    for (const auto& value : labels.m_values) {
      values.push_back(segmentationLabelToJson(value));
    }
    j["values"] = std::move(values);
  }
  return j;
}

serialize::SegmentationLabels segmentationLabelsFromJson(const json& j)
{
  serialize::SegmentationLabels labels;
  if (const auto count = unsignedIntFromJson(j.value("count", json{}))) {
    labels.m_count = *count;
  }
  if (const auto values = j.find("values"); values != j.end() && values->is_array()) {
    labels.m_values.clear();
    labels.m_values.reserve(values->size());
    for (const auto& value : *values) {
      labels.m_values.push_back(segmentationLabelFromJson(value));
    }
  }
  return labels;
}

void to_json(json& j, const serialize::Segmentation& seg)
{
  j = json{{"path", pathToString(seg.m_segFileName)}};

  if (seg.m_settings) {
    json settings = *seg.m_settings;
    addIfNotEmpty(j, "settings", std::move(settings));
  }
}

void from_json(const json& j, serialize::Segmentation& seg)
{
  std::string p;
  j.at("path").get_to(p);
  seg.m_segFileName = p;

  if (j.count("settings")) {
    seg.m_settings = j.at("settings").get<serialize::SegSettings>();
  }
}

void to_json(json& j, const serialize::LandmarkPoint& point)
{
  const serialize::LandmarkPoint defaults;
  j = json::object();
  addIfChanged(j, "position", vec3ToJson(point.m_position), vec3ToJson(defaults.m_position));
  if (!point.m_name.empty()) {
    j["name"] = point.m_name;
  }
}

void from_json(const json& j, serialize::LandmarkPoint& point)
{
  if (const auto index = unsignedIntFromJson(j.value("index", json{}))) {
    point.m_index = *index;
  }
  if (const auto position = j.find("position"); position != j.end() && position->is_array()) {
    point.m_position = vec3FromJson(*position);
  }
  if (const auto name = j.find("name"); name != j.end() && name->is_string()) {
    point.m_name = name->get<std::string>();
  }
}

void to_json(json& j, const serialize::LandmarkGroup& landmarks)
{
  const serialize::LandmarkGroup defaults;
  j = json::object();

  if (landmarks.m_csvFileName) {
    j["path"] = pathToString(*landmarks.m_csvFileName);
  }

  addIfChanged(
    j,
    "coordinateSpace",
    enumToName(landmarks.m_coordinateSpace, k_landmarkCoordinateSpaceNames),
    enumToName(defaults.m_coordinateSpace, k_landmarkCoordinateSpaceNames));

  if (!landmarks.m_points.empty()) {
    j["points"] = landmarks.m_points;
  }

  json display = json::object();
  addIfChanged(display, "name", landmarks.m_name, defaults.m_name);
  addIfChanged(display, "visible", landmarks.m_visible, defaults.m_visible);
  addIfChanged(display, "opacity", landmarks.m_opacity, defaults.m_opacity);
  addIfChanged(display, "color", vec3ToJson(landmarks.m_color), vec3ToJson(defaults.m_color));
  addIfChanged(display, "colorOverride", landmarks.m_colorOverride, defaults.m_colorOverride);
  addIfChanged(display, "glyphRadiusFactor", landmarks.m_glyphRadiusFactor, defaults.m_glyphRadiusFactor);

  json labels = json::object();
  addIfChanged(labels, "showIndices", landmarks.m_renderLandmarkIndices, defaults.m_renderLandmarkIndices);
  addIfChanged(labels, "showNames", landmarks.m_renderLandmarkNames, defaults.m_renderLandmarkNames);
  if (landmarks.m_textColor) {
    labels["textColor"] = vec3ToJson(*landmarks.m_textColor);
  }
  addIfNotEmpty(display, "labels", std::move(labels));
  addIfNotEmpty(j, "display", std::move(display));
}

void from_json(const json& j, serialize::LandmarkGroup& landmarks)
{
  if (const auto path = j.find("path"); path != j.end() && path->is_string()) {
    landmarks.m_csvFileName = path->get<std::string>();
  }

  if (
    const auto parsed = enumFromName<serialize::ProjectLandmarkCoordinateSpace>(
      j.value("coordinateSpace", ""),
      k_landmarkCoordinateSpaceNames))
  {
    landmarks.m_coordinateSpace = *parsed;
  }

  if (const auto points = j.find("points"); points != j.end()) {
    points->get_to(landmarks.m_points);
    for (std::size_t i = 0; i < landmarks.m_points.size(); ++i) {
      landmarks.m_points.at(i).m_index = i;
    }
  }

  if (const auto display = j.find("display"); display != j.end() && display->is_object()) {
    if (const auto name = display->find("name"); name != display->end() && name->is_string()) {
      landmarks.m_name = name->get<std::string>();
    }
    if (const auto visible = display->find("visible"); visible != display->end() && visible->is_boolean()) {
      landmarks.m_visible = visible->get<bool>();
    }
    if (const auto opacity = display->find("opacity"); opacity != display->end() && opacity->is_number()) {
      landmarks.m_opacity = opacity->get<float>();
    }
    if (const auto color = display->find("color"); color != display->end()) {
      landmarks.m_color = vec3FromJson(*color);
    }
    if (const auto overrideColor = display->find("colorOverride");
        overrideColor != display->end() && overrideColor->is_boolean())
    {
      landmarks.m_colorOverride = overrideColor->get<bool>();
    }
    if (const auto radiusFactor = display->find("glyphRadiusFactor");
        radiusFactor != display->end() && radiusFactor->is_number())
    {
      landmarks.m_glyphRadiusFactor = radiusFactor->get<float>();
    }

    if (const auto labels = display->find("labels"); labels != display->end() && labels->is_object()) {
      if (const auto textColor = labels->find("textColor"); textColor != labels->end() && !textColor->is_null()) {
        landmarks.m_textColor = vec3FromJson(*textColor);
      }
      if (const auto showIndices = labels->find("showIndices");
          showIndices != labels->end() && showIndices->is_boolean())
      {
        landmarks.m_renderLandmarkIndices = showIndices->get<bool>();
      }
      if (const auto showNames = labels->find("showNames"); showNames != labels->end() && showNames->is_boolean()) {
        landmarks.m_renderLandmarkNames = showNames->get<bool>();
      }
    }
  }
}

void to_json(json& j, const serialize::ImageIsosurface& isosurface)
{
  const serialize::ImageIsosurface defaults;
  j = json::object();
  addIfChanged(j, "component", isosurface.m_component, defaults.m_component);

  json surface = isosurface.m_surface;
  addIfNotEmpty(j, "surface", std::move(surface));
}

void from_json(const json& j, serialize::ImageIsosurface& isosurface)
{
  if (const auto component = unsignedIntFromJson(j.value("component", json{}))) {
    isosurface.m_component = *component;
  }
  if (const auto surface = j.find("surface"); surface != j.end() && surface->is_object()) {
    isosurface.m_surface = surface->get<Isosurface>();
  }
  else {
    isosurface.m_surface = j.get<Isosurface>();
  }
}

void to_json(json& j, const serialize::DicomSource& source)
{
  j = json::object();

  if (!source.m_rootPath.empty()) {
    j["rootPath"] = pathToString(source.m_rootPath);
  }

  if (!source.m_seriesInstanceUid.empty()) {
    j["seriesInstanceUid"] = source.m_seriesInstanceUid;
  }

  if (!source.m_studyInstanceUid.empty()) {
    j["studyInstanceUid"] = source.m_studyInstanceUid;
  }

  if (!source.m_files.empty()) {
    std::vector<std::string> paths;
    paths.reserve(source.m_files.size());
    for (const auto& file : source.m_files) {
      paths.push_back(pathToString(file));
    }
    j["paths"] = paths;
  }
}

void from_json(const json& j, serialize::DicomSource& source)
{
  if (j.count("rootPath")) {
    source.m_rootPath = j.at("rootPath").get<std::string>();
  }
  if (j.count("studyInstanceUid")) {
    source.m_studyInstanceUid = j.at("studyInstanceUid").get<std::string>();
  }
  if (j.count("seriesInstanceUid")) {
    source.m_seriesInstanceUid = j.at("seriesInstanceUid").get<std::string>();
  }
  if (j.count("paths")) {
    const auto paths = j.at("paths").get<std::vector<std::string>>();
    source.m_files.clear();
    source.m_files.reserve(paths.size());
    for (const auto& path : paths) {
      source.m_files.emplace_back(path);
    }
  }
}

void to_json(json& j, const serialize::Image& image)
{
  j = json{{"path", pathToString(image.m_imageFileName)}};

  if (image.m_spatialMetadata) {
    j["spatialMetadata"] = {
      {"spacing", vec3ToJson(image.m_spatialMetadata->spacingMm)},
      {"origin", vec3ToJson(image.m_spatialMetadata->originMm)},
      {"directions", mat3ToJson(image.m_spatialMetadata->directions)}};
  }

  if (image.m_dicomSource) {
    json dicomSource = *image.m_dicomSource;
    addIfNotEmpty(j, "dicomSource", std::move(dicomSource));
  }

  if (image.m_initialAffineFileName || image.m_initialAffineMatrix) {
    j["initialAffine"] = affineToJson(image.m_initialAffineFileName, image.m_initialAffineMatrix);
  }

  if (image.m_inverseWarpFieldPath) {
    j["inverseWarpField"] = pathObjectToJson(*image.m_inverseWarpFieldPath);
  }

  if (image.m_inverseWarpReferenceImagePath) {
    j["inverseWarpReferenceImage"] = pathObjectToJson(*image.m_inverseWarpReferenceImagePath);
  }

  if (image.m_forwardWarpFieldPath) {
    j["forwardWarpField"] = pathObjectToJson(*image.m_forwardWarpFieldPath);
  }

  if (image.m_manualAffineFileName || image.m_manualAffineMatrix) {
    j["manualAffine"] = affineToJson(image.m_manualAffineFileName, image.m_manualAffineMatrix);
  }

  if (image.m_annotationsFileName || !image.m_annotations.empty()) {
    json annotations = json::object();
    if (image.m_annotationsFileName) {
      annotations["path"] = pathToString(*image.m_annotationsFileName);
    }
    if (!image.m_annotations.empty()) {
      annotations["embedded"] = annotationsToJson(image.m_annotations);
    }
    j["annotations"] = std::move(annotations);
  }

  if (!image.m_segmentations.empty()) {
    j["segmentations"] = image.m_segmentations;
  }

  if (!image.m_landmarkGroups.empty()) {
    j["landmarks"] = image.m_landmarkGroups;
  }

  if (!image.m_isosurfaces.empty()) {
    j["isosurfaces"] = image.m_isosurfaces;
  }

  if (image.m_settings) {
    json settings = *image.m_settings;
    addIfNotEmpty(j, "settings", std::move(settings));
  }
}

void from_json(const json& j, serialize::Image& image)
{
  std::string p;
  j.at("path").get_to(p);
  image.m_imageFileName = p;

  if (const auto metadata = j.find("spatialMetadata"); metadata != j.end() && metadata->is_object()) {
    ImageSpatialMetadata spatialMetadata;
    spatialMetadata.spacingMm = vec3FromJson(metadata->at("spacing"));
    spatialMetadata.originMm = vec3FromJson(metadata->at("origin"));
    spatialMetadata.directions = mat3FromJson(metadata->at("directions"));
    image.m_spatialMetadata = spatialMetadata;
  }

  if (j.count("dicomSource")) {
    image.m_dicomSource = j.at("dicomSource").get<serialize::DicomSource>();
  }

  if (j.count("initialAffine")) {
    affineFromJson(j.at("initialAffine"), image.m_initialAffineFileName, image.m_initialAffineMatrix);
  }

  if (j.count("inverseWarpField")) {
    image.m_inverseWarpFieldPath = pathObjectFromJson(j.at("inverseWarpField"), "inverseWarpField");
  }

  if (j.count("inverseWarpReferenceImage")) {
    image.m_inverseWarpReferenceImagePath =
      pathObjectFromJson(j.at("inverseWarpReferenceImage"), "inverseWarpReferenceImage");
  }

  if (j.count("forwardWarpField")) {
    image.m_forwardWarpFieldPath = pathObjectFromJson(j.at("forwardWarpField"), "forwardWarpField");
  }

  if (j.count("manualAffine")) {
    affineFromJson(j.at("manualAffine"), image.m_manualAffineFileName, image.m_manualAffineMatrix);
  }

  if (const auto annotations = j.find("annotations"); annotations != j.end()) {
    if (!annotations->is_object()) {
      throwDebug("Image annotations entry must be an object");
    }
    if (const auto path = annotations->find("path"); path != annotations->end() && path->is_string()) {
      image.m_annotationsFileName = path->get<std::string>();
    }
    if (const auto embedded = annotations->find("embedded"); embedded != annotations->end()) {
      image.m_annotations = annotationsFromJson(*embedded);
    }
  }

  if (j.count("segmentations")) {
    j.at("segmentations").get_to(image.m_segmentations);
  }

  if (j.count("landmarks")) {
    j.at("landmarks").get_to(image.m_landmarkGroups);
  }

  if (j.count("isosurfaces")) {
    j.at("isosurfaces").get_to(image.m_isosurfaces);
  }

  if (j.count("settings")) {
    image.m_settings = j.at("settings").get<serialize::ImageSettings>();
  }
}

} // namespace serialize
