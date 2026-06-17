#pragma once

#include "layout/LayoutPreset.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace layout
{

/** @brief Contents of a standalone layout preset file. */
struct LayoutFile
{
  std::optional<std::size_t> m_currentLayoutIndex = std::nullopt; //!< Selected layout index, when saved.
  std::vector<LayoutPreset> m_layouts;                            //!< Layout presets in display order.
};

/**
 * @brief Load a standalone layout file.
 * @param file Output layout file.
 * @param fileName File to read.
 * @return true on success; false on parse or I/O error.
 */
bool open(LayoutFile& file, const std::filesystem::path& fileName);
/**
 * @brief Save a standalone layout file.
 * @param file Layout file to save.
 * @param fileName Destination file.
 * @return true on success; false on I/O error.
 */
bool save(const LayoutFile& file, const std::filesystem::path& fileName);

} // namespace layout
