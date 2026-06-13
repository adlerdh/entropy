#pragma once

#include "common/PublicTypes.h"

#include <uuid.h>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

class AppData;
struct GuiData;
class Image;
class ImageColorMap;
class ParcellationLabelTable;

class ParcellationLabelTable;

/**
 * @brief Render one collapsible segmentation header for an image.
 */
void renderSegmentationHeader(
  AppData& appData,
  const uuids::uuid& imageUid,
  std::size_t imageIndex,
  Image* image,
  bool isActiveImage,
  const std::function<void(void)>& updateImageUniforms,
  const std::function<ParcellationLabelTable*(std::size_t tableIndex)>& getLabelTable,
  const std::function<void(std::size_t tableIndex)>& updateLabelColorTableTexture,
  const std::function<void(std::size_t labelIndex)>& moveCrosshairsToSegLabelCentroid,
  const std::function<
    std::optional<uuids::uuid>(const uuids::uuid& matchingImageUid, const std::string& segDisplayName)>& createBlankSeg,
  const std::function<void(const uuids::uuid& imageUid, const std::filesystem::path& fileName)>& addSegmentationFile,
  const std::function<bool(const uuids::uuid& segUid)>& clearSeg,
  const std::function<bool(const uuids::uuid& segUid)>& removeSeg,
  const AllViewsRecenterType& recenterAllViews);
