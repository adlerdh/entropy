#include "ui/ImageExport.h"

#include "ui/NativeFileDialogs.h"

#include "image/Image.h"
#include "logic/app/Data.h"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>

namespace fs = std::filesystem;

namespace
{
std::string sanitizedFileStem(std::string fileName)
{
  std::replace_if(
    fileName.begin(),
    fileName.end(),
    [](unsigned char c) {
      return c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' ||
             c == '|' || std::iscntrl(c) != 0;
    },
    '_');

  while (!fileName.empty() && (fileName.back() == '.' || std::isspace(static_cast<unsigned char>(fileName.back())))) {
    fileName.pop_back();
  }

  return fileName.empty() ? std::string{"image"} : fileName;
}

fs::path defaultExportDirectory(const serialize::DicomSource& source, const Image& image)
{
  if (!source.m_rootPath.empty()) {
    return source.m_rootPath;
  }
  if (!source.m_files.empty()) {
    return source.m_files.front().parent_path();
  }
  return image.header().fileName().parent_path();
}
} // namespace

namespace image_export
{
const serialize::DicomSource* dicomSourceForImage(const AppData& appData, const uuids::uuid& imageUid)
{
  const auto imageIndex = appData.imageIndex(imageUid);
  if (!imageIndex) {
    return nullptr;
  }

  const auto& project = appData.project();
  if (*imageIndex == 0) {
    return project.m_referenceImage.m_dicomSource ? &*project.m_referenceImage.m_dicomSource : nullptr;
  }

  const std::size_t additionalIndex = *imageIndex - 1;
  if (additionalIndex >= project.m_additionalImages.size()) {
    return nullptr;
  }

  const auto& image = project.m_additionalImages.at(additionalIndex);
  return image.m_dicomSource ? &*image.m_dicomSource : nullptr;
}

bool imageHasDicomSource(const AppData& appData, const uuids::uuid& imageUid)
{
  return dicomSourceForImage(appData, imageUid) != nullptr;
}

bool exportDicomImage(AppData& appData, const uuids::uuid& imageUid)
{
  const serialize::DicomSource* dicomSource = dicomSourceForImage(appData, imageUid);
  if (!dicomSource) {
    spdlog::warn("Cannot export image {}; it is not backed by a DICOM series", imageUid);
    return false;
  }

  Image* image = appData.image(imageUid);
  if (!image || !image->hasPixelData()) {
    spdlog::warn("Cannot export DICOM series image {}; pixel data is not loaded", imageUid);
    return false;
  }

  const std::string defaultName = sanitizedFileStem(image->settings().displayName()) + ".nii.gz";
  const auto selectedFile = native_dialog::saveFile(
    native_dialog::medicalImageExportFilters(),
    defaultExportDirectory(*dicomSource, *image),
    defaultName);
  if (!selectedFile) {
    return false;
  }

  if (!image->saveComponentToDisk(0, std::optional<fs::path>{*selectedFile})) {
    spdlog::error("Failed to export DICOM series image {} to {}", imageUid, *selectedFile);
    return false;
  }

  spdlog::info("Exported DICOM series image {} to {}", imageUid, *selectedFile);
  return true;
}
} // namespace image_export
