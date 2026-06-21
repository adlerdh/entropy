#pragma once

#include "common/PublicTypes.h"
#include "image/ImageDerivedData.h"

#include <uuid.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>

class AppData;
struct GuiData;
class Image;
class ImageColorMap;
class ParcellationLabelTable;

/**
 * @brief Render image header metadata, transforms, display settings, and DICOM export controls.
 */
void renderImageHeaderInformation(
  AppData& appData,
  const uuids::uuid& imageUid,
  Image& image,
  const std::function<void(void)>& updateImageUniforms,
  const AllViewsRecenterType& recenterAllViews);

/**
 * @brief Render one collapsible image header in the image properties window.
 */
void renderImageHeader(
  AppData& appData,
  GuiData& guiData,
  const uuids::uuid& imageUid,
  std::size_t imageIndex,
  Image* image,
  bool isActiveImage,
  std::size_t numImages,
  const std::function<void(void)>& updateAllImageUniforms,
  const std::function<void(void)>& updateImageUniforms,
  const std::function<void(void)>& updateImageInterpolationMode,
  const std::function<void(std::size_t cmapIndex)>& updateImageColorMapInterpolationMode,
  const std::function<std::size_t(void)>& getNumImageColorMaps,
  const std::function<ImageColorMap*(std::size_t cmapIndex)>& getImageColorMap,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageBackward,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageForward,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageToBack,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageToFront,
  const std::function<bool(const uuids::uuid& imageUid, bool locked)>& setLockManualImageTransformation,
  const std::function<void(const uuids::uuid& imageUid, ComponentProjectionMode mode)>& requestComponentProjectionImage,
  const std::function<void(const uuids::uuid& imageUid)>& requestSetReferenceImage,
  const std::function<void(const uuids::uuid& imageUid)>& requestRemoveImage,
  const AllViewsRecenterType& recenterAllViews);
