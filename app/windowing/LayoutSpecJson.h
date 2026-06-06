#pragma once

#include "windowing/LayoutSpec.h"

#include <nlohmann/json_fwd.hpp>

namespace layout
{

void to_json(nlohmann::json& j, const ImageSelectionSpec& selection);
void from_json(const nlohmann::json& j, ImageSelectionSpec& selection);

void to_json(nlohmann::json& j, const ViewSpec& view);
void from_json(const nlohmann::json& j, ViewSpec& view);

void to_json(nlohmann::json& j, const LayoutSpec& layout);
void from_json(const nlohmann::json& j, LayoutSpec& layout);

} // namespace layout
