#include "logic/serialization/ProjectSerialization.h"

#include <nlohmann/json.hpp>

#include <glm/vec3.hpp>

#include <optional>

using json = nlohmann::json;

namespace
{
json surfaceVec3ToJson(const glm::vec3& value)
{
  return json::array({value.x, value.y, value.z});
}

std::optional<glm::vec3> surfaceVec3FromJson(const json& value)
{
  if (!value.is_array() || value.size() != 3) {
    return std::nullopt;
  }
  return glm::vec3{value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
}

template<typename T>
void addIfChanged(json& j, const char* key, const T& value, const T& defaultValue)
{
  if (value != defaultValue) {
    j[key] = value;
  }
}

void addIfChanged(json& j, const char* key, const json& value, const json& defaultValue)
{
  if (value != defaultValue) {
    j[key] = value;
  }
}

void addIfNotEmpty(json& j, const char* key, json value)
{
  if (!value.empty()) {
    j[key] = std::move(value);
  }
}
} // namespace

void to_json(json& j, const SurfaceMaterial& material)
{
  const SurfaceMaterial defaults;
  j = json::object();
  addIfChanged(j, "ambient", material.ambient, defaults.ambient);
  addIfChanged(j, "diffuse", material.diffuse, defaults.diffuse);
  addIfChanged(j, "specular", material.specular, defaults.specular);
  addIfChanged(j, "shininess", material.shininess, defaults.shininess);
}

void from_json(const json& j, SurfaceMaterial& material)
{
  if (const auto value = j.find("ambient"); value != j.end() && value->is_number()) {
    material.ambient = value->get<float>();
  }
  if (const auto value = j.find("diffuse"); value != j.end() && value->is_number()) {
    material.diffuse = value->get<float>();
  }
  if (const auto value = j.find("specular"); value != j.end() && value->is_number()) {
    material.specular = value->get<float>();
  }
  if (const auto value = j.find("shininess"); value != j.end() && value->is_number()) {
    material.shininess = value->get<float>();
  }
}

void to_json(json& j, const Isosurface& surface)
{
  const Isosurface defaults;
  j = json::object();
  addIfChanged(j, "name", surface.name, defaults.name);
  addIfChanged(j, "value", surface.value, defaults.value);
  addIfChanged(j, "color", surfaceVec3ToJson(surface.color), surfaceVec3ToJson(defaults.color));

  json material = surface.material;
  addIfNotEmpty(j, "material", std::move(material));

  addIfChanged(j, "opacity", surface.opacity, defaults.opacity);
  addIfChanged(j, "contourFillOpacity", surface.fillOpacity, defaults.fillOpacity);
  addIfChanged(j, "visible", surface.visible, defaults.visible);
  addIfChanged(j, "showContours2D", surface.showIn2d, defaults.showIn2d);

  json rimLighting = json::object();
  addIfChanged(rimLighting, "enabled", surface.rimLightingEnabled, defaults.rimLightingEnabled);
  addIfChanged(rimLighting, "opacity", surface.rimOpacityStrength, defaults.rimOpacityStrength);
  addIfChanged(rimLighting, "glow", surface.rimEmissionStrength, defaults.rimEmissionStrength);
  addIfChanged(rimLighting, "falloff", surface.rimPower, defaults.rimPower);
  addIfNotEmpty(j, "rimLighting", std::move(rimLighting));
}

void from_json(const json& j, Isosurface& surface)
{
  if (const auto value = j.find("name"); value != j.end() && value->is_string()) {
    surface.name = value->get<std::string>();
  }
  if (const auto value = j.find("value"); value != j.end() && value->is_number()) {
    surface.value = value->get<double>();
  }
  if (const auto color = j.find("color"); color != j.end()) {
    if (const auto parsed = surfaceVec3FromJson(*color)) {
      surface.color = *parsed;
    }
  }
  if (const auto value = j.find("material"); value != j.end() && value->is_object()) {
    surface.material = value->get<SurfaceMaterial>();
  }
  if (const auto value = j.find("opacity"); value != j.end() && value->is_number()) {
    surface.opacity = value->get<float>();
  }
  if (const auto value = j.find("contourFillOpacity"); value != j.end() && value->is_number()) {
    surface.fillOpacity = value->get<float>();
  }
  if (const auto value = j.find("visible"); value != j.end() && value->is_boolean()) {
    surface.visible = value->get<bool>();
  }
  if (const auto value = j.find("showContours2D"); value != j.end() && value->is_boolean()) {
    surface.showIn2d = value->get<bool>();
  }
  if (const auto rim = j.find("rimLighting"); rim != j.end() && rim->is_object()) {
    if (const auto value = rim->find("enabled"); value != rim->end() && value->is_boolean()) {
      surface.rimLightingEnabled = value->get<bool>();
    }
    if (const auto value = rim->find("opacity"); value != rim->end() && value->is_number()) {
      surface.rimOpacityStrength = value->get<float>();
    }
    if (const auto value = rim->find("glow"); value != rim->end() && value->is_number()) {
      surface.rimEmissionStrength = value->get<float>();
    }
    if (const auto value = rim->find("falloff"); value != rim->end() && value->is_number()) {
      surface.rimPower = value->get<float>();
    }
  }
}
