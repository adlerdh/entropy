#ifndef SERIALIZE_ANNOT_H
#define SERIALIZE_ANNOT_H

#include "logic/annotation/Annotation.h"

#include <nlohmann/json.hpp>

#include <vector>

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

#endif // SERIALIZE_ANNOT_H
