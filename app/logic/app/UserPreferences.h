#pragma once

#include "logic/app/Settings.h"

#include <filesystem>
#include <string>

struct RenderData;

namespace user_preferences
{

/**
 * @brief Return the current user preferences as versioned JSON text.
 * @param settings Application settings to serialize.
 * @param renderData Rendering defaults to serialize.
 * @return Human-readable JSON representation of the user preferences.
 */
std::string toJsonString(const AppSettings& settings, const RenderData& renderData);

/**
 * @brief Apply user preferences from versioned JSON text.
 * @param settings Application settings to update.
 * @param renderData Rendering defaults to update.
 * @param text JSON text to parse.
 * @param error Optional destination for a parse or validation error.
 * @return True iff the text was parsed and applied successfully.
 */
bool applyJsonString(
  AppSettings& settings,
  RenderData& renderData,
  const std::string& text,
  std::string* error = nullptr);

/**
 * @brief Save user preferences to disk.
 * @param settings Application settings to serialize.
 * @param renderData Rendering defaults to serialize.
 * @param fileName Destination JSON file.
 * @param error Optional destination for an I/O or serialization error.
 * @return True iff the preferences were written successfully.
 */
bool save(
  const AppSettings& settings,
  const RenderData& renderData,
  const std::filesystem::path& fileName,
  std::string* error = nullptr);

/**
 * @brief Load user preferences from disk.
 * @param settings Application settings to update.
 * @param renderData Rendering defaults to update.
 * @param fileName Source JSON file.
 * @param error Optional destination for an I/O or parse error.
 * @return True iff the file was absent or was loaded successfully.
 */
bool load(
  AppSettings& settings,
  RenderData& renderData,
  const std::filesystem::path& fileName,
  std::string* error = nullptr);

/**
 * @brief Restore the built-in application preference defaults.
 * @param settings Application settings to reset.
 * @param renderData Rendering defaults to reset.
 */
void applyDefaults(AppSettings& settings, RenderData& renderData);

} // namespace user_preferences
