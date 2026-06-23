#include "logic/app/WindowTitleStatus.h"

#include <algorithm>

namespace
{
bool hasJsonExtension(const std::filesystem::path& fileName)
{
  std::string extension = fileName.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return extension == ".json";
}
} // namespace

namespace window_title
{
std::string projectDisplayName(const std::filesystem::path& projectFileName)
{
  const std::filesystem::path fileName = projectFileName.filename();
  if (hasJsonExtension(fileName)) {
    return fileName.stem().string();
  }
  return fileName.string();
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

  if (imageDisplayNames.empty()) {
    return projectName;
  }

  return projectName + ": " + imageDisplayNames;
}
} // namespace window_title
