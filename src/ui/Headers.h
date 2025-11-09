#pragma once

#include "common/PublicTypes.h"

#include <functional>
#include <glm/fwd.hpp>
#include <uuid.h>

class AppData;
struct GuiData;

class Image;
class ImageColorMap;
class ImageHeader;
class ImageSettings;
class ImageTransformations;
class ParcellationLabelTable;

/**
 * @brief Render UI for image header information, including data for pixel and component types
 * and transformations.
 *
 * @param[in, out] appData App data
 * @param[in] image Image data
 * @param[in,out] imgTx Image transformation data
 */
void renderImageHeaderInformation(
  AppData& appData,
  const uuids::uuid& imageUid,
  const Image& image,
  const std::function<void(void)>& updateImageUniforms,
  const std::function<void(void)>& recenterAllViews);

void renderImageHeader(
  AppData& appData,
  GuiData& guiData,
  const uuids::uuid& imageUid,
  size_t imageIndex,
  Image* image,
  bool isActiveImage,
  size_t numImages,
  const std::function<void(void)>& updateAllImageUniforms,
  const std::function<void(void)>& updateImageUniforms,
  const std::function<void(void)>& updateImageInterpolationMode,
  const std::function<void(std::size_t cmapIndex)>& updateImageColorMapInterpolationMode,
  const std::function<size_t(void)>& getNumImageColorMaps,
  const std::function<ImageColorMap*(size_t cmapIndex)>& getImageColorMap,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageBackward,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageForward,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageToBack,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageToFront,
  const std::function<bool(const uuids::uuid& imageUid, bool locked)>& setLockManualImageTransformation,
  const AllViewsRecenterType& recenterAllViews);

void renderSegmentationHeader(
  AppData& appData,
  const uuids::uuid& imageUid,
  size_t imageIndex,
  Image* image,
  bool isActiveImage,
  const std::function<void(void)>& updateImageUniforms,
  const std::function<ParcellationLabelTable*(size_t tableIndex)>& getLabelTable,
  const std::function<void(size_t tableIndex)>& updateLabelColorTableTexture,
  const std::function<void(size_t labelIndex)>& moveCrosshairsToSegLabelCentroid,
  const std::function<std::optional<uuids::uuid>(
    const uuids::uuid& matchingImageUid, const std::string& segDisplayName)>& createBlankSeg,
  const std::function<bool(const uuids::uuid& segUid)>& clearSeg,
  const std::function<bool(const uuids::uuid& segUid)>& removeSeg,
  const AllViewsRecenterType& recenterAllViews);

void renderLandmarkGroupHeader(
  AppData& appData,
  const uuids::uuid& imageUid,
  size_t imageIndex,
  bool isActiveImage,
  const AllViewsRecenterType& recenterAllViews);

void renderAnnotationsHeader(
  AppData& appData,
  const uuids::uuid& imageUid,
  size_t imageIndex,
  bool isActiveImage,
  const std::function<void(const uuids::uuid& viewUid, const glm::vec3& worldFwdDirection)>&
    setViewCameraDirection,
  const std::function<void()>& paintActiveSegmentationWithActivePolygon,
  const AllViewsRecenterType& recenterAllViews);
