#include "logic/app/Data.h"
#include "logic/camera/CameraHelpers.h"
#include "common/UuidUtility.h"

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>

#include <cmrc/cmrc.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <sstream>

CMRC_DECLARE(colormaps);

namespace fs = std::filesystem;

namespace
{
using uuid = uuids::uuid;

template<typename Container>
bool swapElementsAt(Container& values, const std::size_t first, const std::size_t second)
{
  if (first >= values.size() || second >= values.size()) {
    return false;
  }

  auto firstIt = std::begin(values);
  auto secondIt = std::begin(values);
  std::advance(firstIt, static_cast<typename Container::difference_type>(first));
  std::advance(secondIt, static_cast<typename Container::difference_type>(second));

  std::iter_swap(firstIt, secondIt);
  return true;
}

} // namespace

AppData::AppData()
  : m_guiData()
  , m_windowData(m_state.crosshairsState())
  , m_project()
  , m_projectFileName(std::nullopt)
  , m_refImageUid(std::nullopt)
  , m_activeImageUid(std::nullopt)

{
  spdlog::debug("Start loading image color maps");
  loadImageColorMaps();
  spdlog::debug("Done loading label color tables and image color maps");

  // Initialize the IPC handler
  // m_ipcHandler.Attach( IPCHandler::GetUserPreferencesFileName().c_str(),
  //                      (short) IPCMessage::VERSION, sizeof( IPCMessage ) );

  // m_windowData.setWorldCrosshairsProvider([this](){ return m_state.worldCrosshairs(); });
  spdlog::debug("Constructed application data");
}

void AppData::setProject(serialize::EntropyProject projectArg)
{
  m_project = std::move(projectArg);
}

void AppData::setProjectFileName(std::optional<fs::path> fileName)
{
  m_projectFileName = std::move(fileName);
}

void AppData::clearProjectData()
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  m_project = {};
  m_projectFileName = std::nullopt;

  m_images.clear();
  m_imageUidsOrdered.clear();
  m_componentProjectionImages.clear();
  m_imageToComponentProjectionImages.clear();
  m_componentProjectionToSourceImage.clear();
  m_segs.clear();
  m_segUidsOrdered.clear();
  m_defs.clear();
  m_defUidsOrdered.clear();

  m_labelTables.clear();
  m_labelTablesUidsOrdered.clear();
  m_landmarkGroups.clear();
  m_landmarkGroupUidsOrdered.clear();
  m_annotations.clear();

  m_refImageUid = std::nullopt;
  m_activeImageUid = std::nullopt;

  m_imageToSegs.clear();
  m_imageToActiveSeg.clear();
  m_imageToDefs.clear();
  m_imageToActiveInverseWarp.clear();
  m_imageToActiveInverseWarpReferenceImage.clear();
  m_imageToActiveForwardWarp.clear();
  m_imageToLandmarkGroups.clear();
  m_imageToActiveLandmarkGroup.clear();
  m_imageToAnnotations.clear();
  m_imageToActiveAnnotation.clear();
  m_imageToComponentData.clear();
  m_imagesBeingSegmented.clear();
  m_savedViewWorldCenterPositions.clear();

  m_windowData.clearLayouts();
  m_state.setAnimating(false);
  m_state.setProjectLoadState(ProjectLoadState::Empty);
  m_registrationJobs = {};
}

const serialize::EntropyProject& AppData::project() const
{
  return m_project;
}

serialize::EntropyProject& AppData::project()
{
  return m_project;
}

const std::optional<fs::path>& AppData::projectFileName() const
{
  return m_projectFileName;
}

void AppData::loadLinearRampImageColorMaps()
{
  // Create and load the default linear color maps. These are linear ramps with 1024 steps,
  // though only 2 steps are required when linear interpolation is used for the maps.
  // More steps reduce banding artifacts.
  static constexpr std::size_t sk_numSteps = 1024;

  const glm::vec4 black(0.0f, 0.0f, 0.0f, 1.0f);
  const glm::vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
  const glm::vec4 green(0.0f, 1.0f, 0.0f, 1.0f);
  const glm::vec4 blue(0.0f, 0.0f, 1.0f, 1.0f);
  const glm::vec4 yellow(1.0f, 1.0f, 0.0f, 1.0f);
  const glm::vec4 cyan(0.0f, 1.0f, 1.0f, 1.0f);
  const glm::vec4 magenta(1.0f, 0.0f, 1.0f, 1.0f);
  const glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);

  const auto greyMapUid = generateRandomUuid();
  const auto redMapUid = generateRandomUuid();
  const auto greenMapUid = generateRandomUuid();
  const auto blueMapUid = generateRandomUuid();
  const auto yellowMapUid = generateRandomUuid();
  const auto cyanMapUid = generateRandomUuid();
  const auto magentaMapUid = generateRandomUuid();
  const auto constantWhiteMapUid = generateRandomUuid();
  const auto constantRedMapUid = generateRandomUuid();

  m_imageColorMaps.emplace(
    greyMapUid,
    ImageColorMap::createLinearImageColorMap(
      black,
      white,
      sk_numSteps,
      "Linear grey",
      "Linear grey",
      "linear_grey_0-100_n1024"));

  m_imageColorMaps.emplace(
    redMapUid,
    ImageColorMap::createLinearImageColorMap(
      black,
      red,
      sk_numSteps,
      "Linear red",
      "Linear red",
      "linear_red_0-100_n1024"));

  m_imageColorMaps.emplace(
    greenMapUid,
    ImageColorMap::createLinearImageColorMap(
      black,
      green,
      sk_numSteps,
      "Linear green",
      "Linear green",
      "linear_green_0-100_n1024"));

  m_imageColorMaps.emplace(
    blueMapUid,
    ImageColorMap::createLinearImageColorMap(
      black,
      blue,
      sk_numSteps,
      "Linear blue",
      "Linear blue",
      "linear_blue_0-100_n1024"));

  m_imageColorMaps.emplace(
    yellowMapUid,
    ImageColorMap::createLinearImageColorMap(
      black,
      yellow,
      sk_numSteps,
      "Linear yellow",
      "Linear yellow",
      "linear_yellow_0-100_n1024"));

  m_imageColorMaps.emplace(
    cyanMapUid,
    ImageColorMap::createLinearImageColorMap(
      black,
      cyan,
      sk_numSteps,
      "Linear cyan",
      "Linear cyan",
      "linear_cyan_0-100_n1024"));

  m_imageColorMaps.emplace(
    magentaMapUid,
    ImageColorMap::createLinearImageColorMap(
      black,
      magenta,
      sk_numSteps,
      "Linear magenta",
      "Linear magenta",
      "linear_magenta_0-100_n1024"));

  const glm::vec4 transparentBlack{0.0f};

  ImageColorMap constantWhiteMap = ImageColorMap::createLinearImageColorMap(
    white,
    white,
    1024,
    "Constant white",
    "Constant white",
    "constant_white_n1024");

  constantWhiteMap.setInterpolationMode(ImageColorMap::InterpolationMode::Nearest);
  constantWhiteMap.setTransparentBorder(true);
  constantWhiteMap.setColorRGBA(0, transparentBlack); // Not sure why this is needed
  m_imageColorMaps.emplace(constantWhiteMapUid, std::move(constantWhiteMap));

  ImageColorMap constantRedMap =
    ImageColorMap::createLinearImageColorMap(red, red, 1024, "Constant red", "Constant red", "constant_red_n1024");

  constantRedMap.setInterpolationMode(ImageColorMap::InterpolationMode::Nearest);
  constantRedMap.setTransparentBorder(true);
  constantRedMap.setColorRGBA(0, transparentBlack); // Not sure why this is needed
  m_imageColorMaps.emplace(constantRedMapUid, std::move(constantRedMap));

  m_imageColorMapUidsOrdered.push_back(greyMapUid);
  m_imageColorMapUidsOrdered.push_back(redMapUid);
  m_imageColorMapUidsOrdered.push_back(greenMapUid);
  m_imageColorMapUidsOrdered.push_back(blueMapUid);
  m_imageColorMapUidsOrdered.push_back(yellowMapUid);
  m_imageColorMapUidsOrdered.push_back(cyanMapUid);
  m_imageColorMapUidsOrdered.push_back(magentaMapUid);
  m_imageColorMapUidsOrdered.push_back(constantWhiteMapUid);
  m_imageColorMapUidsOrdered.push_back(constantRedMapUid);
}

void AppData::loadDiscreteImageColorMaps()
{
  const glm::vec4 blackTransparent(0.0f, 0.0f, 0.0f, 0.0f);
  const glm::vec4 blackOpaque(0.0f, 0.0f, 0.0f, 1.0f);
  const glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);

  const glm::vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
  const glm::vec4 green(0.0f, 1.0f, 0.0f, 1.0f);
  const glm::vec4 blue(0.0f, 0.0f, 1.0f, 1.0f);
  const glm::vec4 yellow(1.0f, 1.0f, 0.0f, 1.0f);
  const glm::vec4 cyan(0.0f, 1.0f, 1.0f, 1.0f);
  const glm::vec4 magenta(1.0f, 0.0f, 1.0f, 1.0f);

  const auto twMapUid = generateRandomUuid();
  const auto trMapUid = generateRandomUuid();
  const auto kwMapUid = generateRandomUuid();
  const auto krMapUid = generateRandomUuid();
  const auto rgbMapUid = generateRandomUuid();
  const auto rgbyMapUid = generateRandomUuid();
  const auto rgbycmMapUid = generateRandomUuid();
  const auto rygcbmMapUid = generateRandomUuid();
  const auto krgbycmwMapUid = generateRandomUuid();

  m_imageColorMaps.emplace(
    kwMapUid,
    ImageColorMap(
      "Discrete black and white",
      "Black, white discrete color map",
      "Black-white_discrete",
      ImageColorMap::InterpolationMode::Nearest,
      std::vector<glm::vec4>{blackOpaque, white}));

  m_imageColorMaps.emplace(
    krMapUid,
    ImageColorMap(
      "Discrete black and red",
      "Black, red discrete color map",
      "Black-red_discrete",
      ImageColorMap::InterpolationMode::Nearest,
      std::vector<glm::vec4>{blackOpaque, red}));

  m_imageColorMaps.emplace(
    twMapUid,
    ImageColorMap(
      "Discrete transparent and white",
      "Transparent-white discrete color map",
      "Transparent-white_discrete",
      ImageColorMap::InterpolationMode::Nearest,
      std::vector<glm::vec4>{blackTransparent, white}));

  m_imageColorMaps.emplace(
    trMapUid,
    ImageColorMap(
      "Discrete transparent and red",
      "Transparent-red discrete color map",
      "Transparent-red_discrete",
      ImageColorMap::InterpolationMode::Nearest,
      std::vector<glm::vec4>{blackTransparent, red}));

  m_imageColorMaps.emplace(
    rgbMapUid,
    ImageColorMap(
      "Discrete RGB",
      "Red-green-blue discrete color map",
      "Red-green-blue_discrete",
      ImageColorMap::InterpolationMode::Nearest,
      std::vector<glm::vec4>{red, green, blue}));

  m_imageColorMaps.emplace(
    rgbyMapUid,
    ImageColorMap(
      "Discrete RGBY",
      "Red-green-blue-yellow discrete color map",
      "Red-green-blue-yellow_discrete",
      ImageColorMap::InterpolationMode::Nearest,
      std::vector<glm::vec4>{red, green, blue, yellow}));

  m_imageColorMaps.emplace(
    rgbycmMapUid,
    ImageColorMap(
      "Discrete RGBYCM",
      "Red-green-blue-yellow-cyan-magnenta discrete color map",
      "Red-green-blue-yellow-cyan-magenta_discrete",
      ImageColorMap::InterpolationMode::Nearest,
      std::vector<glm::vec4>{red, green, blue, yellow, cyan, magenta}));

  m_imageColorMaps.emplace(
    rygcbmMapUid,
    ImageColorMap(
      "Discrete RYGCBM",
      "Red-yellow-green-cyan-blue-magnenta discrete color map",
      "Red-yellow-green-cyan-blue-magnenta_discrete",
      ImageColorMap::InterpolationMode::Nearest,
      std::vector<glm::vec4>{red, yellow, green, cyan, blue, magenta}));

  m_imageColorMaps.emplace(
    krgbycmwMapUid,
    ImageColorMap(
      "Discrete KRGBYCMW",
      "Black-red-green-blue-yellow-cyan-magnenta-white discrete color map",
      "Black-red-green-blue-yellow-cyan-magenta-white_discrete",
      ImageColorMap::InterpolationMode::Nearest,
      std::vector<glm::vec4>{blackOpaque, red, green, blue, yellow, cyan, magenta, white}));

  m_imageColorMapUidsOrdered.push_back(twMapUid);
  m_imageColorMapUidsOrdered.push_back(trMapUid);
  m_imageColorMapUidsOrdered.push_back(kwMapUid);
  m_imageColorMapUidsOrdered.push_back(krMapUid);
  m_imageColorMapUidsOrdered.push_back(rgbMapUid);
  m_imageColorMapUidsOrdered.push_back(rgbyMapUid);
  m_imageColorMapUidsOrdered.push_back(rgbycmMapUid);
  m_imageColorMapUidsOrdered.push_back(rygcbmMapUid);
  m_imageColorMapUidsOrdered.push_back(krgbycmwMapUid);
}

void AppData::loadImageColorMapsFromDisk()
{
  try {
    spdlog::debug("Begin loading image color maps from disk");

    auto loadMapsFromDir = [this](const std::string& dir) {
      auto filesystem = cmrc::colormaps::get_filesystem();
      auto dirIter = filesystem.iterate_directory(dir);

      for (const auto& i : dirIter) {
        if (!i.is_file()) continue;

        cmrc::file f = filesystem.open(dir + i.filename());
        std::istringstream iss(std::string(f.begin(), f.end()));

        if (auto cmap = ImageColorMap::loadImageColorMap(iss)) {
          const auto uid = generateRandomUuid();
          m_imageColorMaps.emplace(uid, std::move(*cmap));
          m_imageColorMapUidsOrdered.push_back(uid);
        }
      }
    };

    loadMapsFromDir("res/colormaps/matplotlib/");
    loadMapsFromDir("res/colormaps/ncl/");
    loadMapsFromDir("res/colormaps/peter_kovesi/");
  }
  catch (const std::exception& e) {
    spdlog::critical("Exception when loading image colormap file: {}", e.what());
  }
}

void AppData::loadImageColorMaps()
{
  loadLinearRampImageColorMaps();
  loadDiscreteImageColorMaps();
  loadImageColorMapsFromDisk();

  spdlog::debug("Loaded {} image color maps", m_imageColorMaps.size());
}

uuid AppData::addImage(Image imageArg)
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  const std::size_t numComps = imageArg.header().numComponentsPerPixel();

  auto uid = generateRandomUuid();
  m_images.emplace(uid, std::move(imageArg));
  m_imageUidsOrdered.push_back(uid);

  if (1 == m_images.size()) {
    // The first loaded image becomes the reference image and the active image
    m_refImageUid = uid;
    m_activeImageUid = uid;
  }

  // Create the per-component data:
  m_imageToComponentData[uid] = std::vector<ComponentData>(numComps);

  return uid;
}

bool AppData::replaceImage(const uuid& imageUidArg, Image imageArg)
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  auto it = m_images.find(imageUidArg);
  if (std::end(m_images) == it) {
    return false;
  }

  const std::size_t numComps = imageArg.header().numComponentsPerPixel();
  it->second = std::move(imageArg);
  m_imageToComponentData[imageUidArg] = std::vector<ComponentData>(numComps);

  if (const auto projectionsIt = m_imageToComponentProjectionImages.find(imageUidArg);
      projectionsIt != m_imageToComponentProjectionImages.end())
  {
    for (const auto& [mode, projectionUid] : projectionsIt->second) {
      (void)mode;
      m_componentProjectionImages.erase(projectionUid);
      m_componentProjectionToSourceImage.erase(projectionUid);
      m_renderData.m_imageTextures.erase(projectionUid);
      m_renderData.m_imageTextureLayouts.erase(projectionUid);
      m_renderData.m_uniforms.erase(projectionUid);
    }
    m_imageToComponentProjectionImages.erase(projectionsIt);
  }

  return true;
}

std::optional<uuid> AppData::addSeg(Image segArg)
{
  if (!isComponentUnsignedInt(segArg.header().memoryComponentType())) {
    spdlog::error(
      "Segmentation image {} with non-unsigned integer component type {} cannot be added",
      segArg.settings().displayName(),
      segArg.header().memoryComponentTypeAsString());
    return std::nullopt;
  }

  auto uid = generateRandomUuid();
  m_segs.emplace(uid, std::move(segArg));
  m_segUidsOrdered.push_back(uid);
  return uid;
}

std::optional<uuid> AppData::addDef(Image defArg)
{
  if (defArg.header().numComponentsPerPixel() < 3) {
    spdlog::error(
      "Warp field image {} with only {} components cannot be added",
      defArg.settings().displayName(),
      defArg.header().numComponentsPerPixel());
    return std::nullopt;
  }

  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  const std::size_t numComps = defArg.header().numComponentsPerPixel();
  auto uid = generateRandomUuid();
  m_images.emplace(uid, std::move(defArg));
  m_imageUidsOrdered.push_back(uid);
  m_defUidsOrdered.push_back(uid);
  m_imageToComponentData[uid] = std::vector<ComponentData>(numComps);

  return uid;
}

uuid AppData::addLandmarkGroup(const LandmarkGroup& lmGroup)
{
  auto uid = generateRandomUuid();
  m_landmarkGroups.emplace(uid, std::move(lmGroup));
  m_landmarkGroupUidsOrdered.push_back(uid);
  return uid;
}

bool AppData::removeLandmarkGroup(const uuid& lmGroupUid)
{
  if (!landmarkGroup(lmGroupUid)) {
    return false;
  }

  m_landmarkGroups.erase(lmGroupUid);
  m_landmarkGroupUidsOrdered.erase(
    std::remove(std::begin(m_landmarkGroupUidsOrdered), std::end(m_landmarkGroupUidsOrdered), lmGroupUid),
    std::end(m_landmarkGroupUidsOrdered));

  for (auto imageIt = std::begin(m_imageToLandmarkGroups); imageIt != std::end(m_imageToLandmarkGroups);) {
    auto& landmarkGroupUids = imageIt->second;
    landmarkGroupUids.erase(
      std::remove(std::begin(landmarkGroupUids), std::end(landmarkGroupUids), lmGroupUid),
      std::end(landmarkGroupUids));

    auto activeIt = m_imageToActiveLandmarkGroup.find(imageIt->first);
    if (activeIt != std::end(m_imageToActiveLandmarkGroup) && activeIt->second == lmGroupUid) {
      if (landmarkGroupUids.empty()) {
        m_imageToActiveLandmarkGroup.erase(activeIt);
      }
      else {
        activeIt->second = landmarkGroupUids.front();
      }
    }

    if (landmarkGroupUids.empty()) {
      m_imageToActiveLandmarkGroup.erase(imageIt->first);
      imageIt = m_imageToLandmarkGroups.erase(imageIt);
    }
    else {
      ++imageIt;
    }
  }

  return true;
}

std::optional<uuid> AppData::addAnnotation(const uuid& imageUidArg, const Annotation& annotationArg)
{
  if (!image(imageUidArg)) {
    return std::nullopt; // invalid image UID
  }

  auto annotUid = generateRandomUuid();
  m_annotations.emplace(annotUid, std::move(annotationArg));
  m_imageToAnnotations[imageUidArg].emplace_back(annotUid);

  // If this is the first annotation or there is no active annotation for the image,
  // then make this the active annotation:
  if (1 == m_imageToAnnotations[imageUidArg].size() || !imageToActiveAnnotationUid(imageUidArg)) {
    assignActiveAnnotationUidToImage(imageUidArg, annotUid);
  }

  return annotUid;
}

bool AppData::addDistanceMap(
  const uuid& imageUidArg,
  ComponentIndexType component,
  Image distanceMap,
  double boundaryIsoValue)
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  const Image* img = image(imageUidArg);
  if (!img) {
    return false; // invalid image UID
  }

  const uint32_t numComps = img->header().numComponentsPerPixel();
  if (component >= numComps) {
    spdlog::error("Invalid component {} for image {}. Cannot set distance map for it.", component, imageUidArg);
    return false;
  }

  auto compDataIt = m_imageToComponentData.find(imageUidArg);
  if (std::end(m_imageToComponentData) != compDataIt) {
    if (component >= compDataIt->second.size()) {
      compDataIt->second.resize(numComps);
    }

    // For now, allow only one distance map:
    compDataIt->second.at(component).m_distanceMaps.clear();
    compDataIt->second.at(component).m_distanceMaps.emplace(boundaryIsoValue, std::move(distanceMap));
    return true;
  }
  else {
    spdlog::error("No component data for image {}. Cannot set distance map.", imageUidArg);
    return false;
  }

  return false;
}

bool AppData::removeDistanceMaps(const uuid& imageUidArg, ComponentIndexType component)
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  const Image* img = image(imageUidArg);
  if (!img) {
    return false;
  }

  if (component >= img->header().numComponentsPerPixel()) {
    spdlog::error("Invalid component {} for image {}. Cannot remove distance maps for it.", component, imageUidArg);
    return false;
  }

  auto compDataIt = m_imageToComponentData.find(imageUidArg);
  if (std::end(m_imageToComponentData) == compDataIt) {
    spdlog::error("No component data for image {}. Cannot remove distance maps.", imageUidArg);
    return false;
  }

  if (component >= compDataIt->second.size()) {
    return false;
  }

  compDataIt->second.at(component).m_distanceMaps.clear();
  return true;
}

bool AppData::addNoiseEstimate(
  const uuid& imageUidArg,
  ComponentIndexType component,
  Image noiseEstimate,
  uint32_t radius)
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  const Image* img = image(imageUidArg);
  if (!img) {
    return false; // invalid image UID
  }

  const uint32_t numComps = img->header().numComponentsPerPixel();
  if (component >= numComps) {
    spdlog::error("Invalid component {} for image {}. Cannot set noise estimate for it.", component, imageUidArg);
    return false;
  }

  auto compDataIt = m_imageToComponentData.find(imageUidArg);
  if (std::end(m_imageToComponentData) != compDataIt) {
    if (component >= compDataIt->second.size()) {
      compDataIt->second.resize(numComps);
    }

    // For now, allow only one noise estimate image:
    compDataIt->second.at(component).m_noiseEstimates.clear();
    compDataIt->second.at(component).m_noiseEstimates.emplace(radius, std::move(noiseEstimate));
    return true;
  }
  else {
    spdlog::error("No component data for image {}. Cannot set noise estimate.", imageUidArg);
    return false;
  }

  return false;
}

std::size_t AppData::addLabelColorTable(std::size_t numLabels, std::size_t maxNumLabels)
{
  const auto uid = generateRandomUuid();
  m_labelTables.try_emplace(uid, numLabels, maxNumLabels);
  m_labelTablesUidsOrdered.push_back(uid);

  return (m_labelTables.size() - 1);
}

std::optional<uuid> AppData::addIsosurface(const uuid& imageUidArg, ComponentIndexType comp, Isosurface isosurfaceArg)
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  const Image* img = image(imageUidArg);
  if (!img) {
    spdlog::error("Cannot add isosurface to invalid image {}", imageUidArg);
    return std::nullopt;
  }

  const uint32_t numComps = img->header().numComponentsPerPixel();
  if (comp >= numComps) {
    spdlog::error("Cannot add isosurface to invalid component {} of image {}", comp, imageUidArg);
    return std::nullopt;
  }

  auto it = m_imageToComponentData.find(imageUidArg);
  if (std::end(m_imageToComponentData) == it) {
    spdlog::error("No component data for image {}; cannot add isosurface", imageUidArg);
    return std::nullopt;
  }

  if (comp >= it->second.size()) {
    it->second.resize(numComps);
  }

  const auto uid = generateRandomUuid();
  auto& data = it->second.at(comp);
  data.m_isosurfaceUidsSorted.push_back(uid);
  data.m_isosurfaces.emplace(uid, std::move(isosurfaceArg));
  return uid;
}

bool AppData::removeImage(const uuid& imageUidArg)
{
  if (!image(imageUidArg)) {
    return false;
  }

  if (m_refImageUid && *m_refImageUid == imageUidArg) {
    return false;
  }

  auto imageOrderIt = std::find(std::begin(m_imageUidsOrdered), std::end(m_imageUidsOrdered), imageUidArg);
  if (std::end(m_imageUidsOrdered) == imageOrderIt) {
    return false;
  }

  const auto imageSegs = imageToSegUids(imageUidArg);
  const auto imageDefs = imageToDefUids(imageUidArg);
  const auto imageLandmarkGroups = imageToLandmarkGroupUids(imageUidArg);
  const auto imageAnnotations = annotationsForImage(imageUidArg);

  m_images.erase(imageUidArg);
  m_imageUidsOrdered.erase(imageOrderIt);
  m_defs.erase(imageUidArg);
  m_defUidsOrdered.erase(
    std::remove(std::begin(m_defUidsOrdered), std::end(m_defUidsOrdered), imageUidArg),
    std::end(m_defUidsOrdered));
  removeWarpReferences(imageUidArg);

  if (const auto projectionsIt = m_imageToComponentProjectionImages.find(imageUidArg);
      projectionsIt != m_imageToComponentProjectionImages.end())
  {
    for (const auto& [mode, projectionUid] : projectionsIt->second) {
      (void)mode;
      m_componentProjectionImages.erase(projectionUid);
      m_componentProjectionToSourceImage.erase(projectionUid);
      m_renderData.m_imageTextures.erase(projectionUid);
      m_renderData.m_imageTextureLayouts.erase(projectionUid);
      m_renderData.m_uniforms.erase(projectionUid);
    }
    m_imageToComponentProjectionImages.erase(projectionsIt);
  }

  m_imageToSegs.erase(imageUidArg);
  m_imageToActiveSeg.erase(imageUidArg);
  m_imageToDefs.erase(imageUidArg);
  m_imageToActiveInverseWarp.erase(imageUidArg);
  m_imageToActiveInverseWarpReferenceImage.erase(imageUidArg);
  for (auto it = m_imageToActiveInverseWarpReferenceImage.begin();
       it != m_imageToActiveInverseWarpReferenceImage.end();)
  {
    if (it->second == imageUidArg) {
      it = m_imageToActiveInverseWarpReferenceImage.erase(it);
    }
    else {
      ++it;
    }
  }
  m_imageToActiveForwardWarp.erase(imageUidArg);
  m_imageToLandmarkGroups.erase(imageUidArg);
  m_imageToActiveLandmarkGroup.erase(imageUidArg);
  m_imageToAnnotations.erase(imageUidArg);
  m_imageToActiveAnnotation.erase(imageUidArg);
  m_imageToComponentData.erase(imageUidArg);
  m_imagesBeingSegmented.erase(imageUidArg);

  if (m_activeImageUid && *m_activeImageUid == imageUidArg) {
    m_activeImageUid =
      m_refImageUid ? m_refImageUid
                    : (!m_imageUidsOrdered.empty() ? std::optional<uuid>{m_imageUidsOrdered.front()} : std::nullopt);
  }

  for (const auto& segUidLocal : imageSegs) {
    bool stillUsed = false;
    for (const auto& [otherImageUid, segUids] : m_imageToSegs) {
      if (otherImageUid == imageUidArg) {
        continue;
      }

      if (std::find(std::begin(segUids), std::end(segUids), segUidLocal) != std::end(segUids)) {
        stillUsed = true;
        break;
      }
    }

    if (!stillUsed) {
      removeSeg(segUidLocal);
    }
  }

  for (const auto& defUidLocal : imageDefs) {
    bool stillUsed = false;
    for (const auto& [otherImageUid, defUids] : m_imageToDefs) {
      if (otherImageUid == imageUidArg) {
        continue;
      }

      if (std::find(std::begin(defUids), std::end(defUids), defUidLocal) != std::end(defUids)) {
        stillUsed = true;
        break;
      }
    }

    if (!stillUsed) {
      removeDef(defUidLocal);
    }
  }

  for (const auto& annotUid : imageAnnotations) {
    removeAnnotation(annotUid);
  }

  for (const auto& lmGroupUid : imageLandmarkGroups) {
    bool stillUsed = false;
    for (const auto& [otherImageUid, lmGroupUids] : m_imageToLandmarkGroups) {
      if (otherImageUid == imageUidArg) {
        continue;
      }

      if (std::find(std::begin(lmGroupUids), std::end(lmGroupUids), lmGroupUid) != std::end(lmGroupUids)) {
        stillUsed = true;
        break;
      }
    }

    if (!stillUsed) {
      removeLandmarkGroup(lmGroupUid);
    }
  }

  return true;
}

bool AppData::removeSeg(const uuid& segUidArg)
{
  auto segMapIt = m_segs.find(segUidArg);
  if (std::end(m_segs) != segMapIt) {
    // Remove the segmentation
    m_segs.erase(segMapIt);
  }
  else {
    // This segmentation does not exist
    return false;
  }

  auto segVecIt = std::find(std::begin(m_segUidsOrdered), std::end(m_segUidsOrdered), segUidArg);
  if (std::end(m_segUidsOrdered) != segVecIt) {
    m_segUidsOrdered.erase(segVecIt);
  }
  else {
    return false;
  }

  // Remove segmentation from image-to-segmentation map for all images
  for (auto& m : m_imageToSegs) {
    m.second.erase(std::remove(std::begin(m.second), std::end(m.second), segUidArg), std::end(m.second));
  }

  // Remove it as an active segmentation
  for (auto it = std::begin(m_imageToActiveSeg); it != std::end(m_imageToActiveSeg);) {
    if (segUidArg == it->second) {
      const auto imageUidLocal = it->first;

      it = m_imageToActiveSeg.erase(it);

      // Set a new active segmentation for this image, if one exists
      if (m_imageToSegs.count(imageUidLocal) > 0) {
        if (!m_imageToSegs[imageUidLocal].empty()) {
          // Set the image's first segmentation as its active one
          m_imageToActiveSeg[imageUidLocal] = m_imageToSegs[imageUidLocal][0];
        }
      }
    }
    else {
      ++it;
    }
  }

  return true;
}

bool AppData::removeDef(const uuid& defUidArg)
{
  auto defMapIt = m_defs.find(defUidArg);
  if (std::end(m_defs) != defMapIt) {
    // Remove the deformation
    m_defs.erase(defMapIt);
  }

  auto defVecIt = std::find(std::begin(m_defUidsOrdered), std::end(m_defUidsOrdered), defUidArg);
  if (std::end(m_defUidsOrdered) != defVecIt) {
    m_defUidsOrdered.erase(defVecIt);
  }
  else {
    return false;
  }

  if (const auto imageIt = std::find(std::begin(m_imageUidsOrdered), std::end(m_imageUidsOrdered), defUidArg);
      std::end(m_imageUidsOrdered) != imageIt)
  {
    m_images.erase(defUidArg);
    m_imageUidsOrdered.erase(imageIt);
    m_imageToComponentData.erase(defUidArg);
    m_renderData.m_imageTextures.erase(defUidArg);
    m_renderData.m_imageTextureLayouts.erase(defUidArg);
    m_renderData.m_uniforms.erase(defUidArg);
  }

  // Remove all image warp assignments that reference this field.
  removeWarpReferences(defUidArg);

  return true;
}

void AppData::removeWarpReferences(const uuid& warpUid)
{
  for (auto& [imageUidLocal, warpUids] : m_imageToDefs) {
    (void)imageUidLocal;
    warpUids.erase(std::remove(std::begin(warpUids), std::end(warpUids), warpUid), std::end(warpUids));
  }

  for (auto it = std::begin(m_imageToActiveInverseWarp); it != std::end(m_imageToActiveInverseWarp);) {
    if (warpUid == it->second) {
      it = m_imageToActiveInverseWarp.erase(it);
    }
    else {
      ++it;
    }
  }

  for (auto it = m_imageToActiveInverseWarpReferenceImage.begin();
       it != m_imageToActiveInverseWarpReferenceImage.end();)
  {
    if (m_imageToActiveInverseWarp.find(it->first) == m_imageToActiveInverseWarp.end()) {
      it = m_imageToActiveInverseWarpReferenceImage.erase(it);
    }
    else {
      ++it;
    }
  }

  for (auto it = std::begin(m_imageToActiveForwardWarp); it != std::end(m_imageToActiveForwardWarp);) {
    if (warpUid == it->second) {
      it = m_imageToActiveForwardWarp.erase(it);
    }
    else {
      ++it;
    }
  }
}

bool AppData::removeAnnotation(const uuid& annotUid)
{
  auto annotMapIt = m_annotations.find(annotUid);
  if (std::end(m_annotations) != annotMapIt) {
    // Remove the annotation
    m_annotations.erase(annotMapIt);
  }
  else {
    // This deformation does not exist
    return false;
  }

  // Remove annotation from image-to-annotation map
  for (auto& p : m_imageToAnnotations) {
    p.second.erase(std::remove(std::begin(p.second), std::end(p.second), annotUid), std::end(p.second));
  }

  // Remove it as the active annotation
  for (auto it = std::begin(m_imageToActiveAnnotation); it != std::end(m_imageToActiveAnnotation);) {
    if (annotUid == it->second) {
      it = m_imageToActiveAnnotation.erase(it);
    }
    else {
      ++it;
    }
  }

  return true;
}

bool AppData::removeIsosurface(const uuid& imageUidArg, ComponentIndexType comp, const uuid& isosurfaceUid)
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  const Image* img = image(imageUidArg);
  if (!img) {
    spdlog::error("Cannot remove isosurface from invalid image {}", imageUidArg);
    return false;
  }

  if (comp >= img->header().numComponentsPerPixel()) {
    spdlog::error("Cannot remove isosurface from invalid component {} of image {}", comp, imageUidArg);
    return false;
  }

  auto it = m_imageToComponentData.find(imageUidArg);
  if (std::end(m_imageToComponentData) == it || comp >= it->second.size()) {
    return false;
  }

  auto& data = it->second.at(comp);

  data.m_isosurfaceUidsSorted.erase(
    std::remove(data.m_isosurfaceUidsSorted.begin(), data.m_isosurfaceUidsSorted.end(), isosurfaceUid),
    data.m_isosurfaceUidsSorted.end());

  return (data.m_isosurfaces.erase(isosurfaceUid) > 0);
}

const Image* AppData::image(const uuid& imageUidArg) const
{
  auto it = m_images.find(imageUidArg);
  if (std::end(m_images) != it) {
    return &it->second;
  }

  auto projectionIt = m_componentProjectionImages.find(imageUidArg);
  if (std::end(m_componentProjectionImages) != projectionIt) {
    return &projectionIt->second;
  }

  return nullptr;
}

Image* AppData::image(const uuid& imageUidArg)
{
  return const_cast<Image*>(const_cast<const AppData*>(this)->image(imageUidArg));
}

std::optional<uuid> AppData::setComponentProjectionImage(
  const uuid& imageUidArg,
  ComponentProjectionMode mode,
  uint32_t timePoint,
  Image imageArg)
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  if (m_images.find(imageUidArg) == m_images.end()) {
    return std::nullopt;
  }

  auto& projections = m_imageToComponentProjectionImages[imageUidArg];
  const ComponentProjectionCacheKey key{mode, timePoint};
  if (const auto projectionIt = projections.find(key); projectionIt != projections.end()) {
    m_componentProjectionImages.insert_or_assign(projectionIt->second, std::move(imageArg));
    m_componentProjectionToSourceImage[projectionIt->second] = imageUidArg;
    return projectionIt->second;
  }

  const uuid projectionUid = generateRandomUuid();
  projections.emplace(key, projectionUid);
  m_componentProjectionImages.emplace(projectionUid, std::move(imageArg));
  m_componentProjectionToSourceImage[projectionUid] = imageUidArg;
  return projectionUid;
}

std::optional<uuid>
AppData::componentProjectionImageUid(const uuid& imageUidArg, ComponentProjectionMode mode, uint32_t timePoint) const
{
  const auto projectionsIt = m_imageToComponentProjectionImages.find(imageUidArg);
  if (m_imageToComponentProjectionImages.end() == projectionsIt) {
    return std::nullopt;
  }

  const auto projectionIt = projectionsIt->second.find(ComponentProjectionCacheKey{mode, timePoint});
  if (projectionsIt->second.end() == projectionIt) {
    return std::nullopt;
  }

  if (m_componentProjectionImages.end() == m_componentProjectionImages.find(projectionIt->second)) {
    return std::nullopt;
  }

  return projectionIt->second;
}

std::optional<uuid> AppData::componentProjectionSourceImageUid(const uuid& projectionUid) const
{
  const auto projectionIt = m_componentProjectionToSourceImage.find(projectionUid);
  if (m_componentProjectionToSourceImage.end() == projectionIt) {
    return std::nullopt;
  }

  if (m_images.end() == m_images.find(projectionIt->second)) {
    return std::nullopt;
  }

  return projectionIt->second;
}

uuid AppData::effectiveImageUidForRendering(const uuid& imageUidArg) const
{
  const auto imageIt = m_images.find(imageUidArg);
  if (m_images.end() == imageIt) {
    return imageUidArg;
  }

  const auto projectionMode = componentProjectionForImage(imageIt->second);
  if (!projectionMode) {
    return imageUidArg;
  }

  const uint32_t timePoint = imageIt->second.timeAxis().clamp(imageIt->second.settings().activeTimePoint());
  return componentProjectionImageUid(imageUidArg, *projectionMode, timePoint).value_or(imageUidArg);
}

/*
auto result = appData.getImage(someUuid);
if (result) {
    const Image& img = result->get();  // or just: *result
    // use img...
} else {
    std::cerr << result.error() << '\n';
}
*/

std::expected<std::reference_wrapper<const Image>, std::string> AppData::getImage(const uuid& imageUidArg) const
{
  auto it = m_images.find(imageUidArg);
  if (std::end(m_images) != it) {
    return std::cref(it->second);
  }
  return std::unexpected(std::format("Image {} does not exist", to_string(imageUidArg)));
}

std::expected<std::reference_wrapper<Image>, std::string> AppData::getImage(const uuid& imageUidArg)
{
  const auto result = const_cast<const AppData*>(this)->getImage(imageUidArg);
  if (!result) {
    return std::unexpected(result.error());
  }
  return std::ref(const_cast<Image&>(result->get()));
}

const Image* AppData::seg(const uuid& segUidArg) const
{
  auto it = m_segs.find(segUidArg);
  if (std::end(m_segs) != it) {
    return &it->second;
  }
  return nullptr;
}

Image* AppData::seg(const uuid& segUidArg)
{
  return const_cast<Image*>(const_cast<const AppData*>(this)->seg(segUidArg));
}

const Image* AppData::def(const uuid& defUidArg) const
{
  auto it = m_defs.find(defUidArg);
  if (std::end(m_defs) != it) return &it->second;

  if (std::find(std::begin(m_defUidsOrdered), std::end(m_defUidsOrdered), defUidArg) != std::end(m_defUidsOrdered)) {
    return image(defUidArg);
  }

  return nullptr;
}

Image* AppData::def(const uuid& defUidArg)
{
  return const_cast<Image*>(const_cast<const AppData*>(this)->def(defUidArg));
}

const Image* AppData::warpField(const uuid& warpUid) const
{
  if (const Image* defImage = def(warpUid)) {
    return defImage;
  }

  const Image* imageLocal = this->image(warpUid);
  if (imageLocal && isVectorFieldCandidate(*imageLocal)) {
    return imageLocal;
  }

  return nullptr;
}

Image* AppData::warpField(const uuid& warpUid)
{
  return const_cast<Image*>(const_cast<const AppData*>(this)->warpField(warpUid));
}

const std::map<double, Image>& AppData::distanceMaps(const uuid& imageUidArg, ComponentIndexType component) const
{
  // Map of distance maps (keyed by isosurface value) for the component:
  static const std::map<double, Image> EMPTY;

  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  auto compDataIt = m_imageToComponentData.find(imageUidArg);
  if (std::end(m_imageToComponentData) != compDataIt) {
    if (component < compDataIt->second.size()) {
      return compDataIt->second.at(component).m_distanceMaps;
    }
    else {
      spdlog::error("Invalid component {} for image {}. Cannot get distance map for it.", component, imageUidArg);
      return EMPTY;
    }
  }
  else {
    if (m_componentProjectionImages.find(imageUidArg) != m_componentProjectionImages.end()) {
      return EMPTY;
    }
    spdlog::error("No component data for image {}. Cannot get distance map for it.", imageUidArg);
    return EMPTY;
  }
}

const std::map<uint32_t, Image>& AppData::noiseEstimates(const uuid& imageUidArg, ComponentIndexType component) const
{
  // Map of noise estimates (keyed by radius value) for the component:
  static const std::map<uint32_t, Image> EMPTY;

  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  auto compDataIt = m_imageToComponentData.find(imageUidArg);

  if (std::end(m_imageToComponentData) != compDataIt) {
    if (component < compDataIt->second.size()) {
      return compDataIt->second.at(component).m_noiseEstimates;
    }
    else {
      spdlog::error("Invalid component {} for image {}. Cannot get noise estimate for it.", component, imageUidArg);
      return EMPTY;
    }
  }
  else {
    if (m_componentProjectionImages.find(imageUidArg) != m_componentProjectionImages.end()) {
      return EMPTY;
    }
    spdlog::error("No component data for image {}. Cannot get noise estimate for it.", imageUidArg);
    return EMPTY;
  }
}

const Isosurface* AppData::isosurface(const uuid& imageUidArg, ComponentIndexType comp, const uuid& isosurfaceUid) const
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  // const Image* img = image(imageUid);
  // if (!img)
  // {
  //   spdlog::error("Cannot get isosurface from invalid image {}.", imageUid);
  //   return nullptr;
  // }

  const auto result = getImage(imageUidArg);
  if (!result) {
    spdlog::warn("Cannot get isosurface: {}", result.error());
    return nullptr;
  }

  const Image& img = result->get();

  if (comp >= img.header().numComponentsPerPixel()) {
    spdlog::error("Cannot get isosurface from invalid component {} of image {}", comp, imageUidArg);
    return nullptr;
  }

  auto it = m_imageToComponentData.find(imageUidArg);
  if (std::end(m_imageToComponentData) == it || comp >= it->second.size()) {
    return nullptr;
  }

  return &(it->second.at(comp).m_isosurfaces.at(isosurfaceUid));
}

Isosurface* AppData::isosurface(const uuid& imageUidArg, ComponentIndexType comp, const uuid& isosurfaceUid)
{
  return const_cast<Isosurface*>(const_cast<const AppData*>(this)->isosurface(imageUidArg, comp, isosurfaceUid));
}

const ImageColorMap* AppData::imageColorMap(const uuid& colorMapUid) const
{
  auto it = m_imageColorMaps.find(colorMapUid);
  if (std::end(m_imageColorMaps) != it) return &it->second;
  return nullptr;
}

ImageColorMap* AppData::imageColorMap(const uuid& colorMapUid)
{
  return const_cast<ImageColorMap*>(const_cast<const AppData*>(this)->imageColorMap(colorMapUid));
}

const ParcellationLabelTable* AppData::labelTable(const uuid& tableUid) const
{
  auto it = m_labelTables.find(tableUid);
  if (std::end(m_labelTables) != it) return &it->second;
  return nullptr;
}

ParcellationLabelTable* AppData::labelTable(const uuid& tableUid)
{
  return const_cast<ParcellationLabelTable*>(const_cast<const AppData*>(this)->labelTable(tableUid));
}

const LandmarkGroup* AppData::landmarkGroup(const uuid& lmGroupUid) const
{
  auto it = m_landmarkGroups.find(lmGroupUid);
  if (std::end(m_landmarkGroups) != it) return &it->second;
  return nullptr;
}

LandmarkGroup* AppData::landmarkGroup(const uuid& lmGroupUid)
{
  return const_cast<LandmarkGroup*>(const_cast<const AppData*>(this)->landmarkGroup(lmGroupUid));
}

const Annotation* AppData::annotation(const uuid& annotUid) const
{
  auto it = m_annotations.find(annotUid);
  if (std::end(m_annotations) != it) return &it->second;
  return nullptr;
}

Annotation* AppData::annotation(const uuid& annotUid)
{
  return const_cast<Annotation*>(const_cast<const AppData*>(this)->annotation(annotUid));
}

std::optional<uuid> AppData::refImageUid() const
{
  return m_refImageUid;
}

bool AppData::setRefImageUid(const uuid& uid)
{
  if (!image(uid)) {
    return false;
  }

  m_refImageUid = uid;

  auto it = std::find(m_imageUidsOrdered.begin(), m_imageUidsOrdered.end(), uid);
  if (m_imageUidsOrdered.end() != it && m_imageUidsOrdered.begin() != it) {
    m_imageUidsOrdered.erase(it);
    m_imageUidsOrdered.insert(m_imageUidsOrdered.begin(), uid);
  }

  return true;
}

std::optional<uuid> AppData::activeImageUid() const
{
  return m_activeImageUid;
}

bool AppData::setActiveImageUid(const uuid& uid)
{
  if (image(uid)) {
    m_activeImageUid = uid;

    if (const auto* table = activeLabelTable()) {
      m_settings.adjustActiveSegmentationLabels(*table);
    }
    return true;
  }

  return false;
}

void AppData::setRainbowColorsForAllImages()
{
  static constexpr float sk_colorSat = 0.65f;
  static constexpr float sk_colorVal = 0.90f;

  // Starting color hue, where hues repeat cyclically over range [0.0, 1.0]
  static constexpr float sk_startHue = -1.0f / 48.0f;

  const float N = static_cast<float>(m_imageUidsOrdered.size());
  std::size_t i = 0;

  for (const auto& imageUidLocal : m_imageUidsOrdered) {
    if (Image* img = image(imageUidLocal)) {
      const float a = (1.0f + sk_startHue + static_cast<float>(i) / N);

      float intPart = std::numeric_limits<float>::quiet_NaN();
      const float fractPart = std::modf(a, &intPart);

      const float hue = 360.0f * fractPart;
      const glm::vec3 color = glm::rgbColor(glm::vec3{hue, sk_colorSat, sk_colorVal});

      img->settings().setBorderColor(color);

      img->settings().setEdgeColor(color);
    }
    ++i;
  }
}

void AppData::setRainbowColorsForAllLandmarkGroups()
{
  // Landmark group color is set to image border color
  for (const auto imageUidLocal : m_imageUidsOrdered) {
    const Image* img = image(imageUidLocal);
    if (!img) continue;

    for (const auto lmGroupUid : imageToLandmarkGroupUids(imageUidLocal)) {
      if (auto* lmGroup = landmarkGroup(lmGroupUid)) {
        lmGroup->setColorOverride(true);
        lmGroup->setColor(img->settings().borderColor());
      }
    }
  }
}

bool AppData::moveImageBackwards(const uuid& imageUidArg)
{
  const auto index = imageIndex(imageUidArg);
  if (!index) return false;

  const std::size_t i = *index;

  // Only allow moving backwards images with index 2 or greater, because
  // image 1 cannot become 0: that is the reference image index.
  if (2 <= i) {
    return swapElementsAt(m_imageUidsOrdered, i - 1, i);
  }

  return false;
}

bool AppData::moveImageForwards(const uuid& imageUidArg)
{
  const auto index = imageIndex(imageUidArg);
  if (!index) return false;

  const std::size_t i = *index;
  const std::size_t N = m_imageUidsOrdered.size();

  if (0 == N) {
    return false;
  }

  // Do not allow moving the reference image or the last image:
  if (0 < i && i < N - 1) {
    return swapElementsAt(m_imageUidsOrdered, i, i + 1);
  }

  return false;
}

bool AppData::moveImageToBack(const uuid& imageUidArg)
{
  auto index = imageIndex(imageUidArg);
  if (!index) return false;

  while (index && *index > 1) {
    if (!moveImageBackwards(imageUidArg)) {
      return false;
    }

    index = imageIndex(imageUidArg);
  }

  return true;
}

bool AppData::moveImageToFront(const uuid& imageUidArg)
{
  auto index = imageIndex(imageUidArg);
  if (!index) return false;

  const std::size_t N = m_imageUidsOrdered.size();

  if (0 == N) {
    return false;
  }

  while (index && *index < N - 1) {
    if (!moveImageForwards(imageUidArg)) {
      return false;
    }

    index = imageIndex(imageUidArg);
  }

  return true;
}

bool AppData::moveAnnotationBackwards(const uuid& imageUidArg, const uuid& annotUid)
{
  const auto index = annotationIndex(imageUidArg, annotUid);
  if (!index) return false;

  const std::size_t i = *index;

  // Only allow moving backwards annotations with index 1 or greater
  if (0 == i) {
    // Already the backmost index
    return true;
  }
  auto& annotList = m_imageToAnnotations.at(imageUidArg);
  return swapElementsAt(annotList, i - 1, i);
}

bool AppData::moveAnnotationForwards(const uuid& imageUidArg, const uuid& annotUid)
{
  const auto index = annotationIndex(imageUidArg, annotUid);
  if (!index) return false;

  const std::size_t i = *index;

  auto& annotList = m_imageToAnnotations.at(imageUidArg);
  const std::size_t N = annotList.size();

  if (i == N - 1) {
    // Already the frontmost index
    return true;
  }
  else if (i <= N - 2) {
    return swapElementsAt(annotList, i, i + 1);
  }

  return false;
}

bool AppData::moveAnnotationToBack(const uuid& imageUidArg, const uuid& annotUid)
{
  auto index = annotationIndex(imageUidArg, annotUid);
  if (!index) return false;

  while (index && *index >= 1) {
    if (!moveAnnotationBackwards(imageUidArg, annotUid)) {
      return false;
    }

    index = annotationIndex(imageUidArg, annotUid);
  }

  return true;
}

bool AppData::moveAnnotationToFront(const uuid& imageUidArg, const uuid& annotUid)
{
  auto index = annotationIndex(imageUidArg, annotUid);
  if (!index) return false;

  const auto& annotList = m_imageToAnnotations.at(imageUidArg);
  const long N = static_cast<long>(annotList.size());

  while (index && static_cast<long>(*index) < N - 1) {
    if (!moveAnnotationForwards(imageUidArg, annotUid)) {
      return false;
    }

    index = annotationIndex(imageUidArg, annotUid);
  }

  return true;
}

std::size_t AppData::numImages() const
{
  return m_images.size();
}
std::size_t AppData::numSegs() const
{
  return m_segs.size();
}
std::size_t AppData::numDefs() const
{
  return m_defUidsOrdered.size();
}
std::size_t AppData::numImageColorMaps() const
{
  return m_imageColorMaps.size();
}
std::size_t AppData::numLabelTables() const
{
  return m_labelTables.size();
}
std::size_t AppData::numLandmarkGroups() const
{
  return m_landmarkGroups.size();
}
std::size_t AppData::numAnnotations() const
{
  return m_annotations.size();
}

const uuid_range_t& AppData::imageUidsOrdered() const
{
  return m_imageUidsOrdered;
}

const uuid_range_t& AppData::segUidsOrdered() const
{
  return m_segUidsOrdered;
}

const uuid_range_t& AppData::defUidsOrdered() const
{
  return m_defUidsOrdered;
}

uuid_range_t AppData::warpFieldCandidateUidsOrdered() const
{
  uuid_range_t candidates = m_defUidsOrdered;

  for (const uuid& imageUidLocal : m_imageUidsOrdered) {
    if (std::find(std::begin(candidates), std::end(candidates), imageUidLocal) != std::end(candidates)) {
      continue;
    }

    const Image* imageLocal = this->image(imageUidLocal);
    if (imageLocal && isVectorFieldCandidate(*imageLocal)) {
      candidates.push_back(imageUidLocal);
    }
  }

  return candidates;
}

const uuid_range_t& AppData::imageColorMapUidsOrdered() const
{
  return m_imageColorMapUidsOrdered;
}

const uuid_range_t& AppData::labelTableUidsOrdered() const
{
  return m_labelTablesUidsOrdered;
}

const uuid_range_t& AppData::landmarkGroupUidsOrdered() const
{
  return m_landmarkGroupUidsOrdered;
}

uuid_range_t AppData::isosurfaceUids(const uuid& imageUidArg, ComponentIndexType comp) const
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  const Image* img = image(imageUidArg);
  if (!img) {
    spdlog::error("Cannot remove isosurface from invalid image {}", imageUidArg);
    return {};
  }

  if (comp >= img->header().numComponentsPerPixel()) {
    return {};
  }

  auto it = m_imageToComponentData.find(imageUidArg);
  if (std::end(m_imageToComponentData) == it || comp >= it->second.size()) {
    return {};
  }

  return it->second.at(comp).m_isosurfaceUidsSorted;
}

std::optional<uuid> AppData::imageToActiveSegUid(const uuid& imageUidArg) const
{
  auto it = m_imageToActiveSeg.find(imageUidArg);
  if (std::end(m_imageToActiveSeg) != it) {
    return it->second;
  }
  return std::nullopt;
}

bool AppData::assignActiveSegUidToImage(const uuid& imageUidArg, const uuid& activeSegUid)
{
  if (image(imageUidArg) && seg(activeSegUid)) {
    m_imageToActiveSeg[imageUidArg] = activeSegUid;

    if (const auto* table = activeLabelTable()) {
      m_settings.adjustActiveSegmentationLabels(*table);
      return true;
    }
    else {
      return false;
    }
  }
  return false;
}

std::optional<uuid> AppData::imageToActiveInverseWarpUid(const uuid& imageUidArg) const
{
  auto it = m_imageToActiveInverseWarp.find(imageUidArg);
  if (std::end(m_imageToActiveInverseWarp) != it) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<uuid> AppData::imageToActiveInverseWarpReferenceImageUid(const uuid& imageUidArg) const
{
  if (imageToActiveInverseWarpUid(imageUidArg)) {
    auto it = m_imageToActiveInverseWarpReferenceImage.find(imageUidArg);
    if (it != m_imageToActiveInverseWarpReferenceImage.end()) {
      return it->second;
    }
    return imageUidArg;
  }
  return std::nullopt;
}

bool AppData::setActiveInverseWarpReferenceImageUid(
  const uuid& imageUidArg,
  const std::optional<uuid>& referenceImageUid)
{
  if (!image(imageUidArg) || !imageToActiveInverseWarpUid(imageUidArg)) {
    return false;
  }
  if (referenceImageUid) {
    if (!image(*referenceImageUid)) {
      return false;
    }
    m_imageToActiveInverseWarpReferenceImage[imageUidArg] = *referenceImageUid;
  }
  else {
    m_imageToActiveInverseWarpReferenceImage.erase(imageUidArg);
  }
  return true;
}

std::optional<uuid> AppData::imageToActiveForwardWarpUid(const uuid& imageUidArg) const
{
  auto it = m_imageToActiveForwardWarp.find(imageUidArg);
  if (std::end(m_imageToActiveForwardWarp) != it) {
    return it->second;
  }
  return std::nullopt;
}

void AppData::clearActiveInverseWarpUidForImage(const uuid& imageUidArg)
{
  m_imageToActiveInverseWarp.erase(imageUidArg);
  m_imageToActiveInverseWarpReferenceImage.erase(imageUidArg);
}

bool AppData::assignActiveInverseWarpUidToImage(const uuid& imageUidArg, const uuid& activeWarpUid)
{
  return assignActiveInverseWarpUidToImage(imageUidArg, activeWarpUid, std::nullopt);
}

bool AppData::assignActiveInverseWarpUidToImage(
  const uuid& imageUidArg,
  const uuid& activeWarpUid,
  const std::optional<uuid>& referenceImageUid)
{
  const Image* defImage = warpField(activeWarpUid);
  const std::optional<uuid> resolvedReferenceUid =
    referenceImageUid ? referenceImageUid : std::optional<uuid>{imageUidArg};
  if (
    image(imageUidArg) && resolvedReferenceUid && image(*resolvedReferenceUid) && defImage &&
    defImage->header().numComponentsPerPixel() >= 3)
  {
    m_imageToActiveInverseWarp[imageUidArg] = activeWarpUid;
    m_imageToActiveInverseWarpReferenceImage[imageUidArg] = *resolvedReferenceUid;
    return true;
  }
  spdlog::error("Cannot assign warp field {} as an inverse warp for image {}", activeWarpUid, imageUidArg);
  return false;
}

void AppData::clearActiveForwardWarpUidForImage(const uuid& imageUidArg)
{
  m_imageToActiveForwardWarp.erase(imageUidArg);
}

bool AppData::assignActiveForwardWarpUidToImage(const uuid& imageUidArg, const uuid& activeWarpUid)
{
  const Image* defImage = warpField(activeWarpUid);
  if (image(imageUidArg) && defImage && defImage->header().numComponentsPerPixel() >= 3) {
    m_imageToActiveForwardWarp[imageUidArg] = activeWarpUid;
    return true;
  }
  spdlog::error("Cannot assign warp field {} as a forward warp for image {}", activeWarpUid, imageUidArg);
  return false;
}

std::vector<uuid> AppData::imageToSegUids(const uuid& imageUidArg) const
{
  auto it = m_imageToSegs.find(imageUidArg);
  if (std::end(m_imageToSegs) != it) {
    return it->second;
  }
  return std::vector<uuid>{};
}

std::vector<uuid> AppData::imageToDefUids(const uuid& imageUidArg) const
{
  auto it = m_imageToDefs.find(imageUidArg);
  if (std::end(m_imageToDefs) != it) {
    return it->second;
  }
  return std::vector<uuid>{};
}

bool AppData::assignSegUidToImage(const uuid& imageUidArg, const uuid& segUidArg)
{
  if (image(imageUidArg) && seg(segUidArg)) {
    m_imageToSegs[imageUidArg].emplace_back(segUidArg);

    if (1 == m_imageToSegs[imageUidArg].size()) {
      // If this is the first segmentation, make it the active one
      assignActiveSegUidToImage(imageUidArg, segUidArg);
    }

    if (const auto* table = activeLabelTable()) {
      m_settings.adjustActiveSegmentationLabels(*table);
      return true;
    }
    else {
      return false;
    }
  }

  return false;
}

bool AppData::assignInverseWarpUidToImage(const uuid& imageUidArg, const uuid& warpUid)
{
  return assignInverseWarpUidToImage(imageUidArg, warpUid, std::nullopt);
}

bool AppData::assignInverseWarpUidToImage(
  const uuid& imageUidArg,
  const uuid& warpUid,
  const std::optional<uuid>& referenceImageUid)
{
  const Image* movingImage = image(imageUidArg);
  const Image* defImage = warpField(warpUid);
  const std::optional<uuid> resolvedReferenceUid =
    referenceImageUid ? referenceImageUid : std::optional<uuid>{imageUidArg};
  if (
    movingImage && resolvedReferenceUid && image(*resolvedReferenceUid) && defImage &&
    defImage->header().numComponentsPerPixel() >= 3)
  {
    const auto& defUids = m_imageToDefs[imageUidArg];
    if (std::find(std::begin(defUids), std::end(defUids), warpUid) != std::end(defUids)) {
      return assignActiveInverseWarpUidToImage(imageUidArg, warpUid, resolvedReferenceUid);
    }

    m_imageToDefs[imageUidArg].emplace_back(warpUid);
    return assignActiveInverseWarpUidToImage(imageUidArg, warpUid, resolvedReferenceUid);
  }

  spdlog::error("Cannot assign inverse warp field {} to image {}", warpUid, imageUidArg);
  return false;
}

bool AppData::assignForwardWarpUidToImage(const uuid& imageUidArg, const uuid& warpUid)
{
  const Image* movingImage = image(imageUidArg);
  const Image* defImage = warpField(warpUid);
  if (movingImage && defImage && defImage->header().numComponentsPerPixel() >= 3) {
    const auto& defUids = m_imageToDefs[imageUidArg];
    if (std::find(std::begin(defUids), std::end(defUids), warpUid) == std::end(defUids)) {
      m_imageToDefs[imageUidArg].emplace_back(warpUid);
    }
    return assignActiveForwardWarpUidToImage(imageUidArg, warpUid);
  }

  spdlog::error("Cannot assign forward warp field {} to image {}", warpUid, imageUidArg);
  return false;
}

const std::vector<uuid>& AppData::imageToLandmarkGroupUids(const uuid& imageUidArg) const
{
  static const std::vector<uuid> sk_emptyUidVector{};

  auto it = m_imageToLandmarkGroups.find(imageUidArg);
  if (std::end(m_imageToLandmarkGroups) != it) {
    return it->second;
  }
  return sk_emptyUidVector;
}

bool AppData::assignActiveLandmarkGroupUidToImage(const uuid& imageUidArg, const uuid& lmGroupUid)
{
  if (image(imageUidArg) && landmarkGroup(lmGroupUid)) {
    m_imageToActiveLandmarkGroup[imageUidArg] = lmGroupUid;
    return true;
  }
  return false;
}

std::optional<uuid> AppData::imageToActiveLandmarkGroupUid(const uuid& imageUidArg) const
{
  auto it = m_imageToActiveLandmarkGroup.find(imageUidArg);
  if (std::end(m_imageToActiveLandmarkGroup) != it) {
    return it->second;
  }
  return std::nullopt;
}

bool AppData::assignLandmarkGroupUidToImage(const uuid& imageUidArg, uuid lmGroupUid)
{
  if (image(imageUidArg) && landmarkGroup(lmGroupUid)) {
    m_imageToLandmarkGroups[imageUidArg].emplace_back(lmGroupUid);

    // If this is the first landmark group for the image, or if the image has no active
    // landmark group, then make this the image's active landmark group:
    if (1 == m_imageToLandmarkGroups[imageUidArg].size() || !imageToActiveLandmarkGroupUid(imageUidArg)) {
      assignActiveLandmarkGroupUidToImage(imageUidArg, lmGroupUid);
    }

    return true;
  }
  return false;
}

bool AppData::assignActiveAnnotationUidToImage(const uuid& imageUidArg, const std::optional<uuid>& annotUid)
{
  if (image(imageUidArg)) {
    if (annotUid && annotation(*annotUid)) {
      m_imageToActiveAnnotation[imageUidArg] = *annotUid;
      return true;
    }
    else if (!annotUid) {
      m_imageToActiveAnnotation.erase(imageUidArg);
      return true;
    }
  }
  return false;
}

std::optional<uuid> AppData::imageToActiveAnnotationUid(const uuid& imageUidArg) const
{
  auto it = m_imageToActiveAnnotation.find(imageUidArg);
  if (std::end(m_imageToActiveAnnotation) != it) {
    return it->second;
  }
  return std::nullopt;
}

const std::list<uuid>& AppData::annotationsForImage(const uuid& imageUidArg) const
{
  static const std::list<uuid> sk_emptyUidList{};

  auto it = m_imageToAnnotations.find(imageUidArg);
  if (std::end(m_imageToAnnotations) != it) {
    return it->second;
  }
  return sk_emptyUidList;
}

void AppData::setImageBeingSegmented(const uuid& imageUidArg, bool set)
{
  if (set) {
    m_imagesBeingSegmented.insert(imageUidArg);
  }
  else {
    m_imagesBeingSegmented.erase(imageUidArg);
  }
}

bool AppData::isImageBeingSegmented(const uuid& imageUidArg) const
{
  return (m_imagesBeingSegmented.count(imageUidArg) > 0);
}

uuid_range_t AppData::imagesBeingSegmented() const
{
  return uuid_range_t{m_imagesBeingSegmented.begin(), m_imagesBeingSegmented.end()};
}

std::optional<uuid> AppData::imageUid(std::size_t index) const
{
  if (index < m_imageUidsOrdered.size()) {
    return m_imageUidsOrdered[index];
  }
  return std::nullopt;
}

std::optional<uuid> AppData::segUid(std::size_t index) const
{
  if (index < m_segUidsOrdered.size()) {
    return m_segUidsOrdered.at(index);
  }
  return std::nullopt;
}

std::optional<uuid> AppData::defUid(std::size_t index) const
{
  if (index < m_defUidsOrdered.size()) {
    return m_defUidsOrdered.at(index);
  }
  return std::nullopt;
}

std::optional<uuid> AppData::imageColorMapUid(std::size_t index) const
{
  if (index < m_imageColorMapUidsOrdered.size()) {
    return m_imageColorMapUidsOrdered.at(index);
  }
  return std::nullopt;
}

std::optional<uuid> AppData::labelTableUid(std::size_t index) const
{
  if (index < m_labelTablesUidsOrdered.size()) {
    return m_labelTablesUidsOrdered.at(index);
  }
  return std::nullopt;
}

std::optional<uuid> AppData::landmarkGroupUid(std::size_t index) const
{
  if (index < m_landmarkGroupUidsOrdered.size()) {
    return m_landmarkGroupUidsOrdered.at(index);
  }
  return std::nullopt;
}

std::optional<std::size_t> AppData::imageIndex(const uuid& imageUidArg) const
{
  std::size_t i = 0;
  for (const auto& uid : m_imageUidsOrdered) {
    if (uid == imageUidArg) {
      return i;
    }
    ++i;
  }
  return std::nullopt;
}

std::optional<std::size_t> AppData::segIndex(const uuid& segUidArg) const
{
  for (std::size_t i = 0; i < m_segUidsOrdered.size(); ++i) {
    if (m_segUidsOrdered.at(i) == segUidArg) {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<std::size_t> AppData::defIndex(const uuid& defUidArg) const
{
  for (std::size_t i = 0; i < m_defUidsOrdered.size(); ++i) {
    if (m_defUidsOrdered.at(i) == defUidArg) {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<std::size_t> AppData::imageColorMapIndex(const uuid& mapUid) const
{
  for (std::size_t i = 0; i < m_imageColorMapUidsOrdered.size(); ++i) {
    if (m_imageColorMapUidsOrdered.at(i) == mapUid) {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<std::size_t> AppData::labelTableIndex(const uuid& tableUid) const
{
  for (std::size_t i = 0; i < m_labelTablesUidsOrdered.size(); ++i) {
    if (m_labelTablesUidsOrdered.at(i) == tableUid) {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<std::size_t> AppData::landmarkGroupIndex(const uuid& lmGroupUid) const
{
  for (std::size_t i = 0; i < m_landmarkGroupUidsOrdered.size(); ++i) {
    if (m_landmarkGroupUidsOrdered.at(i) == lmGroupUid) {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<std::size_t> AppData::annotationIndex(const uuid& imageUidArg, const uuid& annotUid) const
{
  std::size_t i = 0;
  for (const auto& uid : annotationsForImage(imageUidArg)) {
    if (annotUid == uid) {
      return i;
    }
    ++i;
  }
  return std::nullopt;
}

Image* AppData::refImage()
{
  return (refImageUid()) ? image(*refImageUid()) : nullptr;
}

const Image* AppData::refImage() const
{
  return (refImageUid()) ? image(*refImageUid()) : nullptr;
}

Image* AppData::activeImage()
{
  return (activeImageUid()) ? image(*activeImageUid()) : nullptr;
}

const Image* AppData::activeImage() const
{
  return (activeImageUid()) ? image(*activeImageUid()) : nullptr;
}

Image* AppData::activeSeg()
{
  const auto imgUid = activeImageUid();
  if (!imgUid) return nullptr;

  const auto segUidLocal = imageToActiveSegUid(*imgUid);
  if (!segUidLocal) return nullptr;

  return seg(*segUidLocal);
}

ParcellationLabelTable* AppData::activeLabelTable()
{
  ParcellationLabelTable* activeLabelTable = nullptr;

  if (m_activeImageUid) {
    if (const auto activeSegUid = imageToActiveSegUid(*m_activeImageUid)) {
      if (const Image* activeSegLocal = seg(*activeSegUid)) {
        if (const auto tableUid = labelTableUid(activeSegLocal->settings().labelTableIndex())) {
          activeLabelTable = labelTable(*tableUid);
        }
      }
    }
  }

  return activeLabelTable;
}

std::string AppData::getAllImageDisplayNames() const
{
  std::ostringstream allImageDisplayNames;

  bool first = true;

  for (const auto& imageUidLocal : imageUidsOrdered()) {
    if (const Image* img = image(imageUidLocal)) {
      if (!first) allImageDisplayNames << ", ";
      allImageDisplayNames << img->settings().displayName();
      first = false;
    }
  }

  return allImageDisplayNames.str();
}

const AppSettings& AppData::settings() const
{
  return m_settings;
}
AppSettings& AppData::settings()
{
  return m_settings;
}

const AppState& AppData::state() const
{
  return m_state;
}
AppState& AppData::state()
{
  return m_state;
}

const GuiData& AppData::guiData() const
{
  return m_guiData;
}
GuiData& AppData::guiData()
{
  return m_guiData;
}

const RenderData& AppData::renderData() const
{
  return m_renderData;
}
RenderData& AppData::renderData()
{
  return m_renderData;
}

const WindowData& AppData::windowData() const
{
  return m_windowData;
}
WindowData& AppData::windowData()
{
  return m_windowData;
}

const registration::JobStore& AppData::registrationJobs() const
{
  return m_registrationJobs;
}

registration::JobStore& AppData::registrationJobs()
{
  return m_registrationJobs;
}

void AppData::saveAllViewWorldCenterPositions()
{
  // const Image* img = refImage();
  // if (!img) {
  //   return;
  // }

  // const glm::mat4 refSubject_T_world = img->transformations().subject_T_worldDef();

  m_savedViewWorldCenterPositions.clear();

  for (const auto& layout : m_windowData.layouts()) {
    MapViewUidToCenterPos m;
    for (const auto& [viewUid, view] : layout.views()) {
      if (view) {
        // const glm::vec4 worldPos{helper::worldOrigin(view->camera()), 1};
        // const glm::vec4 refSubjectPos = refSubject_T_world * worldPos;
        // m.emplace(viewUid, glm::vec3{refSubjectPos / refSubjectPos.w});

        const glm::vec4 anatomyPos = glm::inverse(view->camera().camera_T_anatomy()) * glm::vec4{0, 0, 0, 1};
        m.emplace(viewUid, glm::vec3{anatomyPos});
      }
    }

    m_savedViewWorldCenterPositions.emplace_back(std::move(m));
  }

  // spdlog::info("\nSAVING");
  // for (const auto& [viewUid, view] :
  // m_windowData.layouts().at(m_windowData.currentLayoutIndex()).views()) {
  //   // const glm::vec4 worldPos{helper::worldOrigin(view->camera()), 1};
  //   // const glm::vec4 refSubjectPos = refSubject_T_world * worldPos;
  //   // spdlog::info("{} : {}", viewUid, glm::to_string(refSubjectPos));

  //   const glm::vec4 anatomyPos = glm::inverse(view->camera().camera_T_anatomy()) * glm::vec4{0,
  //   0, 0, 1}; spdlog::info("{} : {}", viewUid, glm::to_string(glm::vec3{anatomyPos}));
  // }
}

void AppData::restoreAllViewWorldCenterPositions()
{
  // const Image* img = refImage();
  // if (!img) {
  //   return;
  // }

  // const glm::mat4 world_T_refSubject = img->transformations().worldDef_T_subject();

  for (const auto& mapViewUidToWorldCameraPos : m_savedViewWorldCenterPositions) {
    // for (const auto& [viewUid, refSubjectPos] : mapViewUidToWorldCameraPos) {
    for (const auto& [viewUid, anatomyPos] : mapViewUidToWorldCameraPos) {
      if (View* view = m_windowData.getView(viewUid)) {
        // const glm::vec4 worldPos = world_T_refSubject * glm::vec4{refSubjectPos, 1};
        // helper::setCameraOrigin(view->camera(), glm::vec3{worldPos / worldPos.w});

        const glm::vec4 worldPos = glm::inverse(view->camera().anatomy_T_start()) * glm::vec4{anatomyPos, 1};

        helper::setCameraOrigin(view->camera(), glm::vec3{worldPos / worldPos.w});
      }
    }
  }

  // spdlog::info("\nRESTORING");
  // const glm::mat4 refSubject_T_world = img->transformations().subject_T_worldDef();

  // for (const auto& [viewUid, view] :
  // m_windowData.layouts().at(m_windowData.currentLayoutIndex()).views())
  // {
  //   const glm::vec4 worldPos{helper::worldOrigin(view->camera()), 1};
  //   const glm::vec4 refSubjectPos = refSubject_T_world * worldPos;
  //   spdlog::info("{} : {}", viewUid, glm::to_string(glm::vec3{refSubjectPos}));
  // }
}
