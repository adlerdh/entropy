#pragma once

#include "common/Exception.hpp"
#include "logic/annotation/Annotation.h"

#include <nlohmann/json.hpp>

#include <vector>

namespace annotation_json
{
/**
 * @brief Serialize a non-premultiplied RGBA color as normalized `[r,g,b,a]` floats.
 * @param color Color with components in `[0, 1]`
 * @return JSON array containing red, green, blue, and alpha
 */
inline nlohmann::json colorToJson(const glm::vec4& color)
{
  return nlohmann::json::array({color.r, color.g, color.b, color.a});
}

/**
 * @brief Parse a normalized `[r,g,b,a]` color array.
 * @param value JSON array containing red, green, blue, and alpha
 * @return Parsed non-premultiplied RGBA color
 */
inline glm::vec4 colorFromJson(const nlohmann::json& value)
{
  if (!value.is_array() || value.size() != 4) {
    throwDebug("JSON structure contains invalid color");
  }
  return glm::vec4{
    value.at(0).get<float>(),
    value.at(1).get<float>(),
    value.at(2).get<float>(),
    value.at(3).get<float>()};
}
} // namespace annotation_json

/**
 * @brief Create a JSON structure with a vector for the outer boundary vertices.
 * @param[out] j JSON structure
 * @param[in] poly 2D polygon
 */
void to_json(nlohmann::json& j, const AnnotPolygon<float, 2>& poly);

/**
 * @brief Read a JSON structure for a 2D polygon
 * @param[in] j JSON structure
 * @param[out] poly 2D polygon
 */
void from_json(const nlohmann::json& j, AnnotPolygon<float, 2>& poly);

/**
 * @brief Create a JSON structure for the annotation
 * @param[out] j JSON structure
 * @param[in] annot Annotation
 */
void to_json(nlohmann::json& j, const Annotation& annot);

/**
 * @brief Read a JSON structure for an annotation
 * @param[in] j JSON structure
 * @param[out] annot Annotation
 */
void from_json(const nlohmann::json& j, Annotation& annot);

/**
 * @brief Read a JSON structure for a vector of annotations
 * @param[in] j JSON structure
 * @param[out] Vector of annotations
 */
void from_json(const nlohmann::json& j, std::vector<Annotation>& annots);

/**
 * @brief Create the versioned JSON object used for embedded and exported annotations
 * @param annotations Annotations to serialize
 * @return JSON object with schema version and annotation array
 */
nlohmann::json annotationsToJson(const std::vector<Annotation>& annotations);

/**
 * @brief Read annotations from a versioned JSON object
 * @param j JSON object
 * @return Parsed annotations
 */
std::vector<Annotation> annotationsFromJson(const nlohmann::json& j);
