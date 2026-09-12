#include "ui/ImageExport.h"

#include "ui/ExportJobService.h"
#include "ui/NativeFileDialogs.h"
#include "ui/dialogs/NativeMessageDialogs.h"

#include "common/UuidUtility.h"
#include "image/Image.h"
#include "image/ImageWriter.h"
#include "logic/app/Data.h"
#include "logic/serialization/ProjectSerialization.h"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace
{
bool exportCanStart(AppData& appData)
{
  const auto service = appData.guiData().m_exportJobs;
  if (!service) {
    native_dialog::showErrorMessageDialog("Export Failed", "The background export service is unavailable.");
    return false;
  }
  const auto status = service->snapshot();
  if (status.hasJob && ui::export_jobs::Outcome::Running == status.outcome) {
    native_dialog::showErrorMessageDialog(
      "Export Already in Progress",
      "Wait for the current export to finish or cancel it before starting another export.");
    return false;
  }
  return true;
}

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

fs::path defaultExportDirectory(const serialize::DicomSource* source, const Image& image)
{
  if (source && !source->m_rootPath.empty()) {
    return source->m_rootPath;
  }
  if (source && !source->m_files.empty()) {
    return source->m_files.front().parent_path();
  }
  return image.header().fileName().parent_path();
}

std::string medicalImageExportFormats()
{
  return "Select the output format by giving the exported file one of these extensions:\n\n"
         "NIfTI: .nii, .nii.gz\n"
         "NRRD: .nrrd, .nhdr\n"
         "MetaImage: .mha, .mhd\n"
         "Analyze 7.5: .img, .hdr";
}

bool suppressExportFormatGuide(const std::string& title, const std::string& message, const std::string& formats)
{
  const auto result = native_dialog::showMessageDialog(
    {.title = title,
     .message = message,
     .informativeText = formats,
     .firstButton = "Continue",
     .secondButton = "Don't Show Again",
     .thirdButton = "",
     .severity = native_dialog::MessageDialogSeverity::Information});
  return result && *result == native_dialog::MessageDialogResult::SecondButton;
}

void showImageExportFormatGuide(AppSettings& settings, bool allowStandardRasterFormats)
{
  if (!settings.showImageExportFormatGuide()) return;

  std::string formats = medicalImageExportFormats();
  if (allowStandardRasterFormats) {
    formats +=
      "\n\nFor a single 2D image, Entropy also supports:\n"
      "JPEG: .jpg, .jpeg, .jpe\n"
      "PNG: .png\n"
      "TIFF: .tif, .tiff\n"
      "BMP: .bmp, .dib";
  }
  else {
    formats += "\n\nStandard image formats such as PNG and JPEG cannot store this image's volume or time-series data.";
  }

  if (suppressExportFormatGuide(
        "Image Export Formats",
        "Entropy writes the image format selected by the filename extension.",
        formats))
  {
    settings.setShowImageExportFormatGuide(false);
  }
}

void showSegmentationExportFormatGuide(AppSettings& settings)
{
  if (!settings.showSegmentationExportFormatGuide()) return;

  std::string formats = medicalImageExportFormats();
  formats += "\n\nThese medical image formats preserve segmentation voxel values and spatial geometry.";
  if (suppressExportFormatGuide(
        "Segmentation Export Formats",
        "Entropy writes the segmentation format selected by the filename extension.",
        formats))
  {
    settings.setShowSegmentationExportFormatGuide(false);
  }
}

bool exportImageData(
  AppData& appData,
  const Image& image,
  const uuids::uuid& imageUid,
  const std::string& objectName,
  const fs::path& defaultDirectory,
  const image_io::WriteOptions& options,
  bool allowStandardRasterFormats)
{
  const std::string defaultName = sanitizedFileStem(image.settings().displayName()) + ".nii.gz";
  const auto filters =
    allowStandardRasterFormats ? native_dialog::imageExportFilters() : native_dialog::medicalImageExportFilters();
  const auto selectedFile = native_dialog::saveFile(filters, defaultDirectory, defaultName);
  if (!selectedFile) {
    return false;
  }

  const auto service = appData.guiData().m_exportJobs;
  if (!service) {
    native_dialog::showErrorMessageDialog("Export Failed", "The background export service is unavailable.");
    return false;
  }

  const auto imageSnapshot = std::make_shared<Image>(image);
  const std::string imageName = image.settings().displayName();
  const std::string uid = uuids::to_string(imageUid);
  const fs::path& destination = *selectedFile;
  const bool submitted = service->submit(
    {.description = std::format("Exporting {} '{}'", objectName, imageName),
     .destination = destination,
     .task = [imageSnapshot, options, destination, objectName, uid](ui::export_jobs::JobContext& context) mutable {
       ui::export_jobs::StagedOutput staged{destination};
       image_io::WriteOptions writeOptions = options;
       writeOptions.progressCallback = [&context](const std::string_view phase, const std::optional<float> progress) {
         context.update(std::string{phase}, progress);
         return !context.cancellationRequested();
       };

       const image_io::WriteResult result = image_io::writeImage(*imageSnapshot, staged.temporaryPath(), writeOptions);
       if (image_io::WriteError::Cancelled == result.error || context.cancellationRequested()) {
         spdlog::info("Cancelled export of {} {} to '{}'", objectName, uid, destination);
         return ui::export_jobs::Result::cancelled();
       }
       if (!result) {
         spdlog::error("Failed to export {} {} to '{}': {}", objectName, uid, destination, result.message);
         return ui::export_jobs::Result::failure(result.message);
       }

       context.update("Committing image file", 0.98f);
       if (const auto error = staged.commit()) {
         spdlog::error("Failed to commit {} export {} to '{}': {}", objectName, uid, destination, *error);
         return ui::export_jobs::Result::failure("The completed export could not replace the destination: " + *error);
       }
       spdlog::info("Exported {} {} to '{}'", objectName, uid, destination);
       return ui::export_jobs::Result::success({destination});
     }});
  if (!submitted) {
    native_dialog::showErrorMessageDialog(
      "Export Already in Progress",
      "Wait for the current export to finish or cancel it before starting another export.");
  }
  return submitted;
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

bool exportImage(AppData& appData, const uuids::uuid& imageUid)
{
  if (!exportCanStart(appData)) {
    return false;
  }
  Image* image = appData.image(imageUid);
  if (!image || !image->hasPixelData()) {
    spdlog::warn("Cannot export image {}; pixel data is not loaded", imageUid);
    return false;
  }

  const bool allowStandardRasterFormats = image->header().pixelDimensions().z == 1u && !image->isTimeSeries();
  showImageExportFormatGuide(appData.settings(), allowStandardRasterFormats);

  return exportImageData(
    appData,
    *image,
    imageUid,
    "image",
    defaultExportDirectory(dicomSourceForImage(appData, imageUid), *image),
    {},
    allowStandardRasterFormats);
}

bool exportSegmentation(AppData& appData, const uuids::uuid& segmentationUid)
{
  if (!exportCanStart(appData)) {
    return false;
  }
  Image* segmentation = appData.seg(segmentationUid);
  if (!segmentation || !segmentation->hasPixelData()) {
    spdlog::warn("Cannot export segmentation {}; pixel data is not loaded", segmentationUid);
    return false;
  }

  showSegmentationExportFormatGuide(appData.settings());

  return exportImageData(
    appData,
    *segmentation,
    segmentationUid,
    "segmentation",
    segmentation->header().fileName().parent_path(),
    {.component = 0u},
    false);
}
} // namespace image_export
