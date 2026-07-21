#include "logic/annotation/SerializeAnnot.h"
#include "common/Exception.hpp"

#include <spdlog/spdlog.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace
{
static constexpr size_t OUTER_BOUNDARY = 0;
static constexpr int ANNOTATION_JSON_VERSION = 1;
static constexpr const char* POLYGON_ANNOTATION_TYPE = "polygon";

json colorToJson(const glm::vec4& color)
{
  return json{{"r", color.r}, {"g", color.g}, {"b", color.b}, {"a", color.a}};
}

glm::vec4 colorFromJson(const json& value, const glm::vec4& fallback)
{
  if (value.is_object()) {
    return glm::vec4{
      value.value("r", fallback.r),
      value.value("g", fallback.g),
      value.value("b", fallback.b),
      value.value("a", fallback.a)};
  }

  throwDebug("JSON structure contains invalid color");
}

json boundaryToJson(const std::vector<glm::vec2>& vertices)
{
  json boundary = json::array();
  for (const auto& vertex : vertices) {
    boundary.emplace_back(std::vector<float>{vertex.x, vertex.y});
  }
  return boundary;
}

std::vector<glm::vec2> boundaryFromJson(const json& boundaryJson)
{
  std::vector<glm::vec2> vertices;
  for (const auto& vertex : boundaryJson) {
    if (!vertex.is_array() || vertex.size() != 2) {
      throwDebug("JSON structure contains invalid vertex");
    }
    vertices.emplace_back(vertex.at(0).get<float>(), vertex.at(1).get<float>());
  }
  return vertices;
}

json boundariesToJson(const Annotation& annot)
{
  json boundaries = json::array();

  // The annotation model currently edits the first boundary. The array shape is intentional so holes or multiple
  // contours can be added later without renaming the key again.
  if (annot.numBoundaries() > 0) {
    boundaries.emplace_back(boundaryToJson(annot.getBoundaryVertices(OUTER_BOUNDARY)));
  }

  return boundaries;
}

AnnotPolygon<float, 2> polygonFromBoundariesJson(const json& boundariesJson)
{
  AnnotPolygon<float, 2> polygon;
  if (!boundariesJson.is_array() || boundariesJson.empty()) {
    return polygon;
  }

  polygon.setOuterBoundary(boundaryFromJson(boundariesJson.at(OUTER_BOUNDARY)));
  if (boundariesJson.size() > 1) {
    spdlog::warn(
      "Annotation JSON contains {} boundaries; only the outer boundary is currently loaded",
      boundariesJson.size());
  }

  return polygon;
}

void appendAnnotationsFromArray(const json& annotationArrayJson, std::vector<Annotation>& annotations)
{
  if (!annotationArrayJson.is_array()) {
    throwDebug("Annotation JSON must contain an annotation array");
  }

  annotations.reserve(annotations.size() + annotationArrayJson.size());
  for (const auto& annotJson : annotationArrayJson) {
    annotations.emplace_back(annotJson);
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

  const glm::vec4& lineCol = annot.getLineColor();
  const glm::vec4& fillCol = annot.getFillColor();

  const glm::vec4& planeEq = annot.getSubjectPlaneEquation();

  j = json{
    {"type", POLYGON_ANNOTATION_TYPE},
    {"name", annot.getDisplayName()},
    {"visible", annot.isVisible()},
    {"opacity", annot.getOpacity()},
    {"lineThickness", annot.getLineThickness()},
    {"lineColor", colorToJson(lineCol)},
    {"fillColor", colorToJson(fillCol)},
    {"verticesVisible", annot.getVertexVisibility()},
    {"closed", annot.isClosed()},
    {"filled", annot.isFilled()},
    {"smoothed", annot.isSmoothed()},
    {"smoothingFactor", annot.getSmoothingFactor()},
    {"subjectPlaneNormal", {planeEq.x, planeEq.y, planeEq.z}},
    {"subjectPlaneOffset", planeEq[3]},
    {"boundaries", boundariesToJson(annot)}};
}

void from_json(const json& j, Annotation& annot)
{
  // All of these parameters are optional in the JSON:

  const std::string annotationType = j.at("type").get<std::string>();
  if (annotationType != POLYGON_ANNOTATION_TYPE) {
    throwDebug("Unsupported annotation type in JSON");
  }

  std::string displayName;
  if (j.count("name")) {
    displayName = j.at("name").get<std::string>();
  }

  bool visible = true;
  if (j.count("visible")) {
    visible = j.at("visible").get<bool>();
  }

  float opacity = 1.0f;
  if (j.count("opacity")) {
    opacity = j.at("opacity").get<float>();
  }

  float lineThickness = 2.0f;
  if (j.count("lineThickness")) {
    lineThickness = j.at("lineThickness").get<float>();
  }

  glm::vec4 lineColorVec4{1.0f, 0.0f, 0.0f, 1.0f};
  if (j.count("lineColor")) {
    lineColorVec4 = colorFromJson(j.at("lineColor"), lineColorVec4);
  }

  glm::vec4 fillColorVec4{1.0f, 0.0f, 0.0f, 0.5f};
  if (j.count("fillColor")) {
    fillColorVec4 = colorFromJson(j.at("fillColor"), fillColorVec4);
  }

  bool verticesVisible = true;
  if (j.count("verticesVisible")) {
    verticesVisible = j.at("verticesVisible").get<bool>();
  }

  bool closed = true;
  if (j.count("closed")) {
    closed = j.at("closed").get<bool>();
  }

  bool filled = true;
  if (j.count("filled")) {
    filled = j.at("filled").get<bool>();
  }

  bool smoothed = false;
  if (j.count("smoothed")) {
    smoothed = j.at("smoothed").get<bool>();
  }

  float smoothingFactor = 0.0f;
  if (j.count("smoothingFactor")) {
    smoothingFactor = j.at("smoothingFactor").get<float>();
  }

  // The Subject plane normal and offset distance are required in the JSON:
  std::array<float, 3> planeNormal;
  j.at("subjectPlaneNormal").get_to(planeNormal);

  float planeOffset = std::numeric_limits<float>::quiet_NaN();
  j.at("subjectPlaneOffset").get_to(planeOffset);

  const glm::vec4 subjectPlaneEquation{planeNormal[0], planeNormal[1], planeNormal[2], planeOffset};

  // The polygon vertices are required in the JSON:
  AnnotPolygon<float, 2> polygon;
  polygon = polygonFromBoundariesJson(j.at("boundaries"));

  if (polygon.getAllVertices().empty()) {
    spdlog::warn("Polygon read from JSON has no vertices");
  }

  spdlog::debug("Read polygon JSON with {} vertices", polygon.getBoundaryVertices(OUTER_BOUNDARY).size());

  Annotation newAnnot;
  newAnnot.setDisplayName(displayName);
  newAnnot.setSubjectPlane(subjectPlaneEquation);
  newAnnot.setVisible(visible);
  newAnnot.setOpacity(opacity);
  newAnnot.setLineThickness(lineThickness);
  newAnnot.setLineColor(lineColorVec4);
  newAnnot.setVertexColor(lineColorVec4); // vertex color same as line color
  newAnnot.setFillColor(fillColorVec4);
  newAnnot.setVertexVisibility(verticesVisible);
  newAnnot.setClosed(closed);
  newAnnot.setFilled(filled);
  newAnnot.setSmoothed(smoothed);
  newAnnot.setSmoothingFactor(smoothingFactor);
  newAnnot.polygon().setOuterBoundary(polygon.getBoundaryVertices(OUTER_BOUNDARY));
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
  for (const Annotation& annotation : annotations) {
    annotationArray.emplace_back(annotation);
  }

  return json{{"version", ANNOTATION_JSON_VERSION}, {"annotations", std::move(annotationArray)}};
}

std::vector<Annotation> annotationsFromJson(const json& j)
{
  std::vector<Annotation> annotations;
  if (!j.is_object()) {
    throwDebug("Annotation JSON must be an object");
  }

  const int version = j.at("version").get<int>();
  if (version != ANNOTATION_JSON_VERSION) {
    throwDebug("Unsupported annotation JSON version");
  }

  appendAnnotationsFromArray(j.at("annotations"), annotations);
  return annotations;
}
