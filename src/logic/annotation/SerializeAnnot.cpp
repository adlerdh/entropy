#include "logic/annotation/SerializeAnnot.h"
#include "common/Exception.hpp"

#include <vector>

namespace
{
static constexpr size_t OUTER_BOUNDARY = 0;
}

using json = nlohmann::json;

void to_json(json& j, const AnnotPolygon<float, 2>& poly)
{
  j.clear();

  for (const auto& vertices : poly.getBoundaryVertices(OUTER_BOUNDARY))
  {
    j.emplace_back(std::vector<float>{vertices.x, vertices.y});
  }
}

void from_json(const json& j, AnnotPolygon<float, 2>& poly)
{
  AnnotPolygon<float, 2> newPoly;

  for (const auto& vertex : j)
  {
    if (2 != vertex.size())
    {
      throw_debug("JSON structure contains invalid vertex")
    }

    newPoly.addVertexToOuterBoundary(AnnotPolygon<float, 2>::PointType{vertex[0], vertex[1]});
  }

  poly = newPoly;
}

void to_json(json& j, const Annotation& annot)
{
  j.clear();

  const glm::vec4& lineCol = annot.getLineColor();
  const glm::vec4& fillCol = annot.getFillColor();

  const glm::vec4& planeEq = annot.getSubjectPlaneEquation();
  const glm::vec3& planeOr = annot.getSubjectPlaneOrigin();
  const std::pair<glm::vec3, glm::vec3>& planeAxes = annot.getSubjectPlaneAxes();

  j = json{
    {"name", annot.getDisplayName()},
    {"visible", annot.isVisible()},
    {"opacity", annot.getOpacity()},
    {"lineThickness", annot.getLineThickness()},
    {"lineColor", {lineCol.r, lineCol.g, lineCol.b, lineCol.a}},
    {"fillColor", {fillCol.r, fillCol.g, fillCol.b, fillCol.a}},
    {"verticesVisible", annot.getVertexVisibility()},
    {"closed", annot.isClosed()},
    {"filled", annot.isFilled()},
    {"smoothed", annot.isSmoothed()},
    {"smoothingFactor", annot.getSmoothingFactor()},
    {"subjectPlaneNormal", {planeEq.x, planeEq.y, planeEq.z}},
    {"subjectPlaneOffset", planeEq[3]},
    {"subjectPlaneOrigin", {planeOr.x, planeOr.y, planeOr.z}},
    {"subjectPlaneAxes",
     {{planeAxes.first.x, planeAxes.first.y, planeAxes.first.z},
      {planeAxes.second.x, planeAxes.second.y, planeAxes.second.z}}},
    {"polygon", annot.polygon()}
  };
}

void from_json(const json& j, Annotation& annot)
{
  // All of these parameters are optional in the JSON:

  std::string displayName = "";
  if (j.count("name"))
  {
    displayName = j.at("name").get<std::string>();
  }

  bool visible = true;
  if (j.count("visible"))
  {
    visible = j.at("visible").get<bool>();
  }

  float opacity = 1.0f;
  if (j.count("opacity"))
  {
    opacity = j.at("opacity").get<float>();
  }

  float lineThickness = 2.0f;
  if (j.count("lineThickness"))
  {
    lineThickness = j.at("lineThickness").get<float>();
  }

  std::array<float, 4> lineColor{1.0f, 0.0f, 0.0f, 1.0f};
  if (j.count("lineColor"))
  {
    lineColor = j.at("lineColor").get<std::array<float, 4>>();
  }
  const glm::vec4 lineColorVec4{lineColor[0], lineColor[1], lineColor[2], lineColor[3]};

  std::array<float, 4> fillColor{1.0f, 0.0f, 0.0f, 0.5f};
  if (j.count("fillColor"))
  {
    fillColor = j.at("fillColor").get<std::array<float, 4>>();
  }
  const glm::vec4 fillColorVec4{fillColor[0], fillColor[1], fillColor[2], fillColor[3]};

  bool verticesVisible = true;
  if (j.count("verticesVisible"))
  {
    verticesVisible = j.at("verticesVisible").get<bool>();
  }

  bool closed = true;
  if (j.count("closed"))
  {
    closed = j.at("closed").get<bool>();
  }

  bool filled = true;
  if (j.count("filled"))
  {
    filled = j.at("filled").get<bool>();
  }

  bool smoothed = false;
  if (j.count("smoothed"))
  {
    smoothed = j.at("smoothed").get<bool>();
  }

  float smoothingFactor = 0.0f;
  if (j.count("smoothingFactor"))
  {
    smoothingFactor = j.at("smoothingFactor").get<float>();
  }

  // The Subject plane normal and offset distance are required in the JSON:
  std::array<float, 3> planeNormal;
  j.at("subjectPlaneNormal").get_to(planeNormal);

  float planeOffset;
  j.at("subjectPlaneOffset").get_to(planeOffset);

  const glm::vec4 subjectPlaneEquation{planeNormal[0], planeNormal[1], planeNormal[2], planeOffset};

  // The polygon vertices are required in the JSON:
  AnnotPolygon<float, 2> polygon;
  j.at("polygon").get_to(polygon);

  if (polygon.getAllVertices().empty())
  {
    spdlog::warn("Polygon read from JSON has no vertices");
  }

  spdlog::debug(
    "Read polygon JSON with {} vertices", polygon.getBoundaryVertices(OUTER_BOUNDARY).size()
  );

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

  annot = newAnnot;
}

void from_json(const json& j, std::vector<Annotation>& annots)
{
  for (const auto& annotJson : j)
  {
    annots.emplace_back(annotJson);
  }
}
