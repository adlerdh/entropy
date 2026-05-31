#pragma once

#include "Filesystem.h"

#include <optional>
#include <string>
#include <vector>

namespace native_dialog
{
struct Filter
{
  std::string name;
  std::string extensions;
};

std::optional<fs::path> openFile(const std::vector<Filter>& filters = {}, const fs::path& defaultPath = {});
std::optional<fs::path> saveFile(
  const std::vector<Filter>& filters = {},
  const fs::path& defaultPath = {},
  const std::string& defaultName = {});

std::vector<Filter> imageFilters();
std::vector<Filter> projectFilters();
std::vector<Filter> transformFilters();
std::vector<Filter> segmentationFilters();
std::vector<Filter> landmarkFilters();
std::vector<Filter> annotationFilters();
} // namespace native_dialog
