#include "logic/app/LoadingStatusItems.h"

#include "image/DicomSeries.h"
#include "logic/serialization/ProjectSerialization.h"

#include <cstdint>
#include <optional>

namespace fs = std::filesystem;

namespace
{
std::optional<std::uintmax_t> fileSizeBytes(const fs::path& fileName)
{
  std::error_code error;
  const std::uintmax_t bytes = fs::file_size(fileName, error);
  if (error) {
    return std::nullopt;
  }
  return bytes;
}

std::optional<std::uintmax_t> totalFileSizeBytes(const std::vector<fs::path>& fileNames)
{
  std::uintmax_t total = 0;
  bool anyKnownSize = false;
  for (const auto& fileName : fileNames) {
    if (const auto bytes = fileSizeBytes(fileName)) {
      total += *bytes;
      anyKnownSize = true;
    }
  }
  return anyKnownSize ? std::optional<std::uintmax_t>{total} : std::nullopt;
}

GuiData::LoadingStatusItem imageItem(const fs::path& fileName, std::optional<std::uintmax_t> bytes = std::nullopt)
{
  return GuiData::LoadingStatusItem{
    GuiData::LoadingStatusItem::Kind::Image,
    fileName,
    bytes ? bytes : fileSizeBytes(fileName),
    false};
}

GuiData::LoadingStatusItem segmentationItem(const fs::path& fileName)
{
  return GuiData::LoadingStatusItem{
    GuiData::LoadingStatusItem::Kind::Segmentation,
    fileName,
    fileSizeBytes(fileName),
    false};
}

void appendSerializedImageItems(std::vector<GuiData::LoadingStatusItem>& items, const serialize::Image& image)
{
  items.push_back(imageItem(image.m_imageFileName));
  for (const auto& seg : image.m_segmentations) {
    items.push_back(segmentationItem(seg.m_segFileName));
  }
}
} // namespace

namespace loading_status
{
std::vector<GuiData::LoadingStatusItem> imageItems(const std::vector<fs::path>& fileNames)
{
  std::vector<GuiData::LoadingStatusItem> items;
  items.reserve(fileNames.size());
  for (const auto& fileName : fileNames) {
    items.push_back(imageItem(fileName));
  }
  return items;
}

std::vector<GuiData::LoadingStatusItem> projectItems(const serialize::EntropyProject& project)
{
  std::vector<GuiData::LoadingStatusItem> items;
  items.reserve(1 + project.m_additionalImages.size());
  appendSerializedImageItems(items, project.m_referenceImage);
  for (const auto& image : project.m_additionalImages) {
    appendSerializedImageItems(items, image);
  }
  return items;
}

std::vector<GuiData::LoadingStatusItem> dicomSeriesItems(const std::vector<dicom::SeriesInfo>& series)
{
  std::vector<GuiData::LoadingStatusItem> items;
  items.reserve(series.size());
  for (const auto& seriesInfo : series) {
    items.push_back(imageItem(
      seriesInfo.files.empty() ? fs::path{seriesInfo.seriesInstanceUid} : seriesInfo.files.front(),
      totalFileSizeBytes(seriesInfo.files)));
  }
  return items;
}

bool equivalentPath(const fs::path& a, const fs::path& b)
{
  if (a == b) {
    return true;
  }

  std::error_code aError;
  std::error_code bError;
  const fs::path canonicalA = fs::weakly_canonical(a, aError);
  const fs::path canonicalB = fs::weakly_canonical(b, bError);
  if (!aError && !bError && canonicalA == canonicalB) {
    return true;
  }

  return !a.filename().empty() && a.filename() == b.filename();
}
} // namespace loading_status
