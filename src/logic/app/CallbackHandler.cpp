#include "logic/app/CallbackHandler.h"

#include "common/DataHelper.h"
#include "common/MathFuncs.h"
#include "common/SegmentationTypes.h"
#include "common/Types.h"

#include "image/SegUtil.h"

#include "logic/app/Data.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/camera/MathUtility.h"

#include "logic/segmentation/GraphCuts.h"
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

namespace
{

static constexpr float sk_viewAABBoxScaleFactor = 1.10f;

// Angle threshold (in degrees) for checking whether two vectors are parallel
static constexpr float sk_parallelThreshold_degrees = 0.1f;

static constexpr float sk_imageFrontBackTranslationScaleFactor = 10.0f;

} // namespace

CallbackHandler::CallbackHandler(AppData& appData, GlfwWrapper& glfwWrapper, Rendering& rendering)
  : m_appData(appData)
  , m_glfw(glfwWrapper)
  , m_rendering(rendering)
{
}

bool CallbackHandler::clearSegVoxels(const uuids::uuid& segUid)
{
  Image* seg = m_appData.seg(segUid);
  if (!seg)
    return false;

  seg->setAllValues(0);

  const glm::uvec3 dataOffset = glm::uvec3{0};
  const glm::uvec3 dataSize = glm::uvec3{seg->header().pixelDimensions()};

  m_rendering.updateSegTexture(
    segUid, seg->header().memoryComponentType(), dataOffset, dataSize, seg->bufferAsVoid(0)
  );

  return true;
}

std::optional<uuids::uuid> CallbackHandler::createBlankImageAndTexture(
  const uuids::uuid& matchImageUid,
  const ComponentType& componentType,
  uint32_t numComponents,
  const std::string& displayName,
  bool createSegmentation
)
{
  const Image* matchImg = m_appData.image(matchImageUid);

  if (!matchImg)
  {
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

  switch (componentType)
  {
  case ComponentType::Int8:
  {
    buffer_int8.resize(newHeader.numPixels(), 0);
    buffer = static_cast<const void*>(buffer_int8.data());
    break;
  }
  case ComponentType::UInt8:
  {
    buffer_uint8.resize(newHeader.numPixels(), 0u);
    buffer = static_cast<const void*>(buffer_uint8.data());
    break;
  }
  case ComponentType::Int16:
  {
    buffer_int16.resize(newHeader.numPixels(), 0);
    buffer = static_cast<const void*>(buffer_int16.data());
    break;
  }
  case ComponentType::UInt16:
  {
    buffer_uint16.resize(newHeader.numPixels(), 0u);
    buffer = static_cast<const void*>(buffer_uint16.data());
    break;
  }
  case ComponentType::Int32:
  {
    buffer_int32.resize(newHeader.numPixels(), 0);
    buffer = static_cast<const void*>(buffer_int32.data());
    break;
  }
  case ComponentType::UInt32:
  {
    buffer_uint32.resize(newHeader.numPixels(), 0u);
    buffer = static_cast<const void*>(buffer_uint32.data());
    break;
  }
  case ComponentType::Float32:
  {
    buffer_float.resize(newHeader.numPixels(), 0.0f);
    buffer = static_cast<const void*>(buffer_float.data());
    break;
  }
  default:
  {
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
    imageComponents
  );

  image.setHeaderOverrides(matchImg->getHeaderOverrides());

  // Assign the matching image's affine_T_subject transformation to the new image:
  image.transformations().set_affine_T_subject(matchImg->transformations().get_affine_T_subject());

  const uuids::uuid imageUid = m_appData.addImage(std::move(image));

  spdlog::trace("Creating texture for image {}", imageUid);

  const std::vector<uuids::uuid> createdImageTextureUids
    = createImageTextures(m_appData, std::vector<uuids::uuid>{imageUid});

  if (createdImageTextureUids.empty())
  {
    spdlog::error("Unable to create texture for image {}", imageUid);

    // m_data.removeImage( imageUid ); //!< @todo Need to implement this
    return std::nullopt;
  }

  // Synchronize transformation with matching image
  syncManualImageTransformation(matchImageUid, imageUid);

  spdlog::info("Created blank image {} matching header of image {}", imageUid, matchImageUid);
  spdlog::debug("Header:\n{}", image.header());
  spdlog::debug("Transformation:\n{}", image.transformations());

  if (createSegmentation)
  {
    const std::string segDisplayName = std::string("Untitled segmentation for image '")
                                       + image.settings().displayName() + "'";

    createBlankSegWithColorTableAndTextures(imageUid, segDisplayName);
  }

  // Update uniforms for all images
  m_rendering.updateImageUniforms(m_appData.imageUidsOrdered());

  // Reassign rainbow colors
  m_appData.setRainbowColorsForAllImages();

  return imageUid;
}

std::optional<uuids::uuid> CallbackHandler::createBlankSeg(
  const uuids::uuid& matchImageUid, const std::string& displayName
)
{
  const Image* matchImg = m_appData.image(matchImageUid);

  if (!matchImg)
  {
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
    imageData
  );

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

std::optional<uuids::uuid> CallbackHandler::createBlankSegWithColorTableAndTextures(
  const uuids::uuid& matchImageUid, const std::string& displayName
)
{
  spdlog::info(
    "Creating blank segmentation {} with color table for image {}", displayName, matchImageUid
  );

  const Image* matchImage = m_appData.image(matchImageUid);
  if (!matchImage)
  {
    spdlog::error("Cannot create blank segmentation for invalid image {}", matchImageUid);
    return std::nullopt;
  }

  auto segUid = createBlankSeg(matchImageUid, displayName);
  if (!segUid)
  {
    spdlog::error("Error creating blank segmentation for image {}", matchImageUid);
    return std::nullopt;
  }

  spdlog::debug(
    "Created blank segmentation {} ('{}') for image {}", matchImageUid, displayName, matchImageUid
  );

  Image* seg = m_appData.seg(*segUid);
  if (!seg)
  {
    spdlog::error("Null segmentation created {}", *segUid);
    m_appData.removeSeg(*segUid);
    return std::nullopt;
  }

  const auto tableUid = data::createLabelColorTableForSegmentation(m_appData, *segUid);
  bool createdTableTexture = false;

  if (tableUid)
  {
    spdlog::trace("Creating texture for label color table {}", *tableUid);
    createdTableTexture = m_rendering.createLabelColorTableTexture(*tableUid);
  }

  if (!tableUid || !createdTableTexture)
  {
    constexpr size_t k_defaultTableIndex = 0;

    spdlog::error(
      "Unable to create label color table for segmentation {}. "
      "Defaulting to table index {}.",
      *segUid,
      k_defaultTableIndex
    );

    seg->settings().setLabelTableIndex(k_defaultTableIndex);
  }

  if (m_appData.assignSegUidToImage(matchImageUid, *segUid))
  {
    spdlog::info("Assigned segmentation {} to image {}", *segUid, matchImageUid);
  }
  else
  {
    spdlog::error("Unable to assign segmentation {} to image {}", *segUid, matchImageUid);
    m_appData.removeSeg(*segUid);
    return std::nullopt;
  }

  // Make it the active segmentation
  m_appData.assignActiveSegUidToImage(matchImageUid, *segUid);

  spdlog::trace("Creating texture for segmentation {}", *segUid);

  const std::vector<uuids::uuid> createdSegTexUids
    = createSegTextures(m_appData, std::vector<uuids::uuid>{*segUid});

  if (createdSegTexUids.empty())
  {
    spdlog::error("Unable to create texture for segmentation {}", *segUid);
    m_appData.removeSeg(*segUid);
    return std::nullopt;
  }

  // Assign the image's affine_T_subject transformation to its segmentation:
  seg->transformations().set_affine_T_subject(matchImage->transformations().get_affine_T_subject());

  // Synchronize transformation on all segmentations of the image:
  syncManualImageTransformationOnSegs(matchImageUid);

  // Update uniforms for all images
  m_rendering.updateImageUniforms(m_appData.imageUidsOrdered());

  return *segUid;
}

bool CallbackHandler::executeGraphCutsSegmentation(
  const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType& segType
)
{
  // Inputs to algorithm:
  const Image* image = m_appData.image(imageUid);
  const Image* seedSeg = m_appData.seg(seedSegUid);

  if (!image)
  {
    spdlog::error("Null image {} input to graph cuts segmentation", imageUid);
    return false;
  }

  if (!seedSeg)
  {
    spdlog::error("Null seed segmentation {} input to graph cuts segmentation", seedSegUid);
    return false;
  }

  if (image->header().pixelDimensions() != seedSeg->header().pixelDimensions())
  {
    spdlog::error(
      "Dimensions of input image {} ({}) and seed segmentation {} ({}) do not match",
      imageUid,
      glm::to_string(image->header().pixelDimensions()),
      seedSegUid,
      glm::to_string(seedSeg->header().pixelDimensions())
    );
    return false;
  }

  const size_t numSegsForImage = m_appData.imageToSegUids(imageUid).size();

  const std::string resultSegDisplayName = (SeedSegmentationType::Binary == segType)
                                             ? std::string("Binary graph cuts segmentation ")
                                             : std::string("Multi-label graph cuts segmentation ")
                                                 + std::to_string(numSegsForImage + 1)
                                                 + " for image '" + image->settings().displayName()
                                                 + "'";

  const auto resultSegUid = createBlankSegWithColorTableAndTextures(imageUid, resultSegDisplayName);

  if (!resultSegUid)
  {
    spdlog::error("Unable to create blank segmentation for graph cuts");
    return false;
  }

  Image* resultSeg = m_appData.seg(*resultSegUid);

  if (!resultSeg)
  {
    spdlog::error("Null result segmentation {} for graph cuts", *resultSegUid);
    return false;
  }

  spdlog::info(
    "Executing graph cuts segmentation on image {} with seeds {}; "
    "resulting segmentation: {}",
    imageUid,
    seedSegUid,
    *resultSegUid
  );

  const uint32_t imComp = image->settings().activeComponent();

  const VoxelDistances voxelDists = computeVoxelDistances(image->header().spacing(), true);

  const double imLow = image->settings().componentStatistics(imComp).m_quantiles[1];
  const double imHigh = image->settings().componentStatistics(imComp).m_quantiles[99];

  auto weight = [this, &imLow, &imHigh](double diff) -> double
  {
    const double amplitude = m_appData.settings().graphCutsWeightsAmplitude();
    const double sigma = m_appData.settings().graphCutsWeightsSigma();
    const double diffNorm = (diff - imLow) / (imHigh - imLow);

    return amplitude * std::exp(-0.5 * std::pow(diffNorm / sigma, 2.0));
  };

  auto getImageWeight =
    [&weight, &image, &imComp](int x, int y, int z, int dx, int dy, int dz) -> double
  {
    const auto a = image->value<double>(imComp, x, y, z);
    const auto b = image->value<double>(imComp, x + dx, y + dy, z + dz);

    if (a && b)
    {
      return weight((*a) - (*b));
    }
    else
    {
      return 0.0;
    } // weight for very different image values
  };

  auto getImageWeight1D = [&weight, &image, &imComp](int index1, int index2) -> double
  {
    const auto a = image->value<double>(imComp, index1);
    const auto b = image->value<double>(imComp, index2);

    if (a && b)
    {
      return weight((*a) - (*b));
    }
    else
    {
      return 0.0;
    } // weight for very different image values
  };

  auto getSeedValue = [&seedSeg](int x, int y, int z) -> LabelType
  { return seedSeg->value<int64_t>(0, x, y, z).value_or(0); };

  auto setResultSegValue = [&resultSeg](int x, int y, int z, LabelType value)
  { resultSeg->setValue(0, x, y, z, value); };

  bool success = false;

  switch (segType)
  {
  case SeedSegmentationType::Binary:
  {
    success = graphCutsBinarySegmentation(
      m_appData.settings().graphCutsNeighborhood(),
      m_appData.settings().graphCutsWeightsAmplitude(),
      static_cast<LabelType>(m_appData.settings().foregroundLabel()),
      static_cast<LabelType>(m_appData.settings().backgroundLabel()),
      glm::ivec3{image->header().pixelDimensions()},
      voxelDists,
      getImageWeight,
      getSeedValue,
      setResultSegValue
    );
    break;
  }
  case SeedSegmentationType::MultiLabel:
  {
    success = graphCutsMultiLabelSegmentation(
      m_appData.settings().graphCutsNeighborhood(),
      m_appData.settings().graphCutsWeightsAmplitude(),
      glm::ivec3{image->header().pixelDimensions()},
      voxelDists,
      getImageWeight,
      getImageWeight1D,
      getSeedValue,
      setResultSegValue
    );
    break;
  }
  }

  if (!success)
  {
    spdlog::error("Failure during execution of graph cuts segmentation");
    return false;
  }

  spdlog::debug("Start updating segmentation texture");

  m_rendering.updateSegTexture(
    *resultSegUid,
    resultSeg->header().memoryComponentType(),
    glm::uvec3{0},
    resultSeg->header().pixelDimensions(),
    resultSeg->bufferAsVoid(0)
  );

  spdlog::debug("Done updating segmentation texture");

  return true;
}

bool CallbackHandler::executePoissonSegmentation(
  const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType& segType
)
{
  // Algorithm inputs:
  const Image* image = m_appData.image(imageUid);
  const Image* seedSeg = m_appData.seg(seedSegUid);

  if (!image)
  {
    spdlog::error("Null image {} input to Poisson segmentation", imageUid);
    return false;
  }

  if (!seedSeg)
  {
    spdlog::error("Null seed segmentation {} input to Poisson segmentation", seedSegUid);
    return false;
  }

  if (image->header().pixelDimensions() != seedSeg->header().pixelDimensions())
  {
    spdlog::error(
      "Dimensions of image {} ({}) and seed segmentation {} ({}) do not match",
      imageUid,
      glm::to_string(image->header().pixelDimensions()),
      seedSegUid,
      glm::to_string(seedSeg->header().pixelDimensions())
    );
    return false;
  }

  const uint32_t imComp = image->settings().activeComponent();
  const uint32_t sk_seedComp = 0;

  const glm::ivec3 dims{seedSeg->header().pixelDimensions()};

  // Buffer will either point to data of the image (if the image has float components)
  // or to data of a vector with float components:
  const float* imageBuffer = nullptr;
  std::vector<float> imageVector;

  if (ComponentType::Float32 == image->header().memoryComponentType())
  {
    imageBuffer = static_cast<const float*>(image->bufferAsVoid(imComp));
  }
  else
  {
    imageVector.resize(image->header().numPixels(), 0.0f);
    for (std::size_t i = 0; i < image->header().numPixels(); ++i)
    {
      imageVector[i] = image->value<float>(imComp, i).value_or(0.0f);
    }
    imageBuffer = imageVector.data();
  }

  // Buffer will either point to data of the seed segmentation, with all components converted to uint8_t
  // and labels compressed to be in a contiguous range.
  static constexpr bool sk_ignoreBackgroundLabel = false;

  std::vector<uint8_t> seedSegVector(seedSeg->header().numPixels(), 0u);

  for (std::size_t i = 0; i < seedSeg->header().numPixels(); ++i)
  {
    seedSegVector[i] = seedSeg->value<uint8_t>(sk_seedComp, i).value_or(0u);
  }

  const uint8_t* seedSegBuffer = seedSegVector.data();

  const LabelIndexMaps labelMaps
    = createLabelIndexMaps(dims, seedSegBuffer, sk_ignoreBackgroundLabel);

  const size_t numSegsForImage = m_appData.imageToSegUids(imageUid).size();

  const std::string resultSegDisplayName = ((SeedSegmentationType::Binary == segType)
                                              ? std::string("Binary Poisson segmentation ")
                                              : std::string("Multi-label Poisson segmentation "))
                                           + std::to_string(numSegsForImage + 1) + " for image '"
                                           + image->settings().displayName() + "'";

  //    spdlog::trace( "resultSegDisplayName = {}", resultSegDisplayName );

  const auto resultSegUid = createBlankSegWithColorTableAndTextures(imageUid, resultSegDisplayName);

  if (!resultSegUid)
  {
    spdlog::error("Unable to create blank segmentation matching image {}", imageUid);
    return false;
  }

  const std::string potDisplayName = std::string("Potential maps for '")
                                     + image->settings().displayName() + "'";

  // The number of components for the output potential image equals the number of
  // labels in the seed segmentation, including label zero. Component 0 of the
  // image holds the potential for all labels. Component i >= 1 of the image holds the
  // potential of label index i.
  const uint32_t numLabels = labelMaps.labelToIndex.size() - 1;
  const uint32_t numComps = labelMaps.labelToIndex.size();

  // Create potential image with float components
  const auto potImageUid = createBlankImageAndTexture(
    imageUid, ComponentType::Float32, numComps, potDisplayName, numComps
  );

  if (!potImageUid)
  {
    spdlog::error("Unable to create blank potential image matching image {}", imageUid);
    return false;
  }

  spdlog::debug("Generated blank potential image {} with {} components", *potImageUid, numComps);

  // Algorithm outputs:
  Image* resultSeg = m_appData.seg(*resultSegUid);
  Image* potImage = m_appData.image(*potImageUid);

  if (!resultSeg)
  {
    spdlog::error("Null result segmentation {} for Poisson", *resultSegUid);
    return false;
  }

  if (!potImage)
  {
    spdlog::error("Null potential image {} for Poisson", *potImageUid);
    return false;
  }

  if (image->header().pixelDimensions() != resultSeg->header().pixelDimensions())
  {
    spdlog::error(
      "Dimensions of image {} ({}) and result segmentation {} ({}) do not match",
      imageUid,
      glm::to_string(image->header().pixelDimensions()),
      *resultSegUid,
      glm::to_string(resultSeg->header().pixelDimensions())
    );
    return false;
  }

  if (image->header().pixelDimensions() != potImage->header().pixelDimensions())
  {
    spdlog::error(
      "Dimensions of image {} ({}) and potential image {} ({}) do not match",
      imageUid,
      glm::to_string(image->header().pixelDimensions()),
      *potImageUid,
      glm::to_string(potImage->header().pixelDimensions())
    );
    return false;
  }

  spdlog::info(
    "Executing Poisson segmentation on image {} with seeds {}; "
    "resulting segmentation: {}; resulting potential: {}",
    imageUid,
    seedSegUid,
    *resultSegUid,
    *potImageUid
  );

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
  for (uint8_t i = 1; i <= numLabels; ++i)
  {
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
  for (uint32_t i = 0; i < numComps; ++i)
  {
    m_rendering.updateImageTexture(
      *potImageUid,
      i,
      potImage->header().memoryComponentType(),
      glm::uvec3{0},
      potImage->header().pixelDimensions(),
      potImage->bufferAsVoid(i)
    );
  }
  spdlog::debug("Done updating potential image textures");

  spdlog::debug("Start updating segmentation texture");
  m_rendering.updateSegTexture(
    *resultSegUid,
    resultSeg->header().memoryComponentType(),
    glm::uvec3{0},
    resultSeg->header().pixelDimensions(),
    resultSeg->bufferAsVoid(0)
  );
  spdlog::debug("Done updating segmentation texture");

  return true;
}

void CallbackHandler::recenterViews(
  const ImageSelection& imageSelection,
  bool recenterCrosshairs,
  bool recenterOnCurrentCrosshairsPos,
  bool resetObliqueOrientation,
  const std::optional<bool>& resetZoom
)
{
  // On view recenter, force the crosshairs and views to snap to the center of the
  // reference image voxels. This is so that crosshairs/views don't land on a voxel
  // boundary (which causes jitter on view zoom).
  static constexpr CrosshairsSnapping forceSnapping = CrosshairsSnapping::ReferenceImage;

  if (0 == m_appData.numImages())
  {
    spdlog::warn("No images loaded: preparing views using default bounds");
  }

  // Compute the AABB that we are recentering views on:
  const auto worldBox = data::computeWorldAABBoxEnclosingImages(m_appData, imageSelection);

  if (recenterCrosshairs)
  {
    // Crosshairs always snap to voxels
    const glm::vec3 worldPos = math::computeAABBoxCenter(worldBox);
    const glm::vec3 worldPosSnapped
      = data::snapWorldPointToImageVoxels(m_appData, worldPos, forceSnapping);

    m_appData.state().setWorldCrosshairsPos(worldPosSnapped);
  }

  const glm::vec3 worldCenter = (recenterOnCurrentCrosshairsPos)
                                  ? m_appData.state().worldCrosshairs().worldOrigin()
                                  : math::computeAABBoxCenter(worldBox);

  // const glm::vec3 worldCenterSnapped = data::snapWorldPointToImageVoxels( m_appData, worldCenter, forceSnapping );

  const bool _resetZoom = resetZoom ? *resetZoom : !recenterOnCurrentCrosshairsPos;

  m_appData.windowData().recenterAllViews(
    worldCenter,
    sk_viewAABBoxScaleFactor * math::computeAABBoxSize(worldBox),
    _resetZoom,
    resetObliqueOrientation
  );
}

void CallbackHandler::recenterView(const ImageSelection& imageSelection, const uuids::uuid& viewUid)
{
  // On view recenter, force the crosshairs and views to snap to the center of the
  // reference image voxels. This is so that crosshairs/views don't land on a voxel
  // boundary (which causes jitter on view zoom).
  static constexpr CrosshairsSnapping forceSnapping = CrosshairsSnapping::ReferenceImage;
  static constexpr bool sk_resetZoom = false;
  static constexpr bool sk_resetObliqueOrientation = true;

  if (0 == m_appData.numImages())
  {
    spdlog::warn("No images loaded, so recentering view {} using default bounds", viewUid);
  }

  // Size and position the views based on the enclosing AABB of the image selection:
  const auto worldBox = data::computeWorldAABBoxEnclosingImages(m_appData, imageSelection);
  const auto worldBoxSize = math::computeAABBoxSize(worldBox);

  const glm::vec3 worldPos = m_appData.state().worldCrosshairs().worldOrigin();
  const glm::vec3 worldPosSnapped
    = data::snapWorldPointToImageVoxels(m_appData, worldPos, forceSnapping);

  m_appData.windowData().recenterView(
    viewUid,
    worldPosSnapped,
    sk_viewAABBoxScaleFactor * worldBoxSize,
    sk_resetZoom,
    sk_resetObliqueOrientation
  );
}

void CallbackHandler::doCrosshairsMove(const ViewHit& hit)
{
  if (!checkAndSetActiveView(hit.viewUid))
    return;

  m_appData.state().setWorldCrosshairsPos(glm::vec3{hit.worldPos});
}

void CallbackHandler::doCrosshairsScroll(
  const ViewHit& hit, const glm::vec2& scrollOffset, bool fineScroll
)
{
  const float multiplier = fineScroll ? 0.1f : 1.0f;

  const float scrollDistance = multiplier
                               * data::sliceScrollDistance(
                                 m_appData,
                                 hit.worldFrontAxis,
                                 ImageSelection::VisibleImagesInView,
                                 hit.view
                               );

  const glm::vec3 worldPos = m_appData.state().worldCrosshairs().worldOrigin()
                             + scrollOffset.y * scrollDistance * hit.worldFrontAxis;

  const glm::vec3 worldPosSnapped = data::snapWorldPointToImageVoxels(m_appData, worldPos);
  m_appData.state().setWorldCrosshairsPos(worldPosSnapped);
}

void CallbackHandler::doSegment(const ViewHit& hit, bool swapFgAndBg)
{
  static const glm::ivec3 sk_voxelZero{0, 0, 0};

  if (!hit.view)
    return;

  const auto activeImageUid = m_appData.activeImageUid();
  if (!activeImageUid)
    return;

  if (!checkAndSetActiveView(hit.viewUid))
    return;

  if (0 == std::count(std::begin(hit.view->visibleImages()), std::end(hit.view->visibleImages()), *activeImageUid))
  {
    return; // The active image is not visible
  }

  const auto activeSegUid = m_appData.imageToActiveSegUid(*activeImageUid);
  if (!activeSegUid)
    return;

  // The position is in the view bounds; make this the active view:
  m_appData.windowData().setActiveViewUid(hit.viewUid);

  // Do nothing if the active segmentation is null
  Image* activeSeg = m_appData.seg(*activeSegUid);
  if (!activeSeg)
    return;

  // Gather all synchronized segmentations
  std::unordered_set<uuids::uuid> segUids;
  segUids.insert(*activeSegUid);

  for (const auto& imageUid : m_appData.imagesBeingSegmented())
  {
    if (const auto segUid = m_appData.imageToActiveSegUid(imageUid))
    {
      segUids.insert(*segUid);
    }
  }

  // Note: when moving crosshairs, the worldPos would be offset at this stage.
  // i.e.: worldPos -= glm::vec4{ offsetDist * worldCameraFront, 0.0f };
  // However, we want to allow the user to segment on any view, regardless of its offset.
  // Therefore, the offset is not applied.

  const LabelType labelToPaint = static_cast<LabelType>(
    swapFgAndBg ? m_appData.settings().backgroundLabel() : m_appData.settings().foregroundLabel()
  );

  const LabelType labelToReplace = static_cast<LabelType>(
    swapFgAndBg ? m_appData.settings().foregroundLabel() : m_appData.settings().backgroundLabel()
  );

  const int brushSize = static_cast<int>(m_appData.settings().brushSizeInVoxels());

  const AppSettings& settings = m_appData.settings();

  // Paint on each segmentation
  for (const auto& segUid : segUids)
  {
    Image* seg = m_appData.seg(segUid);
    if (!seg)
      continue;

    const glm::ivec3 dims{seg->header().pixelDimensions()};

    // Use the offset position, so that the user can paint in any offset view of a lightbox layout:
    const glm::mat4& pixel_T_worldDef = seg->transformations().pixel_T_worldDef();
    const glm::vec4 pixelPos = pixel_T_worldDef * hit.worldPos_offsetApplied;
    const glm::vec3 pixelPos3 = pixelPos / pixelPos.w;
    const glm::ivec3 roundedPixelPos{glm::round(pixelPos3)};

    if (glm::any(glm::lessThan(roundedPixelPos, sk_voxelZero)) || glm::any(glm::greaterThanEqual(roundedPixelPos, dims)))
    {
      continue; // This pixel is outside the image
    }

    // View plane normal vector transformed into Voxel space:
    const glm::vec3 voxelViewPlaneNormal = glm::normalize(
      glm::inverseTranspose(glm::mat3(pixel_T_worldDef)) * (-hit.worldFrontAxis)
    );

    // View plane equation:
    const glm::vec4 voxelViewPlane = math::makePlane(voxelViewPlaneNormal, pixelPos3);

    auto updateSegTexture = [this, &segUid](
                              const ComponentType& memoryComponentType,
                              const glm::uvec3& dataOffset,
                              const glm::uvec3& dataSize,
                              const LabelType* data
                            )
    {
      m_rendering
        .updateSegTextureWithInt64Data(segUid, memoryComponentType, dataOffset, dataSize, data);
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
      updateSegTexture
    );
  }
}

void CallbackHandler::paintActiveSegmentationWithAnnotation()
{
  const auto activeImageUid = m_appData.activeImageUid();
  if (!activeImageUid)
    return;

  const auto activeSegUid = m_appData.imageToActiveSegUid(*activeImageUid);
  if (!activeSegUid)
  {
    spdlog::debug("There is no active segmentation to paint for image {}", *activeImageUid);
    return;
  }

  const auto activeAnnotUid = m_appData.imageToActiveAnnotationUid(*activeImageUid);
  if (!activeAnnotUid)
  {
    spdlog::debug("There is no active annotation to paint for image {}", *activeImageUid);
    return;
  }

  Image* seg = m_appData.seg(*activeSegUid);
  if (!seg)
  {
    spdlog::error("Segmentation {} is null", *activeSegUid);
    return;
  }

  const Annotation* annot = m_appData.annotation(*activeAnnotUid);
  if (!annot)
  {
    spdlog::error("Annotation {} is null", *activeAnnotUid);
    return;
  }

  /// @todo Implement algorithm for filling smoothed polygons.

  if (!annot->isClosed())
  {
    spdlog::warn(
      "Annotation {} is not closed and so cannot be filled to paint segmentation {}",
      *activeAnnotUid,
      *activeSegUid
    );
    return;
  }

  if (annot->isSmoothed())
  {
    spdlog::warn(
      "Annotation {} is smoothed and so cannot be filled to paint segmentation {}",
      *activeAnnotUid,
      *activeSegUid
    );
    return;
  }

  auto updateSegTexture = [this, &activeSegUid](
                            const ComponentType& memoryComponentType,
                            const glm::uvec3& dataOffset,
                            const glm::uvec3& dataSize,
                            const LabelType* data
                          )
  {
    if (!activeSegUid)
      return;

    m_rendering
      .updateSegTextureWithInt64Data(*activeSegUid, memoryComponentType, dataOffset, dataSize, data);
  };

  fillSegmentationWithPolygon(
    *seg,
    annot,
    static_cast<LabelType>(m_appData.settings().foregroundLabel()),
    static_cast<LabelType>(m_appData.settings().backgroundLabel()),
    m_appData.settings().replaceBackgroundWithForeground(),
    updateSegTexture
  );
}

void CallbackHandler::doWindowLevel(
  const ViewHit& startHit, const ViewHit& prevHit, const ViewHit& currHit, bool fineAdjustment
)
{
  View* viewToWL = startHit.view;

  if (!viewToWL)
    return;

  const float multipler = fineAdjustment ? 0.1f : 1.0f;

  if (camera::IntensityProjectionMode::Xray == viewToWL->intensityProjectionMode())
  {
    // Special logic to adjust W/L for views rendering in x-ray projection mode:

    // Level/width values for x-ray projection mode are in range [0.0, 1.0]
    static constexpr float levelMin = 0.0f;
    static constexpr float levelMax = 1.0f;
    static constexpr float winMin = 0.0f;
    static constexpr float winMax = 1.0f;

    float oldLevel = m_appData.renderData().m_xrayIntensityLevel;
    float oldWindow = m_appData.renderData().m_xrayIntensityWindow;

    const float levelDelta = multipler * (levelMax - levelMin)
                             * (currHit.windowClipPos.y - prevHit.windowClipPos.y) / 2.0f;

    const float winDelta = multipler * (winMax - winMin)
                           * (currHit.windowClipPos.x - prevHit.windowClipPos.x) / 2.0f;

    const float newLevel = std::min(std::max(oldLevel + levelDelta, levelMin), levelMax);
    const float newWindow = std::min(std::max(oldWindow + winDelta, winMin), winMax);

    m_appData.renderData().m_xrayIntensityLevel = newLevel;
    m_appData.renderData().m_xrayIntensityWindow = newWindow;
  }
  else
  {
    const auto activeImageUid = m_appData.activeImageUid();
    if (!activeImageUid)
      return;

    Image* activeImage = m_appData.image(*activeImageUid);
    if (!activeImage)
      return;

    if (0 == std::count(std::begin(viewToWL->visibleImages()), std::end(viewToWL->visibleImages()), *activeImageUid))
    {
      return; // The active image is not visible
    }

    auto& S = activeImage->settings();

    const double centerDelta
      = multipler * (S.minMaxWindowCenterRange().second - S.minMaxWindowCenterRange().first)
        * static_cast<double>(currHit.windowClipPos.y - prevHit.windowClipPos.y) / 2.0;

    const double windowDelta
      = multipler * (S.minMaxWindowWidthRange().second - S.minMaxWindowWidthRange().first)
        * static_cast<double>(currHit.windowClipPos.x - prevHit.windowClipPos.x) / 2.0;

    S.setWindowCenter(S.windowCenter() + centerDelta);
    S.setWindowWidth(S.windowWidth() + windowDelta);

    m_rendering.updateImageUniforms(*activeImageUid);
  }
}

void CallbackHandler::doOpacity(const ViewHit& prevHit, const ViewHit& currHit)
{
  static constexpr double sk_opMin = 0.0;
  static constexpr double sk_opMax = 1.0;

  if (!currHit.view)
    return;

  const auto activeImageUid = m_appData.activeImageUid();
  if (!activeImageUid)
    return;

  if (0 == std::count(std::begin(currHit.view->visibleImages()), std::end(currHit.view->visibleImages()), *activeImageUid))
  {
    return; // The active image is not visible
  }

  Image* activeImage = m_appData.image(*activeImageUid);
  if (!activeImage)
    return;

  const double opacityDelta = (sk_opMax - sk_opMin)
                              * static_cast<double>(
                                currHit.windowClipPos.y - prevHit.windowClipPos.y
                              )
                              / 2.0;

  const double newOpacity
    = std::min(std::max(activeImage->settings().opacity() + opacityDelta, sk_opMin), sk_opMax);

  activeImage->settings().setOpacity(newOpacity);

  m_rendering.updateImageUniforms(*activeImageUid);
}

void CallbackHandler::doCameraTranslate2d(
  const ViewHit& startHit, const ViewHit& prevHit, const ViewHit& currHit
)
{
  const glm::vec3 worldOrigin = m_appData.state().worldCrosshairs().worldOrigin();

  View* viewToTranslate = startHit.view;
  if (!viewToTranslate)
    return;

  const auto& viewUidToTranslate = startHit.viewUid;

  const auto backupCamera = viewToTranslate->camera();

  panRelativeToWorldPosition(
    viewToTranslate->camera(), prevHit.viewClipPos, currHit.viewClipPos, worldOrigin
  );

  if (const auto transGroupUid = viewToTranslate->cameraTranslationSyncGroupUid())
  {
    for (const auto& syncedViewUid :
         m_appData.windowData().cameraTranslationGroupViewUids(*transGroupUid))
    {
      if (syncedViewUid == viewUidToTranslate)
        continue;

      View* syncedView = m_appData.windowData().getCurrentView(syncedViewUid);

      if (!syncedView)
        continue;
      if (syncedView->viewType() != viewToTranslate->viewType())
        continue;

      if (camera::areViewDirectionsParallel(
            syncedView->camera(), backupCamera, Directions::View::Back, sk_parallelThreshold_degrees
          ))
      {
        panRelativeToWorldPosition(
          syncedView->camera(), prevHit.viewClipPos, currHit.viewClipPos, worldOrigin
        );
      }
    }
  }
}

void CallbackHandler::doCameraRotate2d(
  const ViewHit& startHit,
  const ViewHit& prevHit,
  const ViewHit& currHit,
  const RotationOrigin& rotationOrigin
)
{
  View* viewToRotate = startHit.view;
  if (!viewToRotate)
    return;

  const auto& viewUidToRotate = startHit.viewUid;

  // Only allow rotation of oblique and 3D views
  if (ViewType::Oblique != viewToRotate->viewType() && ViewType::ThreeD != viewToRotate->viewType())
  {
    return;
  }

  // Point about which to rotate the view:
  glm::vec3 worldRotationCenterPos;

  switch (rotationOrigin)
  {
  case RotationOrigin::Crosshairs:
  {
    worldRotationCenterPos = m_appData.state().worldCrosshairs().worldOrigin();
    break;
  }
  case RotationOrigin::CameraEye:
  case RotationOrigin::ViewCenter:
  {
    worldRotationCenterPos = camera::worldOrigin(viewToRotate->camera());
    break;
  }
  }

  glm::vec4 clipRotationCenterPos
    = camera::clip_T_world(viewToRotate->camera())
      * glm::vec4{m_appData.state().worldCrosshairs().worldOrigin(), 1.0f};

  clipRotationCenterPos /= clipRotationCenterPos.w;

  const auto backupCamera = viewToRotate->camera();

  camera::rotateInPlane(
    viewToRotate->camera(),
    prevHit.viewClipPos,
    currHit.viewClipPos,
    glm::vec2{clipRotationCenterPos}
  );

  // Rotate the synchronized views:
  if (const auto rotGroupUid = viewToRotate->cameraRotationSyncGroupUid())
  {
    for (const auto& syncedViewUid :
         m_appData.windowData().cameraRotationGroupViewUids(*rotGroupUid))
    {
      if (syncedViewUid == viewUidToRotate)
        continue;

      View* syncedView = m_appData.windowData().getCurrentView(syncedViewUid);

      if (!syncedView)
        continue;
      if (syncedView->viewType() != viewToRotate->viewType())
        continue;

      if (!camera::areViewDirectionsParallel(
            syncedView->camera(), backupCamera, Directions::View::Back, sk_parallelThreshold_degrees
          ))
      {
        continue;
      }

      camera::rotateInPlane(
        syncedView->camera(),
        prevHit.viewClipPos,
        currHit.viewClipPos,
        glm::vec2{clipRotationCenterPos}
      );
    }
  }
}

void CallbackHandler::doCameraRotate3d(
  const ViewHit& startHit,
  const ViewHit& prevHit,
  const ViewHit& currHit,
  const RotationOrigin& rotationOrigin,
  const AxisConstraint& constraint
)
{
  View* viewToRotate = startHit.view;
  if (!viewToRotate)
    return;

  const auto& viewUidToRotate = startHit.viewUid;

  // Only allow rotation of oblique and 3D views
  if (ViewType::Oblique != viewToRotate->viewType() && ViewType::ThreeD != viewToRotate->viewType())
  {
    return;
  }

  glm::vec2 viewClipPrevPos = prevHit.viewClipPos;
  glm::vec2 viewClipCurrPos = currHit.viewClipPos;

  switch (constraint)
  {
  case AxisConstraint::X:
  {
    viewClipPrevPos.x = 0.0f;
    viewClipCurrPos.x = 0.0f;
    break;
  }
  case AxisConstraint::Y:
  {
    viewClipPrevPos.y = 0.0f;
    viewClipCurrPos.y = 0.0f;
    break;
  }
  case AxisConstraint::None:
  default:
  {
    break;
  }
  }

  // Point about which to rotate the view:
  glm::vec3 worldRotationCenterPos;

  switch (rotationOrigin)
  {
  case RotationOrigin::Crosshairs:
  {
    worldRotationCenterPos = m_appData.state().worldCrosshairs().worldOrigin();
    break;
  }
  case RotationOrigin::CameraEye:
  case RotationOrigin::ViewCenter:
  {
    worldRotationCenterPos = camera::worldOrigin(viewToRotate->camera());
    break;
  }
  }

  camera::rotateAboutWorldPoint(
    viewToRotate->camera(), viewClipPrevPos, viewClipCurrPos, worldRotationCenterPos
  );

  const auto backupCamera = viewToRotate->camera();

  // Rotate the synchronized views:
  if (const auto rotGroupUid = viewToRotate->cameraRotationSyncGroupUid())
  {
    for (const auto& syncedViewUid :
         m_appData.windowData().cameraRotationGroupViewUids(*rotGroupUid))
    {
      if (syncedViewUid == viewUidToRotate)
        continue;

      View* syncedView = m_appData.windowData().getCurrentView(syncedViewUid);

      if (!syncedView)
        continue;
      if (syncedView->viewType() != viewToRotate->viewType())
        continue;

      if (!camera::areViewDirectionsParallel(
            syncedView->camera(), backupCamera, Directions::View::Back, sk_parallelThreshold_degrees
          ))
      {
        continue;
      }

      camera::rotateAboutWorldPoint(
        syncedView->camera(), viewClipPrevPos, viewClipCurrPos, worldRotationCenterPos
      );
    }
  }
}

void CallbackHandler::doCameraRotate3d(
  const uuids::uuid& viewUid, const glm::quat& camera_T_world_rotationDelta
)
{
  auto& windowData = m_appData.windowData();

  View* view = windowData.getView(viewUid);
  if (!view)
    return;

  if (camera::ViewRenderMode::Disabled == view->renderMode())
    return;
  if (ViewType::Oblique != view->viewType())
    return;

  const glm::vec3 worldOrigin = m_appData.state().worldCrosshairs().worldOrigin();

  const auto backupCamera = view->camera();

  camera::applyViewRotationAboutWorldPoint(view->camera(), camera_T_world_rotationDelta, worldOrigin);

  if (const auto rotGroupUid = view->cameraRotationSyncGroupUid())
  {
    for (const auto& syncedViewUid : windowData.cameraRotationGroupViewUids(*rotGroupUid))
    {
      if (syncedViewUid == viewUid)
        continue;

      View* syncedView = windowData.getCurrentView(syncedViewUid);

      if (!syncedView)
        continue;
      if (syncedView->viewType() != view->viewType())
        continue;

      if (!camera::areViewDirectionsParallel(
            syncedView->camera(), backupCamera, Directions::View::Back, sk_parallelThreshold_degrees
          ))
      {
        continue;
      }

      camera::applyViewRotationAboutWorldPoint(
        syncedView->camera(), camera_T_world_rotationDelta, worldOrigin
      );
    }
  }
}

void CallbackHandler::handleSetViewForwardDirection(
  const uuids::uuid& viewUid, const glm::vec3& worldForwardDirection
)
{
  auto& windowData = m_appData.windowData();

  View* view = windowData.getView(viewUid);
  if (!view)
    return;

  if (camera::ViewRenderMode::Disabled == view->renderMode())
    return;
  if (ViewType::Oblique != view->viewType())
    return;

  const glm::vec3 worldXhairsPos = m_appData.state().worldCrosshairs().worldOrigin();
  camera::setWorldForwardDirection(view->camera(), worldForwardDirection);
  camera::setWorldTarget(view->camera(), worldXhairsPos, std::nullopt);

  if (const auto rotGroupUid = view->cameraRotationSyncGroupUid())
  {
    for (const auto& syncedViewUid : windowData.cameraRotationGroupViewUids(*rotGroupUid))
    {
      if (syncedViewUid == viewUid)
        continue;

      View* syncedView = windowData.getCurrentView(syncedViewUid);

      if (!syncedView)
        continue;
      if (syncedView->viewType() != view->viewType())
        continue;

      camera::setWorldForwardDirection(syncedView->camera(), worldForwardDirection);
      camera::setWorldTarget(syncedView->camera(), worldXhairsPos, std::nullopt);
    }
  }
}

void CallbackHandler::doCameraZoomDrag(
  const ViewHit& startHit,
  const ViewHit& prevHit,
  const ViewHit& currHit,
  const ZoomBehavior& zoomBehavior,
  bool syncZoomForAllViews
)
{
  static const glm::vec2 sk_ndcCenter{0.0f, 0.0f};

  View* viewToZoom = startHit.view;
  if (!viewToZoom)
    return;

  const auto& viewUidToZoom = startHit.viewUid;

  auto getCenterViewClipPos = [this, &zoomBehavior, &startHit](const View* view) -> glm::vec2
  {
    glm::vec2 viewClipCenterPos{0.0f};

    switch (zoomBehavior)
    {
    case ZoomBehavior::ToCrosshairs:
    {
      viewClipCenterPos
        = camera::ndc_T_world(view->camera(), m_appData.state().worldCrosshairs().worldOrigin());
      break;
    }
    case ZoomBehavior::ToStartPosition:
    {
      const glm::vec4 _viewClipStartPos = camera::clip_T_world(view->camera()) * startHit.worldPos;
      viewClipCenterPos = glm::vec2{_viewClipStartPos / _viewClipStartPos.w};
      break;
    }
    case ZoomBehavior::ToViewCenter:
    {
      viewClipCenterPos = sk_ndcCenter;
      break;
    }
    }

    return viewClipCenterPos;
  };

  const float factor = 2.0f * (currHit.windowClipPos.y - prevHit.windowClipPos.y) / 2.0f + 1.0f;
  camera::zoomNdc(viewToZoom->camera(), factor, getCenterViewClipPos(viewToZoom));

  if (syncZoomForAllViews)
  {
    // Apply zoom to all other views:
    for (const auto& otherViewUid : m_appData.windowData().currentViewUids())
    {
      if (otherViewUid == viewUidToZoom)
        continue;

      if (View* otherView = m_appData.windowData().getCurrentView(otherViewUid))
      {
        camera::zoomNdc(otherView->camera(), factor, getCenterViewClipPos(otherView));
      }
    }
  }
  else if (const auto zoomGroupUid = viewToZoom->cameraZoomSyncGroupUid())
  {
    // Apply zoom to all views other synchronized with the view:
    for (const auto& syncedViewUid : m_appData.windowData().cameraZoomGroupViewUids(*zoomGroupUid))
    {
      if (syncedViewUid == viewUidToZoom)
        continue;

      if (View* syncedView = m_appData.windowData().getCurrentView(syncedViewUid))
      {
        camera::zoomNdc(syncedView->camera(), factor, getCenterViewClipPos(syncedView));
      }
    }
  }
}

void CallbackHandler::doCameraZoomScroll(
  const ViewHit& hit,
  const glm::vec2& scrollOffset,
  const ZoomBehavior& zoomBehavior,
  bool syncZoomForAllViews
)
{
  static constexpr float sk_zoomFactor = 0.01f;
  static const glm::vec2 sk_ndcCenter{0.0f, 0.0f};

  if (!hit.view)
    return;

  // The pointer is in the view bounds! Make this the active view
  m_appData.windowData().setActiveViewUid(hit.viewUid);

  auto getCenterViewClipPos = [this, &zoomBehavior, &hit](const View* view) -> glm::vec2
  {
    glm::vec2 viewClipCenterPos{0.0f};

    switch (zoomBehavior)
    {
    case ZoomBehavior::ToCrosshairs:
    {
      viewClipCenterPos
        = camera::ndc_T_world(view->camera(), m_appData.state().worldCrosshairs().worldOrigin());
      break;
    }
    case ZoomBehavior::ToStartPosition:
    {
      const glm::vec4 _viewClipCurrPos = camera::clip_T_world(view->camera()) * hit.worldPos;
      viewClipCenterPos = glm::vec2{_viewClipCurrPos / _viewClipCurrPos.w};
      break;
    }
    case ZoomBehavior::ToViewCenter:
    {
      viewClipCenterPos = sk_ndcCenter;
      break;
    }
    }

    return viewClipCenterPos;
  };

  const float factor = 1.0f + sk_zoomFactor * scrollOffset.y;

  camera::zoomNdc(hit.view->camera(), factor, getCenterViewClipPos(hit.view));

  if (syncZoomForAllViews)
  {
    // Apply zoom to all other views:
    for (const auto& otherViewUid : m_appData.windowData().currentViewUids())
    {
      if (otherViewUid == hit.viewUid)
        continue;

      if (View* otherView = m_appData.windowData().getCurrentView(otherViewUid))
      {
        camera::zoomNdc(otherView->camera(), factor, getCenterViewClipPos(otherView));
      }
    }
  }
  else if (const auto zoomGroupUid = hit.view->cameraZoomSyncGroupUid())
  {
    // Apply zoom all other views synchronized with this view:
    for (const auto& syncedViewUid : m_appData.windowData().cameraZoomGroupViewUids(*zoomGroupUid))
    {
      if (syncedViewUid == hit.viewUid)
        continue;

      if (View* syncedView = m_appData.windowData().getCurrentView(syncedViewUid))
      {
        camera::zoomNdc(syncedView->camera(), factor, getCenterViewClipPos(syncedView));
      }
    }
  }
}

void CallbackHandler::scrollViewSlice(const ViewHit& hit, int numSlices)
{
  const float scrollDistance = data::sliceScrollDistance(
    m_appData, hit.worldFrontAxis, ImageSelection::VisibleImagesInView, hit.view
  );

  m_appData.state().setWorldCrosshairsPos(
    m_appData.state().worldCrosshairs().worldOrigin()
    + static_cast<float>(numSlices) * scrollDistance * hit.worldFrontAxis
  );
}

void CallbackHandler::doImageTranslate(
  const ViewHit& startHit, const ViewHit& prevHit, const ViewHit& currHit, bool inPlane
)
{
  View* viewToUse = startHit.view;

  const auto activeImageUid = m_appData.activeImageUid();
  if (!activeImageUid)
    return;

  if (0 == std::count(std::begin(viewToUse->visibleImages()), std::end(viewToUse->visibleImages()), *activeImageUid))
  {
    // The active image is not visible
    return;
  }

  Image* activeImage = m_appData.image(*activeImageUid);
  if (!activeImage)
    return;

  glm::vec3 T{0.0f, 0.0f, 0.0f};

  if (inPlane)
  {
    // Translate the image along the view plane
    static const float ndcZ = -1.0f;

    // Note: for 3D in-plane translation, we'll want to use this instead:
    //camera::ndcZofWorldPoint( view->camera(), imgTx.getWorldSubjectOrigin() );

    T = camera::translationInCameraPlane(
      viewToUse->camera(), prevHit.viewClipPos, currHit.viewClipPos, ndcZ
    );
  }
  else
  {
    // Translate the image in and out of the view plane. Translate by an amount
    // proportional to the slice distance of the active image (the one being translated)
    const float scrollDistance = data::sliceScrollDistance(startHit.worldFrontAxis, *activeImage);

    T = translationAboutCameraFrontBack(
      viewToUse->camera(),
      prevHit.viewClipPos,
      currHit.viewClipPos,
      sk_imageFrontBackTranslationScaleFactor * scrollDistance
    );
  }

  auto& imgTx = activeImage->transformations();
  imgTx.set_worldDef_T_affine_translation(imgTx.get_worldDef_T_affine_translation() + T);

  // Apply same transformation to the segmentations:
  for (const auto segUid : m_appData.imageToSegUids(*activeImageUid))
  {
    if (auto* seg = m_appData.seg(segUid))
    {
      auto& segTx = seg->transformations();
      segTx.set_worldDef_T_affine_translation(segTx.get_worldDef_T_affine_translation() + T);
    }
  }

  m_rendering.updateImageUniforms(*activeImageUid);
}

void CallbackHandler::doImageRotate(
  const ViewHit& startHit, const ViewHit& prevHit, const ViewHit& currHit, bool inPlane
)
{
  View* viewToUse = startHit.view;
  if (!viewToUse)
    return;

  const auto activeImageUid = m_appData.activeImageUid();
  if (!activeImageUid)
    return;

  if (0 == std::count(std::begin(viewToUse->visibleImages()), std::end(viewToUse->visibleImages()), *activeImageUid))
  {
    // The active image is not visible
    return;
  }

  Image* activeImage = m_appData.image(*activeImageUid);
  if (!activeImage)
    return;

  /// @todo Forbid transformation if the view does NOT show the active image [1]

  const glm::vec3 worldRotationCenter = m_appData.state().worldRotationCenter();
  auto& imgTx = activeImage->transformations();

  CoordinateFrame
    imageFrame(imgTx.get_worldDef_T_affine_translation(), imgTx.get_worldDef_T_affine_rotation());

  if (inPlane)
  {
    const glm::vec2 ndcRotationCenter = ndc_T_world(viewToUse->camera(), worldRotationCenter);

    const glm::quat R = camera::rotation2dInCameraPlane(
      viewToUse->camera(), prevHit.viewClipPos, currHit.viewClipPos, ndcRotationCenter
    );

    math::rotateFrameAboutWorldPos(imageFrame, R, worldRotationCenter);
  }
  else
  {
    const glm::quat R
      = rotation3dAboutCameraPlane(viewToUse->camera(), prevHit.viewClipPos, currHit.viewClipPos);

    math::rotateFrameAboutWorldPos(imageFrame, R, worldRotationCenter);
  }

  imgTx.set_worldDef_T_affine_translation(imageFrame.worldOrigin());
  imgTx.set_worldDef_T_affine_rotation(imageFrame.world_T_frame_rotation());

  // Apply same transformation to the segmentations:
  for (const auto segUid : m_appData.imageToSegUids(*activeImageUid))
  {
    if (auto* seg = m_appData.seg(segUid))
    {
      auto& segTx = seg->transformations();
      segTx.set_worldDef_T_affine_translation(imageFrame.worldOrigin());
      segTx.set_worldDef_T_affine_rotation(imageFrame.world_T_frame_rotation());
    }
  }

  m_rendering.updateImageUniforms(*activeImageUid);
}

void CallbackHandler::doImageScale(
  const ViewHit& startHit, const ViewHit& prevHit, const ViewHit& currHit, bool constrainIsotropic
)
{
  static const glm::vec3 sk_zero(0.0f, 0.0f, 0.0f);
  static const glm::vec3 sk_minScale(0.1f);
  static const glm::vec3 sk_maxScale(10.0f);

  View* viewToUse = startHit.view;
  if (!viewToUse)
    return;

  const auto activeImageUid = m_appData.activeImageUid();
  if (!activeImageUid)
    return;

  if (0 == std::count(std::begin(viewToUse->visibleImages()), std::end(viewToUse->visibleImages()), *activeImageUid))
  {
    // The active image is not visible
    return;
  }

  Image* activeImage = m_appData.image(*activeImageUid);
  if (!activeImage)
    return;

  /// @todo Forbid transformation if the view does NOT show the active image [1]

  auto& imgTx = activeImage->transformations();

  // Center of scale is the subject center:
  //    const auto p = m_appData.worldRotationCenter();
  //    const glm::vec3 worldScaleCenter = ( p ? *p : m_appData.worldCrosshairs().worldOrigin() );
  //    glm::vec4 subjectScaleCenter = tx.subject_T_world() * glm::vec4{ worldScaleCenter, 1.0f };

  glm::vec4 lastSubjectPos = imgTx.subject_T_worldDef() * prevHit.worldPos;
  glm::vec4 currSubjectPos = imgTx.subject_T_worldDef() * currHit.worldPos;
  glm::vec4 subjectScaleCenter = imgTx.subject_T_texture() * glm::vec4{0.5, 0.5f, 0.5f, 1.0f};

  lastSubjectPos /= lastSubjectPos.w;
  currSubjectPos /= currSubjectPos.w;
  subjectScaleCenter /= subjectScaleCenter.w;

  const glm::vec3 numer = glm::vec3{currSubjectPos} - glm::vec3{subjectScaleCenter};
  const glm::vec3 denom = glm::vec3{lastSubjectPos} - glm::vec3{subjectScaleCenter};

  if (glm::any(glm::epsilonEqual(denom, sk_zero, glm::epsilon<float>())))
  {
    return;
  }

  glm::vec3 scaleDelta = numer / denom;

  //    glm::vec3 test = glm::abs( scaleDelta - glm::vec3{ 1.0f } );

  //    if ( test.x >= test.y && test.x >= test.z )
  //    {
  //        scaleDelta = glm::vec3{ scaleDelta.x, 1.0f, 1.0f };
  //    }
  //    else if ( test.y >= test.x && test.y >= test.z )
  //    {
  //        scaleDelta = glm::vec3{ 1.0f, scaleDelta.y, 1.0f };
  //    }
  //    else if ( test.z >= test.x && test.z >= test.y )
  //    {
  //        scaleDelta = glm::vec3{ 1.0f, 1.0f, scaleDelta.z };
  //    }

  if (constrainIsotropic)
  {
    float minScale = glm::compMin(scaleDelta);
    float maxScale = glm::compMax(scaleDelta);

    if (maxScale > 1.0f)
    {
      scaleDelta = glm::vec3(maxScale);
    }
    else
    {
      scaleDelta = glm::vec3(minScale);
    }
  }

  // To prevent flipping and making the slide too small:
  if (glm::any(glm::lessThan(scaleDelta, sk_minScale)) || glm::any(glm::greaterThan(scaleDelta, sk_maxScale)))
  {
    return;
  }

  imgTx.set_worldDef_T_affine_scale(scaleDelta * imgTx.get_worldDef_T_affine_scale());

  // Apply same transformation to the segmentations:
  for (const auto segUid : m_appData.imageToSegUids(*activeImageUid))
  {
    if (auto* seg = m_appData.seg(segUid))
    {
      auto& segTx = seg->transformations();
      segTx.set_worldDef_T_affine_scale(scaleDelta * segTx.get_worldDef_T_affine_scale());
    }
  }

  m_rendering.updateImageUniforms(*activeImageUid);
}

void CallbackHandler::flipImageInterpolation()
{
  const auto imgUid = m_appData.activeImageUid();
  if (!imgUid)
    return;

  Image* image = m_appData.image(*imgUid);
  if (!image)
    return;

  const InterpolationMode newMode = (InterpolationMode::NearestNeighbor
                                     == image->settings().interpolationMode())
                                      ? InterpolationMode::Trilinear
                                      : InterpolationMode::NearestNeighbor;

  image->settings().setInterpolationMode(newMode);

  m_rendering.updateImageInterpolation(*imgUid);
}

void CallbackHandler::toggleImageVisibility()
{
  const auto imageUid = m_appData.activeImageUid();
  if (!imageUid)
    return;

  Image* image = m_appData.image(*imageUid);
  if (!image)
    return;

  // Toggle the global visibility if this is a multi-component images and
  // each component is stored as a separate image.
  const bool isMulticomponentImage = ( image->header().numComponentsPerPixel() > 1
            && Image::MultiComponentBufferType::SeparateImages == image->bufferType() );

  if (isMulticomponentImage)
  {
    image->settings().setGlobalVisibility(!image->settings().globalVisibility());
  }
  else
  {
    // Otherwise, toggle visibility of the active component only:
    image->settings().setVisibility(!image->settings().visibility());
  }

  m_rendering.updateImageUniforms(*imageUid);
}

void CallbackHandler::toggleImageEdges()
{
  const auto imageUid = m_appData.activeImageUid();
  if (!imageUid)
    return;

  Image* image = m_appData.image(*imageUid);
  if (!image)
    return;

  image->settings().setShowEdges(!image->settings().showEdges());

  m_rendering.updateImageUniforms(*imageUid);
}

void CallbackHandler::decreaseSegOpacity()
{
  const auto imgUid = m_appData.activeImageUid();
  if (!imgUid)
    return;

  const auto segUid = m_appData.imageToActiveSegUid(*imgUid);
  if (!segUid)
    return;

  Image* seg = m_appData.seg(*segUid);

  const double op = seg->settings().opacity();
  seg->settings().setOpacity(std::max(op - 0.05, 0.0));

  // Update all image uniforms, since the segmentation may be shared by more than one image:
  m_rendering.updateImageUniforms(m_appData.imageUidsOrdered());
}

void CallbackHandler::toggleSegVisibility()
{
  const auto imgUid = m_appData.activeImageUid();
  if (!imgUid)
    return;

  const auto segUid = m_appData.imageToActiveSegUid(*imgUid);
  if (!segUid)
    return;

  Image* seg = m_appData.seg(*segUid);

  const bool vis = seg->settings().visibility();
  seg->settings().setVisibility(!vis);

  // Update all image uniforms, since the segmentation may be shared by more than one image:
  m_rendering.updateImageUniforms(m_appData.imageUidsOrdered());
}

void CallbackHandler::increaseSegOpacity()
{
  const auto imgUid = m_appData.activeImageUid();
  if (!imgUid)
    return;

  const auto segUid = m_appData.imageToActiveSegUid(*imgUid);
  if (!segUid)
    return;

  Image* seg = m_appData.seg(*segUid);

  const double op = seg->settings().opacity();
  seg->settings().setOpacity(std::min(op + 0.05, 1.0));

  // Update all image uniforms, since the segmentation may be shared by more than one image:
  m_rendering.updateImageUniforms(m_appData.imageUidsOrdered());
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
  static int toggle = 0;

  if (0 == (toggle % 2))
  {
    m_appData.guiData().m_renderUiWindows = (!m_appData.guiData().m_renderUiWindows);
  }
  else if (1 == (toggle % 2))
  {
    setShowOverlays(!showOverlays());
  }

  toggle = (toggle + 1) % 4;
}

void CallbackHandler::cycleImageComponent(int i)
{
  const auto imageUid = m_appData.activeImageUid();
  if (!imageUid)
    return;

  Image* image = m_appData.image(*imageUid);
  if (!image)
    return;

  const int N = static_cast<int>(image->settings().numComponents());
  const int c = static_cast<int>(image->settings().activeComponent());

  image->settings().setActiveComponent(static_cast<uint32_t>((N + c + i) % N));
}

void CallbackHandler::cycleActiveImage(int i)
{
  const auto imageUid = m_appData.activeImageUid();
  if (!imageUid)
    return;

  const auto imageIndex = m_appData.imageIndex(*imageUid);
  if (!imageIndex)
    return;

  const int N = static_cast<int>(m_appData.numImages());
  const int idx = static_cast<int>(*imageIndex);

  const std::size_t newImageIndex = static_cast<size_t>((N + idx + i) % N);

  const auto newImageUid = m_appData.imageUid(newImageIndex);
  if (!newImageUid)
    return;

  m_appData.setActiveImageUid(*newImageUid);
}

/// @todo Put into DataHelper
void CallbackHandler::cycleForegroundSegLabel(int i)
{
  constexpr LabelType k_minLabel = 0;

  LabelType label = static_cast<LabelType>(m_appData.settings().foregroundLabel());
  label = std::max(label + i, k_minLabel);

  if (const auto* table = m_appData.activeLabelTable())
  {
    m_appData.settings().setForegroundLabel(static_cast<size_t>(label), *table);
  }
}

/// @todo Put into DataHelper
void CallbackHandler::cycleBackgroundSegLabel(int i)
{
  constexpr LabelType k_minLabel = 0;

  LabelType label = static_cast<LabelType>(m_appData.settings().backgroundLabel());
  label = std::max(label + i, k_minLabel);

  if (const auto* table = m_appData.activeLabelTable())
  {
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

void CallbackHandler::moveCrosshairsOnViewSlice(const ViewHit& hit, int stepX, int stepY)
{
  if (!hit.view)
    return;

  const glm::vec3 worldRightAxis
    = camera::worldDirection(hit.view->camera(), Directions::View::Right);
  const glm::vec3 worldUpAxis = camera::worldDirection(hit.view->camera(), Directions::View::Up);

  const glm::vec2 moveDistances = data::sliceMoveDistance(
    m_appData, worldRightAxis, worldUpAxis, ImageSelection::VisibleImagesInView, hit.view
  );

  const glm::vec3 worldCrosshairs = m_appData.state().worldCrosshairs().worldOrigin();

  m_appData.state().setWorldCrosshairsPos(
    worldCrosshairs + static_cast<float>(stepX) * moveDistances.x * worldRightAxis
    + static_cast<float>(stepY) * moveDistances.y * worldUpAxis
  );
}

void CallbackHandler::moveCrosshairsToSegLabelCentroid(
  const uuids::uuid& imageUid, std::size_t labelIndex
)
{
  static constexpr uint32_t sk_comp0 = 0;

  const auto activeSegUid = m_appData.imageToActiveSegUid(imageUid);
  if (!activeSegUid)
    return;

  Image* seg = m_appData.seg(*activeSegUid);
  if (!seg)
    return;

  const void* data = seg->bufferAsVoid(sk_comp0);
  const glm::ivec3 dims{seg->header().pixelDimensions()};
  const LabelType label = static_cast<LabelType>(labelIndex);

  std::optional<glm::vec3> pixelCentroid = std::nullopt;

  switch (seg->header().memoryComponentType())
  {
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

  if (!pixelCentroid)
  {
    return;
  }

  const glm::vec4 worldCentroid = seg->transformations().worldDef_T_pixel()
                                  * glm::vec4{*pixelCentroid, 1.0f};
  glm::vec3 worldPos{worldCentroid / worldCentroid.w};

  worldPos = data::snapWorldPointToImageVoxels(m_appData, worldPos);
  m_appData.state().setWorldCrosshairsPos(worldPos);
}

void CallbackHandler::setMouseMode(MouseMode mode)
{
  m_appData.state().setMouseMode(mode);
  //    return m_glfw.cursor( mode );
}

void CallbackHandler::toggleFullScreenMode(bool forceWindowMode)
{
  m_glfw.toggleFullScreenMode(forceWindowMode);
}

bool CallbackHandler::setLockManualImageTransformation(const uuids::uuid& imageUid, bool locked)
{
  Image* image = m_appData.image(imageUid);
  if (!image)
    return false;

  image->transformations().set_worldDef_T_affine_locked(locked);

  // Lock/unlock all of the image's segmentations:
  for (const auto segUid : m_appData.imageToSegUids(imageUid))
  {
    if (auto* seg = m_appData.seg(segUid))
    {
      seg->transformations().set_worldDef_T_affine_locked(locked);
    }
  }

  return true;
}

bool CallbackHandler::syncManualImageTransformation(
  const uuids::uuid& refImageUid, const uuids::uuid& otherImageUid
)
{
  const Image* refImage = m_appData.image(refImageUid);
  if (!refImage)
    return false;

  Image* otherImage = m_appData.image(otherImageUid);
  if (!otherImage)
    return false;

  otherImage->transformations().set_worldDef_T_affine_locked(
    refImage->transformations().is_worldDef_T_affine_locked()
  );
  otherImage->transformations().set_worldDef_T_affine_scale(
    refImage->transformations().get_worldDef_T_affine_scale()
  );
  otherImage->transformations().set_worldDef_T_affine_rotation(
    refImage->transformations().get_worldDef_T_affine_rotation()
  );
  otherImage->transformations().set_worldDef_T_affine_translation(
    refImage->transformations().get_worldDef_T_affine_translation()
  );

  return true;
}

bool CallbackHandler::syncManualImageTransformationOnSegs(const uuids::uuid& imageUid)
{
  const Image* image = m_appData.image(imageUid);
  if (!image)
    return false;

  for (const auto segUid : m_appData.imageToSegUids(imageUid))
  {
    if (auto* seg = m_appData.seg(segUid))
    {
      seg->transformations().set_worldDef_T_affine_locked(
        image->transformations().is_worldDef_T_affine_locked()
      );
      seg->transformations().set_worldDef_T_affine_scale(
        image->transformations().get_worldDef_T_affine_scale()
      );
      seg->transformations().set_worldDef_T_affine_rotation(
        image->transformations().get_worldDef_T_affine_rotation()
      );
      seg->transformations().set_worldDef_T_affine_translation(
        image->transformations().get_worldDef_T_affine_translation()
      );
    }
  }

  return true;
}

bool CallbackHandler::checkAndSetActiveView(const uuids::uuid& viewUid)
{
  if (const auto activeViewUid = m_appData.windowData().activeViewUid())
  {
    if (*activeViewUid != viewUid)
    {
      // There is an active view is not this view
      return false;
    }
  }

  // There is no active view, so set this to be the active view:
  m_appData.windowData().setActiveViewUid(viewUid);
  return true;
}
