#pragma once

#include "windowing/LayoutPreset.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace layout
{

struct LayoutFile
{
  std::optional<std::size_t> m_currentLayoutIndex = std::nullopt;
  std::vector<LayoutPreset> m_layouts;
};

bool open(LayoutFile& file, const std::filesystem::path& fileName);
bool save(const LayoutFile& file, const std::filesystem::path& fileName);

} // namespace layout
