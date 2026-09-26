#include "logic/annotation/SerializeAnnot.h"

#include "common/Exception.hpp"

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <map>
#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

using json = nlohmann::json;

namespace
{
static constexpr size_t OUTER_BOUNDARY = 0;
static constexpr int ANNOTATION_JSON_MAJOR_VERSION = 1;
static constexpr int ANNOTATION_JSON_MINOR_VERSION = 0;
static constexpr const char* POLYGON_ANNOTATION_TYPE = "polygon";

json annotationVersionToJson()
{
  return json{{"major", ANNOTATION_JSON_MAJOR_VERSION}, {"minor", ANNOTATION_JSON_MINOR_VERSION}};
}

void validateAnnotationVersion(const json& value)
{
  if (!value.is_object()) {
    throwDebug("Annotation JSON version must be an object");
  }

  const int major = value.at("major").get<int>();
  const int minor = value.at("minor").get<int>();
  if (major != ANNOTATION_JSON_MAJOR_VERSION || minor != ANNOTATION_JSON_MINOR_VERSION) {
    throwDebug("Unsupported annotation JSON version");
  }
}

json boundaryToJson(const std::vector<glm::vec2>& vertices)
{
  json boundary = json::array();
  std::transform(vertices.begin(), vertices.end(), std::back_inserter(boundary), [](const glm::vec2& vertex) {
    return std::vector<float>{vertex.x, vertex.y};
  });
  return boundary;
}

std::vector<glm::vec2> boundaryFromJson(const json& boundaryJson)
{
  if (!boundaryJson.is_array()) {
    throwDebug("Annotation boundary must be an array");
  }
  std::vector<glm::vec2> vertices;
  for (const auto& vertex : boundaryJson) {
    if (!vertex.is_array() || vertex.size() != 2) {
      throwDebug("JSON structure contains invalid vertex");
    }
    const float x = vertex.at(0).get<float>();
    const float y = vertex.at(1).get<float>();
    if (!std::isfinite(x) || !std::isfinite(y)) {
      throwDebug("Annotation vertex coordinates must be finite");
    }
    vertices.emplace_back(x, y);
  }
  return vertices;
}

json boundariesToJson(const Annotation& annot)
{
  json boundaries = json::array();
  const auto& vertices = annot.getAllVertices();
  std::transform(vertices.begin(), vertices.end(), std::back_inserter(boundaries), boundaryToJson);

  return boundaries;
}

std::vector<std::vector<glm::vec2>> boundariesFromJson(const json& boundariesJson)
{
  if (!boundariesJson.is_array()) {
    throwDebug("Annotation boundaries must be an array");
  }

  std::vector<std::vector<glm::vec2>> boundaries;
  boundaries.reserve(boundariesJson.size());
  std::transform(boundariesJson.begin(), boundariesJson.end(), std::back_inserter(boundaries), boundaryFromJson);
  if (boundaries.size() > 1 && boundaries.front().empty()) {
    throwDebug("Annotation holes require a nonempty outer boundary");
  }
  return boundaries;
}

void requireFiniteRange(float value, float lower, float upper, const char* field)
{
  if (!std::isfinite(value) || value < lower || value > upper) {
    throwDebug(std::string{"Invalid annotation "} + field);
  }
}

void appendAnnotationsFromArray(const json& annotationArrayJson, std::vector<Annotation>& annotations)
{
  if (!annotationArrayJson.is_array()) {
    throwDebug("Annotation JSON must contain an annotation array");
  }

  annotations.reserve(annotations.size() + annotationArrayJson.size());
  std::copy(annotationArrayJson.begin(), annotationArrayJson.end(), std::back_inserter(annotations));
}

template<typename T>
void addIfChanged(json& j, const char* key, const T& value, const T& defaultValue)
{
  if (value != defaultValue) {
    j[key] = value;
  }
}
} // namespace

void to_json(json& j, const AnnotPolygon<float, 2>& poly)
{
  j.clear();
  j = boundaryToJson(poly.getBoundaryVertices(OUTER_BOUNDARY));
}

void from_json(const json& j, AnnotPolygon<float, 2>& poly)
{
  AnnotPolygon<float, 2> newPoly;
  newPoly.setOuterBoundary(boundaryFromJson(j));

  poly = newPoly;
}

void to_json(json& j, const Annotation& annot)
{
  j.clear();

  const Annotation defaults;
  const glm::vec4& planeEq = annot.getSubjectPlaneEquation();
  const glm::vec4& defaultPlaneEq = defaults.getSubjectPlaneEquation();

  j = json{{"type", POLYGON_ANNOTATION_TYPE}, {"boundaries", boundariesToJson(annot)}};

  addIfChanged(
    j,
    "subjectPlaneNormal",
    json::array({planeEq.x, planeEq.y, planeEq.z}),
    json::array({defaultPlaneEq.x, defaultPlaneEq.y, defaultPlaneEq.z}));
  addIfChanged(j, "subjectPlaneOffset", planeEq[3], defaultPlaneEq[3]);
  addIfChanged(j, "name", annot.getDisplayName(), defaults.getDisplayName());
  addIfChanged(j, "visible", annot.isVisible(), defaults.isVisible());
  addIfChanged(j, "opacity", annot.getOpacity(), defaults.getOpacity());
  addIfChanged(j, "lineThickness", annot.getLineThickness(), defaults.getLineThickness());
  addIfChanged(
    j,
    "lineColor",
    annotation_json::colorToJson(annot.getLineColor()),
    annotation_json::colorToJson(defaults.getLineColor()));
  addIfChanged(
    j,
    "vertexColor",
    annotation_json::colorToJson(annot.getVertexColor()),
    annotation_json::colorToJson(annot.getLineColor()));
  addIfChanged(
    j,
    "fillColor",
    annotation_json::colorToJson(annot.getFillColor()),
    annotation_json::colorToJson(defaults.getFillColor()));
  addIfChanged(j, "verticesVisible", annot.getVertexVisibility(), defaults.getVertexVisibility());
  addIfChanged(j, "closed", annot.isClosed(), defaults.isClosed());
  addIfChanged(j, "filled", annot.isFilled(), defaults.isFilled());
  addIfChanged(j, "smoothed", annot.isSmoothed(), defaults.isSmoothed());
  addIfChanged(j, "smoothingFactor", annot.getSmoothingFactor(), defaults.getSmoothingFactor());
}

void from_json(const json& j, Annotation& annot)
{
  // All of these parameters are optional in the JSON:

  if (!j.is_object()) {
    throwDebug("Annotation must be an object");
  }

  const std::string annotationType = j.at("type").get<std::string>();
  if (annotationType != POLYGON_ANNOTATION_TYPE) {
    throwDebug("Unsupported annotation type in JSON");
  }

  std::string displayName;
  if (j.count("name")) {
    displayName = j.at("name").get<std::string>();
  }

  Annotation defaults;
  bool visible = defaults.isVisible();
  if (j.count("visible")) {
    visible = j.at("visible").get<bool>();
  }

  float opacity = defaults.getOpacity();
  if (j.count("opacity")) {
    opacity = j.at("opacity").get<float>();
  }
  requireFiniteRange(opacity, 0.0f, 1.0f, "opacity");

  float lineThickness = defaults.getLineThickness();
  if (j.count("lineThickness")) {
    lineThickness = j.at("lineThickness").get<float>();
  }
  requireFiniteRange(lineThickness, 0.0f, std::numeric_limits<float>::max(), "line thickness");

  glm::vec4 lineColorVec4 = defaults.getLineColor();
  if (j.count("lineColor")) {
    lineColorVec4 = annotation_json::colorFromJson(j.at("lineColor"));
  }

  glm::vec4 vertexColorVec4 = lineColorVec4;
  if (j.count("vertexColor")) {
    vertexColorVec4 = annotation_json::colorFromJson(j.at("vertexColor"));
  }

  glm::vec4 fillColorVec4 = defaults.getFillColor();
  if (j.count("fillColor")) {
    fillColorVec4 = annotation_json::colorFromJson(j.at("fillColor"));
  }

  bool verticesVisible = defaults.getVertexVisibility();
  if (j.count("verticesVisible")) {
    verticesVisible = j.at("verticesVisible").get<bool>();
  }

  bool closed = defaults.isClosed();
  if (j.count("closed")) {
    closed = j.at("closed").get<bool>();
  }

  bool filled = defaults.isFilled();
  if (j.count("filled")) {
    filled = j.at("filled").get<bool>();
  }

  bool smoothed = defaults.isSmoothed();
  if (j.count("smoothed")) {
    smoothed = j.at("smoothed").get<bool>();
  }

  float smoothingFactor = defaults.getSmoothingFactor();
  if (j.count("smoothingFactor")) {
    smoothingFactor = j.at("smoothingFactor").get<float>();
  }
  requireFiniteRange(smoothingFactor, 0.0f, std::numeric_limits<float>::max(), "smoothing factor");

  glm::vec4 subjectPlaneEquation = defaults.getSubjectPlaneEquation();
  if (j.count("subjectPlaneNormal")) {
    std::array<float, 3> planeNormal;
    j.at("subjectPlaneNormal").get_to(planeNormal);
    subjectPlaneEquation.x = planeNormal[0];
    subjectPlaneEquation.y = planeNormal[1];
    subjectPlaneEquation.z = planeNormal[2];
  }
  if (j.count("subjectPlaneOffset")) {
    j.at("subjectPlaneOffset").get_to(subjectPlaneEquation[3]);
  }

  // The polygon vertices are required in the JSON:
  auto boundaries = boundariesFromJson(j.at("boundaries"));

  if (boundaries.empty() || boundaries.front().empty()) {
    spdlog::warn("Polygon read from JSON has no vertices");
  }

  spdlog::debug("Read polygon JSON with {} boundaries", boundaries.size());

  Annotation newAnnot;
  newAnnot.setDisplayName(displayName);
  if (!newAnnot.setSubjectPlane(subjectPlaneEquation)) {
    throwDebug("Invalid annotation subject plane");
  }
  newAnnot.setVisible(visible);
  newAnnot.setOpacity(opacity);
  newAnnot.setLineThickness(lineThickness);
  newAnnot.setLineColor(lineColorVec4);
  newAnnot.setVertexColor(vertexColorVec4);
  newAnnot.setFillColor(fillColorVec4);
  newAnnot.setVertexVisibility(verticesVisible);
  newAnnot.setClosed(closed);
  newAnnot.setFilled(filled);
  newAnnot.setSmoothed(smoothed);
  newAnnot.setSmoothingFactor(smoothingFactor);
  newAnnot.polygon().setAllVertices(std::move(boundaries));
  newAnnot.markClean();

  annot = newAnnot;
}

void from_json(const json& j, std::vector<Annotation>& annots)
{
  annots = annotationsFromJson(j);
}

json annotationsToJson(const std::vector<Annotation>& annotations)
{
  json annotationArray = json::array();
  std::copy(annotations.begin(), annotations.end(), std::back_inserter(annotationArray));

  return json{{"version", annotationVersionToJson()}, {"annotations", std::move(annotationArray)}};
}

std::vector<Annotation> annotationsFromJson(const json& j)
{
  std::vector<Annotation> annotations;
  if (!j.is_object()) {
    throwDebug("Annotation JSON must be an object");
  }

  validateAnnotationVersion(j.at("version"));

  appendAnnotationsFromArray(j.at("annotations"), annotations);
  return annotations;
}

std::optional<std::filesystem::path> commonAnnotationFileName(const std::vector<Annotation>& annotations)
{
  if (annotations.empty() || annotations.front().getFileName().empty()) {
    return std::nullopt;
  }
  const auto& fileName = annotations.front().getFileName();
  if (std::any_of(annotations.begin(), annotations.end(), [&fileName](const Annotation& annotation) {
        return annotation.getFileName() != fileName;
      }))
  {
    return std::nullopt;
  }
  return fileName;
}
