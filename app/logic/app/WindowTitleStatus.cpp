#include "logic/app/WindowTitleStatus.h"

namespace window_title
{
std::string projectDisplayName(const std::filesystem::path& projectFileName)
{
  return projectFileName.filename().string();
}

std::string
status(const std::optional<std::filesystem::path>& projectFileName, const std::string& imageDisplayNames, bool dirty)
{
  if (!projectFileName) {
    return imageDisplayNames;
  }

  std::string projectName = projectDisplayName(*projectFileName);
  if (dirty) {
    projectName += "*";
  }

  return projectName;
}
} // namespace window_title
