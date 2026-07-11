#include "logic/app/CallbackHandler.h"

#include "logic/app/DeformationWarp.h"
#include "logic/app/DataHelper.h"
#include "logic/app/ImageScaleInteraction.h"
#include "common/MathFuncs.h"
#include "common/SegmentationTypes.h"
#include "common/Types.h"

#include "image/SegUtil.h"

#include "logic/app/Data.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/camera/MathUtility.h"
#include "logic/camera/RaycastIsoSurfacePicker.h"

#include "logic/segmentation/AnnotationSegmentation.h"
#include "logic/segmentation/Poisson.h"
#include "logic/segmentation/SegHelpers.h"
#include "logic/segmentation/SegHelpers.tpp"

#include "rendering/Rendering.h"
#include "rendering/TextureSetup.h"

#include "windowing/GlfwWrapper.h"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace
{
using uuid = uuids::uuid;

constexpr float viewAABBoxScaleFactor = 1.10f;

// Angle threshold (in degrees) for checking whether two vectors are parallel
constexpr float parallelThreshold_degrees = 0.1f;

constexpr float imageFrontBackTranslationScaleFactor = 10.0f;

std::pair<std::optional<uuid>, Image*> targetTimeSeriesImage(AppData& appData)
{
  auto targetUid = appData.activeImageUid();
  Image* targetImage = targetUid ? appData.image(*targetUid) : nullptr;
  if (targetImage && targetImage->isTimeSeries()) {
    return {targetUid, targetImage};
  }

  for (const auto& imageUid : appData.imageUidsOrdered()) {
    Image* image = appData.image(imageUid);
    if (image && image->isTimeSeries()) {
      return {imageUid, image};
    }
  }
  return {std::nullopt, nullptr};
}

void setTimePoint(AppData& appData, const uuid& imageUid, Image& image, uint32_t timePoint)
{
  const uint32_t clamped = image.timeAxis().clamp(timePoint);
  if (image.settings().activeTimePoint() == clamped) {
    return;
  }
  image.settings().setActiveTimePoint(clamped);
  refreshImageTexturesForActiveTimePoint(appData, imageUid);
}

void setTimePointWithSynchronization(AppData& appData, const uuid& imageUid, Image& image, uint32_t timePoint)
{
  setTimePoint(appData, imageUid, image, timePoint);
  if (!appData.settings().synchronizeTimeSeries()) {
    return;
  }

  for (const auto& otherUid : appData.imageUidsOrdered()) {
    if (otherUid == imageUid) {
      continue;
    }
    Image* other = appData.image(otherUid);
    if (other && other->isTimeSeries()) {
      setTimePoint(appData, otherUid, *other, timePoint);
    }
  }
}

bool anyTimeSeriesPlaybackRunning(const AppData& appData)
{
  for (const auto& imageUid : appData.imageUidsOrdered()) {
    const Image* image = appData.image(imageUid);
    if (image && image->isTimeSeries() && image->settings().timePlaybackPlaying()) {
      return true;
    }
  }
  return false;
}
} // namespace

CallbackHandler::CallbackHandler(AppData& appData, GlfwWrapper& glfwWrapper, Rendering& rendering)
  : m_appData(appData), m_glfw(glfwWrapper), m_rendering(rendering)
{
}

bool CallbackHandler::clearSegVoxels(const uuid& segUid)
{
  Image* seg = m_appData.seg(segUid);
  if (!seg) {
    return false;
  }

  seg->setAllValues(0);

  const glm::uvec3 dataOffset = glm::uvec3{0};
  const glm::uvec3 dataSize = glm::uvec3{seg->header().pixelDimensions()};

  m_rendering.updateSegTexture(segUid, seg->header().memoryComponentType(), dataOffset, dataSize, seg->bufferAsVoid(0));

  return true;
}

std::optional<uuid> CallbackHandler::createBlankImageAndTexture(
  const uuid& matchImageUid,
  const ComponentType& componentType,
  uint32_t numComponents,
  const std::string& displayName,
  bool createSegmentation)
{
  const Image* matchImg = m_appData.image(matchImageUid);
  if (!matchImg) {
    spdlog::debug("Cannot create blank image for invalid matching image {}", matchImageUid);
    return std::nullopt; // Invalid matching image provided
  }

  // Copy the image header, changing it to have the given type and number of components:
  ImageHeader newHeader = matchImg->header();
  newHeader.setExistsOnDisk(false);
  newHeader.setFileName("<unsaved>");
  newHeader.adjustComponents(componentType, numComponents);

  // Buffer pointing to data for a single image component
  const void* buffer = nullptr;

  std::vector<int8_t> buffer_int8;
  std::vector<uint8_t> buffer_uint8;
  std::vector<int16_t> buffer_int16;
  std::vector<uint16_t> buffer_uint16;
  std::vector<int32_t> buffer_int32;
  std::vector<uint32_t> buffer_uint32;
  std::vector<float> buffer_float;

  switch (componentType) {
    case ComponentType::Int8: {
      buffer_int8.resize(newHeader.numPixels(), 0);
      buffer = static_cast<const void*>(buffer_int8.data());
      break;
    }
    case ComponentType::UInt8: {
      buffer_uint8.resize(newHeader.numPixels(), 0u);
      buffer = static_cast<const void*>(buffer_uint8.data());
      break;
    }
    case ComponentType::Int16: {
      buffer_int16.resize(newHeader.numPixels(), 0);
      buffer = static_cast<const void*>(buffer_int16.data());
      break;
    }
    case ComponentType::UInt16: {
      buffer_uint16.resize(newHeader.numPixels(), 0u);
      buffer = static_cast<const void*>(buffer_uint16.data());
      break;
    }
    case ComponentType::Int32: {
      buffer_int32.resize(newHeader.numPixels(), 0);
      buffer = static_cast<const void*>(buffer_int32.data());
      break;
    }
    case ComponentType::UInt32: {
      buffer_uint32.resize(newHeader.numPixels(), 0u);
      buffer = static_cast<const void*>(buffer_uint32.data());
      break;
    }
    case ComponentType::Float32: {
      buffer_float.resize(newHeader.numPixels(), 0.0f);
      buffer = static_cast<const void*>(buffer_float.data());
      break;
    }
    default: {
      spdlog::error("Invalid component type provided to create blank image");
      return std::nullopt;
    }
  }

  // Vector holding numComponents pointers to the same component buffer
  std::vector<const void*> imageComponents(numComponents, buffer);

  Image image(
    newHeader,
    displayName,
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    imageComponents);

  image.setHeaderOverrides(matchImg->getHeaderOverrides());

  // Assign the matching image's affine_T_subject transformation to the new image:
  image.transformations().set_affine_T_subject(matchImg->transformations().get_affine_T_subject());

  const std::string imgDisplayName = image.settings().displayName();
  spdlog::info("Created blank image matching header of existing image {} ({})", imgDisplayName, matchImageUid);
  spdlog::debug("Image header:\n{}", image.header());
  spdlog::debug("Image transformation:\n{}", image.transformations());

  const uuid imageUid = m_appData.addImage(std::move(image));
  spdlog::info("Creating texture for blank image {}", imageUid);

  const std::vector<uuid> createdImageTextureUids = createImageTextures(m_appData, std::vector<uuid>{imageUid});

  if (createdImageTextureUids.empty()) {
    spdlog::error("Unable to create texture for image {}", imageUid);

    // m_data.removeImage( imageUid ); //!< @todo Need to implement this
    return std::nullopt;
  }

  // Synchronize transformation with matching image
  syncManualImageTransformation(matchImageUid, imageUid);

  if (createSegmentation) {
    const std::string segDisplayName = std::string("Untitled segmentation for image '") + imgDisplayName + "'";
    createBlankSegWithColorTableAndTextures(imageUid, segDisplayName);
  }

  m_rendering.updateImageUniforms(m_appData.imageUidsOrdered()); // Update uniforms for all images
  m_appData.setRainbowColorsForAllImages();                      // Reassign rainbow colors

  return imageUid;
}

std::optional<uuid> CallbackHandler::createBlankSeg(const uuid& matchImageUid, const std::string& displayName)
{
  const Image* matchImg = m_appData.image(matchImageUid);
  if (!matchImg) {
    spdlog::debug("Cannot create blank segmentation for invalid matching image {}", matchImageUid);
    return std::nullopt; // Invalid image provided
  }

  // Copy the image header, changing it to scalar with uint8_t components
  ImageHeader newHeader = matchImg->header();
  newHeader.setExistsOnDisk(false);
  newHeader.setFileName("<unsaved>");
  newHeader.adjustComponents(ComponentType::UInt8, 1);

  // Create zeroed-out data buffer for component 0 of segmentation and vector pointing to the buffer
  const std::vector<uint8_t> buffer(newHeader.numPixels(), 0u);
  const std::vector<const void*> imageData{static_cast<const void*>(buffer.data())};

  Image seg(
    newHeader,
    displayName,
    Image::ImageRepresentation::Segmentation,
    Image::MultiComponentBufferType::SeparateImages,
    imageData);

  seg.setHeaderOverrides(matchImg->getHeaderOverrides());
  seg.settings().setOpacity(0.5); // Default opacity

  spdlog::info("Created segmentation matching header of image {}", matchImageUid);
  spdlog::debug("Header:\n{}", seg.header());
  spdlog::debug("Transformation:\n{}", seg.transformations());

  const auto segUid = m_appData.addSeg(std::move(seg));

  // Synchronize transformation on all segmentations of the image
  syncManualImageTransformationOnSegs(matchImageUid);

  // Update uniforms for all images
  m_rendering.updateImageUniforms(m_appData.imageUidsOrdered());

  return segUid;
}

std::optional<uuid> CallbackHandler::createBlankSegWithColorTableAndTextures(
  const uuid& matchImageUid,
  const std::string& displayName)
{
  spdlog::info("Creating blank segmentation {} with color table for image {}", displayName, matchImageUid);

  const Image* matchImage = m_appData.image(matchImageUid);
  if (!matchImage) {
    spdlog::error("Cannot create blank segmentation for invalid image {}", matchImageUid);
    return std::nullopt;
  }

  auto segUid = createBlankSeg(matchImageUid, displayName);
  if (!segUid) {
    spdlog::error("Error creating blank segmentation for image {}", matchImageUid);
    return std::nullopt;
  }

  spdlog::debug("Created blank segmentation {} ('{}') for image {}", *segUid, displayName, matchImageUid);

  return assignSegToImageWithColorTableAndTextures(matchImageUid, *segUid, true, true);
}

std::optional<uuid> CallbackHandler::assignSegToImageWithColorTableAndTextures(
  const uuid& matchImageUid,
  const uuid& segUid,
  bool createLabelColorTable,
  bool removeSegOnFailure)
{
  const Image* matchImage = m_appData.image(matchImageUid);
  if (!matchImage) {
    spdlog::error("Cannot assign segmentation {} to invalid image {}", segUid, matchImageUid);
    return std::nullopt;
  }

  const std::vector<uuid> assignedSegUids = m_appData.imageToSegUids(matchImageUid);
  const bool alreadyAssigned =
    std::find(std::begin(assignedSegUids), std::end(assignedSegUids), segUid) != std::end(assignedSegUids);

  Image* seg = m_appData.seg(segUid);
  if (!seg) {
    spdlog::error("Null segmentation {}", segUid);
    return std::nullopt;
  }

  if (createLabelColorTable) {
    const auto tableUid = data::createLabelColorTableForSegmentation(m_appData, segUid);
    bool createdTableTexture = false;

    if (tableUid) {
      SPDLOG_TRACE("Creating texture for label color table {}", *tableUid);
      createdTableTexture = m_rendering.createLabelColorTableTexture(*tableUid);
    }

    if (!tableUid || !createdTableTexture) {
      constexpr size_t k_defaultTableIndex = 0;

      spdlog::error(
        "Unable to create label color table for segmentation {}. Defaulting to table index {}.",
        segUid,
        k_defaultTableIndex);
      seg->settings().setLabelTableIndex(k_defaultTableIndex);
    }
  }

  if (!alreadyAssigned) {
    if (m_appData.assignSegUidToImage(matchImageUid, segUid)) {
      spdlog::info("Assigned segmentation {} to image {}", segUid, matchImageUid);
    }
    else {
      spdlog::error("Unable to assign segmentation {} to image {}", segUid, matchImageUid);
      if (removeSegOnFailure) {
        m_appData.removeSeg(segUid);
      }
      return std::nullopt;
    }
  }

  // Make it the active segmentation
  m_appData.assignActiveSegUidToImage(matchImageUid, segUid);

  SPDLOG_TRACE("Creating texture for segmentation {}", segUid);

  if (m_appData.renderData().m_segTextures.count(segUid) == 0) {
    const std::vector<uuid> createdSegTexUids = createSegTextures(m_appData, std::vector<uuid>{segUid});
    if (createdSegTexUids.empty()) {
      spdlog::error("Unable to create texture for segmentation {}", segUid);
      if (removeSegOnFailure) {
        m_appData.removeSeg(segUid);
      }
      return std::nullopt;
    }
  }

  // Assign the image's affine_T_subject transformation to its segmentation:
  seg->transformations().set_affine_T_subject(matchImage->transformations().get_affine_T_subject());

  // Synchronize transformation on all segmentations of the image:
  syncManualImageTransformationOnSegs(matchImageUid);

  // Update uniforms for all images
  m_rendering.updateImageUniforms(m_appData.imageUidsOrdered());

  return segUid;
}

bool CallbackHandler::executePoissonSegmentation(
  const uuid& imageUid,
  const uuid& seedSegUid,
  const SeedSegmentationType& segType)
{
  // Algorithm inputs:
  const Image* image = m_appData.image(imageUid);
  const Image* seedSeg = m_appData.seg(seedSegUid);

  if (!image) {
    spdlog::error("Null image {} input to Poisson segmentation", imageUid);
    return false;
  }

  if (!seedSeg) {
    spdlog::error("Null seed segmentation {} input to Poisson segmentation", seedSegUid);
    return false;
  }

  if (image->header().pixelDimensions() != seedSeg->header().pixelDimensions()) {
    spdlog::error(
      "Dimensions of image {} ({}) and seed segmentation {} ({}) do not match",
      imageUid,
      glm::to_string(image->header().pixelDimensions()),
      seedSegUid,
      glm::to_string(seedSeg->header().pixelDimensions()));
    return false;
  }

  const uint32_t imComp = image->settings().activeComponent();
  const uint32_t seedComp = 0;
  const glm::ivec3 dims{seedSeg->header().pixelDimensions()};

  // Buffer will either point to data of the image (if the image has float components)
  // or to data of a vector with float components:
  const float* imageBuffer = nullptr;
  std::vector<float> imageVector;

  if (ComponentType::Float32 == image->header().memoryComponentType()) {
    imageBuffer = static_cast<const float*>(image->bufferAsVoid(imComp));
  }
  else {
    imageVector.resize(image->header().numPixels(), 0.0f);
    for (std::size_t i = 0; i < image->header().numPixels(); ++i) {
      imageVector[i] = image->value<float>(imComp, i).value_or(0.0f);
    }
    imageBuffer = imageVector.data();
  }

  // Buffer will either point to data of the seed segmentation, with all components converted to
  // uint8_t and labels compressed to be in a contiguous range.
  constexpr bool ignoreBackgroundLabel = false;

  std::vector<uint8_t> seedSegVector(seedSeg->header().numPixels(), 0u);

  for (std::size_t i = 0; i < seedSeg->header().numPixels(); ++i) {
    seedSegVector[i] = seedSeg->value<uint8_t>(seedComp, i).value_or(0u);
  }

  const uint8_t* seedSegBuffer = seedSegVector.data();
  const LabelIndexMaps labelMaps = createLabelIndexMaps(dims, seedSegBuffer, ignoreBackgroundLabel);
  const size_t numSegsForImage = m_appData.imageToSegUids(imageUid).size();

  const std::string resultSegDisplayName =
    ((SeedSegmentationType::Binary == segType) ? std::string("Binary Poisson segmentation ")
                                               : std::string("Multi-label Poisson segmentation ")) +
    std::to_string(numSegsForImage + 1) + " for image '" + image->settings().displayName() + "'";

  //    SPDLOG_TRACE( "resultSegDisplayName = {}", resultSegDisplayName );

  const auto resultSegUid = createBlankSegWithColorTableAndTextures(imageUid, resultSegDisplayName);
  if (!resultSegUid) {
    spdlog::error("Unable to create blank segmentation matching image {}", imageUid);
    return false;
  }

  const std::string potDisplayName = std::string("Potential maps for '") + image->settings().displayName() + "'";

  // The number of components for the output potential image equals the number of
  // labels in the seed segmentation, including label zero. Component 0 of the
  // image holds the potential for all labels. Component i >= 1 of the image holds the
  // potential of label index i.
  const uint32_t numLabels = static_cast<uint32_t>(labelMaps.labelToIndex.size() - 1);
  const uint32_t numComps = static_cast<uint32_t>(labelMaps.labelToIndex.size());

  // Create potential image with float components
  const auto potImageUid =
    createBlankImageAndTexture(imageUid, ComponentType::Float32, numComps, potDisplayName, numComps);

  if (!potImageUid) {
    spdlog::error("Unable to create blank potential image matching image {}", imageUid);
    return false;
  }

  spdlog::debug("Generated blank potential image {} with {} components", *potImageUid, numComps);

  // Algorithm outputs:
  Image* resultSeg = m_appData.seg(*resultSegUid);
  Image* potImage = m_appData.image(*potImageUid);

  if (!resultSeg) {
    spdlog::error("Null result segmentation {} for Poisson", *resultSegUid);
    return false;
  }

  if (!potImage) {
    spdlog::error("Null potential image {} for Poisson", *potImageUid);
    return false;
  }

  if (image->header().pixelDimensions() != resultSeg->header().pixelDimensions()) {
    spdlog::error(
      "Dimensions of image {} ({}) and result segmentation {} ({}) do not match",
      imageUid,
      glm::to_string(image->header().pixelDimensions()),
      *resultSegUid,
      glm::to_string(resultSeg->header().pixelDimensions()));
    return false;
  }

  if (image->header().pixelDimensions() != potImage->header().pixelDimensions()) {
    spdlog::error(
      "Dimensions of image {} ({}) and potential image {} ({}) do not match",
      imageUid,
      glm::to_string(image->header().pixelDimensions()),
      *potImageUid,
      glm::to_string(potImage->header().pixelDimensions()));
    return false;
  }

  spdlog::info(
    "Executing Poisson segmentation on image {} with seeds {}; "
    "resulting segmentation: {}; resulting potential: {}",
    imageUid,
    seedSegUid,
    *resultSegUid,
    *potImageUid);

  const VoxelDistances voxelDists = computeVoxelDistances(image->header().spacing(), true);

  const float beta = computeBeta(imageBuffer, dims);
  spdlog::debug("Poisson beta = {}", beta);

  const uint32_t numIts = 10000;
  const float rjac = 0.6f;

  //    switch ( segType )
  //    {
  //    case SeedSegmentationType::Binary:
  //    {
  //        // First potential component is for all labels:
  //        float* potBuffer = static_cast<float*>( potImage->bufferAsVoid(0) );
  //        uint8_t* resultSegBuffer = static_cast<uint8_t*>( resultSeg->bufferAsVoid(0) );

  //        initializePotential( seedSegBuffer, potBuffer, dims, 0u );
  //        sor( seedSegBuffer, imageBuffer, potBuffer, dims, voxelDists, rjac, numIts, beta );
  //        computeResultSeg( potBuffer, resultSegBuffer, dims );
  //        break;
  //    }
  //    case SeedSegmentationType::MultiLabel:
  //    {
  //        break;
  //    }
  //    }

  float* potBuffer = static_cast<float*>(potImage->bufferAsVoid(0));
  uint8_t* resultSegBuffer = static_cast<uint8_t*>(resultSeg->bufferAsVoid(0));

  // 0th component of potential initialized by all labels:
  initializePotential(seedSegBuffer, potBuffer, dims, 0u);
  sor(seedSegBuffer, imageBuffer, potBuffer, dims, voxelDists, rjac, numIts, beta);

  std::vector<const float*> potBuffers(numLabels);

  // Loop over all label indices:
  for (uint32_t i = 1; i <= numLabels; ++i) {
    // ith component of potential initialized by label i:
    potBuffer = static_cast<float*>(potImage->bufferAsVoid(i));
    potBuffers[i - 1] = potBuffer;

    initializePotential(seedSegBuffer, potBuffer, dims, labelMaps.indexToLabel.at(i));
    sor(seedSegBuffer, imageBuffer, potBuffer, dims, voxelDists, rjac, numIts, beta);
  }

  computeResultSeg(potBuffers, resultSegBuffer, dims);

  potImage->updateComponentStats();
  resultSeg->updateComponentStats();

  spdlog::debug("Potential image stats: {}", potImage->settings());
  spdlog::debug("Resulting segmentation image stats: {}", resultSeg->settings());

  spdlog::debug("Start updating potential image textures");
  for (uint32_t i = 0; i < numComps; ++i) {
    m_rendering.updateImageTexture(
      *potImageUid,
      i,
      potImage->header().memoryComponentType(),
      glm::uvec3{0},
      potImage->header().pixelDimensions(),
      potImage->bufferAsVoid(i));
  }
  spdlog::debug("Done updating potential image textures");

  spdlog::debug("Start updating segmentation texture");
  m_rendering.updateSegTexture(
    *resultSegUid,
    resultSeg->header().memoryComponentType(),
    glm::uvec3{0},
    resultSeg->header().pixelDimensions(),
    resultSeg->bufferAsVoid(0));
  spdlog::debug("Done updating segmentation texture");

  return true;
}

void CallbackHandler::recenterViews(
  const ImageSelection& imageSelection,
  bool recenterCrosshairs,
  bool realignCrosshairs,
  bool recenterOnCurrentCrosshairsPos,
  bool resetObliqueOrientation,
  bool resetZoom,
  const std::set<uuid>& excludedViews)
{
  // On view recenter, force the crosshairs and views to snap to the center of the
  // reference image voxels. This is so that crosshairs/views don't land on a voxel
  // boundary (which causes jitter on view zoom).
  constexpr CrosshairsSnapping forceSnapping = CrosshairsSnapping::ReferenceImage;

  if (0 == m_appData.numImages()) {
    spdlog::warn("No images loaded: preparing views using default bounds");
  }

  // Compute the AABB that we are recentering views on:
  const auto worldBox = data::computeWorldAABBoxEnclosingImages(m_appData, imageSelection);

  if (recenterCrosshairs) {
    // Crosshairs always snap to voxels
    const glm::vec3 worldPos = math::computeAABBoxCenter(worldBox);
    const glm::vec3 worldPosSnapped = data::snapWorldPointToImageVoxels(m_appData, worldPos, forceSnapping);
    m_appData.state().setWorldCrosshairsPos(worldPosSnapped);
    updateThreeDViewsFollowingCrosshairs();
  }

  if (realignCrosshairs) {
    CoordinateFrame xhairs = m_appData.state().worldCrosshairs();
    xhairs.setIdentityRotation();
    m_appData.state().setWorldCrosshairs(xhairs);
    updateThreeDViewsFollowingCrosshairs();
  }

  const glm::vec3 worldCenter = recenterOnCurrentCrosshairsPos ? m_appData.state().worldCrosshairs().worldOrigin()
                                                               : math::computeAABBoxCenter(worldBox);

  // const glm::vec3 worldCenterSnapped = data::snapWorldPointToImageVoxels( m_appData, worldCenter,
  // forceSnapping );

  m_appData.windowData().recenterAllViews(
    worldCenter,
    viewAABBoxScaleFactor * math::computeAABBoxSize(worldBox),
    resetZoom,
    resetObliqueOrientation,
    excludedViews);
}

void CallbackHandler::recenterView(const ImageSelection& imageSelection, const uuid& viewUid)
{
  // On view recenter, force the crosshairs and views to snap to the center of the
  // reference image voxels. This is so that crosshairs/views don't land on a voxel
  // boundary (which causes jitter on view zoom).
  constexpr CrosshairsSnapping forceSnapping = CrosshairsSnapping::ReferenceImage;
  constexpr bool resetZoom = false;
  constexpr bool resetObliqueOrientation = true;

  if (0 == m_appData.numImages()) {
    spdlog::warn("No images loaded, so recentering view {} using default bounds", viewUid);
  }

  // Size and position the views based on the enclosing AABB of the image selection:
  const auto worldBox = data::computeWorldAABBoxEnclosingImages(m_appData, imageSelection);
  const auto worldBoxSize = math::computeAABBoxSize(worldBox);

  const glm::vec3 worldPos = m_appData.state().worldCrosshairs().worldOrigin();
  const glm::vec3 worldPosSnapped = data::snapWorldPointToImageVoxels(m_appData, worldPos, forceSnapping);

  m_appData.windowData()
    .recenterView(viewUid, worldPosSnapped, viewAABBoxScaleFactor * worldBoxSize, resetZoom, resetObliqueOrientation);
}

void CallbackHandler::doCrosshairsMove(const ViewHit& hit)
{
  if (!checkAndSetActiveView(hit.viewUid)) {
    return;
  }

  const glm::vec3 newCrosshairs = glm::vec3{hit.worldPos};
  m_appData.state().setWorldCrosshairsPos(newCrosshairs);
  updateThreeDViewsFollowingCrosshairs();
}

void CallbackHandler::doCrosshairsScroll(const ViewHit& hit, const glm::vec2& scrollOffset, bool fineScroll)
{
  const float multiplier = fineScroll ? 0.1f : 1.0f;

  const float scrollDistance =
    multiplier *
    data::sliceScrollDistance(m_appData, hit.worldFrontAxis, ImageSelection::VisibleImagesInView, hit.view);

  const glm::vec3 worldPos =
    m_appData.state().worldCrosshairs().worldOrigin() + scrollOffset.y * scrollDistance * hit.worldFrontAxis;

  const glm::vec3 worldPosSnapped = data::snapWorldPointToImageVoxels(m_appData, worldPos);
  m_appData.state().setWorldCrosshairsPos(worldPosSnapped);
  updateThreeDViewsFollowingCrosshairs();
}

void CallbackHandler::doSegment(const ViewHit& hit, bool swapFgAndBg)
{
  const glm::ivec3 voxelZero{0, 0, 0};

  if (!hit.view) {
    return;
  }

  const auto activeImageUid = m_appData.activeImageUid();
  if (!activeImageUid) {
    return;
  }

  if (!checkAndSetActiveView(hit.viewUid)) {
    return;
  }

  if (0 == std::count(std::begin(hit.view->visibleImages()), std::end(hit.view->visibleImages()), *activeImageUid)) {
    return; // The active image is not visible
  }

  const auto activeSegUid = m_appData.imageToActiveSegUid(*activeImageUid);
  if (!activeSegUid) {
    return;
  }

  // The position is in the view bounds; make this the active view:
  m_appData.windowData().setActiveViewUid(hit.viewUid);

  // Do nothing if the active segmentation is null
  Image* activeSeg = m_appData.seg(*activeSegUid);
  if (!activeSeg) {
    return;
  }
  const glm::vec3 activeBrushSpacing = activeSeg->header().spacing();

  // Gather all synchronized image/segmentation pairs.
  std::vector<std::pair<uuid, uuid>> imageSegUids;
  imageSegUids.emplace_back(*activeImageUid, *activeSegUid);

  for (const auto& imageUid : m_appData.imagesBeingSegmented()) {
    if (imageUid == *activeImageUid) {
      continue;
    }

    if (const auto segUid = m_appData.imageToActiveSegUid(imageUid)) {
      imageSegUids.emplace_back(imageUid, *segUid);
    }
  }

  // Note: when moving crosshairs, the worldPos would be offset at this stage.
  // i.e.: worldPos -= glm::vec4{ offsetDist * worldCameraFront, 0.0f };
  // However, we want to allow the user to segment on any view, regardless of its offset.
  // Therefore, the offset is not applied.

  const LabelType labelToPaint = static_cast<LabelType>(
    swapFgAndBg ? m_appData.settings().backgroundLabel() : m_appData.settings().foregroundLabel());

  const LabelType labelToReplace = static_cast<LabelType>(
    swapFgAndBg ? m_appData.settings().foregroundLabel() : m_appData.settings().backgroundLabel());

  const int brushSize = static_cast<int>(m_appData.settings().brushSizeInVoxels());

  const AppSettings& settings = m_appData.settings();

  // Paint on each segmentation
  for (const auto& [imageUid, segUid] : imageSegUids) {
    Image* seg = m_appData.seg(segUid);
    if (!seg) {
      continue;
    }

    const glm::ivec3 dims{seg->header().pixelDimensions()};
    const glm::vec4 sampleWorldPos =
      deformation_warp::inverseWarpSampleWorldPosition(m_appData, imageUid, hit.worldPos_offsetApplied);

    // Use the offset position, so that the user can paint in any offset view of a lightbox layout:
    const glm::mat4& pixel_T_worldDef = seg->transformations().pixel_T_worldDef();
    const glm::vec4 pixelPos = pixel_T_worldDef * sampleWorldPos;
    const glm::vec3 pixelPos3 = pixelPos / pixelPos.w;
    const glm::ivec3 roundedPixelPos{glm::round(pixelPos3)};

    if (glm::any(glm::lessThan(roundedPixelPos, voxelZero)) || glm::any(glm::greaterThanEqual(roundedPixelPos, dims))) {
      continue; // This pixel is outside the image
    }

    // View plane normal vector transformed into Voxel space:
    const glm::vec3 voxelViewPlaneNormal =
      glm::normalize(glm::inverseTranspose(glm::mat3(pixel_T_worldDef)) * (-hit.worldFrontAxis));

    // View plane equation:
    const glm::vec4 voxelViewPlane = math::makePlane(voxelViewPlaneNormal, pixelPos3);

    auto updateSegTexture = [this, &segUid](
                              const ComponentType& memoryComponentType,
                              const glm::uvec3& dataOffset,
                              const glm::uvec3& dataSize,
                              const LabelType* data) {
      m_rendering.updateSegTextureWithInt64Data(segUid, memoryComponentType, dataOffset, dataSize, data);
    };

    paintSegmentation(
      *seg,
      labelToPaint,
      labelToReplace,
      settings.replaceBackgroundWithForeground(),
      settings.useRoundBrush(),
      settings.use3dBrush(),
      settings.useIsotropicBrush(),
      brushSize,
      roundedPixelPos,
      voxelViewPlane,
      activeBrushSpacing,
      updateSegTexture);
  }
}

void CallbackHandler::clearBrushPreview()
{
  m_lastBrushPreviewHit.reset();
  m_lastBrushPreviewSwapFgAndBg = false;
  m_lastBrushPreviewPainting = false;
  m_lastBrushPreviewRevision = m_appData.settings().brushPreviewRevision();
  m_brushPreviewData.clear();
  m_rendering.clearBrushPreviewTextures();
}

void CallbackHandler::refreshBrushPreviewIfNeeded()
{
  const AppSettings& settings = m_appData.settings();
  if (!m_lastBrushPreviewHit) {
    return;
  }

  if (MouseMode::Segment != m_appData.state().mouseMode() || BrushPreviewMode::Disabled == settings.brushPreviewMode())
  {
    clearBrushPreview();
    return;
  }

  if (settings.brushPreviewRevision() != m_lastBrushPreviewRevision) {
    updateBrushPreview(*m_lastBrushPreviewHit, m_lastBrushPreviewSwapFgAndBg, m_lastBrushPreviewPainting);
  }
}

void CallbackHandler::updateBrushPreview(const ViewHit& hit, bool swapFgAndBg, bool paintingPreview)
{
  const glm::ivec3 voxelZero{0, 0, 0};
  const AppSettings& settings = m_appData.settings();

  m_rendering.hideBrushPreviewTextures();

  if (!hit.view) {
    return;
  }

  const auto activeImageUid = m_appData.activeImageUid();
  if (!activeImageUid) {
    return;
  }

  if (0 == std::count(std::begin(hit.view->visibleImages()), std::end(hit.view->visibleImages()), *activeImageUid)) {
    return;
  }

  const auto activeSegUid = m_appData.imageToActiveSegUid(*activeImageUid);
  if (!activeSegUid) {
    return;
  }
  const Image* activeSeg = m_appData.seg(*activeSegUid);
  if (!activeSeg) {
    return;
  }
  const glm::vec3 activeBrushSpacing = activeSeg->header().spacing();

  m_lastBrushPreviewHit = hit;
  m_lastBrushPreviewSwapFgAndBg = swapFgAndBg;
  m_lastBrushPreviewPainting = paintingPreview;
  m_lastBrushPreviewRevision = settings.brushPreviewRevision();

  std::vector<std::pair<uuid, uuid>> imageSegUids;
  imageSegUids.emplace_back(*activeImageUid, *activeSegUid);

  for (const auto& imageUid : m_appData.imagesBeingSegmented()) {
    if (imageUid == *activeImageUid) {
      continue;
    }

    if (const auto segUid = m_appData.imageToActiveSegUid(imageUid)) {
      imageSegUids.emplace_back(imageUid, *segUid);
    }
  }

  const LabelType labelToPaint = static_cast<LabelType>(
    swapFgAndBg ? m_appData.settings().backgroundLabel() : m_appData.settings().foregroundLabel());

  const LabelType labelToReplace = static_cast<LabelType>(
    swapFgAndBg ? m_appData.settings().foregroundLabel() : m_appData.settings().backgroundLabel());

  const int brushSize = static_cast<int>(settings.brushSizeInVoxels());
  const bool onlyChangedVoxels = !paintingPreview && (BrushPreviewVoxels::Changed == settings.brushPreviewVoxels());

  for (const auto& [imageUid, segUid] : imageSegUids) {
    const Image* img = m_appData.image(imageUid);
    Image* seg = m_appData.seg(segUid);
    if (!img || !seg) {
      continue;
    }

    const glm::ivec3 dims{seg->header().pixelDimensions()};
    const glm::mat4& pixel_T_worldDef = seg->transformations().pixel_T_worldDef();
    const glm::vec4 sampleWorldPos =
      deformation_warp::inverseWarpSampleWorldPosition(m_appData, imageUid, hit.worldPos_offsetApplied);
    const glm::vec4 pixelPos = pixel_T_worldDef * sampleWorldPos;
    const glm::vec3 pixelPos3 = pixelPos / pixelPos.w;
    const glm::ivec3 roundedPixelPos{glm::round(pixelPos3)};

    if (glm::any(glm::lessThan(roundedPixelPos, voxelZero)) || glm::any(glm::greaterThanEqual(roundedPixelPos, dims))) {
      continue;
    }

    const glm::vec3 voxelViewPlaneNormal =
      glm::normalize(glm::inverseTranspose(glm::mat3(pixel_T_worldDef)) * (-hit.worldFrontAxis));
    const glm::vec4 voxelViewPlane = math::makePlane(voxelViewPlaneNormal, pixelPos3);

    auto footprint = computeSegmentationBrushFootprint(
      *seg,
      settings.useRoundBrush(),
      settings.use3dBrush(),
      settings.useIsotropicBrush(),
      brushSize,
      roundedPixelPos,
      voxelViewPlane,
      activeBrushSpacing);

    if (footprint.voxels.empty()) {
      continue;
    }

    if (onlyChangedVoxels) {
      SegmentationVoxelSet changedVoxels;
      glm::ivec3 minVoxel{std::numeric_limits<int>::max()};
      glm::ivec3 maxVoxel{std::numeric_limits<int>::lowest()};

      for (const auto& p : footprint.voxels) {
        const int64_t currentLabel = seg->value<int64_t>(0, p.x, p.y, p.z).value_or(0);
        const bool wouldChange = settings.replaceBackgroundWithForeground()
                                   ? (labelToReplace == currentLabel && labelToPaint != currentLabel)
                                   : (labelToPaint != currentLabel);
        if (!wouldChange) {
          continue;
        }

        changedVoxels.insert(p);
        minVoxel = glm::min(minVoxel, p);
        maxVoxel = glm::max(maxVoxel, p);
      }

      footprint.voxels = std::move(changedVoxels);
      footprint.minVoxel = minVoxel;
      footprint.maxVoxel = maxVoxel;
    }

    if (footprint.voxels.empty()) {
      continue;
    }

    const glm::uvec3 previewSize{footprint.maxVoxel - footprint.minVoxel + glm::ivec3{1}};
    const size_t N =
      static_cast<size_t>(previewSize.x) * static_cast<size_t>(previewSize.y) * static_cast<size_t>(previewSize.z);
    m_brushPreviewData.assign(N, 0);

    for (const auto& p : footprint.voxels) {
      const glm::ivec3 q = p - footprint.minVoxel;
      const size_t index = static_cast<size_t>(q.x) +
                           static_cast<size_t>(previewSize.x) *
                             (static_cast<size_t>(q.y) + static_cast<size_t>(previewSize.y) * static_cast<size_t>(q.z));
      m_brushPreviewData[index] = 1;
    }

    glm::vec4 previewColor{1.0f};
    if (const auto tableUid = m_appData.labelTableUid(seg->settings().labelTableIndex())) {
      if (const ParcellationLabelTable* table = m_appData.labelTable(*tableUid)) {
        const auto labelIndex = static_cast<std::size_t>(labelToPaint);
        if (labelIndex < table->numLabels()) {
          const glm::u8vec3 color = table->getColor(labelIndex);
          previewColor = glm::vec4{glm::vec3{color} / 255.0f, 1.0f};
        }
      }
    }

    const glm::mat4 localVoxel_T_world = glm::translate(-glm::vec3{footprint.minVoxel}) *
                                         seg->transformations().pixel_T_subject() *
                                         img->transformations().subject_T_worldDef();

    m_rendering.updateBrushPreviewTexture(
      imageUid,
      segUid,
      seg->header().memoryComponentType(),
      previewSize,
      localVoxel_T_world,
      previewColor,
      !paintingPreview,
      m_brushPreviewData.data());
  }
}

void CallbackHandler::paintActiveSegmentationWithAnnotation()
{
  const auto activeImageUid = m_appData.activeImageUid();
  if (!activeImageUid) {
    return;
  }

  const auto activeSegUid = m_appData.imageToActiveSegUid(*activeImageUid);
  if (!activeSegUid) {
    spdlog::debug("There is no active segmentation to paint for image {}", *activeImageUid);
    return;
  }

  const auto activeAnnotUid = m_appData.imageToActiveAnnotationUid(*activeImageUid);
  if (!activeAnnotUid) {
    spdlog::debug("There is no active annotation to paint for image {}", *activeImageUid);
    return;
  }

  Image* seg = m_appData.seg(*activeSegUid);
  if (!seg) {
    spdlog::error("Segmentation {} is null", *activeSegUid);
    return;
  }

  const Annotation* annot = m_appData.annotation(*activeAnnotUid);
  if (!annot) {
    spdlog::error("Annotation {} is null", *activeAnnotUid);
    return;
  }

  /// @todo Implement algorithm for filling smoothed polygons.

  if (!annot->isClosed()) {
    spdlog::warn(
      "Annotation {} is not closed and so cannot be filled to paint segmentation {}",
      *activeAnnotUid,
      *activeSegUid);
    return;
  }

  if (annot->isSmoothed()) {
    spdlog::warn(
      "Annotation {} is smoothed and so cannot be filled to paint segmentation {}",
      *activeAnnotUid,
      *activeSegUid);
    return;
  }

  auto updateSegTexture = [this, &activeSegUid](
                            const ComponentType& memoryComponentType,
                            const glm::uvec3& dataOffset,
                            const glm::uvec3& dataSize,
                            const LabelType* data) {
    if (!activeSegUid) {
      return;
    }

    m_rendering.updateSegTextureWithInt64Data(*activeSegUid, memoryComponentType, dataOffset, dataSize, data);
  };

  fillSegmentationWithPolygon(
    *seg,
    annot,
    static_cast<LabelType>(m_appData.settings().foregroundLabel()),
    static_cast<LabelType>(m_appData.settings().backgroundLabel()),
    m_appData.settings().replaceBackgroundWithForeground(),
    updateSegTexture);
}

void CallbackHandler::doWindowLevel(
  const ViewHit& startHit,
  const ViewHit& prevHit,
  const ViewHit& currHit,
  bool fineAdjustment)
{
  View* viewToWL = startHit.view;

  if (!viewToWL) {
    return;
  }

  const float multipler = fineAdjustment ? 0.1f : 1.0f;

  if (IntensityProjectionMode::Xray == viewToWL->intensityProjectionMode()) {
    // Special logic to adjust W/L for views rendering in x-ray projection mode:

    // Level/width values for x-ray projection mode are in range [0.0, 1.0]
    constexpr float levelMin = 0.0f;
    constexpr float levelMax = 1.0f;
    constexpr float winMin = 0.0f;
    constexpr float winMax = 1.0f;

    float oldLevel = m_appData.renderData().m_xrayIntensityLevel;
    float oldWindow = m_appData.renderData().m_xrayIntensityWindow;

    const float levelDelta =
      multipler * (levelMax - levelMin) * (currHit.windowClipPos.y - prevHit.windowClipPos.y) / 2.0f;
    const float winDelta = multipler * (winMax - winMin) * (currHit.windowClipPos.x - prevHit.windowClipPos.x) / 2.0f;

    const float newLevel = std::min(std::max(oldLevel + levelDelta, levelMin), levelMax);
    const float newWindow = std::min(std::max(oldWindow + winDelta, winMin), winMax);

    m_appData.renderData().m_xrayIntensityLevel = newLevel;
    m_appData.renderData().m_xrayIntensityWindow = newWindow;
  }
  else {
    const auto activeImageUid = m_appData.activeImageUid();
    if (!activeImageUid) {
      return;
    }

    Image* activeImage = m_appData.image(*activeImageUid);
    if (!activeImage) {
      return;
    }

    if (0 == std::count(std::begin(viewToWL->visibleImages()), std::end(viewToWL->visibleImages()), *activeImageUid)) {
      return; // The active image is not visible
    }

    auto& S = activeImage->settings();

    const double centerDelta = multipler * (S.minMaxWindowCenterRange().second - S.minMaxWindowCenterRange().first) *
                               static_cast<double>(currHit.windowClipPos.y - prevHit.windowClipPos.y) / 2.0;

    const double windowDelta = multipler * (S.minMaxWindowWidthRange().second - S.minMaxWindowWidthRange().first) *
                               static_cast<double>(currHit.windowClipPos.x - prevHit.windowClipPos.x) / 2.0;

    S.setWindowCenter(S.windowCenter() + centerDelta);
    S.setWindowWidth(S.windowWidth() + windowDelta);

    m_rendering.updateImageUniforms(*activeImageUid);
  }
}

void CallbackHandler::doOpacity(const ViewHit& prevHit, const ViewHit& currHit)
{
  constexpr double opMin = 0.0;
  constexpr double opMax = 1.0;

  if (!currHit.view) {
    return;
  }

  const auto activeImageUid = m_appData.activeImageUid();
  if (!activeImageUid) {
    return;
  }

  if (
    0 ==
    std::count(std::begin(currHit.view->visibleImages()), std::end(currHit.view->visibleImages()), *activeImageUid))
  {
    return; // The active image is not visible
  }

  Image* activeImage = m_appData.image(*activeImageUid);
  if (!activeImage) {
    return;
  }

  const double opacityDelta =
    (opMax - opMin) * static_cast<double>(currHit.windowClipPos.y - prevHit.windowClipPos.y) / 2.0;

  const double newOpacity = std::min(std::max(activeImage->settings().opacity() + opacityDelta, opMin), opMax);

  activeImage->settings().setOpacity(newOpacity);

  m_rendering.updateImageUniforms(*activeImageUid);
}

void CallbackHandler::doCameraTranslate2d(const ViewHit& startHit, const ViewHit& prevHit, const ViewHit& currHit)
{
  const glm::vec3 worldOrigin = m_appData.state().worldCrosshairs().worldOrigin();

  View* viewToTranslate = startHit.view;
  if (!viewToTranslate) {
    return;
  }

  const auto& viewUidToTranslate = startHit.viewUid;

  const auto backupCamera = viewToTranslate->camera();
  helper::panRelativeToWorldPosition(viewToTranslate->camera(), prevHit.viewClipPos, currHit.viewClipPos, worldOrigin);

  if (const auto transGroupUid = viewToTranslate->cameraTranslationSyncGroupUid()) {
    for (const auto& syncedViewUid :
         m_appData.windowData().cameraSyncGroupViewUids(CameraSyncMode::Translation, *transGroupUid))
    {
      if (syncedViewUid == viewUidToTranslate) {
        continue;
      }

      View* syncedView = m_appData.windowData().getCurrentView(syncedViewUid);
      if (!syncedView) {
        continue;
      }
      else if (syncedView->viewType() != viewToTranslate->viewType()) {
        continue;
      }

      if (helper::areViewDirectionsParallel(
            syncedView->camera(),
            backupCamera,
            Directions::View::Back,
            parallelThreshold_degrees))
      {
        helper::panRelativeToWorldPosition(syncedView->camera(), prevHit.viewClipPos, currHit.viewClipPos, worldOrigin);
      }
    }
  }
}

void CallbackHandler::doCameraRotate2d(
  const ViewHit& startHit,
  const ViewHit& prevHit,
  const ViewHit& currHit,
  const RotationOrigin& rotationOrigin)
{
  View* viewToRotate = startHit.view;
  if (!viewToRotate) {
    return;
  }

  const auto& viewUidToRotate = startHit.viewUid;

  // Only allow rotation of oblique and 3D views
  if (ViewType::Oblique != viewToRotate->viewType() && ViewType::ThreeD != viewToRotate->viewType()) {
    return;
  }

  // Point about which to rotate the view:
  glm::vec3 worldRotationCenterPos;

  switch (rotationOrigin) {
    case RotationOrigin::Crosshairs: {
      worldRotationCenterPos = m_appData.state().worldCrosshairs().worldOrigin();
      break;
    }
    case RotationOrigin::CameraEye:
    case RotationOrigin::ViewCenter: {
      worldRotationCenterPos = helper::worldOrigin(viewToRotate->camera());
      break;
    }
  }

  glm::vec4 clipRotationCenterPos =
    helper::clip_T_world(viewToRotate->camera()) * glm::vec4{m_appData.state().worldCrosshairs().worldOrigin(), 1.0f};

  clipRotationCenterPos /= clipRotationCenterPos.w;

  const auto backupCamera = viewToRotate->camera();
  helper::rotateInPlane(
    viewToRotate->camera(),
    prevHit.viewClipPos,
    currHit.viewClipPos,
    glm::vec2{clipRotationCenterPos});

  // Rotate the synchronized views:
  if (const auto rotGroupUid = viewToRotate->cameraRotationSyncGroupUid()) {
    for (const auto& syncedViewUid :
         m_appData.windowData().cameraSyncGroupViewUids(CameraSyncMode::Rotation, *rotGroupUid))
    {
      if (syncedViewUid == viewUidToRotate) {
        continue;
      }

      View* syncedView = m_appData.windowData().getCurrentView(syncedViewUid);
      if (!syncedView) {
        continue;
      }
      else if (syncedView->viewType() != viewToRotate->viewType()) {
        continue;
      }

      if (!helper::areViewDirectionsParallel(
            syncedView->camera(),
            backupCamera,
            Directions::View::Back,
            parallelThreshold_degrees))
      {
        continue;
      }

      helper::rotateInPlane(
        syncedView->camera(),
        prevHit.viewClipPos,
        currHit.viewClipPos,
        glm::vec2{clipRotationCenterPos});
    }
  }
}

void CallbackHandler::doCameraRotate3d(
  const ViewHit& startHit,
  const ViewHit& prevHit,
  const ViewHit& currHit,
  const RotationOrigin& rotationOrigin,
  const AxisConstraint& constraint)
{
  View* viewToRotate = startHit.view;
  if (!viewToRotate) {
    return;
  }

  const auto& viewUidToRotate = startHit.viewUid;

  // Only allow rotation of oblique and 3D views
  if (ViewType::Oblique != viewToRotate->viewType() && ViewType::ThreeD != viewToRotate->viewType()) {
    return;
  }

  glm::vec2 viewClipPrevPos = prevHit.viewClipPos;
  glm::vec2 viewClipCurrPos = currHit.viewClipPos;

  switch (constraint) {
    case AxisConstraint::X: {
      viewClipPrevPos.x = 0.0f;
      viewClipCurrPos.x = 0.0f;
      break;
    }
    case AxisConstraint::Y: {
      viewClipPrevPos.y = 0.0f;
      viewClipCurrPos.y = 0.0f;
      break;
    }
    case AxisConstraint::None:
    default: {
      break;
    }
  }

  // Point about which to rotate the view:
  glm::vec3 worldRotationCenterPos;

  switch (rotationOrigin) {
    case RotationOrigin::Crosshairs: {
      worldRotationCenterPos = m_appData.state().worldCrosshairs().worldOrigin();
      break;
    }
    case RotationOrigin::CameraEye:
    case RotationOrigin::ViewCenter: {
      worldRotationCenterPos = helper::worldOrigin(viewToRotate->camera());
      break;
    }
  }

  const auto backupCamera = viewToRotate->camera();
  helper::rotateAboutWorldPoint(viewToRotate->camera(), viewClipPrevPos, viewClipCurrPos, worldRotationCenterPos);

  // Rotate the synchronized views:
  if (const auto rotGroupUid = viewToRotate->cameraRotationSyncGroupUid()) {
    for (const auto& syncedViewUid :
         m_appData.windowData().cameraSyncGroupViewUids(CameraSyncMode::Rotation, *rotGroupUid))
    {
      if (syncedViewUid == viewUidToRotate) {
        continue;
      }

      View* syncedView = m_appData.windowData().getCurrentView(syncedViewUid);
      if (!syncedView) {
        continue;
      }
      else if (syncedView->viewType() != viewToRotate->viewType()) {
        continue;
      }

      if (!helper::areViewDirectionsParallel(
            syncedView->camera(),
            backupCamera,
            Directions::View::Back,
            parallelThreshold_degrees))
      {
        continue;
      }

      helper::rotateAboutWorldPoint(syncedView->camera(), viewClipPrevPos, viewClipCurrPos, worldRotationCenterPos);
    }
  }
}

namespace
{

float minPositiveSpacing(const Image& image)
{
  const glm::vec3 spacing = image.header().spacing();
  float result = std::numeric_limits<float>::max();
  for (int axis = 0; axis < 3; ++axis) {
    if (spacing[axis] > 0.0f) {
      result = std::min(result, spacing[axis]);
    }
  }
  return std::isfinite(result) ? result : 1.0f;
}

std::vector<double> pickableIsoValues(const AppData& appData, const uuid& imageUid, const Image& image)
{
  const ImageSettings& settings = image.settings();
  if (!settings.globalVisibility() || !settings.visibility() || !settings.isosurfacesVisible()) {
    return {};
  }

  std::vector<double> values;
  const uint32_t activeComponent = settings.activeComponent();
  for (const uuid& surfaceUid : appData.isosurfaceUids(imageUid, activeComponent)) {
    const Isosurface* surface = appData.isosurface(imageUid, activeComponent, surfaceUid);
    if (surface && surface->visible) {
      values.push_back(surface->value);
    }
  }
  return values;
}

camera3d::SceneFrame threeDSceneFrameForView(const AppData& appData, const View& view)
{
  const ImageSelection selection =
    view.visibleImages().empty() ? ImageSelection::AllLoadedImages : ImageSelection::VisibleImagesInView;
  camera3d::SceneFrame scene =
    camera3d::sceneFrameFromAABB(data::computeWorldAABBoxEnclosingImages(appData, selection, &view));
  scene.m_voxelDiagonal = data::computeMinVoxelDiagonalForImages(appData, selection, &view);
  return scene;
}

glm::vec3 threeDTargetForView(const AppData& appData, const View& view, const camera3d::SceneFrame& scene)
{
  return (camera3d::OrbitTargetMode::Crosshairs == view.threeDState().m_orbitTargetMode)
           ? appData.state().worldCrosshairs().worldOrigin()
           : scene.m_center;
}

bool prepareThreeDView(AppData& appData, View* view)
{
  if (!view || ViewType::ThreeD != view->viewType()) {
    return false;
  }

  appData.renderData().m_lastInteractedThreeDViewUid = view->uid();
  const camera3d::SceneFrame scene = threeDSceneFrameForView(appData, *view);
  view->initializeThreeDCameraIfNeeded(scene);
  camera3d::Controller{view->threeDCamera(), view->threeDState()}.updateScene(scene);
  return true;
}

} // namespace

void CallbackHandler::doThreeDCameraOrbit(const ViewHit& startHit, const ViewHit& prevHit, const ViewHit& currHit)
{
  View* view = startHit.view;
  if (!prepareThreeDView(m_appData, view)) {
    return;
  }

  camera3d::State& state = view->threeDState();
  const camera3d::SceneFrame scene = threeDSceneFrameForView(m_appData, *view);
  state.m_orbitTarget = threeDTargetForView(m_appData, *view, scene);
  camera3d::orbit(view->threeDCamera(), state, prevHit.viewClipPos, currHit.viewClipPos);
  if (state.m_viewPositionFollowsCrosshairs) {
    state.m_crosshairsFollowOffset =
      helper::worldOrigin(view->threeDCamera()) - m_appData.state().worldCrosshairs().worldOrigin();
  }
}

void CallbackHandler::doThreeDCameraRotateAboutEye(
  const ViewHit& startHit,
  const ViewHit& prevHit,
  const ViewHit& currHit)
{
  View* view = startHit.view;
  if (!prepareThreeDView(m_appData, view)) {
    return;
  }
  const bool reverseRotation = m_appData.renderData().m_reverseThreeDRotateAboutEye;
  camera3d::rotateAboutEye(
    view->threeDCamera(),
    view->threeDState(),
    reverseRotation ? currHit.viewClipPos : prevHit.viewClipPos,
    reverseRotation ? prevHit.viewClipPos : currHit.viewClipPos);
}

void CallbackHandler::doThreeDCameraRoll(const ViewHit& startHit, const ViewHit& prevHit, const ViewHit& currHit)
{
  View* view = startHit.view;
  if (!prepareThreeDView(m_appData, view)) {
    return;
  }
  camera3d::roll(view->threeDCamera(), view->threeDState(), prevHit.viewClipPos, currHit.viewClipPos);
}

void CallbackHandler::doThreeDCameraPan(const ViewHit& startHit, const ViewHit& prevHit, const ViewHit& currHit)
{
  View* view = startHit.view;
  if (!prepareThreeDView(m_appData, view)) {
    return;
  }
  const camera3d::SceneFrame scene = threeDSceneFrameForView(m_appData, *view);
  camera3d::panOnSceneAabbPlane(
    view->threeDCamera(),
    view->threeDState(),
    scene,
    startHit.viewClipPos,
    prevHit.viewClipPos,
    currHit.viewClipPos);
}

void CallbackHandler::doThreeDCameraScroll(
  const ViewHit& hit,
  const glm::vec2& scrollOffset,
  bool faster,
  bool adjustPerspectiveFov)
{
  if (!prepareThreeDView(m_appData, hit.view)) {
    return;
  }
  camera3d::dollyOrZoom(
    hit.view->threeDCamera(),
    hit.view->threeDState(),
    hit.viewClipPos,
    static_cast<float>(scrollOffset.y),
    faster,
    adjustPerspectiveFov);
}

void CallbackHandler::doThreeDCameraKeyboardPanOrRotate(
  const ViewHit& hit,
  const glm::ivec2& direction,
  bool rotate,
  bool faster)
{
  if (!prepareThreeDView(m_appData, hit.view)) {
    return;
  }

  const float step = faster ? 0.12f : 0.04f;
  const glm::vec2 delta = step * glm::vec2{direction};
  if (rotate) {
    camera3d::rotateAboutEye(hit.view->threeDCamera(), hit.view->threeDState(), glm::vec2{0.0f}, delta);
  }
  else {
    camera3d::pan(hit.view->threeDCamera(), hit.view->threeDState(), glm::vec2{0.0f}, -delta);
  }
}

void CallbackHandler::doCameraRotate3d(const uuid& viewUid, const glm::quat& camera_T_world_rotationDelta)
{
  auto& windowData = m_appData.windowData();

  View* view = windowData.getView(viewUid);
  if (!view) {
    return;
  }

  if (ViewRenderMode::Disabled == view->renderMode()) {
    return;
  }

  if (ViewType::Oblique != view->viewType()) {
    return;
  }

  const glm::vec3 worldOrigin = m_appData.state().worldCrosshairs().worldOrigin();

  const auto backupCamera = view->camera();
  helper::applyViewRotationAboutWorldPoint(view->camera(), camera_T_world_rotationDelta, worldOrigin);

  if (const auto rotGroupUid = view->cameraRotationSyncGroupUid()) {
    for (const auto& syncedViewUid : windowData.cameraSyncGroupViewUids(CameraSyncMode::Rotation, *rotGroupUid)) {
      if (syncedViewUid == viewUid) {
        continue;
      }

      View* syncedView = windowData.getCurrentView(syncedViewUid);
      if (!syncedView) {
        continue;
      }
      else if (syncedView->viewType() != view->viewType()) {
        continue;
      }

      if (!helper::areViewDirectionsParallel(
            syncedView->camera(),
            backupCamera,
            Directions::View::Back,
            parallelThreshold_degrees))
      {
        continue;
      }

      helper::applyViewRotationAboutWorldPoint(syncedView->camera(), camera_T_world_rotationDelta, worldOrigin);
    }
  }
}

void CallbackHandler::handleSetViewForwardDirection(const uuid& viewUid, const glm::vec3& worldForwardDirection)
{
  auto& windowData = m_appData.windowData();

  View* view = windowData.getView(viewUid);
  if (!view) {
    return;
  }

  if (ViewRenderMode::Disabled == view->renderMode()) {
    return;
  }

  if (ViewType::Oblique != view->viewType()) {
    return;
  }

  const glm::vec3 worldXhairsPos = m_appData.state().worldCrosshairs().worldOrigin();
  helper::setWorldForwardDirection(view->camera(), worldForwardDirection);
  helper::setWorldTarget(view->camera(), worldXhairsPos, std::nullopt);

  if (const auto rotGroupUid = view->cameraRotationSyncGroupUid()) {
    for (const auto& syncedViewUid : windowData.cameraSyncGroupViewUids(CameraSyncMode::Rotation, *rotGroupUid)) {
      if (syncedViewUid == viewUid) {
        continue;
      }

      View* syncedView = windowData.getCurrentView(syncedViewUid);
      if (!syncedView) {
        continue;
      }
      else if (syncedView->viewType() != view->viewType()) {
        continue;
      }

      helper::setWorldForwardDirection(syncedView->camera(), worldForwardDirection);
      helper::setWorldTarget(syncedView->camera(), worldXhairsPos, std::nullopt);
    }
  }
}

void CallbackHandler::doThreeDIsosurfacePick(const ViewHit& hit)
{
  if (!hit.view || ViewType::ThreeD != hit.view->viewType()) {
    return;
  }
  if (hit.view->threeDState().m_viewPositionFollowsCrosshairs) {
    return;
  }
  if (!checkAndSetActiveView(hit.viewUid)) {
    return;
  }

  for (const uuid& imageUid : hit.view->visibleImages()) {
    const Image* image = m_appData.image(imageUid);
    if (!image) {
      continue;
    }

    const std::vector<double> isoValues = pickableIsoValues(m_appData, imageUid, *image);
    if (isoValues.empty()) {
      continue;
    }

    const ImageSettings& settings = image->settings();
    const uint32_t activeComponent = settings.activeComponent();
    const uint32_t activeTimePoint = image->timeAxis().clamp(settings.activeTimePoint());
    const float stepLength =
      std::max(1.0e-5f, m_appData.renderData().m_raycastSamplingFactor * minPositiveSpacing(*image));
    const glm::vec3 worldRayOrigin = helper::world_T_ndc(hit.view->threeDCamera(), glm::vec3{hit.viewClipPos, -1.0f});
    const glm::vec3 worldRayDirection = helper::worldRayDirection(hit.view->threeDCamera(), hit.viewClipPos);

    const auto hitResult = camera3d::pickFirstIsoSurfaceHit(
      {.worldRayOrigin = worldRayOrigin,
       .worldRayDirection = worldRayDirection,
       .pixel_T_world = image->transformations().pixel_T_worldDef(),
       .world_T_pixel = image->transformations().worldDef_T_pixel(),
       .pixelDimensions = glm::vec3{image->header().pixelDimensions()},
       .stepLength = stepLength,
       .renderFrontFaces = m_appData.renderData().m_renderFrontFaces,
       .renderBackFaces = m_appData.renderData().m_renderBackFaces,
       .isoValues = isoValues,
       .sampleValue = [image, activeComponent, activeTimePoint](const glm::vec3& pixelPos) {
         return image->valueLinear<double>(activeComponent, pixelPos.x, pixelPos.y, pixelPos.z, activeTimePoint);
       }});

    if (hitResult) {
      m_appData.state().setWorldCrosshairsPos(hitResult->worldPosition);
      updateThreeDViewsFollowingCrosshairs();
    }
    return;
  }
}

void CallbackHandler::doCameraZoomDrag(
  const ViewHit& startHit,
  const ViewHit& prevHit,
  const ViewHit& currHit,
  const ZoomBehavior& zoomBehavior,
  bool syncZoomForAllViews)
{
  const glm::vec2 ndcCenter{0.0f, 0.0f};

  View* viewToZoom = startHit.view;
  if (!viewToZoom) {
    return;
  }

  const auto& viewUidToZoom = startHit.viewUid;

  auto getCenterViewClipPos = [this, &zoomBehavior, &startHit, &ndcCenter](const View* view) -> glm::vec2 {
    glm::vec2 viewClipCenterPos{0.0f};

    switch (zoomBehavior) {
      case ZoomBehavior::ToCrosshairs: {
        viewClipCenterPos = helper::ndc_T_world(view->camera(), m_appData.state().worldCrosshairs().worldOrigin());
        break;
      }
      case ZoomBehavior::ToStartPosition: {
        const glm::vec4 _viewClipStartPos = helper::clip_T_world(view->camera()) * startHit.worldPos;
        viewClipCenterPos = glm::vec2{_viewClipStartPos / _viewClipStartPos.w};
        break;
      }
      case ZoomBehavior::ToViewCenter: {
        viewClipCenterPos = ndcCenter;
        break;
      }
    }

    return viewClipCenterPos;
  };

  const float factor = 2.0f * (currHit.windowClipPos.y - prevHit.windowClipPos.y) / 2.0f + 1.0f;
  helper::zoomNdc(viewToZoom->camera(), factor, getCenterViewClipPos(viewToZoom));

  if (syncZoomForAllViews) {
    // Apply zoom to all other views:
    for (const auto& otherViewUid : m_appData.windowData().currentViewUids()) {
      if (otherViewUid == viewUidToZoom) {
        continue;
      }

      if (View* otherView = m_appData.windowData().getCurrentView(otherViewUid);
          otherView && ViewType::ThreeD != otherView->viewType())
      {
        helper::zoomNdc(otherView->camera(), factor, getCenterViewClipPos(otherView));
      }
    }
  }
  else if (const auto zoomGroupUid = viewToZoom->cameraZoomSyncGroupUid()) {
    // Apply zoom to all views other synchronized with the view:
    for (const auto& syncedViewUid :
         m_appData.windowData().cameraSyncGroupViewUids(CameraSyncMode::Zoom, *zoomGroupUid))
    {
      if (syncedViewUid == viewUidToZoom) {
        continue;
      }

      if (View* syncedView = m_appData.windowData().getCurrentView(syncedViewUid);
          syncedView && ViewType::ThreeD != syncedView->viewType())
      {
        helper::zoomNdc(syncedView->camera(), factor, getCenterViewClipPos(syncedView));
      }
    }
  }
}

void CallbackHandler::doCameraZoomScroll(
  const ViewHit& hit,
  const glm::vec2& scrollOffset,
  const ZoomBehavior& zoomBehavior,
  bool syncZoomForAllViews)
{
  constexpr float zoomFactor = 0.01f;
  const glm::vec2 ndcCenter{0.0f, 0.0f};

  if (!hit.view) {
    return;
  }

  // The pointer is in the view bounds! Make this the active view
  m_appData.windowData().setActiveViewUid(hit.viewUid);

  auto getCenterViewClipPos = [this, &zoomBehavior, &hit, &ndcCenter](const View* view) -> glm::vec2 {
    glm::vec2 viewClipCenterPos{0.0f};

    switch (zoomBehavior) {
      case ZoomBehavior::ToCrosshairs: {
        viewClipCenterPos = helper::ndc_T_world(view->camera(), m_appData.state().worldCrosshairs().worldOrigin());
        break;
      }
      case ZoomBehavior::ToStartPosition: {
        const glm::vec4 _viewClipCurrPos = helper::clip_T_world(view->camera()) * hit.worldPos;
        viewClipCenterPos = glm::vec2{_viewClipCurrPos / _viewClipCurrPos.w};
        break;
      }
      case ZoomBehavior::ToViewCenter: {
        viewClipCenterPos = ndcCenter;
        break;
      }
    }

    return viewClipCenterPos;
  };

  const float factor = 1.0f + zoomFactor * scrollOffset.y;

  helper::zoomNdc(hit.view->camera(), factor, getCenterViewClipPos(hit.view));

  if (syncZoomForAllViews) {
    // Apply zoom to all other views:
    for (const auto& otherViewUid : m_appData.windowData().currentViewUids()) {
      if (otherViewUid == hit.viewUid) {
        continue;
      }

      if (View* otherView = m_appData.windowData().getCurrentView(otherViewUid);
          otherView && ViewType::ThreeD != otherView->viewType())
      {
        helper::zoomNdc(otherView->camera(), factor, getCenterViewClipPos(otherView));
      }
    }
  }
  else if (const auto zoomGroupUid = hit.view->cameraZoomSyncGroupUid()) {
    // Apply zoom all other views synchronized with this view:
    for (const auto& syncedViewUid :
         m_appData.windowData().cameraSyncGroupViewUids(CameraSyncMode::Zoom, *zoomGroupUid))
    {
      if (syncedViewUid == hit.viewUid) {
        continue;
      }

      if (View* syncedView = m_appData.windowData().getCurrentView(syncedViewUid);
          syncedView && ViewType::ThreeD != syncedView->viewType())
      {
        helper::zoomNdc(syncedView->camera(), factor, getCenterViewClipPos(syncedView));
      }
    }
  }
}

void CallbackHandler::scrollViewSlice(const ViewHit& hit, int numSlices)
{
  const float scrollDistance =
    data::sliceScrollDistance(m_appData, hit.worldFrontAxis, ImageSelection::VisibleImagesInView, hit.view);

  m_appData.state().setWorldCrosshairsPos(
    m_appData.state().worldCrosshairs().worldOrigin() +
    static_cast<float>(numSlices) * scrollDistance * hit.worldFrontAxis);
  updateThreeDViewsFollowingCrosshairs();
}

void CallbackHandler::doImageTranslate(
  const ViewHit& startHit,
  const ViewHit& prevHit,
  const ViewHit& currHit,
  bool inPlane)
{
  View* viewToUse = startHit.view;

  const auto activeImageUid = m_appData.activeImageUid();
  if (!activeImageUid) {
    return;
  }

  if (0 == std::count(std::begin(viewToUse->visibleImages()), std::end(viewToUse->visibleImages()), *activeImageUid)) {
    return; // The active image is not visible
  }

  Image* activeImage = m_appData.image(*activeImageUid);
  if (!activeImage) {
    return;
  }

  glm::vec3 T{0.0f, 0.0f, 0.0f};

  if (inPlane) {
    // Translate the image along the view plane
    static const float ndcZ = -1.0f;
    T = helper::translationInCameraPlane(viewToUse->camera(), prevHit.viewClipPos, currHit.viewClipPos, ndcZ);

    // Note: for 3D in-plane translation, we'll want to use this instead:
    // helper::ndcZofWorldPoint( view->camera(), imgTx.getWorldSubjectOrigin() );
  }
  else {
    // Translate the image in and out of the view plane. Translate by an amount
    // proportional to the slice distance of the active image (the one being translated)
    const float scrollDistance = data::sliceScrollDistance(startHit.worldFrontAxis, *activeImage);

    T = helper::translationAboutCameraFrontBack(
      viewToUse->camera(),
      prevHit.viewClipPos,
      currHit.viewClipPos,
      imageFrontBackTranslationScaleFactor * scrollDistance);
  }

  auto& imgTx = activeImage->transformations();
  imgTx.set_worldDef_T_affine_translation(imgTx.get_worldDef_T_affine_translation() + T);

  // Apply same transformation to the segmentations:
  for (const auto segUid : m_appData.imageToSegUids(*activeImageUid)) {
    if (auto* seg = m_appData.seg(segUid)) {
      auto& segTx = seg->transformations();
      segTx.set_worldDef_T_affine_translation(segTx.get_worldDef_T_affine_translation() + T);
    }
  }

  m_rendering.updateImageUniforms(*activeImageUid);
}

void CallbackHandler::doImageRotate(
  const ViewHit& startHit,
  const ViewHit& prevHit,
  const ViewHit& currHit,
  bool inPlane)
{
  View* viewToUse = startHit.view;
  if (!viewToUse) {
    return;
  }

  const auto activeImageUid = m_appData.activeImageUid();
  if (!activeImageUid) {
    return;
  }

  // Forbid transformation if the view does NOT show the active image
  if (0 == std::count(std::begin(viewToUse->visibleImages()), std::end(viewToUse->visibleImages()), *activeImageUid)) {
    return; // The active image is not visible
  }

  Image* activeImage = m_appData.image(*activeImageUid);
  if (!activeImage) {
    return;
  }

  const glm::vec3 worldRotCenter = m_appData.state().worldRotationCenter();
  auto& imgTx = activeImage->transformations();

  CoordinateFrame imageFrame(imgTx.get_worldDef_T_affine_translation(), imgTx.get_worldDef_T_affine_rotation());

  glm::quat R;
  if (inPlane) {
    const glm::vec2 ndcRotCenter = helper::ndc_T_world(viewToUse->camera(), worldRotCenter);
    R = helper::rotation2dInCameraPlane(viewToUse->camera(), prevHit.viewClipPos, currHit.viewClipPos, ndcRotCenter);
  }
  else {
    R = helper::rotation3dAboutCameraPlane(viewToUse->camera(), prevHit.viewClipPos, currHit.viewClipPos);
  }

  math::rotateFrameAboutWorldPos(imageFrame, R, worldRotCenter);

  imgTx.set_worldDef_T_affine_translation(imageFrame.worldOrigin());
  imgTx.set_worldDef_T_affine_rotation(imageFrame.world_T_frame_rotation());

  // Apply same transformation to the segmentations:
  for (const auto segUid : m_appData.imageToSegUids(*activeImageUid)) {
    if (auto* seg = m_appData.seg(segUid)) {
      auto& segTx = seg->transformations();
      segTx.set_worldDef_T_affine_translation(imageFrame.worldOrigin());
      segTx.set_worldDef_T_affine_rotation(imageFrame.world_T_frame_rotation());
    }
  }

  m_rendering.updateImageUniforms(*activeImageUid);
}

void CallbackHandler::doImageScale(
  const ViewHit& startHit,
  const ViewHit& prevHit,
  const ViewHit& currHit,
  entropy::app::ImageScaleConstraint constraint)
{
  View* viewToUse = startHit.view;
  if (!viewToUse) {
    return;
  }

  const auto activeImageUid = m_appData.activeImageUid();
  if (!activeImageUid) {
    return;
  }

  if (0 == std::count(std::begin(viewToUse->visibleImages()), std::end(viewToUse->visibleImages()), *activeImageUid)) {
    // The active image is not visible
    return;
  }

  Image* activeImage = m_appData.image(*activeImageUid);
  if (!activeImage) {
    return;
  }

  auto& imgTx = activeImage->transformations();
  const auto scaleUpdate = entropy::app::computeImageScaleUpdate(
    imgTx.get_worldDef_T_affine(),
    imgTx.get_worldDef_T_affine_scale(),
    m_appData.state().worldRotationCenter(),
    glm::vec3{prevHit.worldPos},
    glm::vec3{currHit.worldPos},
    helper::worldDirection(viewToUse->camera(), Directions::View::Right),
    helper::worldDirection(viewToUse->camera(), Directions::View::Up),
    helper::worldDirection(viewToUse->camera(), Directions::View::Front),
    constraint);

  if (!scaleUpdate) {
    return;
  }

  imgTx.set_worldDef_T_affine_scale(scaleUpdate->m_scale);
  imgTx.set_worldDef_T_affine_translation(scaleUpdate->m_translation);

  // Apply same transformation to the segmentations:
  for (const auto segUid : m_appData.imageToSegUids(*activeImageUid)) {
    if (auto* seg = m_appData.seg(segUid)) {
      auto& segTx = seg->transformations();
      segTx.set_worldDef_T_affine_scale(scaleUpdate->m_scale);
      segTx.set_worldDef_T_affine_translation(scaleUpdate->m_translation);
    }
  }

  m_rendering.updateImageUniforms(*activeImageUid);
}

void CallbackHandler::flipImageInterpolation()
{
  const auto imgUid = m_appData.activeImageUid();
  if (!imgUid) {
    return;
  }

  Image* image = m_appData.image(*imgUid);
  if (!image) {
    return;
  }

  const InterpolationMode newMode = (InterpolationMode::NearestNeighbor == image->settings().interpolationMode())
                                      ? InterpolationMode::Linear
                                      : InterpolationMode::NearestNeighbor;

  image->settings().setInterpolationMode(newMode);
  m_rendering.updateImageInterpolation(*imgUid);
}

void CallbackHandler::toggleImageVisibility()
{
  const auto imageUid = m_appData.activeImageUid();
  if (!imageUid) {
    return;
  }

  // Image* image = m_appData.image(*imageUid);
  // if (!image)
  //   return;

  const auto result = m_appData.getImage(*imageUid);
  if (!result) {
    spdlog::warn("{}", result.error());
    return;
  }

  Image& image = result->get();

  const bool isMulticomponentImage = image.header().numComponentsPerPixel() > 1;

  if (isMulticomponentImage) {
    image.settings().setGlobalVisibility(!image.settings().globalVisibility());
  }
  else {
    // Otherwise, toggle visibility of the active component only:
    image.settings().setVisibility(!image.settings().visibility());
  }

  m_rendering.updateImageUniforms(*imageUid);
}

void CallbackHandler::toggleImageEdges()
{
  const auto imageUid = m_appData.activeImageUid();
  if (!imageUid) {
    return;
  }

  Image* image = m_appData.image(*imageUid);
  if (!image) {
    return;
  }

  image->settings().setShowAnyEdges(!image->settings().showAnyEdges());
  m_rendering.updateImageUniforms(*imageUid);
}

void CallbackHandler::changeImageOpacity(double delta)
{
  const auto imageUid = m_appData.activeImageUid();
  if (!imageUid) {
    return;
  }

  Image* image = m_appData.image(*imageUid);
  if (!image) {
    return;
  }

  const double opacity = image->settings().opacity();
  image->settings().setOpacity(std::clamp(opacity + delta, 0.0, 1.0));
  m_rendering.updateImageUniforms(*imageUid);
}

void CallbackHandler::changeSegOpacity(double delta, bool interior)
{
  if (interior) {
    const float op = m_appData.renderData().m_segInteriorOpacity;
    m_appData.renderData().m_segInteriorOpacity = std::clamp(op + static_cast<float>(delta), 0.0f, 1.0f);
  }
  else {
    const auto imgUid = m_appData.activeImageUid();
    if (!imgUid) {
      return;
    }

    const auto segUid = m_appData.imageToActiveSegUid(*imgUid);
    if (!segUid) {
      return;
    }

    Image* seg = m_appData.seg(*segUid);
    if (!seg) {
      return;
    }

    const double op = seg->settings().opacity();
    seg->settings().setOpacity(std::clamp(op + delta, 0.0, 1.0));

    // Update all image uniforms, since the segmentation may be shared by more than one image:
    m_rendering.updateImageUniforms(m_appData.imageUidsOrdered());
  }
}

void CallbackHandler::toggleSegVisibility()
{
  const auto imgUid = m_appData.activeImageUid();
  if (!imgUid) {
    return;
  }

  const auto segUid = m_appData.imageToActiveSegUid(*imgUid);
  if (!segUid) {
    return;
  }

  Image* seg = m_appData.seg(*segUid);
  if (!seg) {
    return;
  }

  const bool vis = seg->settings().visibility();
  seg->settings().setVisibility(!vis);

  // Update all image uniforms, since the segmentation may be shared by more than one image:
  m_rendering.updateImageUniforms(m_appData.imageUidsOrdered());
}

void CallbackHandler::toggleSegGlobalOutline()
{
  switch (m_appData.renderData().m_segOutlineStyle) {
    case SegmentationOutlineStyle::Disabled: {
      m_appData.renderData().m_segOutlineStyle = SegmentationOutlineStyle::ViewPixel;
      break;
    }
    case SegmentationOutlineStyle::ViewPixel: {
      m_appData.renderData().m_segOutlineStyle = SegmentationOutlineStyle::Disabled;
      break;
    }
    case SegmentationOutlineStyle::ImageVoxel: {
      m_appData.renderData().m_segOutlineStyle = SegmentationOutlineStyle::Disabled;
      break;
    }
  }
}

void CallbackHandler::cyclePrevLayout()
{
  m_appData.windowData().cycleCurrentLayout(-1);
}

void CallbackHandler::cycleNextLayout()
{
  m_appData.windowData().cycleCurrentLayout(1);
}

void CallbackHandler::cycleOverlayAndUiVisibility()
{
  cycleViewOverlays();
}

void CallbackHandler::cycleImageComponent(int i)
{
  const auto imageUid = m_appData.activeImageUid();
  if (!imageUid) {
    return;
  }

  Image* image = m_appData.image(*imageUid);
  if (!image) {
    return;
  }

  const int N = static_cast<int>(image->settings().numComponents());
  const int c = static_cast<int>(image->settings().activeComponent());

  image->settings().setActiveComponent(static_cast<uint32_t>((N + c + i) % N));
}

void CallbackHandler::cycleActiveImage(int i)
{
  const auto imageUid = m_appData.activeImageUid();
  if (!imageUid) {
    return;
  }

  const auto imageIndex = m_appData.imageIndex(*imageUid);
  if (!imageIndex) {
    return;
  }

  const int N = static_cast<int>(m_appData.numImages());
  const int idx = static_cast<int>(*imageIndex);

  const std::size_t newImageIndex = static_cast<size_t>((N + idx + i) % N);

  const auto newImageUid = m_appData.imageUid(newImageIndex);
  if (!newImageUid) {
    return;
  }

  m_appData.setActiveImageUid(*newImageUid);
}

void CallbackHandler::cycleTimePoint(int i)
{
  auto [targetUid, targetImage] = targetTimeSeriesImage(m_appData);

  if (!targetUid || !targetImage) {
    return;
  }

  const int numTimePoints = static_cast<int>(targetImage->timeAxis().numTimePoints());
  const int activeTimePoint = static_cast<int>(targetImage->settings().activeTimePoint());
  const uint32_t requestedTimePoint = static_cast<uint32_t>((numTimePoints + activeTimePoint + i) % numTimePoints);

  setTimePointWithSynchronization(m_appData, *targetUid, *targetImage, requestedTimePoint);
}

void CallbackHandler::toggleTimePlayback()
{
  auto [targetUid, targetImage] = targetTimeSeriesImage(m_appData);
  if (!targetUid || !targetImage) {
    return;
  }

  const bool playing = !targetImage->settings().timePlaybackPlaying();
  targetImage->settings().setTimePlaybackPlaying(playing);
  m_appData.state().setAnimating(playing || anyTimeSeriesPlaybackRunning(m_appData));
}

void CallbackHandler::setTimePointToFirst()
{
  auto [targetUid, targetImage] = targetTimeSeriesImage(m_appData);
  if (!targetUid || !targetImage) {
    return;
  }
  setTimePointWithSynchronization(m_appData, *targetUid, *targetImage, 0u);
}

void CallbackHandler::setTimePointToLast()
{
  auto [targetUid, targetImage] = targetTimeSeriesImage(m_appData);
  if (!targetUid || !targetImage) {
    return;
  }
  setTimePointWithSynchronization(m_appData, *targetUid, *targetImage, targetImage->timeAxis().numTimePoints() - 1u);
}

/// @todo Put into DataHelper
void CallbackHandler::cycleForegroundSegLabel(int i)
{
  constexpr LabelType minLabel = 0;

  LabelType label = static_cast<LabelType>(m_appData.settings().foregroundLabel());
  label = std::max(label + i, minLabel);

  if (const auto* table = m_appData.activeLabelTable()) {
    m_appData.settings().setForegroundLabel(static_cast<size_t>(label), *table);
  }
}

/// @todo Put into DataHelper
void CallbackHandler::cycleBackgroundSegLabel(int i)
{
  constexpr LabelType minLabel = 0;

  LabelType label = static_cast<LabelType>(m_appData.settings().backgroundLabel());
  label = std::max(label + i, minLabel);

  if (const auto* table = m_appData.activeLabelTable()) {
    m_appData.settings().setBackgroundLabel(static_cast<size_t>(label), *table);
  }
}

/// @todo Put into DataHelper
void CallbackHandler::cycleBrushSize(int i)
{
  int brushSizeVox = static_cast<int>(m_appData.settings().brushSizeInVoxels());
  brushSizeVox = std::max(brushSizeVox + i, 1);
  m_appData.settings().setBrushSizeInVoxels(static_cast<uint32_t>(brushSizeVox));
}

bool CallbackHandler::showOverlays() const
{
  return m_appData.settings().overlays();
}

/// @todo Put into DataHelper
void CallbackHandler::setShowOverlays(bool show)
{
  m_appData.settings().setOverlays(show); // this holds the data
  m_rendering.setShowVectorOverlays(show);
  m_appData.guiData().m_renderUiOverlays = show;
}

bool CallbackHandler::showUserInterface() const
{
  return m_appData.guiData().m_renderUiWindows && m_appData.guiData().m_renderUiOverlays;
}

void CallbackHandler::setShowUserInterface(bool show)
{
  m_appData.guiData().m_renderUiWindows = show;
  m_appData.guiData().m_renderUiOverlays = show;
}

void CallbackHandler::toggleCrosshairs()
{
  auto& R = m_appData.renderData();
  R.m_showCrosshairs = !R.m_showCrosshairs;
  R.m_showCrosshairsInLightboxViews = R.m_showCrosshairs;
}

void CallbackHandler::cycleViewOverlays()
{
  enum class OverlayState
  {
    All,
    CrosshairsOnly,
    None,
    Mixed
  };

  auto& R = m_appData.renderData();

  const bool anyOverlay = R.m_showCrosshairs || R.m_showAnatomicalLabels || R.m_showScaleBars ||
                          R.m_showLightboxOffsetLabels || R.m_showThreeDCameraFrustumIn2DViews;
  const bool crosshairsOnly = R.m_showCrosshairs && !R.m_showAnatomicalLabels && !R.m_showScaleBars &&
                              !R.m_showLightboxOffsetLabels && !R.m_showThreeDCameraFrustumIn2DViews;
  const bool allOverlays = R.m_showCrosshairs && R.m_showAnatomicalLabels && R.m_showScaleBars &&
                           R.m_showLightboxOffsetLabels && R.m_showThreeDCameraFrustumIn2DViews;

  const OverlayState state = !anyOverlay      ? OverlayState::None
                             : crosshairsOnly ? OverlayState::CrosshairsOnly
                             : allOverlays    ? OverlayState::All
                                              : OverlayState::Mixed;

  const auto setAll = [&R](bool show) {
    R.m_showCrosshairs = show;
    R.m_showCrosshairsInLightboxViews = show;
    R.m_showAnatomicalLabels = show;
    R.m_showAnatomicalLabelsInLightboxViews = show;
    R.m_showScaleBars = show;
    R.m_showScaleBarsInLightboxViews = show;
    R.m_showLightboxOffsetLabels = show;
    R.m_showThreeDCameraFrustumIn2DViews = show;
  };

  switch (state) {
    case OverlayState::All:
      R.m_showCrosshairs = true;
      R.m_showCrosshairsInLightboxViews = true;
      R.m_showAnatomicalLabels = false;
      R.m_showAnatomicalLabelsInLightboxViews = false;
      R.m_showScaleBars = false;
      R.m_showScaleBarsInLightboxViews = false;
      R.m_showLightboxOffsetLabels = false;
      R.m_showThreeDCameraFrustumIn2DViews = false;
      break;
    case OverlayState::CrosshairsOnly:
      setAll(false);
      break;
    case OverlayState::None:
    case OverlayState::Mixed:
      setAll(true);
      break;
  }
}

void CallbackHandler::moveCrosshairsOnViewSlice(const ViewHit& hit, int stepX, int stepY)
{
  if (!hit.view) {
    return;
  }

  const glm::vec3 worldRightAxis = helper::worldDirection(hit.view->camera(), Directions::View::Right);
  const glm::vec3 worldUpAxis = helper::worldDirection(hit.view->camera(), Directions::View::Up);
  const glm::vec2 moveDistances =
    data::sliceMoveDistance(m_appData, worldRightAxis, worldUpAxis, ImageSelection::VisibleImagesInView, hit.view);
  const glm::vec3 worldCrosshairs = m_appData.state().worldCrosshairs().worldOrigin();

  m_appData.state().setWorldCrosshairsPos(
    worldCrosshairs + static_cast<float>(stepX) * moveDistances.x * worldRightAxis +
    static_cast<float>(stepY) * moveDistances.y * worldUpAxis);
  updateThreeDViewsFollowingCrosshairs();
}

void CallbackHandler::doCrosshairsRotate2D(
  const ViewHit& startHit,
  const ViewHit& prevHit,
  const ViewHit& currHit,
  bool snapCrosshairs)
{
  View* viewToUse = startHit.view;
  if (!viewToUse) {
    return;
  }

  if (ViewType::Oblique == viewToUse->viewType() || ViewType::ThreeD == viewToUse->viewType()) {
    return; // Do not rotate crosshairs in Oblique or 3D view
  }

  AppState& state = m_appData.state();

  if (!state.viewWithRotatingCrosshairs()) {
    // Not in a rotating state, so transition to rotating state. This is done by
    // setting this view as the one rotating crosshairs.
    state.setViewWithRotatingCrosshairs(startHit.viewUid);
    // m_appData.saveAllViewWorldCenterPositions();
  }

  // Rotate the crosshairs frame in the 2D view plane about the crosshairs position
  const glm::vec3 worldRotCenter = state.worldCrosshairs().worldOrigin();
  const glm::vec2 ndcRotCenter = helper::ndc_T_world(viewToUse->camera(), worldRotCenter);

  if (!snapCrosshairs) {
    // Incremental rotation between previous and current hits:
    const glm::quat R =
      helper::rotation2dInCameraPlane(viewToUse->camera(), prevHit.viewClipPos, currHit.viewClipPos, ndcRotCenter);

    // Rotate the current crosshairs by the incremental amount:
    CoordinateFrame worldXhairsRotated = state.worldCrosshairs();
    math::rotateFrameAboutWorldPos(worldXhairsRotated, R, worldRotCenter);

    // Set new crosshairs (used by all other views except the one in which rotation is being done):
    // m_appData.saveAllViewWorldCenterPositions();
    state.setWorldCrosshairs(worldXhairsRotated);
    updateThreeDViewsFollowingCrosshairs();
    // m_appData.restoreAllViewWorldCenterPositions();
  }
  else {
    constexpr float snapAngleDegrees = 15.0f;
    constexpr float clampToleranceDegrees = snapAngleDegrees / 2.0f;

    // Total rotation between start and current hits:
    const glm::quat R = helper::rotation2dInCameraPlaneWithSnapping(
      viewToUse->camera(),
      startHit.viewClipPos,
      prevHit.viewClipPos,
      currHit.viewClipPos,
      snapAngleDegrees,
      clampToleranceDegrees,
      ndcRotCenter);

    // Rotate the old crosshairs by the total amount:
    CoordinateFrame worldXhairsOldRotated = state.crosshairsState().worldCrosshairsOld;
    math::rotateFrameAboutWorldPos(worldXhairsOldRotated, R, worldRotCenter);

    // Set new crosshairs (used by all other views except the one in which rotation is being done):
    // m_appData.saveAllViewWorldCenterPositions();
    state.setWorldCrosshairs(worldXhairsOldRotated);
    updateThreeDViewsFollowingCrosshairs();
    // m_appData.restoreAllViewWorldCenterPositions();
  }

  // Recenter all views on the crosshairs except the current view in which crosshairs are rotating
  recenterViews(m_appData.state().recenteringMode(), false, false, true, false, false, {startHit.viewUid});
}

void CallbackHandler::endCrosshairsRotate2D()
{
  AppState& state = m_appData.state();
  if (state.viewWithRotatingCrosshairs()) {
    state.setViewWithRotatingCrosshairs(std::nullopt);
    recenterViews(state.recenteringMode(), false, false, true, false, false);
  }
}

void CallbackHandler::moveCrosshairsToSegLabelCentroid(const uuid& imageUid, std::size_t labelIndex)
{
  constexpr uint32_t comp0 = 0;

  const auto activeSegUid = m_appData.imageToActiveSegUid(imageUid);
  if (!activeSegUid) {
    return;
  }

  Image* seg = m_appData.seg(*activeSegUid);
  if (!seg) {
    return;
  }

  const void* data = seg->bufferAsVoid(comp0);
  const glm::ivec3 dims{seg->header().pixelDimensions()};
  const LabelType label = static_cast<LabelType>(labelIndex);

  std::optional<glm::vec3> pixelCentroid = std::nullopt;

  switch (seg->header().memoryComponentType()) {
    case ComponentType::Int8:
      pixelCentroid = computePixelCentroid<int8_t>(data, dims, label);
      break;
    case ComponentType::UInt8:
      pixelCentroid = computePixelCentroid<uint8_t>(data, dims, label);
      break;
    case ComponentType::Int16:
      pixelCentroid = computePixelCentroid<int16_t>(data, dims, label);
      break;
    case ComponentType::UInt16:
      pixelCentroid = computePixelCentroid<uint16_t>(data, dims, label);
      break;
    case ComponentType::Int32:
      pixelCentroid = computePixelCentroid<int32_t>(data, dims, label);
      break;
    case ComponentType::UInt32:
      pixelCentroid = computePixelCentroid<uint32_t>(data, dims, label);
      break;
    case ComponentType::Float32:
      pixelCentroid = computePixelCentroid<float>(data, dims, label);
      break;
    default:
      pixelCentroid = std::nullopt;
      break;
  }

  if (!pixelCentroid) {
    return;
  }

  const glm::vec4 worldCentroid = seg->transformations().worldDef_T_pixel() * glm::vec4{*pixelCentroid, 1.0f};
  glm::vec3 worldPos{worldCentroid / worldCentroid.w};

  worldPos = data::snapWorldPointToImageVoxels(m_appData, worldPos);
  m_appData.state().setWorldCrosshairsPos(worldPos);
  updateThreeDViewsFollowingCrosshairs();
}

void CallbackHandler::setMouseMode(MouseMode mode)
{
  if (MouseMode::CrosshairsRotate == m_appData.state().mouseMode() && MouseMode::CrosshairsRotate != mode) {
    // Transition out of crosshairs rotation mode:
    endCrosshairsRotate2D();
  }

  m_appData.state().setMouseMode(mode);

  // return m_glfw.cursor( mode );
}

void CallbackHandler::toggleFullScreenMode(bool forceWindowMode)
{
  m_glfw.toggleFullScreenMode(forceWindowMode);
}

bool CallbackHandler::setLockManualImageTransformation(const uuid& imageUid, bool locked)
{
  Image* image = m_appData.image(imageUid);
  if (!image) {
    return false;
  }

  image->transformations().set_worldDef_T_affine_locked(locked);

  // Lock/unlock all of the image's segmentations:
  for (const auto segUid : m_appData.imageToSegUids(imageUid)) {
    if (auto* seg = m_appData.seg(segUid)) {
      seg->transformations().set_worldDef_T_affine_locked(locked);
    }
  }

  return true;
}

bool CallbackHandler::syncManualImageTransformation(const uuid& refImageUid, const uuid& otherImageUid)
{
  const Image* refImage = m_appData.image(refImageUid);
  if (!refImage) {
    return false;
  }

  Image* otherImage = m_appData.image(otherImageUid);
  if (!otherImage) {
    return false;
  }

  otherImage->transformations().set_worldDef_T_affine_locked(refImage->transformations().is_worldDef_T_affine_locked());
  otherImage->transformations().set_worldDef_T_affine_scale(refImage->transformations().get_worldDef_T_affine_scale());
  otherImage->transformations().set_worldDef_T_affine_rotation(
    refImage->transformations().get_worldDef_T_affine_rotation());
  otherImage->transformations().set_worldDef_T_affine_translation(
    refImage->transformations().get_worldDef_T_affine_translation());
  return true;
}

bool CallbackHandler::syncManualImageTransformationOnSegs(const uuid& imageUid)
{
  const Image* image = m_appData.image(imageUid);
  if (!image) {
    return false;
  }

  for (const auto segUid : m_appData.imageToSegUids(imageUid)) {
    if (auto* seg = m_appData.seg(segUid)) {
      seg->transformations().set_worldDef_T_affine_locked(image->transformations().is_worldDef_T_affine_locked());
      seg->transformations().set_worldDef_T_affine_scale(image->transformations().get_worldDef_T_affine_scale());
      seg->transformations().set_worldDef_T_affine_rotation(image->transformations().get_worldDef_T_affine_rotation());
      seg->transformations().set_worldDef_T_affine_translation(
        image->transformations().get_worldDef_T_affine_translation());
    }
  }

  return true;
}

bool CallbackHandler::checkAndSetActiveView(const uuid& viewUid)
{
  if (const auto activeViewUid = m_appData.windowData().activeViewUid()) {
    if (*activeViewUid != viewUid) {
      return false; // There is an active view is not this view
    }
  }

  // There is no active view, so set this to be the active view:
  m_appData.windowData().setActiveViewUid(viewUid);
  return true;
}

void CallbackHandler::updateThreeDViewsFollowingCrosshairs()
{
  const glm::vec3 crosshairs = m_appData.state().worldCrosshairs().worldOrigin();
  for (const auto& viewUid : m_appData.windowData().currentViewUids()) {
    View* view = m_appData.windowData().getCurrentView(viewUid);
    if (!view || ViewType::ThreeD != view->viewType() || !view->threeDState().m_viewPositionFollowsCrosshairs) {
      continue;
    }
    view->threeDState().m_crosshairsFollowOffset = glm::vec3{0.0f};
    camera3d::followCrosshairs(view->threeDCamera(), view->threeDState(), crosshairs);
  }
}
