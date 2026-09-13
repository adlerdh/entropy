#pragma once

#include <optional>
#include <string>

/**
 * @brief Clipboard representations of the same logical content.
 *
 * Clipboard targets choose the richest format they understand. Plain text should always be populated as the universal
 * fallback. Rich formats are optional and may be ignored by platform backends that cannot publish them.
 */
struct ClipboardPayload
{
  std::string plainText;
  std::optional<std::string> html;
  std::optional<std::string> rtf;
};
