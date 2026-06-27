#pragma once

#include "common/PublicTypes.h"
#include "image/ImageDerivedData.h"

#include <uuid.h>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <utility>

class AppData;
class ImageColorMap;

/**
 * @brief Render the image properties window.
 * @param appData Application data containing image metadata, settings, and window visibility.
 * @param numImages Number of loaded images.
 * @param getImageDisplayAndFileName Callback returning display and file names for an image index.
 * @param getActiveImageIndex Callback returning the active image index.
 * @param setActiveImageIndex Callback that changes the active image.
 * @param getNumImageColorMaps Callback returning the number of color maps.
 * @param getImageColorMap Callback returning a color map by index.
 * @param moveImageBackward Callback that moves an image one layer backward.
 * @param moveImageForward Callback that moves an image one layer forward.
 * @param moveImageToBack Callback that moves an image to the back layer.
 * @param moveImageToFront Callback that moves an image to the front layer.
 * @param updateAllImageUniforms Callback that refreshes all image uniforms.
 * @param updateImageUniforms Callback that refreshes uniforms for one image.
 * @param updateImageInterpolationMode Callback that updates interpolation state for one image.
 * @param updateImageColorMapInterpolationMode Callback that updates interpolation state for one color map.
 * @param loadDeformationField Callback that loads a deformation field and returns its UID.
 * @param setLockManualImageTransformation Callback that toggles manual transform locking for one image.
 * @param requestComponentProjectionImage Callback that starts creation of a scalar component projection.
 * @param requestSetReferenceImage Callback that requests a new reference image.
 * @param requestRemoveImage Callback that requests image removal.
 * @param recenterAllViews Callback used by image controls that reposition views.
 */
void renderImagePropertiesWindow(
  AppData& appData,
  std::size_t numImages,
  const std::function<std::pair<std::string, std::string>(std::size_t index)>& getImageDisplayAndFileName,
  const std::function<std::size_t(void)>& getActiveImageIndex,
  const std::function<void(std::size_t)>& setActiveImageIndex,
  const std::function<std::size_t(void)>& getNumImageColorMaps,
  const std::function<ImageColorMap*(std::size_t cmapIndex)>& getImageColorMap,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageBackward,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageForward,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageToBack,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageToFront,
  const std::function<void(void)>& updateAllImageUniforms,
  const std::function<void(const uuids::uuid& imageUid)>& updateImageUniforms,
  const std::function<void(const uuids::uuid& imageUid)>& updateImageInterpolationMode,
  const std::function<void(std::size_t cmapIndex)>& updateImageColorMapInterpolationMode,
  const std::function<std::optional<uuids::uuid>(const std::filesystem::path& fileName)>& loadDeformationField,
  const std::function<bool(const uuids::uuid& imageUid, bool locked)>& setLockManualImageTransformation,
  const std::function<void(const uuids::uuid& imageUid, ComponentProjectionMode mode)>& requestComponentProjectionImage,
  const std::function<void(const uuids::uuid& imageUid)>& requestSetReferenceImage,
  const std::function<void(const uuids::uuid& imageUid)>& requestRemoveImage,
  const AllViewsRecenterType& recenterAllViews);
