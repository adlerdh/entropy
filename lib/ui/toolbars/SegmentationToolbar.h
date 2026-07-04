#pragma once

#include "common/PublicTypes.h"
#include "common/SegmentationTypes.h"
#include "common/Types.h"

#include <uuid.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>

class AppData;

/**
 * @brief Render the segmentation toolbar.
 */
void renderSegToolbar(
  AppData& appData,
  std::size_t numImages,
  const std::function<std::pair<std::string, std::string>(std::size_t index)>& getImageDisplayAndFileName,
  const std::function<std::size_t(void)>& getActiveImageIndex,
  const std::function<void(std::size_t imageIndex)>& setActiveImageIndex,
  const std::function<bool(std::size_t imageIndex)>& getImageHasActiveSeg,
  const std::function<void(std::size_t imageIndex, bool set)>& setImageHasActiveSeg,
  const std::function<
    std::optional<uuids::uuid>(const uuids::uuid& matchingImageUid, const std::string& segDisplayName)>& createBlankSeg,
  const std::function<void(const uuids::uuid& imageUid)>& updateImageUniforms,
  const std::function<void(MouseMode)>& setMouseMode,
  const std::function<void(void)>& readjustViewport,
  const std::function<bool(const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType&)>&
    executePoissonSeg);
