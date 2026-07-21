#pragma once

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

/**
 * @brief Create the versioned JSON object used for embedded and exported annotations
 * @param annotations Annotations to serialize
 * @return JSON object with schema version and annotation array
 */
nlohmann::json annotationsToJson(const std::vector<Annotation>& annotations);

/**
 * @brief Read annotations from a versioned JSON object or a legacy bare annotation array
 * @param j JSON object or legacy array
 * @return Parsed annotations
 */
std::vector<Annotation> annotationsFromJson(const nlohmann::json& j);
