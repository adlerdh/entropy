#pragma once

#include "common/PublicTypes.h"

#include <uuid.h>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

class AppData;
class ParcellationLabelTable;

/**
 * @brief Render the segmentation properties window.
 * @param appData Application data containing segmentations, image order, and window visibility.
 * @param getLabelTable Callback returning a parcellation label table by index.
 * @param updateImageUniforms Callback that refreshes uniforms for one image after segmentation changes.
 * @param updateLabelColorTableTexture Callback that refreshes a label color table texture.
 * @param moveCrosshairsToSegLabelCentroid Callback that moves the crosshairs to a label centroid.
 * @param createBlankSeg Callback that creates a blank segmentation for an image.
 * @param addSegmentationFile Callback that loads a segmentation file for an image.
 * @param clearSeg Callback that clears a segmentation.
 * @param removeSeg Callback that removes a segmentation.
 * @param recenterAllViews Callback used by segmentation controls that reposition views.
 */
void renderSegmentationPropertiesWindow(
  AppData& appData,
  const std::function<ParcellationLabelTable*(std::size_t tableIndex)>& getLabelTable,
  const std::function<void(const uuids::uuid& imageUid)>& updateImageUniforms,
  const std::function<void(std::size_t labelColorTableIndex)>& updateLabelColorTableTexture,
  const std::function<void(const uuids::uuid& imageUid, std::size_t labelIndex)>& moveCrosshairsToSegLabelCentroid,
  const std::function<
    std::optional<uuids::uuid>(const uuids::uuid& matchingImageUid, const std::string& segDisplayName)>& createBlankSeg,
  const std::function<void(const uuids::uuid& imageUid, const std::filesystem::path& fileName)>& addSegmentationFile,
  const std::function<bool(const uuids::uuid& segUid)>& clearSeg,
  const std::function<bool(const uuids::uuid& segUid)>& removeSeg,
  const AllViewsRecenterType& recenterAllViews);
