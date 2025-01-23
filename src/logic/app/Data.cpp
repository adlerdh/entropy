#include "logic/app/Data.h"

#include "common/UuidUtility.h"

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>

#include <cmrc/cmrc.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <boost/range/adaptor/map.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

CMRC_DECLARE(colormaps);

AppData::AppData()
  : m_settings()
  , m_state()
  ,

  m_guiData()
  , m_renderData()
  , m_windowData()
  , m_project()
  ,

  m_images()
  , m_imageUidsOrdered()
  ,

  m_segs()
  , m_segUidsOrdered()
  ,

  m_defs()
  , m_defUidsOrdered()
  ,

  m_imageColorMaps()
  , m_imageColorMapUidsOrdered()
  ,

  m_labelTables()
  , m_labelTablesUidsOrdered()
  ,

  m_landmarkGroups()
  , m_landmarkGroupUidsOrdered()
  ,

  m_annotations()
  ,

  m_refImageUid(std::nullopt)
  , m_activeImageUid(std::nullopt)
  ,

  m_imageToSegs()
  , m_imageToActiveSeg()
  ,

  m_imageToDefs()
  , m_imageToActiveDef()
  ,

  m_imageToLandmarkGroups()
  , m_imageToActiveLandmarkGroup()
  ,

  m_imageToAnnotations()
  , m_imageToActiveAnnotation()
  ,

  m_imagesBeingSegmented()
{
  spdlog::debug("Start loading image color maps");
  loadImageColorMaps();
  spdlog::debug("Done loading label color tables and image color maps");

  // Initialize the IPC handler
  // m_ipcHandler.Attach( IPCHandler::GetUserPreferencesFileName().c_str(),
  //                      (short) IPCMessage::VERSION, sizeof( IPCMessage ) );

  spdlog::debug("Constructed application data");
}

AppData::~AppData()
{
  //if ( m_ipcHandler.IsAttached() )
  //{
  //    m_ipcHandler.Close();
  //}
}

void AppData::setProject(serialize::EntropyProject project)
{
  m_project = std::move(project);
}

const serialize::EntropyProject& AppData::project() const
{
  return m_project;
}

serialize::EntropyProject& AppData::project()
{
  return m_project;
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
      black, white, sk_numSteps, "Linear grey", "Linear grey", "linear_grey_0-100_n1024"
    )
  );

  m_imageColorMaps.emplace(
    redMapUid,
    ImageColorMap::createLinearImageColorMap(
      black, red, sk_numSteps, "Linear red", "Linear red", "linear_red_0-100_n1024"
    )
  );

  m_imageColorMaps.emplace(
    greenMapUid,
    ImageColorMap::createLinearImageColorMap(
      black, green, sk_numSteps, "Linear green", "Linear green", "linear_green_0-100_n1024"
    )
  );

  m_imageColorMaps.emplace(
    blueMapUid,
    ImageColorMap::createLinearImageColorMap(
      black, blue, sk_numSteps, "Linear blue", "Linear blue", "linear_blue_0-100_n1024"
    )
  );

  m_imageColorMaps.emplace(
    yellowMapUid,
    ImageColorMap::createLinearImageColorMap(
      black, yellow, sk_numSteps, "Linear yellow", "Linear yellow", "linear_yellow_0-100_n1024"
    )
  );

  m_imageColorMaps.emplace(
    cyanMapUid,
    ImageColorMap::createLinearImageColorMap(
      black, cyan, sk_numSteps, "Linear cyan", "Linear cyan", "linear_cyan_0-100_n1024"
    )
  );

  m_imageColorMaps.emplace(
    magentaMapUid,
    ImageColorMap::createLinearImageColorMap(
      black, magenta, sk_numSteps, "Linear magenta", "Linear magenta", "linear_magenta_0-100_n1024"
    )
  );

  const glm::vec4 transparentBlack{0.0f};

  ImageColorMap constantWhiteMap = ImageColorMap::createLinearImageColorMap(
    white, white, 1024, "Constant white", "Constant white", "constant_white_n1024"
  );

  constantWhiteMap.setInterpolationMode(ImageColorMap::InterpolationMode::Nearest);
  constantWhiteMap.setTransparentBorder(true);
  constantWhiteMap.setColorRGBA(0, transparentBlack); // Not sure why this is needed
  m_imageColorMaps.emplace(constantWhiteMapUid, std::move(constantWhiteMap));

  ImageColorMap constantRedMap = ImageColorMap::createLinearImageColorMap(
    red, red, 1024, "Constant red", "Constant red", "constant_red_n1024"
  );

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
      std::vector<glm::vec4>{blackOpaque, white}
    )
  );

  m_imageColorMaps.emplace(
    krMapUid,
    ImageColorMap(
      "Discrete black and red",
      "Black, red discrete color map",
      "Black-red_discrete",
      ImageColorMap::InterpolationMode::Nearest,
      std::vector<glm::vec4>{blackOpaque, red}
    )
  );

  m_imageColorMaps.emplace(
    twMapUid,
    ImageColorMap(
      "Discrete transparent and white",
      "Transparent-white discrete color map",
      "Transparent-white_discrete",
      ImageColorMap::InterpolationMode::Nearest,
      std::vector<glm::vec4>{blackTransparent, white}
    )
  );

  m_imageColorMaps.emplace(
    trMapUid,
    ImageColorMap(
      "Discrete transparent and red",
      "Transparent-red discrete color map",
      "Transparent-red_discrete",
      ImageColorMap::InterpolationMode::Nearest,
      std::vector<glm::vec4>{blackTransparent, red}
    )
  );

  m_imageColorMaps.emplace(
    rgbMapUid,
    ImageColorMap(
      "Discrete RGB",
      "Red-green-blue discrete color map",
      "Red-green-blue_discrete",
      ImageColorMap::InterpolationMode::Nearest,
      std::vector<glm::vec4>{red, green, blue}
    )
  );

  m_imageColorMaps.emplace(
    rgbyMapUid,
    ImageColorMap(
      "Discrete RGBY",
      "Red-green-blue-yellow discrete color map",
      "Red-green-blue-yellow_discrete",
      ImageColorMap::InterpolationMode::Nearest,
      std::vector<glm::vec4>{red, green, blue, yellow}
    )
  );

  m_imageColorMaps.emplace(
    rgbycmMapUid,
    ImageColorMap(
      "Discrete RGBYCM",
      "Red-green-blue-yellow-cyan-magnenta discrete color map",
      "Red-green-blue-yellow-cyan-magenta_discrete",
      ImageColorMap::InterpolationMode::Nearest,
      std::vector<glm::vec4>{red, green, blue, yellow, cyan, magenta}
    )
  );

  m_imageColorMaps.emplace(
    rygcbmMapUid,
    ImageColorMap(
      "Discrete RYGCBM",
      "Red-yellow-green-cyan-blue-magnenta discrete color map",
      "Red-yellow-green-cyan-blue-magnenta_discrete",
      ImageColorMap::InterpolationMode::Nearest,
      std::vector<glm::vec4>{red, yellow, green, cyan, blue, magenta}
    )
  );

  m_imageColorMaps.emplace(
    krgbycmwMapUid,
    ImageColorMap(
      "Discrete KRGBYCMW",
      "Black-red-green-blue-yellow-cyan-magnenta-white discrete color map",
      "Black-red-green-blue-yellow-cyan-magenta-white_discrete",
      ImageColorMap::InterpolationMode::Nearest,
      std::vector<glm::vec4>{blackOpaque, red, green, blue, yellow, cyan, magenta, white}
    )
  );

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
  try
  {
    spdlog::debug("Begin loading image color maps from disk");

    auto loadMapsFromDir = [this](const std::string& dir)
    {
      auto filesystem = cmrc::colormaps::get_filesystem();
      auto dirIter = filesystem.iterate_directory(dir);

      for (const auto& i : dirIter)
      {
        if (!i.is_file())
          continue;

        cmrc::file f = filesystem.open(dir + i.filename());
        std::istringstream iss(std::string(f.begin(), f.end()));

        if (auto cmap = ImageColorMap::loadImageColorMap(iss))
        {
          const auto uid = generateRandomUuid();
          m_imageColorMaps.emplace(uid, std::move(*cmap));
          m_imageColorMapUidsOrdered.push_back(uid);
        }
      }
    };

    loadMapsFromDir("resources/colormaps/matplotlib/");
    loadMapsFromDir("resources/colormaps/ncl/");
    loadMapsFromDir("resources/colormaps/peter_kovesi/");
  }
  catch (const std::exception& e)
  {
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

uuids::uuid AppData::addImage(Image image)
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  const std::size_t numComps = image.header().numComponentsPerPixel();

  auto uid = generateRandomUuid();
  m_images.emplace(uid, std::move(image));
  m_imageUidsOrdered.push_back(uid);

  if (1 == m_images.size())
  {
    // The first loaded image becomes the reference image and the active image
    m_refImageUid = uid;
    m_activeImageUid = uid;
  }

  // Create the per-component data:
  m_imageToComponentData[uid] = std::vector<ComponentData>(numComps);

  return uid;
}

std::optional<uuids::uuid> AppData::addSeg(Image seg)
{
  if (!isComponentUnsignedInt(seg.header().memoryComponentType()))
  {
    spdlog::error(
      "Segmentation image {} with non-unsigned integer component type {} cannot be added",
      seg.settings().displayName(),
      seg.header().memoryComponentTypeAsString()
    );
    return std::nullopt;
  }

  auto uid = generateRandomUuid();
  m_segs.emplace(uid, std::move(seg));
  m_segUidsOrdered.push_back(uid);
  return uid;
}

std::optional<uuids::uuid> AppData::addDef(Image def)
{
  if (def.header().numComponentsPerPixel() < 3)
  {
    spdlog::error(
      "Deformation field image {} with only {} components cannot be added",
      def.settings().displayName(),
      def.header().numComponentsPerPixel()
    );
    return std::nullopt;
  }

  auto uid = generateRandomUuid();
  m_defs.emplace(uid, std::move(def));
  m_defUidsOrdered.push_back(uid);
  return uid;
}

uuids::uuid AppData::addLandmarkGroup(LandmarkGroup lmGroup)
{
  auto uid = generateRandomUuid();
  m_landmarkGroups.emplace(uid, std::move(lmGroup));
  m_landmarkGroupUidsOrdered.push_back(uid);
  return uid;
}

std::optional<uuids::uuid> AppData::addAnnotation(const uuids::uuid& imageUid, Annotation annotation)
{
  if (!image(imageUid))
  {
    return std::nullopt; // invalid image UID
  }

  auto annotUid = generateRandomUuid();
  m_annotations.emplace(annotUid, std::move(annotation));
  m_imageToAnnotations[imageUid].emplace_back(annotUid);

  // If this is the first annotation or there is no active annotation for the image,
  // then make this the active annotation:
  if (1 == m_imageToAnnotations[imageUid].size() || !imageToActiveAnnotationUid(imageUid))
  {
    assignActiveAnnotationUidToImage(imageUid, annotUid);
  }

  return annotUid;
}

bool AppData::addDistanceMap(
  const uuids::uuid& imageUid,
  ComponentIndexType component,
  Image distanceMap,
  double boundaryIsoValue
)
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  const Image* img = image(imageUid);
  if (!img)
  {
    return false; // invalid image UID
  }

  const uint32_t numComps = img->header().numComponentsPerPixel();
  if (component >= numComps)
  {
    spdlog::error(
      "Invalid component {} for image {}. Cannot set distance map for it.", component, imageUid
    );
    return false;
  }

  auto compDataIt = m_imageToComponentData.find(imageUid);

  if (std::end(m_imageToComponentData) != compDataIt)
  {
    if (component >= compDataIt->second.size())
    {
      compDataIt->second.resize(numComps);
    }

    compDataIt->second.at(component).m_distanceMaps.emplace(boundaryIsoValue, std::move(distanceMap));
    return true;
  }
  else
  {
    spdlog::error("No component data for image {}. Cannot set distance map.", imageUid);
    return false;
  }

  return false;
}

bool AppData::addNoiseEstimate(
  const uuids::uuid& imageUid, ComponentIndexType component, Image noiseEstimate, uint32_t radius
)
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  const Image* img = image(imageUid);
  if (!img)
  {
    return false; // invalid image UID
  }

  const uint32_t numComps = img->header().numComponentsPerPixel();
  if (component >= numComps)
  {
    spdlog::error(
      "Invalid component {} for image {}. Cannot set noise estimate for it.", component, imageUid
    );
    return false;
  }

  auto compDataIt = m_imageToComponentData.find(imageUid);

  if (std::end(m_imageToComponentData) != compDataIt)
  {
    if (component >= compDataIt->second.size())
    {
      compDataIt->second.resize(numComps);
    }

    compDataIt->second.at(component).m_noiseEstimates.emplace(radius, std::move(noiseEstimate));
    return true;
  }
  else
  {
    spdlog::error("No component data for image {}. Cannot set noise estimate.", imageUid);
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

std::optional<uuids::uuid> AppData::addIsosurface(
  const uuids::uuid& imageUid, ComponentIndexType component, Isosurface isosurface
)
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  const Image* img = image(imageUid);
  if (!img)
  {
    spdlog::error("Cannot add isosurface to invalid image {}.", imageUid);
    return std::nullopt;
  }

  const uint32_t numComps = img->header().numComponentsPerPixel();
  if (component >= numComps)
  {
    spdlog::error("Cannot add isosurface to invalid component {} of image {}.", component, imageUid);
    return std::nullopt;
  }

  auto compDataIt = m_imageToComponentData.find(imageUid);

  if (std::end(m_imageToComponentData) != compDataIt)
  {
    if (component >= compDataIt->second.size())
    {
      compDataIt->second.resize(numComps);
    }

    auto uid = generateRandomUuid();
    compDataIt->second.at(component).m_isosurfaces.emplace(uid, std::move(isosurface));
    return uid;
  }
  else
  {
    spdlog::error("No component data for image {}. Cannot add isosurface.", imageUid);
    return std::nullopt;
  }

  return std::nullopt;
}

//bool AppData::removeImage( const uuids::uuid& /*imageUid*/ )
//{
//    return false;
//}

bool AppData::removeSeg(const uuids::uuid& segUid)
{
  auto segMapIt = m_segs.find(segUid);
  if (std::end(m_segs) != segMapIt)
  {
    // Remove the segmentation
    m_segs.erase(segMapIt);
  }
  else
  {
    // This segmentation does not exist
    return false;
  }

  auto segVecIt = std::find(std::begin(m_segUidsOrdered), std::end(m_segUidsOrdered), segUid);
  if (std::end(m_segUidsOrdered) != segVecIt)
  {
    m_segUidsOrdered.erase(segVecIt);
  }
  else
  {
    return false;
  }

  // Remove segmentation from image-to-segmentation map for all images
  for (auto& m : m_imageToSegs)
  {
    m.second.erase(std::remove(std::begin(m.second), std::end(m.second), segUid), std::end(m.second));
  }

  // Remove it as an active segmentation
  for (auto it = std::begin(m_imageToActiveSeg); it != std::end(m_imageToActiveSeg);)
  {
    if (segUid == it->second)
    {
      const auto imageUid = it->first;

      it = m_imageToActiveSeg.erase(it);

      // Set a new active segmentation for this image, if one exists
      if (m_imageToSegs.count(imageUid) > 0)
      {
        if (!m_imageToSegs[imageUid].empty())
        {
          // Set the image's first segmentation as its active one
          m_imageToActiveSeg[imageUid] = m_imageToSegs[imageUid][0];
        }
      }
    }
    else
    {
      ++it;
    }
  }

  return true;
}

bool AppData::removeDef(const uuids::uuid& defUid)
{
  auto defMapIt = m_defs.find(defUid);
  if (std::end(m_defs) != defMapIt)
  {
    // Remove the deformation
    m_defs.erase(defMapIt);
  }
  else
  {
    // This deformation does not exist
    return false;
  }

  auto defVecIt = std::find(std::begin(m_defUidsOrdered), std::end(m_defUidsOrdered), defUid);
  if (std::end(m_defUidsOrdered) != defVecIt)
  {
    m_defUidsOrdered.erase(defVecIt);
  }
  else
  {
    return false;
  }

  // Remove deformation from image-to-deformation map for all images
  for (auto& m : m_imageToDefs)
  {
    m.second.erase(std::remove(std::begin(m.second), std::end(m.second), defUid), std::end(m.second));
  }

  // Remove it as an active deformation
  for (auto it = std::begin(m_imageToActiveDef); it != std::end(m_imageToActiveDef);)
  {
    if (defUid == it->second)
    {
      it = m_imageToActiveDef.erase(it);
    }
    else
    {
      ++it;
    }
  }

  return true;
}

bool AppData::removeAnnotation(const uuids::uuid& annotUid)
{
  auto annotMapIt = m_annotations.find(annotUid);
  if (std::end(m_annotations) != annotMapIt)
  {
    // Remove the annotation
    m_annotations.erase(annotMapIt);
  }
  else
  {
    // This deformation does not exist
    return false;
  }

  // Remove annotation from image-to-annotation map
  for (auto& p : m_imageToAnnotations)
  {
    p.second
      .erase(std::remove(std::begin(p.second), std::end(p.second), annotUid), std::end(p.second));
  }

  // Remove it as the active annotation
  for (auto it = std::begin(m_imageToActiveAnnotation); it != std::end(m_imageToActiveDef);)
  {
    if (annotUid == it->second)
    {
      it = m_imageToActiveAnnotation.erase(it);
    }
    else
    {
      ++it;
    }
  }

  return true;
}

bool AppData::removeIsosurface(
  const uuids::uuid& imageUid, ComponentIndexType component, const uuids::uuid& isosurfaceUid
)
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  const Image* img = image(imageUid);
  if (!img)
  {
    spdlog::error("Cannot remove isosurface from invalid image {}.", imageUid);
    return false;
  }

  const uint32_t numComps = img->header().numComponentsPerPixel();
  if (component >= numComps)
  {
    spdlog::error(
      "Cannot remove isosurface from invalid component {} of image {}.", component, imageUid
    );
    return false;
  }

  auto compDataIt = m_imageToComponentData.find(imageUid);
  if (std::end(m_imageToComponentData) != compDataIt)
  {
    if (component < compDataIt->second.size())
    {
      return (compDataIt->second.at(component).m_isosurfaces.erase(isosurfaceUid) > 0);
    }
  }

  return false;
}

const Image* AppData::image(const uuids::uuid& imageUid) const
{
  auto it = m_images.find(imageUid);
  if (std::end(m_images) != it)
    return &it->second;
  return nullptr;
}

Image* AppData::image(const uuids::uuid& imageUid)
{
  return const_cast<Image*>(const_cast<const AppData*>(this)->image(imageUid));
}

const Image* AppData::seg(const uuids::uuid& segUid) const
{
  auto it = m_segs.find(segUid);
  if (std::end(m_segs) != it)
    return &it->second;
  return nullptr;
}

Image* AppData::seg(const uuids::uuid& segUid)
{
  return const_cast<Image*>(const_cast<const AppData*>(this)->seg(segUid));
}

const Image* AppData::def(const uuids::uuid& defUid) const
{
  auto it = m_defs.find(defUid);
  if (std::end(m_defs) != it)
    return &it->second;
  return nullptr;
}

Image* AppData::def(const uuids::uuid& defUid)
{
  return const_cast<Image*>(const_cast<const AppData*>(this)->def(defUid));
}

const std::map<double, Image>& AppData::distanceMaps(
  const uuids::uuid& imageUid, ComponentIndexType component
) const
{
  // Map of distance maps (keyed by isosurface value) for the component:
  static const std::map<double, Image> EMPTY;

  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  auto compDataIt = m_imageToComponentData.find(imageUid);

  if (std::end(m_imageToComponentData) != compDataIt)
  {
    if (component < compDataIt->second.size())
    {
      return compDataIt->second.at(component).m_distanceMaps;
    }
    else
    {
      spdlog::error(
        "Invalid component {} for image {}. Cannot get distance map for it.", component, imageUid
      );
      return EMPTY;
    }
  }
  else
  {
    spdlog::error("No component data for image {}. Cannot get distance map for it.", imageUid);
    return EMPTY;
  }

  return EMPTY;
}

const std::map<uint32_t, Image>& AppData::noiseEstimates(
  const uuids::uuid& imageUid, ComponentIndexType component
) const
{
  // Map of noise estimates (keyed by radius value) for the component:
  static const std::map<uint32_t, Image> EMPTY;

  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  auto compDataIt = m_imageToComponentData.find(imageUid);

  if (std::end(m_imageToComponentData) != compDataIt)
  {
    if (component < compDataIt->second.size())
    {
      return compDataIt->second.at(component).m_noiseEstimates;
    }
    else
    {
      spdlog::error(
        "Invalid component {} for image {}. Cannot get distance map for it.", component, imageUid
      );
      return EMPTY;
    }
  }
  else
  {
    spdlog::error("No component data for image {}. Cannot get distance map for it.", imageUid);
    return EMPTY;
  }

  return EMPTY;
}

const Isosurface* AppData::isosurface(
  const uuids::uuid& imageUid, ComponentIndexType component, const uuids::uuid& isosurfaceUid
) const
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  const Image* img = image(imageUid);
  if (!img)
  {
    spdlog::error("Cannot get isosurface from invalid image {}.", imageUid);
    return nullptr;
  }

  const uint32_t numComps = img->header().numComponentsPerPixel();
  if (component >= numComps)
  {
    spdlog::error("Cannot get isosurface from invalid component {} of image {}.", component, imageUid);
    return nullptr;
  }

  auto compDataIt = m_imageToComponentData.find(imageUid);
  if (std::end(m_imageToComponentData) != compDataIt)
  {
    if (component < compDataIt->second.size())
    {
      return &(compDataIt->second.at(component).m_isosurfaces.at(isosurfaceUid));
    }
  }

  return nullptr;
}

Isosurface* AppData::isosurface(
  const uuids::uuid& imageUid, ComponentIndexType component, const uuids::uuid& isosurfaceUid
)
{
  return const_cast<Isosurface*>(
    const_cast<const AppData*>(this)->isosurface(imageUid, component, isosurfaceUid)
  );
}

bool AppData::updateIsosurfaceMeshCpuRecord(
  const uuids::uuid& imageUid,
  ComponentIndexType component,
  const uuids::uuid& isosurfaceUid,
  std::unique_ptr<MeshCpuRecord> cpuRecord
)
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  auto compDataIt = m_imageToComponentData.find(imageUid);

  if (std::end(m_imageToComponentData) != compDataIt)
  {
    if (component < compDataIt->second.size())
    {
      auto& isosurfaces = compDataIt->second.at(component).m_isosurfaces;
      auto surfaceIt = isosurfaces.find(isosurfaceUid);

      if (std::end(isosurfaces) != surfaceIt)
      {
        surfaceIt->second.mesh.setCpuData(std::move(cpuRecord));
        return true;
      }
    }
  }

  return false;
}

bool AppData::updateIsosurfaceMeshGpuRecord(
  const uuids::uuid& imageUid,
  ComponentIndexType component,
  const uuids::uuid& isosurfaceUid,
  std::unique_ptr<MeshGpuRecord> gpuRecord
)
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  auto compDataIt = m_imageToComponentData.find(imageUid);

  if (std::end(m_imageToComponentData) != compDataIt)
  {
    if (component < compDataIt->second.size())
    {
      auto& isosurfaces = compDataIt->second.at(component).m_isosurfaces;
      auto surfaceIt = isosurfaces.find(isosurfaceUid);

      if (std::end(isosurfaces) != surfaceIt)
      {
        surfaceIt->second.mesh.setGpuData(std::move(gpuRecord));
        return true;
      }
    }
  }

  return false;
}

const ImageColorMap* AppData::imageColorMap(const uuids::uuid& colorMapUid) const
{
  auto it = m_imageColorMaps.find(colorMapUid);
  if (std::end(m_imageColorMaps) != it)
    return &it->second;
  return nullptr;
}

ImageColorMap* AppData::imageColorMap(const uuids::uuid& colorMapUid)
{
  return const_cast<ImageColorMap*>(const_cast<const AppData*>(this)->imageColorMap(colorMapUid));
}

const ParcellationLabelTable* AppData::labelTable(const uuids::uuid& labelUid) const
{
  auto it = m_labelTables.find(labelUid);
  if (std::end(m_labelTables) != it)
    return &it->second;
  return nullptr;
}

ParcellationLabelTable* AppData::labelTable(const uuids::uuid& labelUid)
{
  return const_cast<ParcellationLabelTable*>(const_cast<const AppData*>(this)->labelTable(labelUid));
}

const LandmarkGroup* AppData::landmarkGroup(const uuids::uuid& lmGroupUid) const
{
  auto it = m_landmarkGroups.find(lmGroupUid);
  if (std::end(m_landmarkGroups) != it)
    return &it->second;
  return nullptr;
}

LandmarkGroup* AppData::landmarkGroup(const uuids::uuid& lmGroupUid)
{
  return const_cast<LandmarkGroup*>(const_cast<const AppData*>(this)->landmarkGroup(lmGroupUid));
}

const Annotation* AppData::annotation(const uuids::uuid& annotUid) const
{
  auto it = m_annotations.find(annotUid);
  if (std::end(m_annotations) != it)
    return &it->second;
  return nullptr;
}

Annotation* AppData::annotation(const uuids::uuid& annotUid)
{
  return const_cast<Annotation*>(const_cast<const AppData*>(this)->annotation(annotUid));
}

std::optional<uuids::uuid> AppData::refImageUid() const
{
  return m_refImageUid;
}

bool AppData::setRefImageUid(const uuids::uuid& uid)
{
  if (image(uid))
  {
    m_refImageUid = uid;
    return true;
  }

  return false;
}

std::optional<uuids::uuid> AppData::activeImageUid() const
{
  return m_activeImageUid;
}

bool AppData::setActiveImageUid(const uuids::uuid& uid)
{
  if (image(uid))
  {
    m_activeImageUid = uid;

    if (const auto* table = activeLabelTable())
    {
      m_settings.adjustActiveSegmentationLabels(*table);
      return true;
    }
    else
    {
      return false;
    }
  }

  return false;
}

void AppData::setRainbowColorsForAllImages()
{
  static constexpr float sk_colorSat = 0.80f;
  static constexpr float sk_colorVal = 0.90f;

  // Starting color hue, where hues repeat cyclically over range [0.0, 1.0]
  static constexpr float sk_startHue = -1.0f / 48.0f;

  const float N = static_cast<float>(m_imageUidsOrdered.size());
  std::size_t i = 0;

  for (const auto& imageUid : m_imageUidsOrdered)
  {
    if (Image* img = image(imageUid))
    {
      const float a = (1.0f + sk_startHue + static_cast<float>(i) / N);

      float fractPart, intPart;
      fractPart = std::modf(a, &intPart);

      const float hue = 360.0f * fractPart;
      const glm::vec3 color = glm::rgbColor(glm::vec3{hue, sk_colorSat, sk_colorVal});

      img->settings().setBorderColor(color);

      // All image components get the same edge color
      for (uint32_t c = 0; c < img->header().numComponentsPerPixel(); ++c)
      {
        img->settings().setEdgeColor(c, color);
      }
    }
    ++i;
  }
}

void AppData::setRainbowColorsForAllLandmarkGroups()
{
  // Landmark group color is set to image border color
  for (const auto imageUid : m_imageUidsOrdered)
  {
    const Image* img = image(imageUid);
    if (!img)
      continue;

    for (const auto lmGroupUid : imageToLandmarkGroupUids(imageUid))
    {
      if (auto* lmGroup = landmarkGroup(lmGroupUid))
      {
        lmGroup->setColorOverride(true);
        lmGroup->setColor(img->settings().borderColor());
      }
    }
  }
}

bool AppData::moveImageBackwards(const uuids::uuid imageUid)
{
  const auto index = imageIndex(imageUid);
  if (!index)
    return false;

  const long i = static_cast<long>(*index);

  // Only allow moving backwards images with index 2 or greater, because
  // image 1 cannot become 0: that is the reference image index.
  if (2 <= i)
  {
    auto itFirst = std::begin(m_imageUidsOrdered);
    auto itSecond = std::begin(m_imageUidsOrdered);

    std::advance(itFirst, i - 1);
    std::advance(itSecond, i);

    std::iter_swap(itFirst, itSecond);
    return true;
  }

  return false;
}

bool AppData::moveImageForwards(const uuids::uuid imageUid)
{
  const auto index = imageIndex(imageUid);
  if (!index)
    return false;

  const long i = static_cast<long>(*index);
  const long N = static_cast<long>(m_imageUidsOrdered.size());

  if (0 == N)
  {
    return false;
  }

  // Do not allow moving the reference image or the last image:
  if (0 < i && i < N - 1)
  {
    auto itFirst = std::begin(m_imageUidsOrdered);
    auto itSecond = std::begin(m_imageUidsOrdered);

    std::advance(itFirst, i);
    std::advance(itSecond, i + 1);

    std::iter_swap(itFirst, itSecond);
    return true;
  }

  return false;
}

bool AppData::moveImageToBack(const uuids::uuid imageUid)
{
  auto index = imageIndex(imageUid);
  if (!index)
    return false;

  while (index && *index > 1)
  {
    if (!moveImageBackwards(imageUid))
    {
      return false;
    }

    index = imageIndex(imageUid);
  }

  return true;
}

bool AppData::moveImageToFront(const uuids::uuid imageUid)
{
  auto index = imageIndex(imageUid);
  if (!index)
    return false;

  const long i = static_cast<long>(*index);
  const long N = static_cast<long>(m_imageUidsOrdered.size());

  if (0 == N)
  {
    return false;
  }

  while (index && i < N - 1)
  {
    if (!moveImageForwards(imageUid))
    {
      return false;
    }

    index = imageIndex(imageUid);
  }

  return true;
}

bool AppData::moveAnnotationBackwards(const uuids::uuid imageUid, const uuids::uuid annotUid)
{
  const auto index = annotationIndex(imageUid, annotUid);
  if (!index)
    return false;

  const long i = static_cast<long>(*index);

  // Only allow moving backwards annotations with index 1 or greater
  if (0 == i)
  {
    // Already the backmost index
    return true;
  }
  else if (1 <= i)
  {
    auto& annotList = m_imageToAnnotations.at(imageUid);
    auto itFirst = std::begin(annotList);
    auto itSecond = std::begin(annotList);

    std::advance(itFirst, i - 1);
    std::advance(itSecond, i);

    std::iter_swap(itFirst, itSecond);
    return true;
  }

  return false;
}

bool AppData::moveAnnotationForwards(const uuids::uuid imageUid, const uuids::uuid annotUid)
{
  const auto index = annotationIndex(imageUid, annotUid);
  if (!index)
    return false;

  const long i = static_cast<long>(*index);

  auto& annotList = m_imageToAnnotations.at(imageUid);
  const long N = static_cast<long>(annotList.size());

  if (i == N - 1)
  {
    // Already the frontmost index
    return true;
  }
  else if (i <= N - 2)
  {
    auto itFirst = std::begin(annotList);
    auto itSecond = std::begin(annotList);

    std::advance(itFirst, i);
    std::advance(itSecond, i + 1);

    std::iter_swap(itFirst, itSecond);
    return true;
  }

  return false;
}

bool AppData::moveAnnotationToBack(const uuids::uuid imageUid, const uuids::uuid annotUid)
{
  auto index = annotationIndex(imageUid, annotUid);
  if (!index)
    return false;

  while (index && *index >= 1)
  {
    if (!moveAnnotationBackwards(imageUid, annotUid))
    {
      return false;
    }

    index = annotationIndex(imageUid, annotUid);
  }

  return true;
}

bool AppData::moveAnnotationToFront(const uuids::uuid imageUid, const uuids::uuid annotUid)
{
  auto index = annotationIndex(imageUid, annotUid);
  if (!index)
    return false;

  auto& annotList = m_imageToAnnotations.at(imageUid);
  const long N = static_cast<long>(annotList.size());

  while (index && static_cast<long>(*index) < N - 1)
  {
    if (!moveAnnotationForwards(imageUid, annotUid))
    {
      return false;
    }

    index = annotationIndex(imageUid, annotUid);
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
  return m_defs.size();
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

uuid_range_t AppData::imageUidsOrdered() const
{
  return m_imageUidsOrdered;
}

uuid_range_t AppData::segUidsOrdered() const
{
  return m_segUidsOrdered;
}

uuid_range_t AppData::defUidsOrdered() const
{
  return m_defUidsOrdered;
}

uuid_range_t AppData::imageColorMapUidsOrdered() const
{
  return m_imageColorMapUidsOrdered;
}

uuid_range_t AppData::labelTableUidsOrdered() const
{
  return m_labelTablesUidsOrdered;
}

uuid_range_t AppData::landmarkGroupUidsOrdered() const
{
  return m_landmarkGroupUidsOrdered;
}

uuid_range_t AppData::isosurfaceUids(const uuids::uuid& imageUid, ComponentIndexType component) const
{
  std::lock_guard<std::mutex> lock(m_componentDataMutex);

  const Image* img = image(imageUid);
  if (!img)
  {
    spdlog::error("Cannot remove isosurface from invalid image {}.", imageUid);
    return {};
  }

  const uint32_t numComps = img->header().numComponentsPerPixel();
  if (component >= numComps)
  {
    return {};
  }

  auto compDataIt = m_imageToComponentData.find(imageUid);
  if (std::end(m_imageToComponentData) != compDataIt)
  {
    if (component < compDataIt->second.size())
    {
      return compDataIt->second.at(component).m_isosurfaces | boost::adaptors::map_keys;
    }
  }

  return {};
}

std::optional<uuids::uuid> AppData::imageToActiveSegUid(const uuids::uuid& imageUid) const
{
  auto it = m_imageToActiveSeg.find(imageUid);
  if (std::end(m_imageToActiveSeg) != it)
  {
    return it->second;
  }
  return std::nullopt;
}

bool AppData::assignActiveSegUidToImage(const uuids::uuid& imageUid, const uuids::uuid& activeSegUid)
{
  if (image(imageUid) && seg(activeSegUid))
  {
    m_imageToActiveSeg[imageUid] = activeSegUid;

    if (const auto* table = activeLabelTable())
    {
      m_settings.adjustActiveSegmentationLabels(*table);
      return true;
    }
    else
    {
      return false;
    }
  }
  return false;
}

std::optional<uuids::uuid> AppData::imageToActiveDefUid(const uuids::uuid& imageUid) const
{
  auto it = m_imageToActiveDef.find(imageUid);
  if (std::end(m_imageToActiveDef) != it)
  {
    return it->second;
  }
  return std::nullopt;
}

bool AppData::assignActiveDefUidToImage(const uuids::uuid& imageUid, const uuids::uuid& activeDefUid)
{
  if (image(imageUid) && seg(activeDefUid))
  {
    m_imageToActiveDef[imageUid] = activeDefUid;
    return true;
  }
  return false;
}

std::vector<uuids::uuid> AppData::imageToSegUids(const uuids::uuid& imageUid) const
{
  auto it = m_imageToSegs.find(imageUid);
  if (std::end(m_imageToSegs) != it)
  {
    return it->second;
  }
  return std::vector<uuids::uuid>{};
}

std::vector<uuids::uuid> AppData::imageToDefUids(const uuids::uuid& imageUid) const
{
  auto it = m_imageToDefs.find(imageUid);
  if (std::end(m_imageToDefs) != it)
  {
    return it->second;
  }
  return std::vector<uuids::uuid>{};
}

bool AppData::assignSegUidToImage(const uuids::uuid& imageUid, const uuids::uuid& segUid)
{
  if (image(imageUid) && seg(segUid))
  {
    m_imageToSegs[imageUid].emplace_back(segUid);

    if (1 == m_imageToSegs[imageUid].size())
    {
      // If this is the first segmentation, make it the active one
      assignActiveSegUidToImage(imageUid, segUid);
    }

    if (const auto* table = activeLabelTable())
    {
      m_settings.adjustActiveSegmentationLabels(*table);
      return true;
    }
    else
    {
      return false;
    }
  }

  return false;
}

bool AppData::assignDefUidToImage(const uuids::uuid& imageUid, const uuids::uuid& defUid)
{
  if (image(imageUid) && def(defUid))
  {
    m_imageToDefs[imageUid].emplace_back(defUid);

    if (1 == m_imageToDefs[imageUid].size())
    {
      // If this is the first deformation field, make it the active one
      assignActiveDefUidToImage(imageUid, defUid);
    }

    return true;
  }

  return false;
}

const std::vector<uuids::uuid>& AppData::imageToLandmarkGroupUids(const uuids::uuid& imageUid) const
{
  static const std::vector<uuids::uuid> sk_emptyUidVector{};

  auto it = m_imageToLandmarkGroups.find(imageUid);
  if (std::end(m_imageToLandmarkGroups) != it)
  {
    return it->second;
  }
  return sk_emptyUidVector;
}

bool AppData::assignActiveLandmarkGroupUidToImage(
  const uuids::uuid& imageUid, const uuids::uuid& lmGroupUid
)
{
  if (image(imageUid) && landmarkGroup(lmGroupUid))
  {
    m_imageToActiveLandmarkGroup[imageUid] = lmGroupUid;
    return true;
  }
  return false;
}

std::optional<uuids::uuid> AppData::imageToActiveLandmarkGroupUid(const uuids::uuid& imageUid) const
{
  auto it = m_imageToActiveLandmarkGroup.find(imageUid);
  if (std::end(m_imageToActiveLandmarkGroup) != it)
  {
    return it->second;
  }
  return std::nullopt;
}

bool AppData::assignLandmarkGroupUidToImage(const uuids::uuid& imageUid, uuids::uuid lmGroupUid)
{
  if (image(imageUid) && landmarkGroup(lmGroupUid))
  {
    m_imageToLandmarkGroups[imageUid].emplace_back(lmGroupUid);

    // If this is the first landmark group for the image, or if the image has no active
    // landmark group, then make this the image's active landmark group:
    if (1 == m_imageToLandmarkGroups[imageUid].size() || !imageToActiveLandmarkGroupUid(imageUid))
    {
      assignActiveLandmarkGroupUidToImage(imageUid, lmGroupUid);
    }

    return true;
  }
  return false;
}

bool AppData::assignActiveAnnotationUidToImage(
  const uuids::uuid& imageUid, const std::optional<uuids::uuid>& annotUid
)
{
  if (image(imageUid))
  {
    if (annotUid && annotation(*annotUid))
    {
      m_imageToActiveAnnotation[imageUid] = *annotUid;
      return true;
    }
    else if (!annotUid)
    {
      m_imageToActiveAnnotation.erase(imageUid);
      return true;
    }
  }
  return false;
}

std::optional<uuids::uuid> AppData::imageToActiveAnnotationUid(const uuids::uuid& imageUid) const
{
  auto it = m_imageToActiveAnnotation.find(imageUid);
  if (std::end(m_imageToActiveAnnotation) != it)
  {
    return it->second;
  }
  return std::nullopt;
}

const std::list<uuids::uuid>& AppData::annotationsForImage(const uuids::uuid& imageUid) const
{
  static const std::list<uuids::uuid> sk_emptyUidList{};

  auto it = m_imageToAnnotations.find(imageUid);
  if (std::end(m_imageToAnnotations) != it)
  {
    return it->second;
  }
  return sk_emptyUidList;
}

void AppData::setImageBeingSegmented(const uuids::uuid& imageUid, bool set)
{
  if (set)
  {
    m_imagesBeingSegmented.insert(imageUid);
  }
  else
  {
    m_imagesBeingSegmented.erase(imageUid);
  }
}

bool AppData::isImageBeingSegmented(const uuids::uuid& imageUid) const
{
  return (m_imagesBeingSegmented.count(imageUid) > 0);
}

uuid_range_t AppData::imagesBeingSegmented() const
{
  return m_imagesBeingSegmented;
}

std::optional<uuids::uuid> AppData::imageUid(std::size_t index) const
{
  if (index < m_imageUidsOrdered.size())
  {
    return m_imageUidsOrdered[index];
  }
  return std::nullopt;
}

std::optional<uuids::uuid> AppData::segUid(std::size_t index) const
{
  if (index < m_segUidsOrdered.size())
  {
    return m_segUidsOrdered.at(index);
  }
  return std::nullopt;
}

std::optional<uuids::uuid> AppData::defUid(std::size_t index) const
{
  if (index < m_defUidsOrdered.size())
  {
    return m_defUidsOrdered.at(index);
  }
  return std::nullopt;
}

std::optional<uuids::uuid> AppData::imageColorMapUid(std::size_t index) const
{
  if (index < m_imageColorMapUidsOrdered.size())
  {
    return m_imageColorMapUidsOrdered.at(index);
  }
  return std::nullopt;
}

std::optional<uuids::uuid> AppData::labelTableUid(std::size_t index) const
{
  if (index < m_labelTablesUidsOrdered.size())
  {
    return m_labelTablesUidsOrdered.at(index);
  }
  return std::nullopt;
}

std::optional<uuids::uuid> AppData::landmarkGroupUid(std::size_t index) const
{
  if (index < m_landmarkGroupUidsOrdered.size())
  {
    return m_landmarkGroupUidsOrdered.at(index);
  }
  return std::nullopt;
}

std::optional<std::size_t> AppData::imageIndex(const uuids::uuid& imageUid) const
{
  std::size_t i = 0;
  for (const auto& uid : m_imageUidsOrdered)
  {
    if (uid == imageUid)
    {
      return i;
    }
    ++i;
  }
  return std::nullopt;
}

std::optional<std::size_t> AppData::segIndex(const uuids::uuid& segUid) const
{
  for (std::size_t i = 0; i < m_segUidsOrdered.size(); ++i)
  {
    if (m_segUidsOrdered.at(i) == segUid)
    {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<std::size_t> AppData::defIndex(const uuids::uuid& defUid) const
{
  for (std::size_t i = 0; i < m_defUidsOrdered.size(); ++i)
  {
    if (m_defUidsOrdered.at(i) == defUid)
    {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<std::size_t> AppData::imageColorMapIndex(const uuids::uuid& mapUid) const
{
  for (std::size_t i = 0; i < m_imageColorMapUidsOrdered.size(); ++i)
  {
    if (m_imageColorMapUidsOrdered.at(i) == mapUid)
    {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<std::size_t> AppData::labelTableIndex(const uuids::uuid& tableUid) const
{
  for (std::size_t i = 0; i < m_labelTablesUidsOrdered.size(); ++i)
  {
    if (m_labelTablesUidsOrdered.at(i) == tableUid)
    {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<std::size_t> AppData::landmarkGroupIndex(const uuids::uuid& lmGroupUid) const
{
  for (std::size_t i = 0; i < m_landmarkGroupUidsOrdered.size(); ++i)
  {
    if (m_landmarkGroupUidsOrdered.at(i) == lmGroupUid)
    {
      return i;
    }
  }
  return std::nullopt;
}

std::optional<std::size_t> AppData::annotationIndex(
  const uuids::uuid& imageUid, const uuids::uuid& annotUid
) const
{
  std::size_t i = 0;
  for (const auto& uid : annotationsForImage(imageUid))
  {
    if (annotUid == uid)
    {
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
  if (!imgUid)
    return nullptr;

  const auto segUid = imageToActiveSegUid(*imgUid);
  if (!segUid)
    return nullptr;

  return seg(*segUid);
}

ParcellationLabelTable* AppData::activeLabelTable()
{
  ParcellationLabelTable* activeLabelTable = nullptr;

  if (m_activeImageUid)
  {
    if (const auto activeSegUid = imageToActiveSegUid(*m_activeImageUid))
    {
      if (const Image* activeSeg = seg(*activeSegUid))
      {
        if (const auto tableUid = labelTableUid(activeSeg->settings().labelTableIndex()))
        {
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

  for (const auto& imageUid : imageUidsOrdered())
  {
    if (const Image* img = image(imageUid))
    {
      if (!first)
        allImageDisplayNames << ", ";
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
