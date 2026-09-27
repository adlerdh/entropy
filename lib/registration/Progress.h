#pragma once

#include "registration/Types.h"

#include <optional>
#include <string>
#include <string_view>

namespace registration
{

/**
 * @brief Parse one JSON-lines progress message from a backend.
 * @param line One stdout line.
 * @param error Optional parse error output.
 * @return Parsed event, or std::nullopt when the line is empty or invalid.
 */
std::optional<ProgressEvent> parseProgressEventLine(std::string_view line, std::string* error = nullptr);

/**
 * @brief Serialize a progress event as one JSON-lines message without a trailing newline.
 * @param event Event to serialize.
 * @return Compact JSON object text.
 */
std::string progressEventLine(const ProgressEvent& event);

} // namespace registration
