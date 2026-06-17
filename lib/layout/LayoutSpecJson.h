#pragma once

#include "layout/LayoutSpec.h"

#include <nlohmann/json_fwd.hpp>

namespace layout
{

/**
 * @brief Serialize an image selection specification to JSON.
 * @param j Output JSON value.
 * @param selection Image selection to serialize.
 */
void to_json(nlohmann::json& j, const ImageSelectionSpec& selection);
/**
 * @brief Deserialize an image selection specification from JSON.
 * @param j Input JSON value.
 * @param selection Output image selection.
 * @throws nlohmann::json::exception If `j` contains incompatible field types.
 */
void from_json(const nlohmann::json& j, ImageSelectionSpec& selection);

/**
 * @brief Serialize one view specification to JSON.
 * @param j Output JSON value.
 * @param view View specification to serialize.
 */
void to_json(nlohmann::json& j, const ViewSpec& view);
/**
 * @brief Deserialize one view specification from JSON.
 * @param j Input JSON value.
 * @param view Output view specification.
 * @throws nlohmann::json::exception If `j` contains incompatible field types.
 */
void from_json(const nlohmann::json& j, ViewSpec& view);

/**
 * @brief Serialize a complete layout specification to JSON.
 * @param j Output JSON value.
 * @param layout Layout specification to serialize.
 */
void to_json(nlohmann::json& j, const LayoutSpec& layout);
/**
 * @brief Deserialize a complete layout specification from JSON.
 * @param j Input JSON value.
 * @param layout Output layout specification.
 * @throws nlohmann::json::exception If `j` contains incompatible field types.
 */
void from_json(const nlohmann::json& j, LayoutSpec& layout);

} // namespace layout
