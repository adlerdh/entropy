#include "EntropyApp.h"

#include "common/DirectionMaps.h"
#include "common/Exception.hpp"
#include "common/MathFuncs.h"

#include "image/ImageUtility.h"
#include "image/DicomSeries.h"

#include "layout/LayoutFileSerialization.h"

#include "logic/app/DataHelper.h"
#include "logic/app/ImageSelectionPolicy.h"
#include "logic/app/LoadingStatusItems.h"
#include "logic/app/ProjectSnapshotComparison.h"
#include "logic/app/ProjectSnapshotSettings.h"

#include "logic/annotation/Annotation.h"
#include "logic/annotation/LandmarkGroup.h"
#include "logic/camera/MathUtility.h"
#include "logic/serialization/ProjectSerialization.h"
#include "logic/DistanceMap.h"

#include "ui/NativeFileDialogs.h"

#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>
#include <glm/gtx/string_cast.hpp>

#include <spdlog/fmt/std.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

// Without undefining min and max, there are some errors compiling in Visual Studio
#undef min
#undef max

namespace fs = std::filesystem;

namespace
{
bool promptForChar(const char* prompt, char& readch)
{
  std::string tmp;
  std::cout << prompt << '\n';

  if (std::getline(std::cin, tmp)) {
    // Only accept single character input
    if (1 == tmp.length()) {
      readch = tmp[0];
    }
    else {
      // For most input, char zero is an appropriate sentinel
      readch = '\0';
    }

    return true;
  }

  return false;
}

std::vector<fs::path> nonEmptyPaths(const std::vector<fs::path>& fileNames)
{
  std::vector<fs::path> paths;
  paths.reserve(fileNames.size());

  for (const auto& fileName : fileNames) {
    if (!fileName.empty()) {
      paths.push_back(fileName);
    }
  }

  return paths;
}

serialize::EntropyProject createProjectFromImageFiles(const std::vector<fs::path>& fileNames)
{
  serialize::EntropyProject project;
  if (fileNames.empty()) {
    return project;
  }

  project.m_referenceImage.m_imageFileName = fileNames.front();
  for (std::size_t i = 1; i < fileNames.size(); ++i) {
    serialize::Image image;
    image.m_imageFileName = fileNames[i];
    project.m_additionalImages.emplace_back(std::move(image));
  }

  return project;
}

bool isJsonProjectFile(const fs::path& fileName)
{
  std::string extension = fileName.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return extension == ".json";
}

bool isDicomInputPath(const fs::path& path)
{
  if (path.empty()) {
    return false;
  }

  std::error_code ec;
  if (fs::is_directory(path, ec)) {
    return true;
  }

  return dicom::canReadDicomHeader(path);
}

std::size_t numSerializedImages(const serialize::EntropyProject& project)
{
  return 1u + project.m_additionalImages.size();
}

serialize::Image* serializedImageAt(serialize::EntropyProject& project, std::size_t index)
{
  if (0 == index) {
    return &project.m_referenceImage;
  }
  index--;
  if (index >= project.m_additionalImages.size()) {
    return nullptr;
  }
  return &project.m_additionalImages[index];
}

void eraseSerializedImageAt(serialize::EntropyProject& project, std::size_t index)
{
  if (0 == index) {
    return;
  }
  index--;
  if (index < project.m_additionalImages.size()) {
    project.m_additionalImages.erase(project.m_additionalImages.begin() + static_cast<std::ptrdiff_t>(index));
  }
}

bool containsDicomInputPath(const std::vector<fs::path>& paths)
{
  return std::any_of(paths.begin(), paths.end(), [](const fs::path& path) { return isDicomInputPath(path); });
}

ViewType nativeSliceViewType(const Image& image)
{
  const glm::vec3 sliceDirection = image.header().directions()[2];
  if (glm::length(sliceDirection) <= 0.0f) {
    return ViewType::Axial;
  }

  const glm::vec3 normal = glm::abs(glm::normalize(sliceDirection));
  if (normal.x >= normal.y && normal.x >= normal.z) {
    return ViewType::Sagittal;
  }
  if (normal.y >= normal.x && normal.y >= normal.z) {
    return ViewType::Coronal;
  }
  return ViewType::Axial;
}

serialize::DicomSource makeDicomSourceSnapshot(const dicom::SeriesInfo& series)
{
  serialize::DicomSource source;
  source.m_rootPath = series.rootPath;
  source.m_studyInstanceUid = series.metadata.studyInstanceUid;
  source.m_seriesInstanceUid = series.seriesInstanceUid;
  source.m_files = series.files;
  return source;
}

void logLoadedImageDetails(const Image& image, const fs::path& sourceFileName)
{
  spdlog::info("Read image from file {}", sourceFileName);

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
  std::ostringstream ss;
  image.metaData(ss);

  SPDLOG_TRACE("Meta data:\n{}", ss.str());
#endif
  spdlog::info("Header:\n{}", image.header());
  spdlog::info("Transformation:\n{}", image.transformations());
  spdlog::info("Settings:\n{}", image.settings());
}

std::optional<dicom::SeriesInfo> resolveDicomSource(const serialize::DicomSource& source)
{
  std::vector<fs::path> inputs;
  if (!source.m_rootPath.empty()) {
    inputs.push_back(source.m_rootPath);
  }
  else {
    inputs.insert(inputs.end(), source.m_files.begin(), source.m_files.end());
  }

  if (inputs.empty()) {
    return std::nullopt;
  }

  dicom::DiscoverResult result = dicom::discoverSeries(inputs);
  for (const auto& series : result.series) {
    if (!series.loadable()) {
      continue;
    }
    if (!source.m_seriesInstanceUid.empty() && series.seriesInstanceUid != source.m_seriesInstanceUid) {
      continue;
    }
    if (!source.m_studyInstanceUid.empty() && series.metadata.studyInstanceUid != source.m_studyInstanceUid) {
      continue;
    }
    return series;
  }

  return std::nullopt;
}

constexpr uint64_t LargeImageWarningBytes = 2ull * 1024ull * 1024ull * 1024ull;

bool shouldPromptForLargeImage(const ImageHeader& header)
{
  return header.memoryImageSizeInBytes() >= LargeImageWarningBytes;
}

} // namespace

std::pair<std::optional<uuids::uuid>, bool> EntropyApp::loadImage(const fs::path& fileName, bool ignoreIfAlreadyLoaded)
{
  if (ignoreIfAlreadyLoaded) {
    // Has this image already been loaded? Search for its file name:
    for (const auto& imageUid : m_data.imageUidsOrdered()) {
      const Image* image = m_data.image(imageUid);
      if (!image) {
        continue;
      }

      if (image->header().fileName() == fileName) {
        spdlog::info("Image {} has already been loaded as {}", fileName, imageUid);
        markLoadingStatusItemLoaded(GuiData::LoadingStatusItem::Kind::Image, fileName);
        return {imageUid, false};
      }
    }
  }

  std::optional<Image> dicomImage;
  if (dicom::canReadDicomHeader(fileName)) {
    dicom::DiscoverResult result = dicom::discoverSeries({fileName});
    std::error_code ec;
    const fs::path canonicalFileName = fs::weakly_canonical(fileName, ec);
    const fs::path comparableFileName = ec ? fileName : canonicalFileName;

    const dicom::SeriesInfo* matchingSeries = nullptr;
    for (const auto& series : result.series) {
      if (!series.loadable()) {
        continue;
      }
      const auto fileIt = std::find_if(series.files.begin(), series.files.end(), [&](const fs::path& seriesFile) {
        std::error_code seriesEc;
        const fs::path canonicalSeriesFile = fs::weakly_canonical(seriesFile, seriesEc);
        return (seriesEc ? seriesFile : canonicalSeriesFile) == comparableFileName;
      });
      if (fileIt != series.files.end()) {
        matchingSeries = &series;
        break;
      }
      if (!matchingSeries) {
        matchingSeries = &series;
      }
    }

    if (matchingSeries) {
      spdlog::info(
        "Image file {} is a DICOM slice; loading containing series {}",
        fileName,
        matchingSeries->seriesInstanceUid);
      dicomImage = dicom::loadSeriesImage(*matchingSeries);
    }
  }

  Image image = dicomImage
                  ? std::move(*dicomImage)
                  : Image(fileName, Image::ImageRepresentation::Image, Image::MultiComponentBufferType::SeparateImages);

  logLoadedImageDetails(image, fileName);

  auto loadedImage = std::make_pair(m_data.addImage(std::move(image)), true);
  spdlog::info("Loaded image from {} as {}", fileName, loadedImage.first);
  markLoadingStatusItemLoaded(GuiData::LoadingStatusItem::Kind::Image, fileName);
  return loadedImage;
}

std::pair<std::optional<uuids::uuid>, bool> EntropyApp::loadDicomSeriesImage(const dicom::SeriesInfo& series)
{
  if (series.files.empty()) {
    spdlog::error("Could not load DICOM series {} because it has no files", series.seriesInstanceUid);
    return {std::nullopt, false};
  }

  std::optional<Image> image = dicom::loadSeriesImage(series);
  if (!image) {
    spdlog::error("Could not load DICOM series {}", series.seriesInstanceUid);
    return {std::nullopt, false};
  }

  logLoadedImageDetails(*image, series.files.front());
  auto loadedImage = std::make_pair(m_data.addImage(std::move(*image)), true);
  spdlog::info(
    "Loaded DICOM series {} from {} as {}",
    series.seriesInstanceUid,
    series.files.front(),
    loadedImage.first);
  markLoadingStatusItemLoaded(GuiData::LoadingStatusItem::Kind::Image, series.files.front());
  return loadedImage;
}

std::unordered_map<uuids::uuid, ViewType> EntropyApp::dicomNativeViewTypesByImage() const
{
  std::unordered_map<uuids::uuid, ViewType> viewTypesByImage;
  viewTypesByImage.reserve(m_dicomSourcesByImageUid.size());

  for (const auto& [imageUid, source] : m_dicomSourcesByImageUid) {
    (void)source;
    const Image* image = m_data.image(imageUid);
    if (!image) {
      continue;
    }

    viewTypesByImage.emplace(imageUid, nativeSliceViewType(*image));
  }

  return viewTypesByImage;
}

std::pair<std::optional<uuids::uuid>, bool> EntropyApp::loadSegmentation(
  const fs::path& fileName,
  const std::optional<uuids::uuid>& matchingImageUid)
{
  // Setting indicating that the same segmentation image file can be loaded twice:
  constexpr bool canLoadSameSegFileTwice = false;
  constexpr auto EPS = glm::epsilon<float>();

  // Return value indicating that segmentation was not loaded:
  static const std::pair<std::optional<uuids::uuid>, bool> noSegLoaded{std::nullopt, false};

  // Has this segmentation already been loaded? Search for its file name:
  for (const auto& segUid : m_data.segUidsOrdered()) {
    const Image* seg = m_data.seg(segUid);
    if (seg && seg->header().fileName() == fileName) {
      spdlog::info("Segmentation from file {} has already been loaded as {}", fileName, segUid);

      if (!canLoadSameSegFileTwice) {
        markLoadingStatusItemLoaded(GuiData::LoadingStatusItem::Kind::Segmentation, fileName);
        return {segUid, false};
      }
    }
  }

  // Creating an image as a segmentation will convert the pixel components to the most
  // suitable unsigned integer type
  Image seg(fileName, Image::ImageRepresentation::Segmentation, Image::MultiComponentBufferType::SeparateImages);

  // Set the default opacity:
  seg.settings().setOpacity(0.5);

  spdlog::info("Read segmentation image from file {}", fileName);

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
  std::ostringstream ss;
  seg.metaData(ss);

  SPDLOG_TRACE("Meta data:\n{}", ss.str());
#endif
  spdlog::info("Header:\n{}", seg.header());
  spdlog::info("Transformation:\n{}", seg.transformations());

  const Image* matchImg = (matchingImageUid) ? m_data.image(*matchingImageUid) : nullptr;

  if (!matchImg) {
    // No valid image was provided to match with this segmentation.
    // Add just the segmentation without pairing it to an image.
    if (const auto segUid = m_data.addSeg(std::move(seg))) {
      spdlog::info("Loaded segmentation from file {} as {}", fileName, *segUid);
      markLoadingStatusItemLoaded(GuiData::LoadingStatusItem::Kind::Segmentation, fileName);
      return {*segUid, true};
    }
    else {
      return noSegLoaded;
    }
  }

  // Compare header of segmentation with header of its matching image:
  const auto& imgTx = matchImg->transformations();
  const auto& segTx = seg.transformations();

  // TODO: Is there a better way to handle non-matching matrices?
  // Perhaps there should be a setting in the project file that allows us to override
  // the segmentation's subject_T_texture matrix with the matrix of the image

  if (!math::areMatricesEqual(imgTx.subject_T_texture(), segTx.subject_T_texture())) {
    spdlog::warn(
      "The subject_T_texture transformations for image {} "
      "and segmentation from file {} do not match:",
      *matchingImageUid,
      fileName);

    spdlog::info("subject_T_texture matrix for image:\n{}", glm::to_string(imgTx.subject_T_texture()));
    spdlog::info("subject_T_texture matrix for segmentation:\n{}", glm::to_string(segTx.subject_T_texture()));

    const auto& imgHdr = matchImg->header();
    const auto& segHdr = seg.header();

    if (glm::any(glm::epsilonNotEqual(imgHdr.origin(), segHdr.origin(), EPS))) {
      spdlog::warn(
        "The origins of image ({}) and segmentation ({}) do not match",
        glm::to_string(imgHdr.origin()),
        glm::to_string(segHdr.origin()));
    }

    if (glm::any(glm::epsilonNotEqual(imgHdr.spacing(), segHdr.spacing(), EPS))) {
      spdlog::warn(
        "The voxel spacings of image ({}) and segmentation ({}) do not match",
        glm::to_string(imgHdr.spacing()),
        glm::to_string(segHdr.spacing()));
    }

    if (!math::areMatricesEqual(imgHdr.directions(), segHdr.directions())) {
      spdlog::warn(
        "The direction vectors of image ({}) and segmentation ({}) do not match",
        glm::to_string(imgHdr.directions()),
        glm::to_string(segHdr.directions()));
    }

    if (imgHdr.pixelDimensions() != segHdr.pixelDimensions()) {
      spdlog::warn(
        "The pixel dimensions of image ({}) and segmentation ({}) do not match",
        glm::to_string(imgHdr.pixelDimensions()),
        glm::to_string(segHdr.pixelDimensions()));
    }

    char type = '\0';

    while (promptForChar("\nContinue loading the segmentation despite transformation mismatch? [y/n]", type)) {
      if ('n' == type || 'N' == type) {
        spdlog::info("The segmentation from file {} will be loaded", fileName);
        return noSegLoaded;
      }
      else if ('y' == type || 'Y' == type) {
        spdlog::info("The segmentation from file {} will not be loaded due to subject_T_texture mismatch", fileName);
        break;
      }
    }
  }

  // The image and segmentation transformations match!

  if (!isComponentUnsignedInt(seg.header().memoryComponentType())) {
    spdlog::error(
      "The segmentation from file {} does not have unsigned integer pixel "
      "component type and so will not be loaded.",
      fileName);
    return noSegLoaded;
  }

  // Synchronize transformation on all segmentations of the image:
  m_callbackHandler.syncManualImageTransformationOnSegs(*matchingImageUid);

  if (const auto segUid = m_data.addSeg(std::move(seg))) {
    spdlog::info("Loaded segmentation from file {} for image {} as {}", fileName, *matchingImageUid, *segUid);
    markLoadingStatusItemLoaded(GuiData::LoadingStatusItem::Kind::Segmentation, fileName);
    return {*segUid, true};
  }

  return noSegLoaded;
}

std::pair<std::optional<uuids::uuid>, bool> EntropyApp::loadDeformationField(const fs::path& fileName)
{
  // Return value indicating that warp field was not loaded:
  const std::pair<std::optional<uuids::uuid>, bool> noDefLoaded{std::nullopt, false};

  // Has this warp field already been loaded? Search for its file name:
  for (const auto& defUid : m_data.defUidsOrdered()) {
    if (const Image* def = m_data.def(defUid)) {
      if (def->header().fileName() == fileName) {
        spdlog::info("Warp field from {} has already been loaded as {}", fileName, defUid);
        markLoadingStatusItemLoaded(GuiData::LoadingStatusItem::Kind::Image, fileName);
        return {defUid, false};
      }
    }
  }

  for (const auto& imageUid : m_data.imageUidsOrdered()) {
    const Image* image = m_data.warpField(imageUid);
    if (image && image->header().fileName() == fileName) {
      if (Image* mutableImage = m_data.image(imageUid)) {
        mutableImage->settings().setComponentRenderMode(ComponentRenderMode::Magnitude);
      }
      spdlog::info("Using already-loaded image {} from {} as a warp field", imageUid, fileName);
      markLoadingStatusItemLoaded(GuiData::LoadingStatusItem::Kind::Image, fileName);
      return {imageUid, false};
    }
  }

  Image def(fileName, Image::ImageRepresentation::Image, Image::MultiComponentBufferType::InterleavedImage);

  if (def.header().numComponentsPerPixel() < 3) {
    spdlog::error(
      "The warp field from file {} has fewer than three components per pixel "
      "and so will not be loaded.",
      fileName);
    return noDefLoaded;
  }

  if (!def.bufferAsVoid(0, def.timeAxis().clamp(def.settings().activeTimePoint()))) {
    spdlog::error("The warp field from file {} does not expose loaded pixel data and so will not be loaded.", fileName);
    return noDefLoaded;
  }

  spdlog::info("Read warp field image from file {}", fileName);

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
  std::ostringstream ss;
  def.metaData(ss);

  SPDLOG_TRACE("Meta data:\n{}", ss.str());
#endif
  spdlog::info("Header:\n{}", def.header());
  spdlog::info("Transformation:\n{}", def.transformations());
  spdlog::info("Settings:\n{}", def.settings());

  // TODO: Do check of warp field header against the reference image header?
  def.settings().setComponentRenderMode(ComponentRenderMode::Magnitude);

  if (const auto defUid = m_data.addDef(std::move(def))) {
    spdlog::info("Loaded warp field image from file {} as {}", fileName, *defUid);
    markLoadingStatusItemLoaded(GuiData::LoadingStatusItem::Kind::Image, fileName);
    return {*defUid, true};
  }

  return noDefLoaded;
}

void EntropyApp::loadAndAssignDeformationField(
  const uuids::uuid& imageUid,
  const fs::path& fileName,
  bool forwardWarp,
  std::optional<uuids::uuid> inverseWarpReferenceImageUid)
{
  std::error_code error;
  const auto bytes = fs::file_size(fileName, error);
  std::vector<GuiData::LoadingStatusItem> loadingItems{GuiData::LoadingStatusItem{
    GuiData::LoadingStatusItem::Kind::Image,
    fileName,
    error ? std::nullopt : std::optional<std::uintmax_t>{bytes},
    false}};

  m_preserveLayoutsOnImagesReady = true;
  m_pendingAddedImageUids.clear();

  startAsyncImageLoad(
    forwardWarp ? "Loading forward warp..." : "Loading inverse warp...",
    [this, imageUid, fileName, forwardWarp, inverseWarpReferenceImageUid]() {
      if (m_imageLoadCancelled) {
        return false;
      }

      const auto [warpUid, loaded] = loadDeformationField(fileName);
      if (!warpUid) {
        spdlog::error("Unable to load warp field from {}", fileName);
        return false;
      }

      m_pendingWarpAssignment =
        PendingWarpAssignment{imageUid, *warpUid, loaded, forwardWarp, inverseWarpReferenceImageUid};
      return true;
    },
    [this]() {
      m_data.state().setAnimating(false);
      m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
      updateWindowTitleStatus();
    },
    false,
    std::move(loadingItems),
    forwardWarp ? "Loading Forward Warp" : "Loading Inverse Warp");
}

bool EntropyApp::loadSerializedImage(
  const serialize::Image& serializedImage,
  bool isReferenceImage,
  const dicom::SeriesInfo* resolvedDicomSeries)
{
  constexpr size_t defaultImageColorMapIndex = 0;

  // Do NOT ignore images if they have already been loaded:
  // (i.e. load duplicate images again anyway):
  constexpr bool ignoreImageIfAlreadyLoaded = false;

  serialize::Image imageToLoad = serializedImage;
  std::optional<dicom::SeriesInfo> ownedResolvedDicomSeries;
  std::optional<serialize::DicomSource> resolvedDicomSource = serializedImage.m_dicomSource;

  if (resolvedDicomSeries) {
    if (resolvedDicomSeries->files.empty()) {
      spdlog::error(
        "Could not resolve DICOM source for series {} because it has no files",
        resolvedDicomSeries->seriesInstanceUid);
      return false;
    }
    imageToLoad.m_imageFileName = resolvedDicomSeries->files.front();
    resolvedDicomSource = makeDicomSourceSnapshot(*resolvedDicomSeries);
  }
  else if (serializedImage.m_dicomSource) {
    ownedResolvedDicomSeries = resolveDicomSource(*serializedImage.m_dicomSource);
    if (!ownedResolvedDicomSeries) {
      spdlog::error("Could not resolve DICOM source for series {}", serializedImage.m_dicomSource->m_seriesInstanceUid);
      return false;
    }
    if (ownedResolvedDicomSeries->files.empty()) {
      spdlog::error(
        "Could not resolve DICOM source for series {} because it has no files",
        serializedImage.m_dicomSource->m_seriesInstanceUid);
      return false;
    }
    imageToLoad.m_imageFileName = ownedResolvedDicomSeries->files.front();
    resolvedDicomSource = makeDicomSourceSnapshot(*ownedResolvedDicomSeries);
    resolvedDicomSeries = &*ownedResolvedDicomSeries;
  }

  // Load image:
  std::optional<uuids::uuid> imageUid;
  bool isNewImage = false;

  try {
    spdlog::debug("Attempting to load image from {}", imageToLoad.m_imageFileName);
    if (resolvedDicomSeries) {
      std::tie(imageUid, isNewImage) = loadDicomSeriesImage(*resolvedDicomSeries);
    }
    else {
      std::tie(imageUid, isNewImage) = loadImage(imageToLoad.m_imageFileName, ignoreImageIfAlreadyLoaded);
    }
  }
  catch (const std::exception& e) {
    spdlog::error("Exception loading image from {}: {}", imageToLoad.m_imageFileName, e.what());
    return false;
  }

  if (!imageUid) {
    spdlog::error("Unable to load image from {}", imageToLoad.m_imageFileName);
    return false;
  }

  if (!isNewImage) {
    spdlog::info("Image from {} already exists in this project as {}", imageToLoad.m_imageFileName, *imageUid);

    if (ignoreImageIfAlreadyLoaded) {
      // Because this setting is true, cancel loading the rest of the data for this image:
      return true;
    }
  }

  Image* image = m_data.image(*imageUid);
  if (!image) {
    spdlog::error("Null image {}", *imageUid);
    return false;
  }

  if (resolvedDicomSource) {
    m_dicomSourcesByImageUid[*imageUid] = *resolvedDicomSource;
  }

  if (serializedImage.m_spatialMetadata) {
    image->setUserSpatialMetadata(*serializedImage.m_spatialMetadata);
  }
  else if (!resolvedDicomSource && isStandardRasterImageFile(imageToLoad.m_imageFileName)) {
    image->setUserSpatialMetadata(ImageSpatialMetadata{});
  }
  if (const auto& metadata = image->header().userSpatialMetadata()) {
    for (const auto& segUid : m_data.imageToSegUids(*imageUid)) {
      if (Image* seg = m_data.seg(segUid)) {
        seg->setUserSpatialMetadata(*metadata);
      }
    }
  }

  if (serializedImage.m_settings) {
    project_snapshot::applyImageSettings(*image, *serializedImage.m_settings);
  }

  // Disable the initial affine and manual transformations for the reference image:
  image->transformations().set_enable_worldDef_T_affine(!isReferenceImage);
  image->transformations().set_enable_affine_T_subject(!isReferenceImage);

  // Lock all affine transformations to the reference image, which defines the World space:
  image->transformations().set_worldDef_T_affine_locked(true);

  // Load and set the initial/imported affine transformation for non-reference images.
  if (serializedImage.m_initialAffineMatrix || serializedImage.m_initialAffineFileName) {
    glm::dmat4 affine_T_subject(1.0);

    if (isReferenceImage) {
      if (serializedImage.m_initialAffineFileName) {
        spdlog::warn(
          "An affine transformation file ({}) was provided for the reference image. "
          "It will be ignored, since the reference image defines the World coordinate "
          "space, which cannot be transformed.",
          *serializedImage.m_initialAffineFileName);
      }
      else {
        spdlog::warn(
          "An embedded initial affine transformation was provided for the reference image. It will be ignored, since "
          "the reference image defines World space.");
      }

      image->transformations().set_affine_T_subject_fileName(std::nullopt);
    }
    else {
      if (serializedImage.m_initialAffineMatrix) {
        affine_T_subject = glm::dmat4{*serializedImage.m_initialAffineMatrix};
        image->transformations().set_affine_T_subject_fileName(std::nullopt);
      }
      else if (serializedImage.m_initialAffineFileName) {
        if (!serialize::openAffineTxFile(affine_T_subject, *serializedImage.m_initialAffineFileName)) {
          spdlog::error(
            "Unable to read affine transformation from {} for image {}",
            *serializedImage.m_initialAffineFileName,
            *imageUid);

          image->transformations().set_affine_T_subject_fileName(std::nullopt);
        }
        else {
          image->transformations().set_affine_T_subject_fileName(serializedImage.m_initialAffineFileName);
        }
      }

      image->transformations().set_affine_T_subject(glm::mat4{affine_T_subject});
    }
  }
  else {
    // No affine transformation provided:
    image->transformations().set_affine_T_subject_fileName(std::nullopt);
  }

  if (serializedImage.m_manualAffineMatrix || serializedImage.m_manualAffineFileName) {
    if (isReferenceImage) {
      spdlog::warn(
        "A manual transformation was provided for the reference image. It will be ignored, since the reference "
        "image defines World space.");
    }
    else {
      glm::dmat4 worldDef_T_affine(1.0);
      bool manualAffineAvailable = true;
      if (serializedImage.m_manualAffineMatrix) {
        worldDef_T_affine = glm::dmat4{*serializedImage.m_manualAffineMatrix};
      }
      else if (
        serializedImage.m_manualAffineFileName &&
        !serialize::openAffineTxFile(worldDef_T_affine, *serializedImage.m_manualAffineFileName))
      {
        spdlog::error(
          "Unable to read manual affine transformation from {} for image {}",
          *serializedImage.m_manualAffineFileName,
          *imageUid);
        manualAffineAvailable = false;
      }

      if (manualAffineAvailable) {
        image->transformations().set_worldDef_T_affine_locked(false);
        image->transformations().set_enable_worldDef_T_affine(true);
        image->transformations().set_worldDef_T_affine(glm::mat4{worldDef_T_affine});
        image->transformations().set_worldDef_T_affine_locked(true);
      }
    }
  }

  if (serializedImage.m_inverseWarpFieldPath) {
    std::optional<uuids::uuid> inverseWarpUid;
    bool isInverseWarpNewImage = false;

    try {
      spdlog::debug("Attempting to load inverse warp image from {}", *serializedImage.m_inverseWarpFieldPath);
      std::tie(inverseWarpUid, isInverseWarpNewImage) = loadDeformationField(*serializedImage.m_inverseWarpFieldPath);
    }
    catch (const std::exception& e) {
      spdlog::error("Exception loading inverse warp from {}: {}", *serializedImage.m_inverseWarpFieldPath, e.what());
    }

    do {
      if (!inverseWarpUid) {
        spdlog::error(
          "Unable to load inverse warp from {} for image {}",
          *serializedImage.m_inverseWarpFieldPath,
          *imageUid);
        break;
      }

      if (!isInverseWarpNewImage) {
        spdlog::info(
          "Inverse warp from {} already exists in this project as image {}",
          *serializedImage.m_inverseWarpFieldPath,
          *inverseWarpUid);
      }

      Image* inverseWarp = m_data.def(*inverseWarpUid);

      if (!inverseWarp) {
        spdlog::error("Null inverse warp image {}", *inverseWarpUid);
        break;
      }

      if (isInverseWarpNewImage) {
        inverseWarp->settings().setDisplayName(inverseWarp->settings().displayName() + " (deformation)");
      }

      const std::optional<uuids::uuid> inverseWarpReferenceImageUid = imageUid;

      if (m_data.assignInverseWarpUidToImage(*imageUid, *inverseWarpUid, inverseWarpReferenceImageUid)) {
        spdlog::info("Assigned inverse warp {} to image {}", *inverseWarpUid, *imageUid);
        if (serializedImage.m_inverseWarpReferenceImagePath) {
          m_pendingInverseWarpReferences.push_back(
            PendingInverseWarpReference{*imageUid, *serializedImage.m_inverseWarpReferenceImagePath});
        }
      }
      else {
        spdlog::error("Unable to assign inverse warp {} to image {}", *inverseWarpUid, *imageUid);
        if (isInverseWarpNewImage) {
          m_data.removeDef(*inverseWarpUid);
        }
        break;
      }

      break;
    } while (true);

    // TODO: Warp field images are special:
    // 1) no segmentation is created
    // 2) no affine transformation can be applied: it copies the affine tx of its image
    // 3) need warning when header tx doesn't match that of reference
    // 4) even if all components are loaded as RGB texture, we should be able to view each component
    // separately in a shader that takes in as a uniform the active component
  }

  if (serializedImage.m_forwardWarpFieldPath) {
    std::optional<uuids::uuid> forwardWarpUid;
    bool isForwardWarpNewImage = false;

    try {
      spdlog::debug("Attempting to load forward warp image from {}", *serializedImage.m_forwardWarpFieldPath);
      std::tie(forwardWarpUid, isForwardWarpNewImage) = loadDeformationField(*serializedImage.m_forwardWarpFieldPath);
    }
    catch (const std::exception& e) {
      spdlog::error("Exception loading forward warp from {}: {}", *serializedImage.m_forwardWarpFieldPath, e.what());
    }

    do {
      if (!forwardWarpUid) {
        spdlog::error(
          "Unable to load forward warp from {} for image {}",
          *serializedImage.m_forwardWarpFieldPath,
          *imageUid);
        break;
      }

      if (!isForwardWarpNewImage) {
        spdlog::info(
          "Forward warp from {} already exists in this project as image {}",
          *serializedImage.m_forwardWarpFieldPath,
          *forwardWarpUid);
      }

      Image* forwardWarp = m_data.def(*forwardWarpUid);

      if (!forwardWarp) {
        spdlog::error("Null forward warp image {}", *forwardWarpUid);
        break;
      }

      if (isForwardWarpNewImage) {
        forwardWarp->settings().setDisplayName(forwardWarp->settings().displayName() + " (deformation)");
      }

      if (m_data.assignForwardWarpUidToImage(*imageUid, *forwardWarpUid)) {
        spdlog::info("Assigned forward warp {} to image {}", *forwardWarpUid, *imageUid);
      }
      else {
        spdlog::error("Unable to assign forward warp {} to image {}", *forwardWarpUid, *imageUid);
        if (isForwardWarpNewImage) {
          m_data.removeDef(*forwardWarpUid);
        }
        break;
      }

      break;
    } while (true);
  }

  auto addAnnotationsToImage = [this,
                                &imageUid](std::vector<Annotation> annots, const std::optional<fs::path>& fileName) {
    for (auto& annot : annots) {
      if (fileName) {
        annot.setFileName(*fileName);
      }

      if (const auto annotUid = m_data.addAnnotation(*imageUid, annot)) {
        m_data.assignActiveAnnotationUidToImage(*imageUid, annotUid);
        spdlog::debug("Added annotation {} for image {}", *annotUid, *imageUid);
      }
      else {
        spdlog::error("Unable to add annotation to image {}", *imageUid);
      }
    }
  };

  if (!serializedImage.m_annotations.empty()) {
    if (serializedImage.m_annotationsFileName) {
      spdlog::warn(
        "Image {} has embedded annotations and annotations.path {}; using embedded annotations",
        *imageUid,
        *serializedImage.m_annotationsFileName);
    }

    spdlog::info("Loaded {} embedded annotation(s) for image {}", serializedImage.m_annotations.size(), *imageUid);
    addAnnotationsToImage(serializedImage.m_annotations, serializedImage.m_annotationsFileName);
  }
  else if (serializedImage.m_annotationsFileName) {
    std::vector<Annotation> annots;

    if (serialize::openAnnotationsFromJsonFile(annots, *serializedImage.m_annotationsFileName)) {
      spdlog::info(
        "Loaded annotations from JSON file {} for image {}",
        *serializedImage.m_annotationsFileName,
        *imageUid);

      addAnnotationsToImage(std::move(annots), serializedImage.m_annotationsFileName);
    }
    else {
      spdlog::error(
        "Unable to open annotations from JSON file {} for image {}",
        *serializedImage.m_annotationsFileName,
        *imageUid);
    }
  }

  const auto hueMinMax = std::make_pair(0.0f, 360.0f);
  const auto satMinMax = std::make_pair(0.6f, 1.0f);
  const auto valMinMax = std::make_pair(0.6f, 1.0f);

  // Set embedded landmarks or load path-backed landmark CSV files.
  for (const auto& lm : serializedImage.m_landmarkGroups) {
    std::map<size_t, PointRecord<glm::vec3> > landmarks;

    bool loadedLandmarks = false;
    if (!lm.m_points.empty()) {
      for (const auto& point : lm.m_points) {
        landmarks.try_emplace(point.m_index, point.m_position, point.m_name);
      }
      loadedLandmarks = true;
      spdlog::info("Loaded {} embedded landmarks for image {}", landmarks.size(), *imageUid);
    }
    else if (lm.m_csvFileName && serialize::openLandmarkGroupCsvFile(landmarks, *lm.m_csvFileName)) {
      loadedLandmarks = true;
      spdlog::info("Loaded landmarks from CSV file {} for image {}", *lm.m_csvFileName, *imageUid);
    }

    if (loadedLandmarks) {
      // Assign random colors to the landmarks. Make sure that landmarks with the same index
      // in different groups have the same color. This is done by seeding the random number
      // generator with the landmark index.
      for (auto& p : landmarks) {
        const auto colors =
          math::generateRandomHsvSamples(1, hueMinMax, satMinMax, valMinMax, static_cast<uint32_t>(p.first));

        if (!colors.empty()) {
          p.second.setColor(glm::rgbColor(colors[0]));
        }
      }

      LandmarkGroup lmGroup;
      if (lm.m_csvFileName) {
        lmGroup.setFileName(*lm.m_csvFileName);
      }
      lmGroup.setName(
        !lm.m_name.empty() ? lm.m_name
                           : (lm.m_csvFileName ? getFileName(lm.m_csvFileName->string(), false) : "Landmarks"));
      lmGroup.setPoints(landmarks);
      lmGroup.setVisibility(lm.m_visible);
      lmGroup.setOpacity(lm.m_opacity);
      lmGroup.setColor(lm.m_color);
      lmGroup.setColorOverride(lm.m_colorOverride);
      lmGroup.setTextColor(lm.m_textColor);
      lmGroup.setRenderLandmarkIndices(lm.m_renderLandmarkIndices);
      lmGroup.setRenderLandmarkNames(lm.m_renderLandmarkNames);
      lmGroup.setRadiusFactor(lm.m_glyphRadiusFactor);

      if (serialize::ProjectLandmarkCoordinateSpace::Voxel == lm.m_coordinateSpace) {
        lmGroup.setInVoxelSpace(true);
        spdlog::info("Landmarks are defined in Voxel space");
      }
      else {
        lmGroup.setInVoxelSpace(false);
        spdlog::info("Landmarks are defined in physical Subject space");
      }

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
      for (const auto& p : landmarks) {
        SPDLOG_TRACE("Landmark {} ('{}') : {}", p.first, p.second.getName(), glm::to_string(p.second.getPosition()));
      }
#endif

      const auto lmGroupUid = m_data.addLandmarkGroup(lmGroup);
      const bool linked = m_data.assignLandmarkGroupUidToImage(*imageUid, lmGroupUid);

      if (!linked) {
        spdlog::error("Unable to assigned landmark group {} to image {}", lmGroupUid, *imageUid);
      }
      else {
        spdlog::info("Added landmark group {} to image {}", lmGroupUid, *imageUid);
      }
    }
    else {
      if (lm.m_csvFileName) {
        spdlog::error("Unable to open landmarks from CSV file {} for image {}", *lm.m_csvFileName, *imageUid);
      }
      else {
        spdlog::warn("Skipping empty landmark group for image {}", *imageUid);
      }
    }
  }

  // Create distance maps for all components:
  // To conserve GPU memory, the map is downsampled by 0.25 relative to the original image size
  constexpr float distanceMapDownsample = 0.25f;
  createDistanceMaps(*image, *imageUid, distanceMapDownsample, m_data);

  for (const auto& serializedSurface : serializedImage.m_isosurfaces) {
    if (serializedSurface.m_component >= image->header().numComponentsPerPixel()) {
      spdlog::warn(
        "Skipping isosurface '{}' for image {}; component {} is outside the image component range",
        serializedSurface.m_surface.name,
        *imageUid,
        serializedSurface.m_component);
      continue;
    }

    if (
      const auto surfaceUid =
        m_data.addIsosurface(*imageUid, serializedSurface.m_component, serializedSurface.m_surface))
    {
      spdlog::debug(
        "Loaded isosurface {} ('{}') for image {} component {}",
        *surfaceUid,
        serializedSurface.m_surface.name,
        *imageUid,
        serializedSurface.m_component);
    }
    else {
      spdlog::error(
        "Unable to add isosurface '{}' to image {} component {}",
        serializedSurface.m_surface.name,
        *imageUid,
        serializedSurface.m_component);
    }
  }

  // Load segmentation images:

  // Structure for holding information about a segmentation being loaded
  struct SegInfo
  {
    std::optional<uuids::uuid> uid;      // Seg UID assigned by AppData after the image is loaded from disk
    bool isNewSeg = false;               // Is this a new segmentation (true) or a repeat of a previous one (false)?
    bool needsNewLabelColorTable = true; // Does the segmentation need a new label color table?
  };

  // Holds info about all segmentations being loaded:
  std::vector<SegInfo> allSegInfos;

  for (const auto& serializedSeg : serializedImage.m_segmentations) {
    SegInfo segInfo;

    try {
      spdlog::debug("Attempting to load segmentation image from {}", serializedSeg.m_segFileName);
      std::tie(segInfo.uid, segInfo.isNewSeg) = loadSegmentation(serializedSeg.m_segFileName, imageUid);
    }
    catch (const std::exception& e) {
      spdlog::error("Exception loading segmentation from {}: {}", serializedSeg.m_segFileName, e.what());
      continue; // Skip this segmentation
    }

    if (segInfo.uid) {
      if (segInfo.isNewSeg) {
        // New segmentation needs a new table
        segInfo.needsNewLabelColorTable = true;
      }
      else {
        spdlog::info(
          "Segmentation from {} already exists as {}, so it was not loaded again. "
          "This segmentation will be shared across all images that reference it.",
          serializedSeg.m_segFileName,
          *segInfo.uid);

        // Existing segmentation does not need a new table
        segInfo.needsNewLabelColorTable = false;
      }

      allSegInfos.push_back(segInfo);
    }
  }

  if (allSegInfos.empty()) {
    // No segmentation was loaded!
    spdlog::debug("No segmentation loaded for image {}; creating blank segmentation.", *imageUid);

    try {
      const std::string segDisplayName =
        std::string("Untitled segmentation for image '") + image->settings().displayName() + "'";

      SegInfo segInfo;
      segInfo.uid = m_callbackHandler.createBlankSeg(*imageUid, segDisplayName);
      segInfo.isNewSeg = true;
      segInfo.needsNewLabelColorTable = true;

      if (segInfo.uid) {
        spdlog::debug("Created blank segmentation {} ('{}') for image {}", *segInfo.uid, segDisplayName, *imageUid);
      }
      else {
        // This is a problem that we can't recover from:
        spdlog::error(
          "Error creating blank segmentation for image {}. "
          "No segmentation will be assigned to the image.",
          *imageUid);
        return false;
      }

      allSegInfos.push_back(segInfo);
    }
    catch (const std::exception& e) {
      spdlog::error("Exception creating blank segmentation for image {}: {}", *imageUid, e.what());
      spdlog::error("No segmentation will be assigned to the image.");
      return false;
    }
  }

  // TODO: Put all this into the loadSeg function?
  for (const SegInfo& segInfo : allSegInfos) {
    Image* seg = m_data.seg(*segInfo.uid);

    if (!seg) {
      spdlog::error("Null segmentation {}", *segInfo.uid);
      m_data.removeSeg(*segInfo.uid);
      continue;
    }

    if (segInfo.needsNewLabelColorTable) {
      if (!data::createLabelColorTableForSegmentation(m_data, *segInfo.uid)) {
        constexpr size_t defaultTableIndex = 0;

        spdlog::error(
          "Unable to create label color table for segmentation {}. "
          "Defaulting to table index {}.",
          *segInfo.uid,
          defaultTableIndex);

        seg->settings().setLabelTableIndex(defaultTableIndex);
      }
    }

    if (m_data.assignSegUidToImage(*imageUid, *segInfo.uid)) {
      spdlog::info("Assigned segmentation {} to image {}", *segInfo.uid, *imageUid);
    }
    else {
      spdlog::error("Unable to assign segmentation {} to image {}", *segInfo.uid, *imageUid);
      m_data.removeSeg(*segInfo.uid);
      continue;
    }

    if (segInfo.isNewSeg) {
      const auto serializedSegIt = std::find_if(
        serializedImage.m_segmentations.begin(),
        serializedImage.m_segmentations.end(),
        [seg](const serialize::Segmentation& serializedSeg) {
          return serializedSeg.m_segFileName == seg->header().fileName();
        });

      if (serializedImage.m_segmentations.end() != serializedSegIt && serializedSegIt->m_settings) {
        project_snapshot::applySegmentationSettings(m_data, *seg, *serializedSegIt->m_settings);
      }
    }

    // Assign the image's affine_T_subject transformation to its segmentation:
    seg->transformations().set_affine_T_subject(image->transformations().get_affine_T_subject());
  }

  // Checks that the image has at least one segmentation:
  if (m_data.imageToSegUids(*imageUid).empty()) {
    spdlog::error("Image {} has no segmentation", *imageUid);
    return false;
  }
  else if (!m_data.imageToActiveSegUid(*imageUid)) {
    // The image has no active segmentation, so assign the first seg as the active one:
    const auto firstSegUid = m_data.imageToSegUids(*imageUid).front();
    m_data.assignActiveSegUidToImage(*imageUid, firstSegUid);
  }

  if (!serializedImage.m_settings || serializedImage.m_settings->m_colorMapIndices.empty()) {
    for (uint32_t i = 0; i < image->header().numComponentsPerPixel(); ++i) {
      image->settings().setColorMapIndex(i, defaultImageColorMapIndex);
    }
  }

  return true;
}

void EntropyApp::loadImageFile(const fs::path& fileName)
{
  loadImageFiles({fileName});
}

void EntropyApp::loadImageFiles(const std::vector<fs::path>& fileNames)
{
  const std::vector<fs::path> imageFiles = nonEmptyPaths(fileNames);
  if (imageFiles.empty()) {
    return;
  }

  spdlog::info("Opening {} image file(s)", imageFiles.size());
  for (const auto& fileName : imageFiles) {
    spdlog::info("Opening image file {}", fileName);
  }

  if (containsDicomInputPath(imageFiles)) {
    openDicomSeriesFolders(imageFiles);
    return;
  }

  m_pendingProjectReplacementPaths = imageFiles;
  if (requestProjectReplacement(GuiData::UnsavedProjectAction::OpenImages)) {
    return;
  }
  clearPendingProjectReplacement();
  performLoadImageFiles(imageFiles);
}

void EntropyApp::performLoadImageFiles(const std::vector<fs::path>& fileNames)
{
  const std::vector<fs::path> imageFiles = nonEmptyPaths(fileNames);
  if (imageFiles.empty()) {
    return;
  }

  if (containsDicomInputPath(imageFiles)) {
    performOpenDicomSeriesFolders(imageFiles);
    return;
  }

  recordRecentImageGroup(imageFiles);

  if (ProjectLoadState::Loaded == m_data.state().projectLoadState() && m_data.refImageUid()) {
    closeProject();
  }

  spdlog::info("Loading {} image file(s) as a new project", imageFiles.size());

  serialize::EntropyProject project = createProjectFromImageFiles(imageFiles);

  m_pendingRasterImageHeaderContext = RasterImageHeaderContext::Project;
  m_pendingRasterProject = std::move(project);
  m_pendingRasterAddImages.clear();
  m_pendingRasterImageIndex = 0;
  continueRasterImageHeaderPreflight();
  return;
}

void EntropyApp::continueRasterImageHeaderPreflight()
{
  auto promptForImage = [this](serialize::Image& image, const std::vector<fs::path>& allImages) {
    if (!isStandardRasterImageFile(image.m_imageFileName) || image.m_spatialMetadata) {
      return false;
    }

    const auto header = readImageHeaderOnly(
      image.m_imageFileName,
      Image::ImageRepresentation::Image,
      Image::MultiComponentBufferType::SeparateImages);
    if (!header) {
      spdlog::error("Could not read image header from {}", image.m_imageFileName);
      return false;
    }

    std::error_code ec;
    const std::uintmax_t fileSize = fs::file_size(image.m_imageFileName, ec);
    GuiData::RasterImageHeaderPrompt prompt;
    prompt.fileName = image.m_imageFileName;
    prompt.format = imageFormatName(image.m_imageFileName);
    prompt.fileSizeBytes = ec ? 0u : fileSize;
    prompt.dimensions = header->pixelDimensions();
    prompt.allImages = allImages;
    m_data.guiData().m_pendingRasterImageHeaderPrompt = std::move(prompt);
    m_data.guiData().m_showRasterImageHeaderPrompt = true;
    m_glfw.postEmptyEvent();
    return true;
  };

  if (m_pendingRasterImageHeaderContext == RasterImageHeaderContext::Project && m_pendingRasterProject) {
    std::vector<fs::path> allImages;
    allImages.reserve(numSerializedImages(*m_pendingRasterProject));
    allImages.push_back(m_pendingRasterProject->m_referenceImage.m_imageFileName);
    for (const auto& image : m_pendingRasterProject->m_additionalImages) {
      allImages.push_back(image.m_imageFileName);
    }

    while (m_pendingRasterImageIndex < numSerializedImages(*m_pendingRasterProject)) {
      serialize::Image* image = serializedImageAt(*m_pendingRasterProject, m_pendingRasterImageIndex);
      if (!image) {
        m_pendingRasterImageIndex++;
        continue;
      }
      if (promptForImage(*image, allImages)) {
        return;
      }
      m_pendingRasterImageIndex++;
    }

    serialize::EntropyProject project = std::move(*m_pendingRasterProject);
    m_pendingRasterImageHeaderContext = RasterImageHeaderContext::None;
    m_pendingRasterProject = std::nullopt;
    m_pendingRasterImageIndex = 0;

    m_pendingLargeImageLoadContext = LargeImageLoadContext::Project;
    m_pendingLargeProject = std::move(project);
    m_pendingLargeProjectFileName = std::nullopt;
    m_pendingLargeProjectImageIndex = 0;
    continueLargeImageProjectPreflight();
    return;
  }

  if (m_pendingRasterImageHeaderContext == RasterImageHeaderContext::AddImage) {
    std::vector<fs::path> allImages;
    allImages.reserve(m_pendingRasterAddImages.size());
    for (const auto& image : m_pendingRasterAddImages) {
      allImages.push_back(image.m_imageFileName);
    }

    while (m_pendingRasterImageIndex < m_pendingRasterAddImages.size()) {
      if (promptForImage(m_pendingRasterAddImages[m_pendingRasterImageIndex], allImages)) {
        return;
      }
      m_pendingRasterImageIndex++;
    }

    std::vector<serialize::Image> imagesToAdd = std::move(m_pendingRasterAddImages);
    std::vector<fs::path> imageFiles;
    imageFiles.reserve(imagesToAdd.size());
    for (const auto& image : imagesToAdd) {
      imageFiles.push_back(image.m_imageFileName);
    }
    m_pendingRasterImageHeaderContext = RasterImageHeaderContext::None;
    m_pendingRasterAddImages.clear();
    m_pendingRasterImageIndex = 0;

    m_preserveLayoutsOnImagesReady = true;
    m_pendingAddedImageUids.clear();

    if (imagesToAdd.empty()) {
      m_preserveLayoutsOnImagesReady = false;
      m_glfw.postEmptyEvent();
      return;
    }

    startAsyncImageLoad(
      imagesToAdd.size() == 1 ? "Adding image..." : "Adding images...",
      [this, imagesToAdd = std::move(imagesToAdd)]() {
        const std::size_t previousNumImages = m_data.numImages();
        std::vector<uuids::uuid> addedImageUids;
        addedImageUids.reserve(imagesToAdd.size());

        for (const auto& serializedImage : imagesToAdd) {
          if (m_imageLoadCancelled) {
            return false;
          }

          const std::size_t numImagesBeforeLoad = m_data.numImages();
          const bool loaded = loadSerializedImage(serializedImage, false);

          if (!loaded || m_data.numImages() <= numImagesBeforeLoad) {
            spdlog::error("Could not add image from {}", serializedImage.m_imageFileName);
            continue;
          }

          if (const auto addedImageUid = m_data.imageUid(m_data.numImages() - 1)) {
            addedImageUids.push_back(*addedImageUid);
          }
        }

        if (addedImageUids.empty() || m_data.numImages() <= previousNumImages) {
          return false;
        }

        m_data.setActiveImageUid(addedImageUids.back());
        m_data.setRainbowColorsForAllImages();
        m_data.setRainbowColorsForAllLandmarkGroups();
        m_data.setProject(createProjectSnapshot());
        m_pendingAddedImageUids = std::move(addedImageUids);
        return true;
      },
      [this]() {
        m_preserveLayoutsOnImagesReady = false;
        m_pendingAddedImageUids.clear();
        m_data.state().setProjectLoadState(ProjectLoadState::Loaded);
        m_data.state().setAnimating(false);
        hideLoadingStatus();
        updateWindowTitleStatus();
        m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
      },
      true,
      loading_status::imageItems(imageFiles));
  }
}

void EntropyApp::handleRasterImageHeaderDecision(
  GuiData::RasterImageHeaderDecision decision,
  ImageSpatialMetadata metadata,
  bool applyToAll)
{
  auto applyToSerializedImage = [&](serialize::Image& image) {
    if (isStandardRasterImageFile(image.m_imageFileName)) {
      image.m_spatialMetadata = metadata;
    }
  };

  if (m_pendingRasterImageHeaderContext == RasterImageHeaderContext::Project && m_pendingRasterProject) {
    if (decision == GuiData::RasterImageHeaderDecision::Cancel) {
      if (m_pendingRasterImageIndex == 0u) {
        spdlog::info(
          "Cancelled loading reference raster image {}; cancelling project load",
          m_pendingRasterProject->m_referenceImage.m_imageFileName);
        m_pendingRasterImageHeaderContext = RasterImageHeaderContext::None;
        m_pendingRasterProject = std::nullopt;
        m_pendingRasterImageIndex = 0;
        m_glfw.postEmptyEvent();
        return;
      }
      eraseSerializedImageAt(*m_pendingRasterProject, m_pendingRasterImageIndex);
      continueRasterImageHeaderPreflight();
      return;
    }

    if (applyToAll) {
      applyToSerializedImage(m_pendingRasterProject->m_referenceImage);
      for (auto& image : m_pendingRasterProject->m_additionalImages) {
        applyToSerializedImage(image);
      }
      m_pendingRasterImageIndex = numSerializedImages(*m_pendingRasterProject);
    }
    else if (serialize::Image* image = serializedImageAt(*m_pendingRasterProject, m_pendingRasterImageIndex)) {
      applyToSerializedImage(*image);
      m_pendingRasterImageIndex++;
    }
    continueRasterImageHeaderPreflight();
    return;
  }

  if (m_pendingRasterImageHeaderContext == RasterImageHeaderContext::AddImage) {
    if (decision == GuiData::RasterImageHeaderDecision::Cancel) {
      if (m_pendingRasterImageIndex < m_pendingRasterAddImages.size()) {
        m_pendingRasterAddImages.erase(
          m_pendingRasterAddImages.begin() + static_cast<std::ptrdiff_t>(m_pendingRasterImageIndex));
      }
      continueRasterImageHeaderPreflight();
      return;
    }

    if (applyToAll) {
      for (auto& image : m_pendingRasterAddImages) {
        applyToSerializedImage(image);
      }
      m_pendingRasterImageIndex = m_pendingRasterAddImages.size();
    }
    else if (m_pendingRasterImageIndex < m_pendingRasterAddImages.size()) {
      applyToSerializedImage(m_pendingRasterAddImages[m_pendingRasterImageIndex]);
      m_pendingRasterImageIndex++;
    }
    continueRasterImageHeaderPreflight();
  }
}

void EntropyApp::addImageFile(const fs::path& fileName)
{
  addImageFiles({fileName});
}

void EntropyApp::addImageFiles(const std::vector<fs::path>& fileNames)
{
  const std::vector<fs::path> imageFiles = nonEmptyPaths(fileNames);
  if (imageFiles.empty()) {
    return;
  }

  spdlog::info("Adding {} image file(s)", imageFiles.size());
  for (const auto& fileName : imageFiles) {
    spdlog::info("Adding image file {}", fileName);
  }

  if (containsDicomInputPath(imageFiles)) {
    const bool addToExistingProject =
      ProjectLoadState::Loaded == m_data.state().projectLoadState() && m_data.refImageUid();
    beginDicomSeriesScan(imageFiles, addToExistingProject);
    return;
  }

  if (ProjectLoadState::Loaded != m_data.state().projectLoadState() || !m_data.refImageUid()) {
    performLoadImageFiles(imageFiles);
    return;
  }

  recordRecentImageGroup(imageFiles);

  if (std::any_of(imageFiles.begin(), imageFiles.end(), isStandardRasterImageFile)) {
    m_pendingRasterImageHeaderContext = RasterImageHeaderContext::AddImage;
    m_pendingRasterProject = std::nullopt;
    m_pendingRasterAddImages.clear();
    m_pendingRasterAddImages.reserve(imageFiles.size());
    for (const auto& fileName : imageFiles) {
      serialize::Image image;
      image.m_imageFileName = fileName;
      m_pendingRasterAddImages.emplace_back(std::move(image));
    }
    m_pendingRasterImageIndex = 0;
    continueRasterImageHeaderPreflight();
    return;
  }

  const bool bypassPreflight = m_data.guiData().m_bypassNextImageLoadPreflight;
  m_data.guiData().m_bypassNextImageLoadPreflight = false;

  if (imageFiles.size() == 1 && !bypassPreflight) {
    const auto header = readImageHeaderOnly(
      imageFiles.front(),
      Image::ImageRepresentation::Image,
      Image::MultiComponentBufferType::SeparateImages);

    if (!header) {
      spdlog::error("Could not read image header from {}", imageFiles.front());
      return;
    }

    if (shouldPromptForLargeImage(*header)) {
      spdlog::warn(
        "Image {} is large: estimated in-memory size is {:.2f} GiB",
        imageFiles.front(),
        static_cast<double>(header->memoryImageSizeInBytes()) / (1024.0 * 1024.0 * 1024.0));

      m_pendingLargeImageLoadContext = LargeImageLoadContext::AddImage;
      m_pendingLargeAddImageFile = imageFiles.front();
      m_data.guiData().m_pendingLargeImageLoadPrompt =
        GuiData::LargeImageLoadPrompt{imageFiles.front(), *header, false, true};
      m_data.guiData().m_showLargeImageLoadPrompt = true;
      m_glfw.postEmptyEvent();
      return;
    }
  }

  m_preserveLayoutsOnImagesReady = true;
  m_pendingAddedImageUids.clear();

  startAsyncImageLoad(
    imageFiles.size() == 1 ? "Adding image..." : "Adding images...",
    [this, imageFiles]() {
      const std::size_t previousNumImages = m_data.numImages();
      std::vector<uuids::uuid> addedImageUids;
      addedImageUids.reserve(imageFiles.size());

      for (const auto& fileName : imageFiles) {
        if (m_imageLoadCancelled) {
          return false;
        }

        serialize::Image serializedImage;
        serializedImage.m_imageFileName = fileName;

        const std::size_t numImagesBeforeLoad = m_data.numImages();
        const bool loaded = loadSerializedImage(serializedImage, false);

        if (!loaded || m_data.numImages() <= numImagesBeforeLoad) {
          spdlog::error("Could not add image from {}", fileName);
          continue;
        }

        if (const auto addedImageUid = m_data.imageUid(m_data.numImages() - 1)) {
          addedImageUids.push_back(*addedImageUid);
        }
      }

      if (addedImageUids.empty() || m_data.numImages() <= previousNumImages) {
        return false;
      }

      m_data.setActiveImageUid(addedImageUids.back());
      m_data.setRainbowColorsForAllImages();
      m_data.setRainbowColorsForAllLandmarkGroups();
      m_data.setProject(createProjectSnapshot());
      m_pendingAddedImageUids = std::move(addedImageUids);
      return true;
    },
    [this]() {
      m_preserveLayoutsOnImagesReady = false;
      m_pendingAddedImageUids.clear();
      m_data.state().setProjectLoadState(ProjectLoadState::Loaded);
      m_data.state().setAnimating(false);
      hideLoadingStatus();
      updateWindowTitleStatus();
      m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
    },
    false,
    loading_status::imageItems(imageFiles));
}

void EntropyApp::handleDroppedFiles(const std::vector<fs::path>& fileNames)
{
  const std::vector<fs::path> droppedFiles = nonEmptyPaths(fileNames);
  if (droppedFiles.empty()) {
    return;
  }

  if (ProjectLoadState::Loading == m_data.state().projectLoadState()) {
    spdlog::warn("Ignoring dropped files because a project is already loading");
    return;
  }

  if (droppedFiles.size() == 1 && isJsonProjectFile(droppedFiles.front())) {
    loadProjectFile(droppedFiles.front());
    return;
  }

  std::vector<fs::path> imageFiles;
  imageFiles.reserve(droppedFiles.size());
  for (const auto& fileName : droppedFiles) {
    if (isJsonProjectFile(fileName)) {
      spdlog::warn("Ignoring project file {} in mixed drag-and-drop operation", fileName);
      continue;
    }
    imageFiles.push_back(fileName);
  }

  addImageFiles(imageFiles);
}

void EntropyApp::openDicomSeriesFolders(const std::vector<fs::path>& folderNames)
{
  const std::vector<fs::path> dicomFolders = nonEmptyPaths(folderNames);
  if (dicomFolders.empty()) {
    return;
  }

  m_pendingProjectReplacementPaths = dicomFolders;
  if (requestProjectReplacement(GuiData::UnsavedProjectAction::OpenDicomSeries)) {
    return;
  }
  clearPendingProjectReplacement();
  performOpenDicomSeriesFolders(dicomFolders);
}

void EntropyApp::performOpenDicomSeriesFolders(const std::vector<fs::path>& folderNames)
{
  const std::vector<fs::path> dicomFolders = nonEmptyPaths(folderNames);
  if (dicomFolders.empty()) {
    return;
  }

  if (ProjectLoadState::Loaded == m_data.state().projectLoadState() && m_data.refImageUid()) {
    closeProject();
  }

  spdlog::info("Opening {} DICOM input path(s) as a new project", dicomFolders.size());

  beginDicomSeriesScan(dicomFolders, false);
}

void EntropyApp::beginDicomSeriesScan(const std::vector<fs::path>& inputPaths, bool addToExistingProject)
{
  const std::vector<fs::path> scanInputs = nonEmptyPaths(inputPaths);
  if (scanInputs.empty()) {
    return;
  }

  if (ProjectLoadState::Loading == m_data.state().projectLoadState() || m_data.state().animating()) {
    spdlog::warn("Ignoring DICOM request because another load task is running");
    return;
  }

  recordRecentDicomGroup(scanInputs);

  spdlog::info("Scanning {} DICOM input path(s)", scanInputs.size());
  for (const auto& inputPath : scanInputs) {
    spdlog::info("Scanning DICOM input {}", inputPath);
  }

  if (m_futureDiscoverDicom.valid()) {
    m_futureDiscoverDicom.wait();
    m_futureDiscoverDicom = {};
  }

  m_pendingDicomScanAddToExistingProject = addToExistingProject;
  auto& guiData = m_data.guiData();
  guiData.m_dicomSeriesScanInProgress = true;
  guiData.m_pendingDicomScanRoot = scanInputs.front();
  guiData.m_pendingDicomSeriesSelectionPrompt = std::nullopt;
  guiData.m_showDicomSeriesSelectionPopup = false;
  m_glfw.setEventProcessingMode(EventProcessingMode::Poll);
  m_glfw.postEmptyEvent();

  m_futureDiscoverDicom = std::async(std::launch::async, [scanInputs]() {
    return dicom::discoverSeries(
      scanInputs,
      dicom::DiscoverOptions{.recursive = true, .includePrivateMetadata = false});
  });
}

void EntropyApp::pollDicomSeriesScan()
{
  if (!m_futureDiscoverDicom.valid()) {
    return;
  }

  using namespace std::chrono_literals;
  if (m_futureDiscoverDicom.wait_for(0ms) != std::future_status::ready) {
    m_glfw.postEmptyEvent();
    return;
  }

  dicom::DiscoverResult result;
  try {
    result = m_futureDiscoverDicom.get();
  }
  catch (const std::exception& e) {
    spdlog::error("Exception while scanning DICOM series: {}", e.what());
    result.warnings.push_back(std::string{"Exception while scanning DICOM series: "} + e.what());
  }
  catch (...) {
    spdlog::error("Unknown exception while scanning DICOM series");
    result.warnings.emplace_back("Unknown exception while scanning DICOM series");
  }
  m_futureDiscoverDicom = {};

  auto& guiData = m_data.guiData();
  guiData.m_dicomSeriesScanInProgress = false;
  guiData.m_pendingDicomScanRoot = fs::path{};

  if (result.series.empty()) {
    for (const auto& warning : result.warnings) {
      spdlog::warn("DICOM scan warning: {}", warning);
    }
    spdlog::error("No DICOM image series found");
    m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
    m_glfw.postEmptyEvent();
    return;
  }

  GuiData::DicomSeriesSelectionPrompt prompt;
  prompt.series = std::move(result.series);
  prompt.warnings = std::move(result.warnings);
  prompt.selected.resize(prompt.series.size(), false);
  prompt.previewCache.resize(prompt.series.size());
  prompt.previewErrors.resize(prompt.series.size());
  prompt.addToExistingProject = m_pendingDicomScanAddToExistingProject;
  prompt.allowReferenceSelection = !m_data.refImageUid();

  for (std::size_t i = 0; i < prompt.series.size(); ++i) {
    prompt.selected.at(i) = prompt.series.at(i).loadable();
    if (prompt.selected.at(i)) {
      prompt.referenceSeriesIndex = static_cast<int>(i);
      prompt.previewSeriesIndex = i;
      break;
    }
  }

  guiData.m_pendingDicomSeriesSelectionPrompt = std::move(prompt);
  guiData.m_showDicomSeriesSelectionPopup = true;
  m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
  m_glfw.postEmptyEvent();
}

void EntropyApp::loadDicomSeries(
  const std::vector<dicom::SeriesInfo>& series,
  std::optional<std::size_t> referenceSeriesIndex,
  bool addToExistingProject)
{
  if (series.empty()) {
    return;
  }

  spdlog::info("{} {} selected DICOM series", addToExistingProject ? "Adding" : "Opening", series.size());
  for (const auto& seriesInfo : series) {
    spdlog::info("Selected DICOM series {} with {} file(s)", seriesInfo.seriesInstanceUid, seriesInfo.files.size());
  }

  std::vector<dicom::SeriesInfo> seriesToLoad = series;
  if (
    !addToExistingProject && referenceSeriesIndex && *referenceSeriesIndex < seriesToLoad.size() &&
    *referenceSeriesIndex != 0)
  {
    std::rotate(
      seriesToLoad.begin(),
      seriesToLoad.begin() + static_cast<std::ptrdiff_t>(*referenceSeriesIndex),
      seriesToLoad.begin() + static_cast<std::ptrdiff_t>(*referenceSeriesIndex + 1));
  }

  if (!addToExistingProject) {
    closeProject();
    m_data.setProjectFileName(std::nullopt);
  }

  m_preserveLayoutsOnImagesReady = addToExistingProject;
  m_pendingAddedImageUids.clear();

  startAsyncImageLoad(
    "Loading DICOM series...",
    [this, seriesToLoad, addToExistingProject]() {
      const std::size_t previousNumImages = m_data.numImages();
      std::vector<uuids::uuid> addedImageUids;
      addedImageUids.reserve(seriesToLoad.size());

      for (const auto& seriesInfo : seriesToLoad) {
        if (m_imageLoadCancelled) {
          return false;
        }

        if (seriesInfo.files.empty()) {
          spdlog::error("Could not load DICOM series {} because it has no files", seriesInfo.seriesInstanceUid);
          continue;
        }

        serialize::Image serializedImage;
        serializedImage.m_imageFileName = seriesInfo.files.front();
        serializedImage.m_dicomSource = makeDicomSourceSnapshot(seriesInfo);

        const std::size_t numImagesBeforeLoad = m_data.numImages();
        const bool isReferenceImage = !addToExistingProject && addedImageUids.empty();
        const bool loaded = loadSerializedImage(serializedImage, isReferenceImage, &seriesInfo);
        if (!loaded || m_data.numImages() <= numImagesBeforeLoad) {
          spdlog::error("Could not load DICOM series {}", seriesInfo.seriesInstanceUid);
          continue;
        }

        if (const auto imageUid = m_data.imageUid(m_data.numImages() - 1)) {
          addedImageUids.push_back(*imageUid);
          spdlog::info("Loaded DICOM series {} as {}", seriesInfo.seriesInstanceUid, *imageUid);
        }
      }

      if (addedImageUids.empty() || m_data.numImages() <= previousNumImages) {
        return false;
      }

      if (addToExistingProject) {
        m_data.setActiveImageUid(addedImageUids.back());
        m_pendingAddedImageUids = addedImageUids;
      }
      else if (
        const auto activeImageIndex =
          app::image_selection_policy::defaultActiveImageIndexForInitialLoad(m_data.numImages()))
      {
        if (const auto activeImageUid = m_data.imageUid(*activeImageIndex)) {
          m_data.setActiveImageUid(*activeImageUid);
        }
      }

      m_data.setRainbowColorsForAllImages();
      m_data.setRainbowColorsForAllLandmarkGroups();
      m_data.setProject(createProjectSnapshot());
      return true;
    },
    [this, addToExistingProject]() {
      if (!addToExistingProject) {
        m_data.clearProjectData();
        m_data.state().setProjectLoadState(ProjectLoadState::Failed);
      }
      m_preserveLayoutsOnImagesReady = false;
      m_pendingAddedImageUids.clear();
      m_data.state().setAnimating(false);
      hideLoadingStatus();
      updateWindowTitleStatus();
      m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
    },
    !addToExistingProject,
    loading_status::dicomSeriesItems(seriesToLoad));
}

void EntropyApp::addSegmentationFile(const fs::path& fileName)
{
  const auto activeImageUid = m_data.activeImageUid();
  if (!activeImageUid) {
    spdlog::error("Cannot add segmentation {} because there is no active image", fileName);
    return;
  }

  addSegmentationFileToImage(fileName, *activeImageUid);
}

void EntropyApp::addSegmentationFileToImage(const fs::path& fileName, const uuids::uuid& imageUid)
{
  if (ProjectLoadState::Loaded != m_data.state().projectLoadState()) {
    spdlog::error("Cannot add segmentation {} to image {} because no project is loaded", fileName, imageUid);
    return;
  }

  if (!m_data.image(imageUid)) {
    spdlog::error("Cannot add segmentation {} to invalid image {}", fileName, imageUid);
    return;
  }

  std::optional<uuids::uuid> segUid;
  bool isNewSeg = false;

  try {
    spdlog::debug("Attempting to add segmentation image from {} to image {}", fileName, imageUid);
    std::tie(segUid, isNewSeg) = loadSegmentation(fileName, imageUid);
  }
  catch (const std::exception& e) {
    spdlog::error("Exception adding segmentation from {} to image {}: {}", fileName, imageUid, e.what());
    return;
  }

  if (!segUid) {
    spdlog::error("Could not add segmentation from {}", fileName);
    return;
  }

  if (!m_callbackHandler.assignSegToImageWithColorTableAndTextures(imageUid, *segUid, isNewSeg, isNewSeg)) {
    spdlog::error("Could not assign segmentation {} from {} to image {}", *segUid, fileName, imageUid);
    return;
  }

  m_data.setProject(createProjectSnapshot());
  spdlog::info("Added segmentation {} from {} to image {}", *segUid, fileName, imageUid);
}

bool EntropyApp::setReferenceImage(const uuids::uuid& imageUid)
{
  Image* newReferenceImage = m_data.image(imageUid);
  if (!newReferenceImage) {
    spdlog::error("Cannot set invalid image {} as reference image", imageUid);
    return false;
  }

  if (m_data.refImageUid() && *m_data.refImageUid() == imageUid) {
    return true;
  }

  const glm::mat4 oldWorld_T_newReferenceSubject = newReferenceImage->transformations().worldDef_T_subject();
  const glm::mat4 newWorld_T_oldWorld = glm::inverse(oldWorld_T_newReferenceSubject);

  for (const auto& currentImageUid : m_data.imageUidsOrdered()) {
    Image* image = m_data.image(currentImageUid);
    if (!image) {
      continue;
    }

    auto& tx = image->transformations();
    const bool wasManualTransformLocked = tx.is_worldDef_T_affine_locked();
    tx.set_worldDef_T_affine_locked(false);

    if (currentImageUid == imageUid) {
      tx.set_enable_affine_T_subject(false);
      tx.set_enable_worldDef_T_affine(false);
      tx.set_affine_T_subject_fileName(std::nullopt);
      tx.reset_worldDef_T_affine();
      tx.set_worldDef_T_affine_locked(true);
    }
    else {
      const glm::mat4 newWorld_T_subject = newWorld_T_oldWorld * tx.worldDef_T_subject();
      const glm::mat4 manual_T_affine = newWorld_T_subject * glm::inverse(tx.get_affine_T_subject());
      tx.set_enable_worldDef_T_affine(true);
      tx.set_worldDef_T_affine(manual_T_affine);
      tx.set_worldDef_T_affine_locked(wasManualTransformLocked);
    }

    for (const auto& segUid : m_data.imageToSegUids(currentImageUid)) {
      Image* seg = m_data.seg(segUid);
      if (!seg) {
        continue;
      }

      auto& segTx = seg->transformations();
      segTx.set_worldDef_T_affine_locked(false);
      segTx.set_enable_affine_T_subject(tx.get_enable_affine_T_subject());
      segTx.set_affine_T_subject(tx.get_affine_T_subject());
      segTx.set_enable_worldDef_T_affine(tx.get_enable_worldDef_T_affine());
      segTx.set_worldDef_T_affine(tx.get_worldDef_T_affine());
      segTx.set_worldDef_T_affine_locked(tx.is_worldDef_T_affine_locked());
    }
  }

  if (!m_data.setRefImageUid(imageUid)) {
    spdlog::error("Unable to set {} as reference image", imageUid);
    return false;
  }

  m_data.windowData().updateImageOrdering(m_data.imageUidsOrdered());
  m_data.setActiveImageUid(imageUid);
  m_data.setRainbowColorsForAllImages();
  m_data.setRainbowColorsForAllLandmarkGroups();
  m_data.setProject(createProjectSnapshot());
  m_rendering.updateImageUniforms(m_data.imageUidsOrdered());
  m_callbackHandler.recenterViews(m_data.state().recenteringMode(), true, true, false, true, true);
  m_glfw.postEmptyEvent();

  spdlog::info("Set {} as the reference image", imageUid);
  return true;
}

bool EntropyApp::removeImage(const uuids::uuid& imageUid)
{
  if (!m_data.image(imageUid)) {
    spdlog::error("Cannot remove invalid image {}", imageUid);
    return false;
  }

  if (m_data.refImageUid() && *m_data.refImageUid() == imageUid) {
    spdlog::error("Cannot remove the reference image {}", imageUid);
    return false;
  }

  const auto segUids = m_data.imageToSegUids(imageUid);
  const auto defUids = m_data.imageToDefUids(imageUid);

  if (!m_data.removeImage(imageUid)) {
    spdlog::error("Unable to remove image {}", imageUid);
    return false;
  }

  m_dicomSourcesByImageUid.erase(imageUid);

  auto& renderData = m_data.renderData();
  renderData.m_imageTextures.erase(imageUid);
  renderData.m_imageTextureLayouts.erase(imageUid);
  renderData.m_distanceMapTextures.erase(imageUid);
  renderData.m_uniforms.erase(imageUid);

  for (const auto& segUid : segUids) {
    if (!m_data.seg(segUid)) {
      renderData.m_segTextures.erase(segUid);
      renderData.m_segTextureLayouts.erase(segUid);
    }
  }

  for (const auto& defUid : defUids) {
    if (!m_data.def(defUid)) {
      renderData.m_imageTextures.erase(defUid);
      renderData.m_imageTextureLayouts.erase(defUid);
      renderData.m_distanceMapTextures.erase(defUid);
      renderData.m_uniforms.erase(defUid);
    }
  }

  m_data.windowData().removeImageFromLayouts(imageUid, m_data.imageUidsOrdered());
  m_data.windowData().reconcileImageDependentLayouts(m_data, dicomNativeViewTypesByImage());
  m_data.setRainbowColorsForAllImages();
  m_data.setRainbowColorsForAllLandmarkGroups();
  m_data.setProject(createProjectSnapshot());
  m_rendering.updateImageUniforms(m_data.imageUidsOrdered());
  updateWindowTitleStatus();
  m_glfw.postEmptyEvent();

  spdlog::info("Removed image {}", imageUid);
  return true;
}

void EntropyApp::loadImagesFromParams(const InputParams& params)
{
  if (!params.dicomPaths.empty()) {
    m_pendingLayoutsFile = params.layoutsFile;
    performOpenDicomSeriesFolders(params.dicomPaths);
    return;
  }

  const std::optional<fs::path> projectFileName = params.imageFiles.empty() ? params.projectFile : std::nullopt;
  m_pendingLayoutsFile = params.layoutsFile;
  m_data.setProject(serialize::createProjectFromInputParams(params));
  m_data.setProjectFileName(projectFileName);

  startAsyncImageLoad(
    "Loading project...",
    [this]() { return loadProject(m_data.project()); },
    [this]() {
      m_data.clearProjectData();
      m_data.state().setProjectLoadState(ProjectLoadState::Failed);
      m_data.state().setAnimating(false);
      m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
    },
    true,
    loading_status::projectItems(m_data.project()));
}

void EntropyApp::beginLoadingStatus(std::string title, std::vector<GuiData::LoadingStatusItem> items)
{
  if (!m_data.guiData().m_loadingStatus) {
    m_data.guiData().m_loadingStatus = std::make_shared<GuiData::LoadingStatus>();
  }

  std::scoped_lock lock(m_data.guiData().m_loadingStatus->mutex);
  m_data.guiData().m_loadingStatus->title = std::move(title);
  m_data.guiData().m_loadingStatus->items = std::move(items);
  m_data.guiData().m_loadingStatus->visible = !m_data.guiData().m_loadingStatus->items.empty();
}

void EntropyApp::markLoadingStatusItemLoaded(GuiData::LoadingStatusItem::Kind kind, const fs::path& fileName)
{
  if (!m_data.guiData().m_loadingStatus) {
    return;
  }

  std::scoped_lock lock(m_data.guiData().m_loadingStatus->mutex);
  for (auto& item : m_data.guiData().m_loadingStatus->items) {
    if (!item.loaded && item.kind == kind && loading_status::equivalentPath(item.fileName, fileName)) {
      item.loaded = true;
      break;
    }
  }
}

void EntropyApp::hideLoadingStatus()
{
  if (!m_data.guiData().m_loadingStatus) {
    return;
  }

  std::scoped_lock lock(m_data.guiData().m_loadingStatus->mutex);
  m_data.guiData().m_loadingStatus->visible = false;
  m_data.guiData().m_loadingStatus->items.clear();
}

void EntropyApp::startAsyncImageLoad(
  const std::string& windowTitleStatus,
  std::function<bool()> loadTask,
  std::function<void()> onLoadFailed,
  bool showLoadingOverlay,
  std::vector<GuiData::LoadingStatusItem> loadingItems,
  std::string loadingStatusTitle)
{
  if (m_futureLoadProject.valid()) {
    m_futureLoadProject.wait();
    m_futureLoadProject = {};
  }
  if (m_futureDiscoverDicom.valid()) {
    m_futureDiscoverDicom.wait();
    m_futureDiscoverDicom = {};
  }

  m_imageLoadCancelled = false;
  m_imagesReady = false;
  m_imageLoadFailed = false;
  m_data.guiData().m_visibleImageCountDuringLoad = m_data.numImages();
  beginLoadingStatus(std::move(loadingStatusTitle), std::move(loadingItems));

  m_glfw.setWindowTitleStatus(windowTitleStatus);
  m_glfw.setEventProcessingMode(EventProcessingMode::Poll);
  if (showLoadingOverlay) {
    m_data.state().setProjectLoadState(ProjectLoadState::Loading);
  }
  m_data.state().setAnimating(true);
  m_glfw.postEmptyEvent();

  auto onProjectLoadingDone = [this, onLoadFailed = std::move(onLoadFailed)](bool projectLoadedSuccessfully) {
    if (projectLoadedSuccessfully) {
      m_imagesReady = true;
      m_imageLoadFailed = false;
      m_glfw.postEmptyEvent();
      spdlog::debug("Done loading images");
    }
    else {
      spdlog::critical("Failed to load images");
      if (onLoadFailed) {
        onLoadFailed();
      }
      hideLoadingStatus();
      m_data.guiData().m_visibleImageCountDuringLoad = std::nullopt;
      m_imagesReady = false;
      m_imageLoadFailed = false;
      m_glfw.postEmptyEvent();
    }
  };

  m_futureLoadProject = std::async(
    std::launch::async,
    [loadTask = std::move(loadTask), onProjectLoadingDone = std::move(onProjectLoadingDone)]() mutable {
      bool loaded = false;
      try {
        if (loadTask) {
          loaded = loadTask();
        }
      }
      catch (const std::exception& e) {
        spdlog::error("Exception while loading images: {}", e.what());
      }
      catch (...) {
        spdlog::error("Unknown exception while loading images");
      }

      onProjectLoadingDone(loaded);
    });
}

bool EntropyApp::loadProject(const serialize::EntropyProject& projectToLoad)
{
  static constexpr size_t defaultReferenceImageIndex = 0;

  m_preserveLayoutsOnImagesReady = false;
  m_pendingAddedImageUids.clear();
  m_pendingWarpAssignment = std::nullopt;
  m_pendingInverseWarpReferences.clear();

  spdlog::debug("Begin loading images in new thread");

  if (m_imageLoadCancelled) {
    return false;
  }

  if (!loadSerializedImage(projectToLoad.m_referenceImage, true)) {
    spdlog::critical("Could not load reference image from {}", projectToLoad.m_referenceImage.m_imageFileName);
    return false;
  }

  if (m_imageLoadCancelled) {
    return false;
  }

  for (const auto& additionalImage : projectToLoad.m_additionalImages) {
    if (!loadSerializedImage(additionalImage, false)) {
      spdlog::error("Could not load additional image from {}; skipping it", additionalImage.m_imageFileName);
    }

    if (m_imageLoadCancelled) {
      return false;
    }
  }

  for (const PendingInverseWarpReference& pendingReference : m_pendingInverseWarpReferences) {
    std::optional<uuids::uuid> resolvedReferenceUid;
    for (const uuids::uuid& candidateUid : m_data.imageUidsOrdered()) {
      const Image* candidateImage = m_data.image(candidateUid);
      if (!candidateImage) {
        continue;
      }
      std::error_code equivalentError;
      const bool equivalent =
        fs::equivalent(candidateImage->header().fileName(), pendingReference.referenceImagePath, equivalentError);
      if (equivalent || candidateImage->header().fileName() == pendingReference.referenceImagePath) {
        resolvedReferenceUid = candidateUid;
        break;
      }
    }

    if (resolvedReferenceUid) {
      if (!m_data.setActiveInverseWarpReferenceImageUid(pendingReference.imageUid, resolvedReferenceUid)) {
        spdlog::warn(
          "Unable to restore inverse warp reference image {} for image {}",
          pendingReference.referenceImagePath,
          pendingReference.imageUid);
      }
    }
    else {
      spdlog::warn(
        "Unable to find saved inverse warp reference image {} for image {}; using the moving image",
        pendingReference.referenceImagePath,
        pendingReference.imageUid);
    }
  }
  m_pendingInverseWarpReferences.clear();

  const auto refImageUid = m_data.imageUid(defaultReferenceImageIndex);

  if (refImageUid && m_data.setRefImageUid(*refImageUid)) {
    spdlog::info("Set {} as the reference image", *refImageUid);
  }
  else {
    spdlog::critical("Unable to set reference image");
    return false;
  }

  const auto desiredActiveImageIndex =
    app::image_selection_policy::defaultActiveImageIndexForInitialLoad(m_data.numImages());
  const auto desiredActiveImageUid = desiredActiveImageIndex ? m_data.imageUid(*desiredActiveImageIndex) : refImageUid;

  if (desiredActiveImageUid && m_data.setActiveImageUid(*desiredActiveImageUid)) {
    spdlog::info("Set {} as the active image", *desiredActiveImageUid);
  }
  else {
    spdlog::error("Unable to set {} as the active image", *desiredActiveImageUid);
  }

  m_data.setRainbowColorsForAllImages();
  m_data.setRainbowColorsForAllLandmarkGroups();
  return true;
}
