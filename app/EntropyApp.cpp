#include "EntropyApp.h"
#include "BuildStamp.h"

#include "logic/app/DataHelper.h"
#include "logic/app/AppPaths.h"
#include "logic/app/ImageSelectionPolicy.h"
#include "logic/app/UserPreferences.h"
#include "common/DirectionMaps.h"
#include "common/Exception.hpp"
#include "common/MathFuncs.h"
#include <spdlog/fmt/std.h>

#include "image/ImageUtility.h"
#include "image/DicomSeries.h"

#include "logic/annotation/Annotation.h"
#include "logic/annotation/LandmarkGroup.h"
#include "logic/camera/MathUtility.h"
#include "logic/serialization/ProjectSerialization.h"
#include "logic/states/FsmList.hpp"
#include "logic/DistanceMap.h"
#include "ui/NativeFileDialogs.h"
#include "layout/LayoutFileSerialization.h"

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>
#include <glm/gtx/string_cast.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>

// Without undefining min and max, there are some errors compiling in Visual Studio
#undef min
#undef max

namespace fs = std::filesystem;

namespace
{
bool promptForChar(const char* prompt, char& readch)
{
  std::string tmp;
  std::cout << prompt << std::endl;

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

GuiData::LayoutTabPlacement guiLayoutTabPlacement(UiLayoutTabPlacement placement)
{
  return UiLayoutTabPlacement::Bottom == placement ? GuiData::LayoutTabPlacement::Bottom
                                                   : GuiData::LayoutTabPlacement::Top;
}

serialize::ProjectLayoutTabPlacement projectLayoutTabPlacement(UiLayoutTabPlacement placement)
{
  return UiLayoutTabPlacement::Bottom == placement ? serialize::ProjectLayoutTabPlacement::Bottom
                                                   : serialize::ProjectLayoutTabPlacement::Top;
}

UiLayoutTabPlacement uiLayoutTabPlacement(serialize::ProjectLayoutTabPlacement placement)
{
  return serialize::ProjectLayoutTabPlacement::Bottom == placement ? UiLayoutTabPlacement::Bottom
                                                                   : UiLayoutTabPlacement::Top;
}

void syncLayoutTabGuiDataFromSettings(AppData& appData)
{
  appData.guiData().m_showLayoutTabs = appData.settings().showLayoutTabs();
  appData.guiData().m_layoutTabPlacement = guiLayoutTabPlacement(appData.settings().layoutTabPlacement());
}

std::uint32_t clampProjectPrecision(std::uint32_t precision)
{
  constexpr std::uint32_t kMaxPrecision = 9;
  return std::min(precision, kMaxPrecision);
}

std::string projectPrecisionFormat(std::uint32_t precision)
{
  return std::string{"%0."} + std::to_string(clampProjectPrecision(precision)) + "f";
}

serialize::ProjectInterfaceSettings projectInterfaceSettings(const AppData& appData)
{
  return serialize::ProjectInterfaceSettings{
    .m_showLayoutTabs = appData.settings().showLayoutTabs(),
    .m_layoutTabPlacement = projectLayoutTabPlacement(appData.settings().layoutTabPlacement()),
    .m_imageValuePrecision = appData.guiData().m_imageValuePrecision,
    .m_coordsPrecision = appData.guiData().m_coordsPrecision,
    .m_txPrecision = appData.guiData().m_txPrecision,
    .m_percentilePrecision = appData.guiData().m_percentilePrecision};
}

void applyProjectInterfaceSettings(AppData& appData, const serialize::ProjectInterfaceSettings& settings)
{
  appData.settings().setShowLayoutTabs(settings.m_showLayoutTabs);
  appData.settings().setLayoutTabPlacement(uiLayoutTabPlacement(settings.m_layoutTabPlacement));
  syncLayoutTabGuiDataFromSettings(appData);

  appData.guiData().m_imageValuePrecision = clampProjectPrecision(settings.m_imageValuePrecision);
  appData.guiData().m_imageValuePrecisionFormat = projectPrecisionFormat(settings.m_imageValuePrecision);
  appData.guiData().m_coordsPrecision = clampProjectPrecision(settings.m_coordsPrecision);
  appData.guiData().setCoordsPrecisionFormat();
  appData.guiData().m_txPrecision = clampProjectPrecision(settings.m_txPrecision);
  appData.guiData().setTxPrecisionFormat();
  appData.guiData().m_percentilePrecision = clampProjectPrecision(settings.m_percentilePrecision);
  appData.guiData().m_percentilePrecisionFormat = projectPrecisionFormat(settings.m_percentilePrecision);
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

serialize::ImageSettings makeImageSettingsSnapshot(const Image& image)
{
  const ImageSettings& imageSettings = image.settings();
  const auto thresholds = image.settings().thresholds();

  serialize::ImageSettings settings;
  settings.m_displayName = imageSettings.displayName();
  settings.m_level = imageSettings.windowCenter();
  settings.m_window = imageSettings.windowWidth();
  settings.m_thresholdLow = thresholds.first;
  settings.m_thresholdHigh = thresholds.second;
  settings.m_opacity = imageSettings.opacity();
  settings.m_edgeDetectionMethod = EdgeDetectionMethod::Pixel == imageSettings.edgeDetectionMethod()
                                     ? serialize::ProjectEdgeDetectionMethod::Pixel
                                     : serialize::ProjectEdgeDetectionMethod::Voxel;
  settings.m_showEdges = imageSettings.showAnyEdges();
  settings.m_thresholdEdges = EdgeDetectionMethod::Pixel == imageSettings.edgeDetectionMethod()
                                ? imageSettings.thresholdPixelEdges()
                                : imageSettings.thresholdEdges();
  settings.m_thinPixelEdges = imageSettings.thinPixelEdges();
  settings.m_overlayEdges = EdgeDetectionMethod::Pixel == imageSettings.edgeDetectionMethod()
                              ? imageSettings.overlayPixelEdges()
                              : imageSettings.overlayEdges();
  settings.m_colormapEdges =
    EdgeDetectionMethod::Voxel == imageSettings.edgeDetectionMethod() && imageSettings.colormapEdges();
  settings.m_edgeMagnitude = imageSettings.edgeMagnitude();
  settings.m_pixelEdgeScale = imageSettings.pixelEdgeScale();
  settings.m_pixelEdgeThreshold = imageSettings.pixelEdgeThreshold();
  settings.m_edgeColor = imageSettings.edgeColor();
  settings.m_edgeOpacity = imageSettings.edgeOpacity();
  return settings;
}

serialize::SegSettings makeSegSettingsSnapshot(const Image& seg)
{
  serialize::SegSettings settings;
  settings.m_opacity = seg.settings().opacity();
  return settings;
}

void applyImageSettingsSnapshot(Image& image, const serialize::ImageSettings& settings)
{
  ImageSettings& imageSettings = image.settings();
  if (!settings.m_displayName.empty()) {
    imageSettings.setDisplayName(settings.m_displayName);
  }

  imageSettings.setWindowCenter(settings.m_level);
  imageSettings.setWindowWidth(settings.m_window);
  imageSettings.setThresholdLow(settings.m_thresholdLow);
  imageSettings.setThresholdHigh(settings.m_thresholdHigh);
  imageSettings.setOpacity(settings.m_opacity);
  imageSettings.setEdgeDetectionMethod(
    serialize::ProjectEdgeDetectionMethod::Pixel == settings.m_edgeDetectionMethod ? EdgeDetectionMethod::Pixel
                                                                                   : EdgeDetectionMethod::Voxel);
  imageSettings.setShowAnyEdges(settings.m_showEdges);
  imageSettings.setThresholdEdges(settings.m_thresholdEdges);
  imageSettings.setThresholdPixelEdges(settings.m_thresholdEdges);
  imageSettings.setThinPixelEdges(settings.m_thinPixelEdges);
  imageSettings.setOverlayEdges(settings.m_overlayEdges);
  imageSettings.setOverlayPixelEdges(settings.m_overlayEdges);
  imageSettings.setColormapEdges(
    serialize::ProjectEdgeDetectionMethod::Voxel == settings.m_edgeDetectionMethod && settings.m_colormapEdges);
  imageSettings.setEdgeMagnitude(settings.m_edgeMagnitude);
  imageSettings.setPixelEdgeScale(settings.m_pixelEdgeScale);
  imageSettings.setPixelEdgeThreshold(settings.m_pixelEdgeThreshold);
  imageSettings.setEdgeColor(settings.m_edgeColor);
  imageSettings.setEdgeOpacity(settings.m_edgeOpacity);
}

void applySegSettingsSnapshot(Image& seg, const serialize::SegSettings& settings)
{
  seg.settings().setOpacity(settings.m_opacity);
}

fs::path projectSavePath(fs::path fileName)
{
  if (fileName.extension().empty()) {
    fileName += ".json";
  }
  return fileName;
}

constexpr uint64_t LargeImageWarningBytes = 2ull * 1024ull * 1024ull * 1024ull;

bool shouldPromptForLargeImage(const ImageHeader& header)
{
  return header.memoryImageSizeInBytes() >= LargeImageWarningBytes;
}

std::size_t numSerializedImages(const serialize::EntropyProject& project)
{
  return 1 + project.m_additionalImages.size();
}

serialize::Image* serializedImageAt(serialize::EntropyProject& project, std::size_t index)
{
  if (0 == index) {
    return &project.m_referenceImage;
  }

  const std::size_t additionalIndex = index - 1;
  if (additionalIndex < project.m_additionalImages.size()) {
    return &project.m_additionalImages[additionalIndex];
  }

  return nullptr;
}

void eraseSerializedImageAt(serialize::EntropyProject& project, std::size_t index)
{
  if (0 == index) {
    return;
  }

  const std::size_t additionalIndex = index - 1;
  if (additionalIndex < project.m_additionalImages.size()) {
    project.m_additionalImages.erase(project.m_additionalImages.begin() + static_cast<std::ptrdiff_t>(additionalIndex));
  }
}

bool imageSettingsEqual(
  const std::optional<serialize::ImageSettings>& a,
  const std::optional<serialize::ImageSettings>& b)
{
  if (a.has_value() != b.has_value()) {
    return false;
  }
  if (!a) {
    return true;
  }

  return a->m_displayName == b->m_displayName && a->m_level == b->m_level && a->m_window == b->m_window &&
         a->m_thresholdLow == b->m_thresholdLow && a->m_thresholdHigh == b->m_thresholdHigh &&
         a->m_opacity == b->m_opacity && a->m_edgeDetectionMethod == b->m_edgeDetectionMethod &&
         a->m_showEdges == b->m_showEdges && a->m_thresholdEdges == b->m_thresholdEdges &&
         a->m_thinPixelEdges == b->m_thinPixelEdges && a->m_overlayEdges == b->m_overlayEdges &&
         a->m_colormapEdges == b->m_colormapEdges && a->m_edgeMagnitude == b->m_edgeMagnitude &&
         a->m_pixelEdgeScale == b->m_pixelEdgeScale && a->m_pixelEdgeThreshold == b->m_pixelEdgeThreshold &&
         a->m_edgeColor == b->m_edgeColor && a->m_edgeOpacity == b->m_edgeOpacity;
}

bool segSettingsEqual(const std::optional<serialize::SegSettings>& a, const std::optional<serialize::SegSettings>& b)
{
  if (a.has_value() != b.has_value()) {
    return false;
  }
  return !a || a->m_opacity == b->m_opacity;
}

bool matricesEqual(const std::optional<glm::mat4>& a, const std::optional<glm::mat4>& b)
{
  if (a.has_value() != b.has_value()) {
    return false;
  }
  if (!a) {
    return true;
  }

  for (glm::length_t c = 0; c < 4; ++c) {
    for (glm::length_t r = 0; r < 4; ++r) {
      if ((*a)[c][r] != (*b)[c][r]) {
        return false;
      }
    }
  }
  return true;
}

bool segmentationsEqual(const serialize::Segmentation& a, const serialize::Segmentation& b)
{
  return a.m_segFileName == b.m_segFileName && segSettingsEqual(a.m_settings, b.m_settings);
}

bool landmarkGroupsEqual(const serialize::LandmarkGroup& a, const serialize::LandmarkGroup& b)
{
  return a.m_csvFileName == b.m_csvFileName && a.m_inVoxelSpace == b.m_inVoxelSpace;
}

template<typename T, typename Equal>
bool vectorsEqual(const std::vector<T>& a, const std::vector<T>& b, Equal equal);

bool imageSelectionsEqual(const layout::ImageSelectionSpec& a, const layout::ImageSelectionSpec& b)
{
  return a.m_renderedImageIndices == b.m_renderedImageIndices && a.m_metricImageIndices == b.m_metricImageIndices;
}

bool viewLayoutsEqual(const layout::ViewSpec& a, const layout::ViewSpec& b)
{
  return a.m_left == b.m_left && a.m_bottom == b.m_bottom && a.m_width == b.m_width && a.m_height == b.m_height &&
         a.m_viewType == b.m_viewType && a.m_renderMode == b.m_renderMode &&
         a.m_intensityProjectionMode == b.m_intensityProjectionMode && a.m_offsetMode == b.m_offsetMode &&
         a.m_absoluteOffset == b.m_absoluteOffset && a.m_relativeOffsetSteps == b.m_relativeOffsetSteps &&
         a.m_offsetImageIndex == b.m_offsetImageIndex && a.m_rotationSyncGroup == b.m_rotationSyncGroup &&
         a.m_translationSyncGroup == b.m_translationSyncGroup && a.m_zoomSyncGroup == b.m_zoomSyncGroup &&
         a.m_preferredDefaultRenderedImages == b.m_preferredDefaultRenderedImages &&
         a.m_defaultRenderAllImages == b.m_defaultRenderAllImages &&
         imageSelectionsEqual(a.m_imageSelection, b.m_imageSelection);
}

bool projectLayoutsEqual(const layout::LayoutSpec& a, const layout::LayoutSpec& b)
{
  return a.m_isLightbox == b.m_isLightbox && a.m_viewType == b.m_viewType && a.m_renderMode == b.m_renderMode &&
         a.m_intensityProjectionMode == b.m_intensityProjectionMode &&
         a.m_preferredDefaultRenderedImages == b.m_preferredDefaultRenderedImages &&
         a.m_defaultRenderAllImages == b.m_defaultRenderAllImages &&
         imageSelectionsEqual(a.m_imageSelection, b.m_imageSelection) &&
         vectorsEqual(a.m_views, b.m_views, viewLayoutsEqual);
}

template<typename T, typename Equal>
bool vectorsEqual(const std::vector<T>& a, const std::vector<T>& b, Equal equal)
{
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), equal);
}

bool dicomSourcesEqual(const std::optional<serialize::DicomSource>& a, const std::optional<serialize::DicomSource>& b)
{
  if (a.has_value() != b.has_value()) {
    return false;
  }
  if (!a) {
    return true;
  }
  return a->m_rootPath == b->m_rootPath && a->m_studyInstanceUid == b->m_studyInstanceUid &&
         a->m_seriesInstanceUid == b->m_seriesInstanceUid && a->m_files == b->m_files;
}

bool imagesEqual(const serialize::Image& a, const serialize::Image& b)
{
  return a.m_imageFileName == b.m_imageFileName && dicomSourcesEqual(a.m_dicomSource, b.m_dicomSource) &&
         a.m_affineTxFileName == b.m_affineTxFileName && a.m_deformationFileName == b.m_deformationFileName &&
         matricesEqual(a.m_worldDefTx, b.m_worldDefTx) && a.m_annotationsFileName == b.m_annotationsFileName &&
         imageSettingsEqual(a.m_settings, b.m_settings) &&
         vectorsEqual(a.m_segmentations, b.m_segmentations, segmentationsEqual) &&
         vectorsEqual(a.m_landmarkGroups, b.m_landmarkGroups, landmarkGroupsEqual);
}

bool projectInterfaceSettingsEqual(
  const serialize::ProjectInterfaceSettings& a,
  const serialize::ProjectInterfaceSettings& b)
{
  return a.m_showLayoutTabs == b.m_showLayoutTabs && a.m_layoutTabPlacement == b.m_layoutTabPlacement &&
         a.m_imageValuePrecision == b.m_imageValuePrecision && a.m_coordsPrecision == b.m_coordsPrecision &&
         a.m_txPrecision == b.m_txPrecision && a.m_percentilePrecision == b.m_percentilePrecision;
}

bool projectsEqual(const serialize::EntropyProject& a, const serialize::EntropyProject& b)
{
  return imagesEqual(a.m_referenceImage, b.m_referenceImage) &&
         vectorsEqual(a.m_additionalImages, b.m_additionalImages, imagesEqual) &&
         vectorsEqual(a.m_layouts, b.m_layouts, projectLayoutsEqual) &&
         a.m_currentLayoutIndex == b.m_currentLayoutIndex &&
         projectInterfaceSettingsEqual(a.m_interface, b.m_interface);
}

bool isApproximatelyIdentity(const glm::mat4& matrix)
{
  constexpr float epsilon = 1.0e-5f;
  const glm::mat4 identity{1.0f};

  for (int c = 0; c < 4; ++c) {
    for (int r = 0; r < 4; ++r) {
      if (std::abs(matrix[c][r] - identity[c][r]) > epsilon) {
        return false;
      }
    }
  }

  return true;
}
} // namespace

EntropyApp::EntropyApp()
  : m_imageLoadCancelled(false)
  , m_imagesReady(false)
  , m_imageLoadFailed(false)
  , m_glfw(this, GL_VERSION_MAJOR, GL_VERSION_MINOR) // GLFW creates the OpenGL contex
  , m_data()
  , m_rendering(m_data)                            // Requires OpenGL context
  , m_callbackHandler(m_data, m_glfw, m_rendering) // Requires OpenGL context
  , m_itkSnapSync(m_data)
  , m_entropyInstanceSync(m_data)
  , m_imgui(m_glfw.window(), m_data, m_callbackHandler) // Requires OpenGL context
// m_IPCHandler()
{
  spdlog::debug("Begin constructing application");

  setCallbacks();

  // Initialize the IPC handler
  //    m_IPCHandler.Attach( IPCHandler::GetUserPreferencesFileName().c_str(),
  //                         (short) IPCMessage::VERSION, sizeof( IPCMessage ) );

  spdlog::debug("Done constructing application");
}

EntropyApp::~EntropyApp()
{
  if (m_futureLoadProject.valid()) {
    m_futureLoadProject.wait();
  }
  if (m_futureDiscoverDicom.valid()) {
    m_futureDiscoverDicom.wait();
  }

  //    if ( m_IPCHandler.IsAttached() )
  //    {
  //        m_IPCHandler.Close();
  //    }
}

void EntropyApp::init()
{
  spdlog::debug("Begin initializing application");

  // Start the annotation state machine
  state::annot::fsm_list::start();

  if (auto* state = state::annot::fsm_list::current_state_ptr) {
    state->setAppData(&m_data);
    state->setCallbacks([this]() { m_imgui.render(); });
  }
  else {
    spdlog::error("Null annotation state machine");
    throw_debug("Null annotation state machine")
  }

  m_data.guiData().m_renderUiWindows = true;
#if defined(__APPLE__) || defined(_WIN32)
  m_data.guiData().m_showMainMenuBar = false;
#else
  m_data.guiData().m_showMainMenuBar = true;
#endif

  {
    std::string preferencesError;
    const fs::path settingsFile = app_paths::userSettingsFile();
    if (!user_preferences::load(m_data.settings(), m_data.renderData(), settingsFile, &preferencesError)) {
      spdlog::warn("Using built-in settings after failing to load {}: {}", settingsFile, preferencesError);
    }
    m_imgui.setUserScaleOverride(m_data.settings().uiScaleOverride());
    m_imgui.requestFontReload();
    m_imgui.applyUiColorPreset(m_data.settings().uiColorPreset());
    m_imgui.applyUiDensityPreset(m_data.settings().uiDensityPreset());
    m_imgui.applyUiWindowBgOpacity(m_data.settings().uiWindowBgOpacity());
    syncLayoutTabGuiDataFromSettings(m_data);
  }

  m_rendering.init();
  m_glfw.init(); // Trigger initial windowing callbacks

  spdlog::debug("Done initializing application");
}

void EntropyApp::run()
{
  spdlog::debug("Begin application run loop");

  auto checkIfAppShouldQuit = [this]() {
    return m_data.state().quitApp();
  };

  m_glfw.renderLoop(m_imagesReady, m_imageLoadFailed, checkIfAppShouldQuit, [this]() { onImagesReady(); });

  // Cancel image loading, in case it's still going on
  m_imageLoadCancelled = true;

  spdlog::debug("Done application run loop");
}

void EntropyApp::onImagesReady()
{
  // Recenter the crosshairs, but don't recenter views on the crosshairs:
  constexpr bool recenterCrosshairs = true;
  constexpr bool realignCrosshairs = true;
  constexpr bool doNotRecenterOnCurrentCrosshairsPos = false;
  constexpr bool resetObliqueOrientation = true;
  constexpr bool resetZoom = true;

  const bool preserveLayouts = m_preserveLayoutsOnImagesReady;
  const std::vector<uuids::uuid> pendingAddedImageUids = m_pendingAddedImageUids;
  const std::optional<fs::path> pendingLayoutsFile = m_pendingLayoutsFile;
  m_preserveLayoutsOnImagesReady = false;
  m_pendingAddedImageUids.clear();
  m_pendingLayoutsFile = std::nullopt;

  spdlog::debug("Images are loaded.");

  const Image* refImg = m_data.refImage();
  if (!refImg) {
    // At a minimum, we need a reference image to do anything.
    // If the reference image is null, then image loading has failed.
    spdlog::critical("The reference image is null");
    throw_debug("The reference image is null")
  }

  if (!preserveLayouts) {
    m_data.windowData().resetDefaultLayouts();
    m_data.windowData().setCurrentLayoutIndex(1);
  }

  auto& renderData = m_data.renderData();
  renderData.m_imageTextures.clear();
  renderData.m_distanceMapTextures.clear();
  renderData.m_segTextures.clear();
  renderData.m_labelBufferTextures.clear();
  renderData.m_colormapTextures.clear();
  renderData.m_uniforms.clear();

  m_rendering.initTextures();
  m_rendering.updateImageUniforms(m_data.imageUidsOrdered());

  spdlog::debug("Textures and uniforms ready; rendering enabled");

  // Stop animation rendering (which plays during loading) and render only on events:
  m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
  updateWindowTitleStatus();

  m_data.state().setAnimating(false);
  m_data.settings().setOverlays(true);

  m_data.guiData().m_renderUiWindows = true;
  m_data.guiData().m_renderUiOverlays = true;

  spdlog::debug("Begin setting up window state");

  if (preserveLayouts) {
    for (const auto& pendingAddedImageUid : pendingAddedImageUids) {
      m_data.windowData().appendImageToDefaultRenderedImages(m_data, pendingAddedImageUid);
    }

    m_data.windowData().updateImageOrdering(m_data.imageUidsOrdered());
    m_data.windowData().reconcileImageDependentLayouts(m_data, dicomNativeViewTypesByImage());
  }
  else {
    m_data.windowData().reconcileImageDependentLayouts(m_data, dicomNativeViewTypesByImage());
    m_data.windowData().setDefaultRenderedImagesForAllLayouts(m_data);

    if (!m_data.project().m_layouts.empty()) {
      m_data.windowData().applyProjectLayoutSnapshots(
        m_data.project().m_layouts,
        m_data.imageUidsOrdered(),
        m_data.project().m_currentLayoutIndex);
    }

    if (pendingLayoutsFile) {
      loadLayoutsFile(*pendingLayoutsFile);
    }

    m_callbackHandler.recenterViews(
      m_data.state().recenteringMode(),
      recenterCrosshairs,
      realignCrosshairs,
      doNotRecenterOnCurrentCrosshairsPos,
      resetObliqueOrientation,
      resetZoom);
  }

  m_callbackHandler.setMouseMode(MouseMode::Pointer);

  // Trigger two UI renders in order to freshen up its internal state.
  // Without both render calls, the UI state is not correctly set up.
  m_imgui.render();
  m_imgui.render();

  // Trigger a resize in order to correctly set the viewport, since UI
  // state changes in the render call:
  resize(m_data.windowData().getWindowSize().x, m_data.windowData().getWindowSize().y);

  m_data.state().setProjectLoadState(ProjectLoadState::Loaded);
  if (!preserveLayouts) {
    if (m_data.projectFileName()) {
      markProjectSavedSnapshot();
    }
    else {
      m_savedProjectSnapshot = std::nullopt;
    }
  }

  spdlog::debug("Done setting up window state");
}

void EntropyApp::resize(int windowWidth, int windowHeight)
{
  const GuiData::Margins margins = guiData().computeMargins();

  // This call sets the window size and viewport
  // app->resize( windowWidth, windowHeight );
  windowData().setWindowSize(windowWidth, windowHeight);

  if (const std::optional<glm::vec4>& renderViewport = guiData().m_renderViewport) {
    const GuiData::Margins toolbarMargins = guiData().computeToolbarMargins();
    const float minLeft = margins.left;
    const float minBottom = margins.bottom;
    const float maxRight = std::max(minLeft + 1.0f, static_cast<float>(windowWidth) - margins.right);
    const float maxTop = std::max(minBottom + 1.0f, static_cast<float>(windowHeight) - margins.top);

    const float left = std::clamp(renderViewport->x + toolbarMargins.left, minLeft, maxRight - 1.0f);
    const float bottom = std::clamp(renderViewport->y + toolbarMargins.bottom, minBottom, maxTop - 1.0f);
    const float right = std::clamp(renderViewport->x + renderViewport->z - toolbarMargins.right, left + 1.0f, maxRight);
    const float top = std::clamp(renderViewport->y + renderViewport->w - toolbarMargins.top, bottom + 1.0f, maxTop);

    windowData().setViewport(left, bottom, std::max(1.0f, right - left), std::max(1.0f, top - bottom));
    return;
  }

  // Set viewport to account for margins.
  windowData().setViewport(
    margins.left,
    margins.bottom,
    std::max(1.0f, static_cast<float>(windowWidth) - (margins.left + margins.right)),
    std::max(1.0f, static_cast<float>(windowHeight) - (margins.bottom + margins.top)));
}

void EntropyApp::render()
{
  pollDicomSeriesScan();
  m_glfw.renderOnce();
}

CallbackHandler& EntropyApp::callbackHandler()
{
  return m_callbackHandler;
}

const AppData& EntropyApp::appData() const
{
  return m_data;
}

AppData& EntropyApp::appData()
{
  return m_data;
}

const AppSettings& EntropyApp::appSettings() const
{
  return m_data.settings();
}

AppSettings& EntropyApp::appSettings()
{
  return m_data.settings();
}

const AppState& EntropyApp::appState() const
{
  return m_data.state();
}

AppState& EntropyApp::appState()
{
  return m_data.state();
}

const GuiData& EntropyApp::guiData() const
{
  return m_data.guiData();
}

GuiData& EntropyApp::guiData()
{
  return m_data.guiData();
}

const GlfwWrapper& EntropyApp::glfw() const
{
  return m_glfw;
}

GlfwWrapper& EntropyApp::glfw()
{
  return m_glfw;
}

const ImGuiWrapper& EntropyApp::imgui() const
{
  return m_imgui;
}

ImGuiWrapper& EntropyApp::imgui()
{
  return m_imgui;
}

const WindowData& EntropyApp::windowData() const
{
  return m_data.windowData();
}

WindowData& EntropyApp::windowData()
{
  return m_data.windowData();
}

void EntropyApp::logPreamble()
{
  spdlog::info("{} (version {})", APP_NAME, VERSION_FULL);
  spdlog::info("{}", COPYRIGHT_LINE);

  spdlog::debug("Git branch: {}", GIT_BRANCH);
  spdlog::debug("Git commit hash: {}", GIT_COMMIT_SHA1);
  spdlog::debug("Git commit timestamp: {}", GIT_COMMIT_TIMESTAMP);
  spdlog::debug("Build timestamp: {}", BUILD_TIMESTAMP);
  spdlog::debug("Build type: {}", CMAKE_BUILD_TYPE);
}

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

  return {m_data.addImage(std::move(image)), true};
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
  return {m_data.addImage(std::move(*image)), true};
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
    spdlog::info("Loaded segmentation from file {}", fileName);
    return {*segUid, true};
  }

  return noSegLoaded;
}

std::pair<std::optional<uuids::uuid>, bool> EntropyApp::loadDeformationField(const fs::path& fileName)
{
  // Return value indicating that deformation field was not loaded:
  const std::pair<std::optional<uuids::uuid>, bool> noDefLoaded{std::nullopt, false};

  // Has this deformation field already been loaded? Search for its file name:
  for (const auto& defUid : m_data.defUidsOrdered()) {
    if (const Image* def = m_data.def(defUid)) {
      if (def->header().fileName() == fileName) {
        spdlog::info("Deformation field from {} has already been loaded as {}", fileName, defUid);
        return {defUid, false};
      }
    }
  }

  // Components of a deformation field image are loaded as interleaved images
  Image def(fileName, Image::ImageRepresentation::Image, Image::MultiComponentBufferType::InterleavedImage);

  if (def.header().numComponentsPerPixel() < 3) {
    spdlog::error(
      "The deformation field from file {} has fewer than three components per pixel "
      "and so will not be loaded.",
      fileName);
    return noDefLoaded;
  }

  spdlog::info("Read deformation field image from file {}", fileName);

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
  std::ostringstream ss;
  def.metaData(ss);

  SPDLOG_TRACE("Meta data:\n{}", ss.str());
#endif
  spdlog::info("Header:\n{}", def.header());
  spdlog::info("Transformation:\n{}", def.transformations());
  spdlog::info("Settings:\n{}", def.settings());

  // TODO: Do check of deformation field header against the reference image header?

  if (const auto defUid = m_data.addDef(std::move(def))) {
    spdlog::info("Loaded deformation field image from file {} as {}", fileName, *defUid);
    return {*defUid, true};
  }

  return noDefLoaded;
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

  spdlog::info("Loaded image from {} as {}", imageToLoad.m_imageFileName, *imageUid);

  if (resolvedDicomSource) {
    m_dicomSourcesByImageUid[*imageUid] = *resolvedDicomSource;
  }

  if (serializedImage.m_settings) {
    applyImageSettingsSnapshot(*image, *serializedImage.m_settings);
  }

  // Disable the initial affine and manual transformations for the reference image:
  image->transformations().set_enable_worldDef_T_affine(isReferenceImage ? false : true);
  image->transformations().set_enable_affine_T_subject(isReferenceImage ? false : true);

  // Lock all affine transformations to the reference image, which defines the World space:
  image->transformations().set_worldDef_T_affine_locked(true);

  // Load and set affine transformation from file (for non-reference images only):
  if (serializedImage.m_affineTxFileName) {
    glm::dmat4 affine_T_subject(1.0);

    if (isReferenceImage) {
      spdlog::warn(
        "An affine transformation file ({}) was provided for the reference image. "
        "It will be ignored, since the reference image defines the World coordinate "
        "space, which cannot be transformed.",
        *serializedImage.m_affineTxFileName);

      image->transformations().set_affine_T_subject_fileName(std::nullopt);
    }
    else {
      if (!serialize::openAffineTxFile(affine_T_subject, *serializedImage.m_affineTxFileName)) {
        spdlog::error(
          "Unable to read affine transformation from {} for image {}",
          *serializedImage.m_affineTxFileName,
          *imageUid);

        image->transformations().set_affine_T_subject_fileName(std::nullopt);
      }

      image->transformations().set_affine_T_subject_fileName(serializedImage.m_affineTxFileName);
      image->transformations().set_affine_T_subject(glm::mat4{affine_T_subject});
    }
  }
  else {
    // No affine transformation provided:
    image->transformations().set_affine_T_subject_fileName(std::nullopt);
  }

  if (serializedImage.m_worldDefTx) {
    if (isReferenceImage) {
      spdlog::warn(
        "A manual transformation was provided for the reference image. It will be ignored, since the reference "
        "image defines World space.");
    }
    else {
      image->transformations().set_worldDef_T_affine_locked(false);
      image->transformations().set_enable_worldDef_T_affine(true);
      image->transformations().set_worldDef_T_affine(*serializedImage.m_worldDefTx);
      image->transformations().set_worldDef_T_affine_locked(true);
    }
  }

  //    if ( serializedImage.m_deformationFileName && isReferenceImage )
  //    {
  //        spdlog::warn( "A deformable transformation file ({}) was provided for the reference
  //        image. "
  //                      "It will be ignored, since the reference image defines the World
  //                      coordinate " "space, which cannot be transformed.",
  //                      *serializedImage.m_deformationFileName );
  //    }
  //    else
  if (serializedImage.m_deformationFileName) {
    std::optional<uuids::uuid> deformationUid;
    bool isDeformationNewImage = false;

    try {
      spdlog::debug("Attempting to load deformation field image from {}", *serializedImage.m_deformationFileName);
      std::tie(deformationUid, isDeformationNewImage) = loadDeformationField(*serializedImage.m_deformationFileName);
    }
    catch (const std::exception& e) {
      spdlog::error(
        "Exception loading deformation field from {}: {}",
        *serializedImage.m_deformationFileName,
        e.what());
    }

    do {
      if (!deformationUid) {
        spdlog::error(
          "Unable to load deformation field from {} for image {}",
          *serializedImage.m_deformationFileName,
          *imageUid);
        break;
      }

      if (!isDeformationNewImage) {
        spdlog::info(
          "Deformation field from {} already exists in this project as image {}",
          *serializedImage.m_deformationFileName,
          *deformationUid);
        break;
      }

      Image* deformation = m_data.def(*deformationUid);

      if (!deformation) {
        spdlog::error("Null deformation field image {}", *deformationUid);
        break;
      }

      deformation->settings().setDisplayName(deformation->settings().displayName() + " (deformation)");

      // TODO: Load this from project settings.
      for (uint32_t i = 0; i < deformation->header().numComponentsPerPixel(); ++i) {
        deformation->settings().setColorMapIndex(i, 25);
      }

      if (m_data.assignDefUidToImage(*imageUid, *deformationUid)) {
        spdlog::info("Assigned deformation field {} to image {}", *deformationUid, *imageUid);
      }
      else {
        spdlog::error("Unable to assign deformation field {} to image {}", *deformationUid, *imageUid);
        m_data.removeDef(*deformationUid);
        break;
      }

      break;
    } while (1);

    // TODO: Deformation field images are special:
    // 1) no segmentation is created
    // 2) no affine transformation can be applied: it copies the affine tx of its image
    // 3) need warning when header tx doesn't match that of reference
    // 4) even if all components are loaded as RGB texure, we should be able to view each component
    // separately in a shader that takes in as a uniform the active component
  }

  // Set annotations from file:
  if (serializedImage.m_annotationsFileName) {
    std::vector<Annotation> annots;

    if (serialize::openAnnotationsFromJsonFile(annots, *serializedImage.m_annotationsFileName)) {
      spdlog::info(
        "Loaded annotations from JSON file {} for image {}",
        *serializedImage.m_annotationsFileName,
        *imageUid);

      for (auto& annot : annots) {
        // Assign the annotation the file name from which it was read:
        annot.setFileName(*serializedImage.m_annotationsFileName);

        if (const auto annotUid = m_data.addAnnotation(*imageUid, annot)) {
          m_data.assignActiveAnnotationUidToImage(*imageUid, *annotUid);
          spdlog::debug("Added annotation {} for image {}", *annotUid, *imageUid);
        }
        else {
          spdlog::error("Unable to add annotation to image {}", *imageUid);
        }
      }
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

  // Set landmarks from file:
  for (const auto& lm : serializedImage.m_landmarkGroups) {
    std::map<size_t, PointRecord<glm::vec3> > landmarks;

    if (serialize::openLandmarkGroupCsvFile(landmarks, lm.m_csvFileName)) {
      spdlog::info("Loaded landmarks from CSV file {} for image {}", lm.m_csvFileName, *imageUid);

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
      lmGroup.setFileName(lm.m_csvFileName);
      lmGroup.setName(getFileName(lm.m_csvFileName, false));
      lmGroup.setPoints(landmarks);
      lmGroup.setRenderLandmarkNames(false);

      if (lm.m_inVoxelSpace) {
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
    }
    else {
      spdlog::error("Unable to open landmarks from CSV file {} for image {}", lm.m_csvFileName, *imageUid);
    }
  }

  // Create distance maps for all components:
  // To conserve GPU memory, the map is downsampled by 0.25 relative to the original image size
  constexpr float distanceMapDownsample = 0.25f;
  createDistanceMaps(*image, *imageUid, distanceMapDownsample, m_data);

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
      std::tie(segInfo.uid, segInfo.isNewSeg) = loadSegmentation(serializedSeg.m_segFileName, *imageUid);
    }
    catch (const std::exception& e) {
      spdlog::error("Exception loading segmentation from {}: {}", serializedSeg.m_segFileName, e.what());
      continue; // Skip this segmentation
    }

    if (segInfo.uid) {
      if (segInfo.isNewSeg) {
        spdlog::info(
          "Loaded segmentation from file {} for image {} as {}",
          serializedSeg.m_segFileName,
          *imageUid,
          *segInfo.uid);

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
        applySegSettingsSnapshot(*seg, *serializedSegIt->m_settings);
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

  for (uint32_t i = 0; i < image->header().numComponentsPerPixel(); ++i) {
    image->settings().setColorMapIndex(i, defaultImageColorMapIndex);
  }

  return true;
}

serialize::EntropyProject EntropyApp::createProjectSnapshot() const
{
  serialize::EntropyProject project;
  const auto imageUids = m_data.imageUidsOrdered();

  if (imageUids.empty()) {
    return project;
  }

  const uuids::uuid referenceImageUid = m_data.refImageUid().value_or(imageUids.front());
  project.m_referenceImage = createImageSnapshot(referenceImageUid);

  for (const auto& imageUid : imageUids) {
    if (imageUid == referenceImageUid) {
      continue;
    }

    project.m_additionalImages.emplace_back(createImageSnapshot(imageUid));
  }

  project.m_layouts = m_data.windowData().createProjectLayoutSnapshots(imageUids);
  project.m_currentLayoutIndex = m_data.windowData().currentLayoutIndex();
  project.m_interface = projectInterfaceSettings(m_data);

  return project;
}

serialize::Image EntropyApp::createImageSnapshot(const uuids::uuid& imageUid) const
{
  serialize::Image serializedImage;
  const Image* image = m_data.image(imageUid);

  if (!image) {
    spdlog::warn("Cannot serialize missing image {}", imageUid);
    return serializedImage;
  }

  serializedImage.m_imageFileName = image->header().fileName();
  if (const auto sourceIt = m_dicomSourcesByImageUid.find(imageUid); sourceIt != m_dicomSourcesByImageUid.end()) {
    serializedImage.m_dicomSource = sourceIt->second;
  }
  serializedImage.m_affineTxFileName = image->transformations().get_affine_T_subject_fileName();
  if (
    image->transformations().get_enable_worldDef_T_affine() &&
    !isApproximatelyIdentity(image->transformations().get_worldDef_T_affine()))
  {
    serializedImage.m_worldDefTx = image->transformations().get_worldDef_T_affine();
  }
  serializedImage.m_settings = makeImageSettingsSnapshot(*image);

  const auto defUids = m_data.imageToDefUids(imageUid);
  if (!defUids.empty()) {
    const Image* def = m_data.def(defUids.front());
    if (def && def->header().existsOnDisk() && !def->header().fileName().empty()) {
      serializedImage.m_deformationFileName = def->header().fileName();
    }

    if (defUids.size() > 1) {
      spdlog::warn(
        "Image {} has {} deformation fields, but project files currently save only the first one",
        imageUid,
        defUids.size());
    }
  }

  for (const auto& segUid : m_data.imageToSegUids(imageUid)) {
    const Image* seg = m_data.seg(segUid);
    if (!seg) {
      spdlog::warn("Cannot serialize missing segmentation {} for image {}", segUid, imageUid);
      continue;
    }

    if (!seg->header().existsOnDisk() || seg->header().fileName().empty()) {
      spdlog::debug("Skipping unsaved segmentation {} for image {}; it has no file-backed path", segUid, imageUid);
      continue;
    }

    serialize::Segmentation serializedSeg;
    serializedSeg.m_segFileName = seg->header().fileName();
    serializedSeg.m_settings = makeSegSettingsSnapshot(*seg);
    serializedImage.m_segmentations.emplace_back(std::move(serializedSeg));
  }

  for (const auto& lmUid : m_data.imageToLandmarkGroupUids(imageUid)) {
    const LandmarkGroup* lmGroup = m_data.landmarkGroup(lmUid);
    if (!lmGroup) {
      spdlog::warn("Cannot serialize missing landmark group {} for image {}", lmUid, imageUid);
      continue;
    }

    if (lmGroup->getFileName().empty()) {
      spdlog::warn("Skipping unsaved landmark group {} for image {}; it has no file name", lmUid, imageUid);
      continue;
    }

    serialize::LandmarkGroup serializedLandmarks;
    serializedLandmarks.m_csvFileName = lmGroup->getFileName().string();
    serializedLandmarks.m_inVoxelSpace = lmGroup->getInVoxelSpace();
    serializedImage.m_landmarkGroups.emplace_back(std::move(serializedLandmarks));
  }

  for (const auto& annotationUid : m_data.annotationsForImage(imageUid)) {
    const Annotation* annotation = m_data.annotation(annotationUid);
    if (!annotation) {
      spdlog::warn("Cannot serialize missing annotation {} for image {}", annotationUid, imageUid);
      continue;
    }

    if (annotation->getFileName().empty()) {
      spdlog::warn("Skipping unsaved annotation {} for image {}; it has no file name", annotationUid, imageUid);
      continue;
    }

    if (serializedImage.m_annotationsFileName && *serializedImage.m_annotationsFileName != annotation->getFileName()) {
      spdlog::warn(
        "Image {} has annotations from multiple files; project files currently save only {}",
        imageUid,
        *serializedImage.m_annotationsFileName);
      continue;
    }

    serializedImage.m_annotationsFileName = annotation->getFileName();
  }

  return serializedImage;
}

bool EntropyApp::hasUnsavedAnnotations() const
{
  for (const auto& imageUid : m_data.imageUidsOrdered()) {
    for (const auto& annotationUid : m_data.annotationsForImage(imageUid)) {
      const Annotation* annotation = m_data.annotation(annotationUid);
      if (annotation && (annotation->isDirty() || annotation->getFileName().empty())) {
        return true;
      }
    }
  }

  return false;
}

bool EntropyApp::projectHasUnsavedChanges() const
{
  if (ProjectLoadState::Loaded != m_data.state().projectLoadState() || 0 == m_data.numImages()) {
    return false;
  }

  if (hasUnsavedAnnotations()) {
    return true;
  }

  if (!m_data.projectFileName()) {
    return true;
  }

  if (!m_savedProjectSnapshot) {
    return true;
  }

  return !projectsEqual(createProjectSnapshot(), *m_savedProjectSnapshot);
}

bool EntropyApp::saveAnnotationsForImage(const uuids::uuid& imageUid, const fs::path& fileName)
{
  nlohmann::json annotationsJson;

  for (const auto& annotationUid : m_data.annotationsForImage(imageUid)) {
    const Annotation* annotation = m_data.annotation(annotationUid);
    if (annotation) {
      serialize::appendAnnotationToJson(*annotation, annotationsJson);
    }
  }

  if (!serialize::saveToJsonFile(annotationsJson, fileName)) {
    spdlog::error("Could not save annotations for image {} to {}", imageUid, fileName);
    return false;
  }

  for (const auto& annotationUid : m_data.annotationsForImage(imageUid)) {
    Annotation* annotation = m_data.annotation(annotationUid);
    if (annotation) {
      annotation->setFileName(fileName);
      annotation->markClean();
    }
  }

  spdlog::info("Saved annotations for image {} to JSON file {}", imageUid, fileName);
  return true;
}

bool EntropyApp::saveDirtyAnnotationsWithDialogs()
{
  for (const auto& imageUid : m_data.imageUidsOrdered()) {
    bool needsSave = false;
    std::optional<fs::path> existingFileName = std::nullopt;

    for (const auto& annotationUid : m_data.annotationsForImage(imageUid)) {
      const Annotation* annotation = m_data.annotation(annotationUid);
      if (!annotation) {
        continue;
      }

      if (!annotation->getFileName().empty() && !existingFileName) {
        existingFileName = annotation->getFileName();
      }

      if (annotation->isDirty() || annotation->getFileName().empty()) {
        needsSave = true;
      }
    }

    if (!needsSave) {
      continue;
    }

    fs::path annotationFileName;
    if (existingFileName) {
      annotationFileName = *existingFileName;
    }
    else {
      const Image* image = m_data.image(imageUid);
      const fs::path defaultDirectory = image ? image->header().fileName().parent_path() : fs::path{};
      const std::string defaultName =
        image ? (image->header().fileName().stem().string() + "_annotations.json") : std::string{"annotations.json"};
      const auto selectedFile =
        native_dialog::saveFile(native_dialog::annotationFilters(), defaultDirectory, defaultName);
      if (!selectedFile) {
        return false;
      }
      annotationFileName = *selectedFile;
    }

    if (!saveAnnotationsForImage(imageUid, annotationFileName)) {
      return false;
    }
  }

  return true;
}

void EntropyApp::markProjectSavedSnapshot()
{
  m_savedProjectSnapshot = createProjectSnapshot();
  m_data.setProject(*m_savedProjectSnapshot);
}

std::string EntropyApp::windowTitleStatus() const
{
  const std::string imageDisplayNames = m_data.getAllImageDisplayNames();
  if (!m_data.projectFileName()) {
    return imageDisplayNames;
  }

  const std::string projectDisplayName = m_data.projectFileName()->filename().string();
  if (imageDisplayNames.empty()) {
    return projectDisplayName;
  }

  return projectDisplayName + ": " + imageDisplayNames;
}

void EntropyApp::updateWindowTitleStatus()
{
  m_glfw.setWindowTitleStatus(windowTitleStatus());
}

bool EntropyApp::saveProject()
{
  if (!saveDirtyAnnotationsWithDialogs()) {
    return false;
  }

  if (!m_data.projectFileName()) {
    spdlog::warn("Cannot save project because it has not been saved to a file yet");
    return false;
  }

  return saveProjectAs(*m_data.projectFileName());
}

bool EntropyApp::saveProjectAs(const fs::path& fileName)
{
  if (!saveDirtyAnnotationsWithDialogs()) {
    return false;
  }

  const fs::path normalizedFileName = projectSavePath(fileName);
  serialize::EntropyProject project = createProjectSnapshot();

  if (project.m_referenceImage.m_imageFileName.empty()) {
    spdlog::error("Cannot save project without a reference image");
    return false;
  }

  if (!serialize::save(project, normalizedFileName)) {
    spdlog::error("Could not save project file {}", normalizedFileName);
    return false;
  }

  m_data.setProject(project);
  m_savedProjectSnapshot = project;
  m_data.setProjectFileName(normalizedFileName);
  updateWindowTitleStatus();
  m_glfw.postEmptyEvent();
  return true;
}

void EntropyApp::loadLayoutsFile(const fs::path& fileName)
{
  if (ProjectLoadState::Loaded != m_data.state().projectLoadState()) {
    m_pendingLayoutsFile = fileName;
    return;
  }

  layout::LayoutFile layoutFile;
  if (!layout::open(layoutFile, fileName)) {
    return;
  }

  if (layoutFile.m_layouts.empty()) {
    spdlog::warn("Layout file {} contains no layouts; using a Three-up layout", fileName);
  }

  if (!m_data.windowData().applyLayoutPresets(m_data, layoutFile.m_layouts, layoutFile.m_currentLayoutIndex)) {
    spdlog::error("Could not apply layout file {}", fileName);
    return;
  }

  m_data.setProject(createProjectSnapshot());
  m_glfw.postEmptyEvent();
  spdlog::info("Loaded layouts from {}", fileName);
}

bool EntropyApp::saveLayoutsFile(const fs::path& fileName)
{
  if (ProjectLoadState::Loaded != m_data.state().projectLoadState()) {
    spdlog::warn("Cannot save layouts because no project is loaded");
    return false;
  }

  layout::LayoutFile layoutFile{
    .m_currentLayoutIndex = m_data.windowData().currentLayoutIndex(),
    .m_layouts = m_data.windowData().createLayoutPresets(m_data.imageUidsOrdered())};

  const bool saved = layout::save(layoutFile, fileName);
  if (saved) {
    spdlog::info("Saved layouts to {}", fileName);
  }
  return saved;
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

  if (ProjectLoadState::Loaded == m_data.state().projectLoadState() && m_data.refImageUid()) {
    closeProject();
  }

  serialize::EntropyProject project = createProjectFromImageFiles(imageFiles);

  m_pendingLargeImageLoadContext = LargeImageLoadContext::Project;
  m_pendingLargeProject = std::move(project);
  m_pendingLargeProjectFileName = std::nullopt;
  m_pendingLargeProjectImageIndex = 0;
  continueLargeImageProjectPreflight();
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
      updateWindowTitleStatus();
      m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
    },
    false);
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
    result.warnings.push_back("Unknown exception while scanning DICOM series");
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
      updateWindowTitleStatus();
      m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
    },
    !addToExistingProject);
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
  renderData.m_distanceMapTextures.erase(imageUid);
  renderData.m_uniforms.erase(imageUid);

  for (const auto& segUid : segUids) {
    if (!m_data.seg(segUid)) {
      renderData.m_segTextures.erase(segUid);
    }
  }

  for (const auto& defUid : defUids) {
    if (!m_data.def(defUid)) {
      renderData.m_imageTextures.erase(defUid);
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

void EntropyApp::loadProjectFile(const fs::path& fileName)
{
  if (fileName.empty()) {
    return;
  }

  m_pendingProjectReplacementPaths = {fileName};
  if (requestProjectReplacement(GuiData::UnsavedProjectAction::OpenProject)) {
    return;
  }
  clearPendingProjectReplacement();
  performLoadProjectFile(fileName);
}

void EntropyApp::performLoadProjectFile(const fs::path& fileName)
{
  serialize::EntropyProject project;

  if (!serialize::open(project, fileName)) {
    spdlog::error("Could not open project file {}", fileName);
    if (ProjectLoadState::Loaded != m_data.state().projectLoadState()) {
      m_data.state().setProjectLoadState(ProjectLoadState::Failed);
    }
    m_glfw.postEmptyEvent();
    return;
  }

  if (ProjectLoadState::Loaded == m_data.state().projectLoadState() && m_data.refImageUid()) {
    closeProject();
  }

  m_pendingLargeImageLoadContext = LargeImageLoadContext::Project;
  m_pendingLargeProject = std::move(project);
  m_pendingLargeProjectFileName = fileName;
  m_pendingLargeProjectImageIndex = 0;
  continueLargeImageProjectPreflight();
}

bool EntropyApp::requestProjectReplacement(GuiData::UnsavedProjectAction action)
{
  if (!projectHasUnsavedChanges()) {
    return false;
  }

  m_data.guiData().m_pendingUnsavedProjectAction = action;
  m_data.guiData().m_showUnsavedProjectPopup = true;
  m_glfw.postEmptyEvent();
  return true;
}

void EntropyApp::clearPendingProjectReplacement()
{
  m_pendingProjectReplacementPaths.clear();
}

void EntropyApp::beginLoadProject(serialize::EntropyProject project, std::optional<fs::path> projectFileName)
{
  closeProject();
  m_data.setProject(std::move(project));
  m_data.setProjectFileName(std::move(projectFileName));
  applyProjectInterfaceSettings(m_data, m_data.project().m_interface);

  startAsyncImageLoad(
    "Loading project...",
    [this]() { return loadProject(m_data.project()); },
    [this]() {
      m_data.clearProjectData();
      m_data.state().setProjectLoadState(ProjectLoadState::Failed);
      m_data.state().setAnimating(false);
      m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
    });
}

void EntropyApp::continueLargeImageProjectPreflight()
{
  if (!m_pendingLargeProject) {
    return;
  }

  while (m_pendingLargeProjectImageIndex < numSerializedImages(*m_pendingLargeProject)) {
    serialize::Image* image = serializedImageAt(*m_pendingLargeProject, m_pendingLargeProjectImageIndex);
    if (!image) {
      m_pendingLargeProjectImageIndex++;
      continue;
    }

    const auto header = readImageHeaderOnly(
      image->m_imageFileName,
      Image::ImageRepresentation::Image,
      Image::MultiComponentBufferType::SeparateImages);

    if (!header) {
      if (0 == m_pendingLargeProjectImageIndex) {
        spdlog::error("Could not read reference image header from {}; cancelling project load", image->m_imageFileName);
        m_pendingLargeImageLoadContext = LargeImageLoadContext::None;
        m_pendingLargeProject = std::nullopt;
        m_pendingLargeProjectFileName = std::nullopt;
        m_pendingLargeProjectImageIndex = 0;
        if (ProjectLoadState::Loaded != m_data.state().projectLoadState()) {
          m_data.state().setProjectLoadState(ProjectLoadState::Failed);
        }
        m_glfw.postEmptyEvent();
        return;
      }

      spdlog::error("Could not read image header from {}; skipping it", image->m_imageFileName);
      eraseSerializedImageAt(*m_pendingLargeProject, m_pendingLargeProjectImageIndex);
      continue;
    }

    if (shouldPromptForLargeImage(*header)) {
      spdlog::warn(
        "Image {} is large: estimated in-memory size is {:.2f} GiB",
        image->m_imageFileName,
        static_cast<double>(header->memoryImageSizeInBytes()) / (1024.0 * 1024.0 * 1024.0));

      m_pendingLargeImageLoadContext = LargeImageLoadContext::Project;
      m_data.guiData().m_pendingLargeImageLoadPrompt =
        GuiData::LargeImageLoadPrompt{image->m_imageFileName, *header, true, 0 != m_pendingLargeProjectImageIndex};
      m_data.guiData().m_showLargeImageLoadPrompt = true;
      m_glfw.postEmptyEvent();
      return;
    }

    m_pendingLargeProjectImageIndex++;
  }

  serialize::EntropyProject project = std::move(*m_pendingLargeProject);
  std::optional<fs::path> projectFileName = std::move(m_pendingLargeProjectFileName);
  m_pendingLargeImageLoadContext = LargeImageLoadContext::None;
  m_pendingLargeProject = std::nullopt;
  m_pendingLargeProjectFileName = std::nullopt;
  m_pendingLargeProjectImageIndex = 0;
  beginLoadProject(std::move(project), std::move(projectFileName));
}

void EntropyApp::handleLargeImageLoadDecision(GuiData::LargeImageLoadDecision decision)
{
  switch (m_pendingLargeImageLoadContext) {
    case LargeImageLoadContext::AddImage: {
      const std::optional<fs::path> fileName = m_pendingLargeAddImageFile;
      m_pendingLargeImageLoadContext = LargeImageLoadContext::None;
      m_pendingLargeAddImageFile = std::nullopt;

      if (GuiData::LargeImageLoadDecision::LoadOriginal == decision && fileName) {
        m_data.guiData().m_bypassNextImageLoadPreflight = true;
        addImageFile(*fileName);
      }
      break;
    }
    case LargeImageLoadContext::Project: {
      if (!m_pendingLargeProject) {
        m_pendingLargeImageLoadContext = LargeImageLoadContext::None;
        break;
      }

      if (GuiData::LargeImageLoadDecision::CancelProject == decision) {
        spdlog::info("Cancelled project load during large-image preflight");
        m_pendingLargeImageLoadContext = LargeImageLoadContext::None;
        m_pendingLargeProject = std::nullopt;
        m_pendingLargeProjectFileName = std::nullopt;
        m_pendingLargeProjectImageIndex = 0;
        m_glfw.postEmptyEvent();
        break;
      }

      if (GuiData::LargeImageLoadDecision::SkipImage == decision) {
        if (0 == m_pendingLargeProjectImageIndex) {
          spdlog::info("Cannot skip the reference image; cancelling project load");
          m_pendingLargeImageLoadContext = LargeImageLoadContext::None;
          m_pendingLargeProject = std::nullopt;
          m_pendingLargeProjectFileName = std::nullopt;
          m_pendingLargeProjectImageIndex = 0;
          m_glfw.postEmptyEvent();
          break;
        }

        serialize::Image* image = serializedImageAt(*m_pendingLargeProject, m_pendingLargeProjectImageIndex);
        if (image) {
          spdlog::info("Skipping large image {} during project load", image->m_imageFileName);
        }
        eraseSerializedImageAt(*m_pendingLargeProject, m_pendingLargeProjectImageIndex);
      }
      else {
        m_pendingLargeProjectImageIndex++;
      }

      continueLargeImageProjectPreflight();
      break;
    }
    case LargeImageLoadContext::None:
      break;
  }
}

void EntropyApp::requestCloseProject()
{
  if (projectHasUnsavedChanges()) {
    m_data.guiData().m_pendingUnsavedProjectAction = GuiData::UnsavedProjectAction::CloseProject;
    m_data.guiData().m_showUnsavedProjectPopup = true;
    m_glfw.postEmptyEvent();
    return;
  }

  closeProject();
}

void EntropyApp::requestQuitApp()
{
  if (projectHasUnsavedChanges()) {
    m_data.guiData().m_pendingUnsavedProjectAction = GuiData::UnsavedProjectAction::QuitApp;
    m_data.guiData().m_showUnsavedProjectPopup = true;
    m_glfw.postEmptyEvent();
    return;
  }

  m_data.guiData().m_showConfirmCloseAppPopup = true;
  m_glfw.postEmptyEvent();
}

void EntropyApp::quitAppWithoutPrompt()
{
  m_data.state().setQuitApp(true);
  m_glfw.postEmptyEvent();
}

void EntropyApp::continueAfterUnsavedProjectPrompt()
{
  const GuiData::UnsavedProjectAction action = m_data.guiData().m_pendingUnsavedProjectAction;
  const std::vector<fs::path> pendingPaths = m_pendingProjectReplacementPaths;
  clearPendingProjectReplacement();

  switch (action) {
    case GuiData::UnsavedProjectAction::CloseProject:
      closeProject();
      return;
    case GuiData::UnsavedProjectAction::OpenImages:
      closeProject();
      performLoadImageFiles(pendingPaths);
      return;
    case GuiData::UnsavedProjectAction::OpenDicomSeries:
      closeProject();
      performOpenDicomSeriesFolders(pendingPaths);
      return;
    case GuiData::UnsavedProjectAction::OpenProject:
      if (!pendingPaths.empty()) {
        performLoadProjectFile(pendingPaths.front());
      }
      return;
    case GuiData::UnsavedProjectAction::QuitApp:
      quitAppWithoutPrompt();
      return;
  }
}

void EntropyApp::closeProject()
{
  m_imageLoadCancelled = true;

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
  m_preserveLayoutsOnImagesReady = false;
  m_pendingAddedImageUids.clear();
  m_pendingLayoutsFile = std::nullopt;
  m_pendingLargeImageLoadContext = LargeImageLoadContext::None;
  m_pendingLargeAddImageFile = std::nullopt;
  m_pendingLargeProject = std::nullopt;
  m_pendingLargeProjectFileName = std::nullopt;
  m_pendingLargeProjectImageIndex = 0;
  m_savedProjectSnapshot = std::nullopt;
  clearPendingProjectReplacement();
  m_data.guiData().m_pendingLargeImageLoadPrompt = std::nullopt;
  m_data.guiData().m_dicomSeriesScanInProgress = false;
  m_data.guiData().m_pendingDicomScanRoot = fs::path{};
  m_data.guiData().m_pendingDicomSeriesSelectionPrompt = std::nullopt;
  m_data.guiData().m_showDicomSeriesSelectionPopup = false;
  m_data.guiData().m_showUnsavedProjectPopup = false;
  m_data.guiData().m_showConfirmCloseAppPopup = false;
  m_data.guiData().m_showLargeImageLoadPrompt = false;

  auto& renderData = m_data.renderData();
  renderData.m_imageTextures.clear();
  renderData.m_distanceMapTextures.clear();
  renderData.m_segTextures.clear();
  renderData.m_labelBufferTextures.clear();
  renderData.m_colormapTextures.clear();
  renderData.m_uniforms.clear();

  m_data.clearProjectData();
  m_glfw.setWindowTitleStatus("");
  m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
  m_glfw.postEmptyEvent();
}

void EntropyApp::loadImagesFromParams(const InputParams& params)
{
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
    });
}

void EntropyApp::startAsyncImageLoad(
  std::string windowTitleStatus,
  std::function<bool()> loadTask,
  std::function<void()> onLoadFailed,
  bool showLoadingOverlay)
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

void EntropyApp::setCallbacks()
{
  m_glfw.setCallbacks(
    [this](std::chrono::time_point<std::chrono::steady_clock>& lastFrameTime) {
      m_rendering.framerateLimiter(lastFrameTime);
    },
    [this]() { m_rendering.render(); },
    [this]() { m_imgui.render(); },
    [this]() {
      pollDicomSeriesScan();
      m_itkSnapSync.update();
      m_entropyInstanceSync.update();
      const bool syncEnabled = m_data.settings().cursorSyncEnabled() || m_data.settings().entropyInstanceSyncEnabled();
      if (syncEnabled && !m_data.state().animating()) {
        m_glfw.setEventProcessingMode(EventProcessingMode::WaitTimeout);
        m_glfw.setWaitTimeout(1.0 / 30.0);
      }
      else if (!m_data.state().animating()) {
        m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
      }
    });

  ImGuiWrapperCallbacks imguiCallbacks;

  imguiCallbacks.platform.postEmptyGlfwEvent = [this]() {
    m_glfw.postEmptyEvent();
  };
  imguiCallbacks.platform.readjustViewport = [this]() {
    resize(m_data.windowData().getWindowSize().x, m_data.windowData().getWindowSize().y);
  };

  imguiCallbacks.project.openImageFiles = [this](const std::vector<fs::path>& fileNames) {
    loadImageFiles(fileNames);
  };
  imguiCallbacks.project.addImageFiles = [this](const std::vector<fs::path>& fileNames) {
    addImageFiles(fileNames);
  };
  imguiCallbacks.project.openDicomFolders = [this](const std::vector<fs::path>& folderNames) {
    openDicomSeriesFolders(folderNames);
  };
  imguiCallbacks.project.addSegmentationFile = [this](const fs::path& fileName) {
    addSegmentationFile(fileName);
  };
  imguiCallbacks.project.addSegmentationFileToImage = [this](const uuids::uuid& imageUid, const fs::path& fileName) {
    addSegmentationFileToImage(fileName, imageUid);
  };
  imguiCallbacks.project.openProjectFile = [this](const fs::path& fileName) {
    loadProjectFile(fileName);
  };
  imguiCallbacks.project.largeImageLoadDecision = [this](GuiData::LargeImageLoadDecision decision) {
    handleLargeImageLoadDecision(decision);
  };
  imguiCallbacks.project.loadDicomSeries = [this](
                                             const std::vector<dicom::SeriesInfo>& series,
                                             std::optional<std::size_t> referenceSeriesIndex,
                                             bool addToExistingProject) {
    loadDicomSeries(series, referenceSeriesIndex, addToExistingProject);
  };
  imguiCallbacks.project.saveProject = [this]() {
    return saveProject();
  };
  imguiCallbacks.project.saveProjectAs = [this](const fs::path& fileName) {
    return saveProjectAs(fileName);
  };
  imguiCallbacks.project.closeProject = [this]() {
    requestCloseProject();
  };
  imguiCallbacks.project.loadLayoutsFile = [this](const fs::path& fileName) {
    loadLayoutsFile(fileName);
  };
  imguiCallbacks.project.saveLayoutsFile = [this](const fs::path& fileName) {
    return saveLayoutsFile(fileName);
  };
  imguiCallbacks.project.closeProjectWithoutPrompt = [this]() {
    continueAfterUnsavedProjectPrompt();
  };
  imguiCallbacks.project.requestQuitApp = [this]() {
    requestQuitApp();
  };
  imguiCallbacks.project.quitAppWithoutPrompt = [this]() {
    quitAppWithoutPrompt();
  };

  imguiCallbacks.view.recenterView = [this](const uuids::uuid& viewUid) {
    m_callbackHandler.recenterView(m_data.state().recenteringMode(), viewUid);
  };

  imguiCallbacks.view.recenterCurrentViews = [this](
                                               bool recenterCrosshairs,
                                               bool realignCrosshairs,
                                               bool recenterOnCurrentCrosshairsPosition,
                                               bool resetObliqueOrientation,
                                               bool resetZoom) {
    m_callbackHandler.recenterViews(
      m_data.state().recenteringMode(),
      recenterCrosshairs,
      realignCrosshairs,
      recenterOnCurrentCrosshairsPosition,
      resetObliqueOrientation,
      resetZoom);
  };

  imguiCallbacks.view.getOverlayVisibility = [this]() {
    return m_callbackHandler.showOverlays();
  };
  imguiCallbacks.view.setOverlayVisibility = [this](bool show) {
    m_callbackHandler.setShowOverlays(show);
  };
  imguiCallbacks.view.updateAllImageUniforms = [this]() {
    m_rendering.updateImageUniforms(m_data.imageUidsOrdered());
  };
  imguiCallbacks.view.updateImageUniforms = [this](const uuids::uuid& imageUid) {
    m_rendering.updateImageUniforms(imageUid);
  };
  imguiCallbacks.view.updateImageInterpolationMode = [this](const uuids::uuid& imageUid) {
    m_rendering.updateImageInterpolation(imageUid);
  };
  imguiCallbacks.view.updateImageColorMapInterpolationMode = [this](std::size_t cmapIndex) {
    m_rendering.updateImageColorMapInterpolation(cmapIndex);
  };
  imguiCallbacks.view.updateLabelColorTableTexture = [this](size_t labelColorTableIndex) {
    m_rendering.updateLabelColorTableTexture(labelColorTableIndex);
  };

  imguiCallbacks.view.moveCrosshairsToSegLabelCentroid = [this](const uuids::uuid& imageUid, size_t labelIndex) {
    m_callbackHandler.moveCrosshairsToSegLabelCentroid(imageUid, labelIndex);
  };

  imguiCallbacks.view.updateMetricUniforms = [this]() {
    m_rendering.updateMetricUniforms();
  };
  imguiCallbacks.inspection.getWorldDeformedPos = [this]() {
    return m_data.state().worldCrosshairs().worldOrigin();
  };

  imguiCallbacks.inspection.getSubjectPos = [this](size_t imageIndex) -> std::optional<glm::vec3> {
    const auto imageUid = m_data.imageUid(imageIndex);
    const Image* image = imageUid ? m_data.image(*imageUid) : nullptr;
    if (!image) {
      return std::nullopt;
    }

    const glm::vec4 subjectPos =
      image->transformations().subject_T_worldDef() * glm::vec4{m_data.state().worldCrosshairs().worldOrigin(), 1.0f};

    return glm::vec3{subjectPos / subjectPos.w};
  };

  imguiCallbacks.inspection.getVoxelPos = [this](size_t imageIndex) {
    return data::getImageVoxelCoordsAtCrosshairs(m_data, imageIndex);
  };

  imguiCallbacks.inspection.setSubjectPos = [this](size_t imageIndex, const glm::vec3& subjectPos) {
    const auto imageUid = m_data.imageUid(imageIndex);
    const Image* image = imageUid ? m_data.image(*imageUid) : nullptr;
    if (!image) {
      return;
    }

    const glm::vec4 worldPos = image->transformations().worldDef_T_subject() * glm::vec4{subjectPos, 1.0f};

    m_data.state().setWorldCrosshairsPos(glm::vec3{worldPos / worldPos.w});
  };

  imguiCallbacks.inspection.setVoxelPos = [this](size_t imageIndex, const glm::ivec3& voxelPos) {
    const auto imageUid = m_data.imageUid(imageIndex);
    const Image* image = imageUid ? m_data.image(*imageUid) : nullptr;
    if (!image) {
      return;
    }

    // TODO: Put this in CallbackHandler as separate function, because it is used frequently.
    // TODO: All logic related to rounding crosshairs positions should be in one place.

    const glm::vec4 worldPos = image->transformations().worldDef_T_pixel() * glm::vec4{voxelPos, 1.0f};
    const glm::vec3 worldPosRounded =
      data::roundPointToNearestImageVoxelCenter(*image, glm::vec3{worldPos / worldPos.w});

    m_data.state().setWorldCrosshairsPos(worldPosRounded);
  };

  imguiCallbacks.inspection.getImageValuesNN =
    [this](size_t imageIndex, bool getOnlyActiveComponent) -> std::vector<double> {
    std::vector<double> values;

    const auto imageUid = m_data.imageUid(imageIndex);
    const Image* image = imageUid ? m_data.image(*imageUid) : nullptr;
    if (!image) {
      return values;
    }

    if (const auto coords = data::getImageVoxelCoordsAtCrosshairs(m_data, imageIndex)) {
      if (getOnlyActiveComponent) {
        if (const auto a = image->value<double>(image->settings().activeComponent(), coords->x, coords->y, coords->z)) {
          // Return empty vector if component has undefined value
          values.push_back(*a);
        }
        else {
          return std::vector<double>{};
        }
      }
      else {
        for (uint32_t i = 0; i < image->header().numComponentsPerPixel(); ++i) {
          if (const auto a = image->value<double>(i, coords->x, coords->y, coords->z)) {
            values.push_back(*a);
          }
          else {
            // Return empty vector if any component has undefined value
            return std::vector<double>{};
          }
        }
      }
    }

    return values;
  };

  imguiCallbacks.inspection.getImageValuesLinear =
    [this](size_t imageIndex, bool getOnlyActiveComponent) -> std::vector<double> {
    std::vector<double> values;

    const auto imageUid = m_data.imageUid(imageIndex);
    const Image* image = imageUid ? m_data.image(*imageUid) : nullptr;
    if (!image) {
      return values;
    }

    if (const auto coords = data::getImageVoxelCoordsContinuousAtCrosshairs(m_data, imageIndex)) {
      if (getOnlyActiveComponent) {
        if (
          const auto a =
            image->valueLinear<double>(image->settings().activeComponent(), coords->x, coords->y, coords->z))
        {
          // Return empty vector if component has undefined value
          values.push_back(*a);
        }
        else {
          return std::vector<double>{};
        }
      }
      else {
        for (uint32_t i = 0; i < image->header().numComponentsPerPixel(); ++i) {
          if (const auto a = image->valueLinear<double>(i, coords->x, coords->y, coords->z)) {
            values.push_back(*a);
          }
          else {
            // Return empty vector if any component has undefined value
            return std::vector<double>{};
          }
        }
      }
    }

    return values;
  };

  imguiCallbacks.inspection.getSegLabel = [this](size_t imageIndex) -> std::optional<int64_t> {
    const auto imageUid = m_data.imageUid(imageIndex);
    if (!imageUid) {
      return std::nullopt;
    }

    const auto segUid = m_data.imageToActiveSegUid(*imageUid);
    const Image* seg = segUid ? m_data.seg(*segUid) : nullptr;
    if (!seg) {
      return std::nullopt;
    }

    if (const auto coords = data::getSegVoxelCoordsAtCrosshairs(m_data, *segUid, *imageUid)) {
      const uint32_t activeComp = seg->settings().activeComponent();
      return seg->value<int64_t>(activeComp, coords->x, coords->y, coords->z);
    }

    return std::nullopt;
  };

  imguiCallbacks.editing.createBlankSeg =
    [this](const uuids::uuid& matchingImageUid, const std::string& segDisplayName) {
      return m_callbackHandler.createBlankSegWithColorTableAndTextures(matchingImageUid, segDisplayName);
    };

  imguiCallbacks.editing.clearSeg = [this](const uuids::uuid& segUid) -> bool {
    return m_callbackHandler.clearSegVoxels(segUid);
  };

  imguiCallbacks.editing.removeSeg = [this](const uuids::uuid& segUid) -> bool {
    bool success = false;
    success |= m_data.removeSeg(segUid);
    success |= m_rendering.removeSegTexture(segUid);
    return success;
  };

  imguiCallbacks.editing.executePoissonSeg =
    [this](const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType& segType) -> bool {
    return m_callbackHandler.executePoissonSegmentation(imageUid, seedSegUid, segType);
  };

  imguiCallbacks.editing.setLockManualImageTransformation = [this](const uuids::uuid& imageUid, bool locked) -> bool {
    return m_callbackHandler.setLockManualImageTransformation(imageUid, locked);
  };

  imguiCallbacks.editing.setReferenceImage = [this](const uuids::uuid& imageUid) -> bool {
    return setReferenceImage(imageUid);
  };

  imguiCallbacks.editing.removeImage = [this](const uuids::uuid& imageUid) -> bool {
    return removeImage(imageUid);
  };

  imguiCallbacks.editing.paintActiveSegmentationWithActivePolygon = [this]() {
    return m_callbackHandler.paintActiveSegmentationWithAnnotation();
  };

  m_imgui.setCallbacks(std::move(imguiCallbacks));
}

// void EntropyApp::broadcastCrosshairsPosition()
//{
//     // We need a reference image to move the crosshairs
//     const Image* refImg = m_data.refImage();
//     if ( ! refImg ) return;

//    const auto& refTx = refImg->transformations();

//    // Convert World to reference Subject position:
//    glm::vec4 refSubjectPos = refTx.subject_T_world() *
//        glm::vec4{ m_data.state().worldCrosshairs().worldOrigin(), 1.0f };

//    refSubjectPos /= refSubjectPos.w;

//    // Read the contents of shared memory into the local message object
//    IPCMessage message;
//    m_IPCHandler.Read( static_cast<void*>( &message ) );

//    spdlog::info( "cursor = {}, {}, {}", message.cursor[0], message.cursor[1], message.cursor[2]
//);

//    // Convert LPS to RAS
//    message.cursor[0] = -refSubjectPos[0];
//    message.cursor[1] = -refSubjectPos[1];
//    message.cursor[2] = refSubjectPos[2];

//    if ( ! m_IPCHandler.Broadcast( static_cast<void*>( &message ) ) )
//    {
//        spdlog::warn( "Failed to broadcast IPC" );
//    }
//}
