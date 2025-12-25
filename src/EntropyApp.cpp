#include "EntropyApp.h"
#include "BuildStamp.h"

#include "common/DataHelper.h"
#include "common/DirectionMaps.h"
#include "common/Exception.hpp"
#include "common/MathFuncs.h"

#include "image/ImageUtility.h"
#include "image/ImageUtility.tpp"

#include "logic/annotation/Annotation.h"
#include "logic/annotation/LandmarkGroup.h"
#include "logic/camera/MathUtility.h"
#include "logic/serialization/ProjectSerialization.h"
#include "logic/states/FsmList.hpp"
#include "logic/DistanceMap.h"

//#include "logic/ipc/IPCMessage.h"

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>
#include <glm/gtx/string_cast.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>

// Without undefining min and max, there are some errors compiling in Visual Studio
#undef min
#undef max

namespace
{
bool promptForChar(const char* prompt, char& readch)
{
  std::string tmp;
  std::cout << prompt << std::endl;

  if (std::getline(std::cin, tmp))
  {
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
} // namespace

EntropyApp::EntropyApp()
  : m_imageLoadCancelled(false)
  , m_imagesReady(false)
  , m_imageLoadFailed(false)
  , m_glfw(this, GL_VERSION_MAJOR, GL_VERSION_MINOR) // GLFW creates the OpenGL contex
  , m_data()
  , m_rendering(m_data) // Requires OpenGL context
  , m_callbackHandler(m_data, m_glfw, m_rendering) // Requires OpenGL context
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
  m_futureLoadProject.wait();

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

  m_rendering.init();
  m_glfw.init(); // Trigger initial windowing callbacks

  spdlog::debug("Done initializing application");
}

void EntropyApp::run()
{
  spdlog::debug("Begin application run loop");

  auto checkIfAppShouldQuit = [this]() { return m_data.state().quitApp(); };

  m_glfw.renderLoop(m_imagesReady, m_imageLoadFailed, checkIfAppShouldQuit,
                    [this]() { onImagesReady(); });

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

  spdlog::debug("Images are loaded.");

  const Image* refImg = m_data.refImage();
  if (!refImg) {
    // At a minimum, we need a reference image to do anything.
    // If the reference image is null, then image loading has failed.
    spdlog::critical("The reference image is null");
    throw_debug("The reference image is null")
  }

  m_rendering.initTextures();
  m_rendering.updateImageUniforms(m_data.imageUidsOrdered());

  spdlog::debug("Textures and uniforms ready; rendering enabled");

  // Stop animation rendering (which plays during loading) and render only on events:
  m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
  m_glfw.setWindowTitleStatus(m_data.getAllImageDisplayNames());

  m_data.state().setAnimating(false);
  m_data.settings().setOverlays(true);

  m_data.guiData().m_renderUiWindows = true;
  m_data.guiData().m_renderUiOverlays = true;

  spdlog::debug("Begin setting up window state");

  // Prepare view layouts:
  if (1 < m_data.numImages())
  {
    // Add a new layout with one row and a different image in each column:
    static constexpr bool sk_offsetViews = false;
    static constexpr bool sk_isLightbox = false;

    if (const auto& refUid = m_data.refImageUid())
    {
      m_data.windowData().addGridLayout(ViewType::Axial, m_data.numImages(), 1,
                                        sk_offsetViews, sk_isLightbox, 0, *refUid);
    }
  }

  // Add axial, coronal, sagittal layout, with one row for each image:
  m_data.windowData().addAxCorSagLayout(m_data.numImages());

  // Create axial, coronal, sagittal lightbox layouts for all images:
  std::size_t imageIndex = 0;

  for (const auto& imageUid : m_data.imageUidsOrdered())
  {
    const Image* image = m_data.image(imageUid);
    if (!image) {
      continue;
    }

    m_data.windowData().addLightboxLayoutForImage(
      ViewType::Axial, data::computeNumImageSlicesAlongWorldDirection(
        *image, Directions::get(Directions::Anatomy::Inferior)), imageIndex, imageUid);

    m_data.windowData().addLightboxLayoutForImage(
      ViewType::Coronal, data::computeNumImageSlicesAlongWorldDirection(
        *image, Directions::get(Directions::Anatomy::Anterior)), imageIndex, imageUid);

    m_data.windowData().addLightboxLayoutForImage(
      ViewType::Sagittal, data::computeNumImageSlicesAlongWorldDirection(
        *image, Directions::get(Directions::Anatomy::Right)), imageIndex, imageUid);

    ++imageIndex;
  }

  m_data.windowData().setDefaultRenderedImagesForAllLayouts(m_data.imageUidsOrdered());

  m_callbackHandler.recenterViews(
    m_data.state().recenteringMode(), recenterCrosshairs, realignCrosshairs,
    doNotRecenterOnCurrentCrosshairsPos, resetObliqueOrientation, resetZoom);

  m_callbackHandler.setMouseMode(MouseMode::Pointer);

  // Trigger two UI renders in order to freshen up its internal state.
  // Without both render calls, the UI state is not correctly set up.
  m_imgui.render();
  m_imgui.render();

  // Trigger a resize in order to correctly set the viewport, since UI
  // state changes in the render call:
  resize(m_data.windowData().getWindowSize().x, m_data.windowData().getWindowSize().y);

  spdlog::debug("Done setting up window state");
}

void EntropyApp::resize(int windowWidth, int windowHeight)
{
  const GuiData::Margins margins = guiData().computeMargins();

  // This call sets the window size and viewport
  // app->resize( windowWidth, windowHeight );
  windowData().setWindowSize(windowWidth, windowHeight);

  // Set viewport to account for margins
  windowData().setViewport(
    margins.left, margins.bottom,
    static_cast<float>(windowWidth) - (margins.left + margins.right),
    static_cast<float>(windowHeight) - (margins.bottom + margins.top));
}

void EntropyApp::render()
{
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
  spdlog::info("{}\n", ORG_NAME_1, ORG_NAME_2);

  spdlog::debug("Git branch: {}", GIT_BRANCH);
  spdlog::debug("Git commit hash: {}", GIT_COMMIT_SHA1);
  spdlog::debug("Git commit timestamp: {}", GIT_COMMIT_TIMESTAMP);
  spdlog::debug("Build timestamp: {}", BUILD_TIMESTAMP);
  spdlog::debug("Build type: {}", CMAKE_BUILD_TYPE);
}

std::pair<std::optional<uuids::uuid>, bool> EntropyApp::loadImage(
  const fs::path& fileName, bool ignoreIfAlreadyLoaded)
{
  if (ignoreIfAlreadyLoaded)
  {
    // Has this image already been loaded? Search for its file name:
    for (const auto& imageUid : m_data.imageUidsOrdered())
    {
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

  Image image(fileName, Image::ImageRepresentation::Image, Image::MultiComponentBufferType::SeparateImages);

  spdlog::info("Read image from file {}", fileName);

  std::ostringstream ss;
  image.metaData(ss);

  spdlog::trace("Meta data:\n{}", ss.str());
  spdlog::info("Header:\n{}", image.header());
  spdlog::info("Transformation:\n{}", image.transformations());
  spdlog::info("Settings:\n{}", image.settings());

  return {m_data.addImage(std::move(image)), true};
}

std::pair<std::optional<uuids::uuid>, bool> EntropyApp::loadSegmentation(
  const fs::path& fileName, const std::optional<uuids::uuid>& matchingImageUid)
{
  // Setting indicating that the same segmentation image file can be loaded twice:
  constexpr bool canLoadSameSegFileTwice = false;
  constexpr auto EPS = glm::epsilon<float>();

  // Return value indicating that segmentation was not loaded:
  static const std::pair<std::optional<uuids::uuid>, bool> noSegLoaded{std::nullopt, false};

  // Has this segmentation already been loaded? Search for its file name:
  for (const auto& segUid : m_data.segUidsOrdered())
  {
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
  Image seg(fileName, Image::ImageRepresentation::Segmentation,
            Image::MultiComponentBufferType::SeparateImages);

  // Set the default opacity:
  seg.settings().setOpacity(0.5);

  spdlog::info("Read segmentation image from file {}", fileName);

  std::ostringstream ss;
  seg.metaData(ss);

  spdlog::trace("Meta data:\n{}", ss.str());
  spdlog::info("Header:\n{}", seg.header());
  spdlog::info("Transformation:\n{}", seg.transformations());

  const Image* matchImg = (matchingImageUid) ? m_data.image(*matchingImageUid) : nullptr;

  if (!matchImg)
  {
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

  /// @todo Is there a better way to handle non-matching matrices?
  /// Perhaps there should be a setting in the project file that allows us to override
  /// the segmentation's subject_T_texture matrix with the matrix of the image

  if (!math::areMatricesEqual(imgTx.subject_T_texture(), segTx.subject_T_texture()))
  {
    spdlog::warn("The subject_T_texture transformations for image {} "
                 "and segmentation from file {} do not match:",
                 *matchingImageUid, fileName);

    spdlog::info("subject_T_texture matrix for image:\n{}", glm::to_string(imgTx.subject_T_texture()));
    spdlog::info("subject_T_texture matrix for segmentation:\n{}", glm::to_string(segTx.subject_T_texture()));

    const auto& imgHdr = matchImg->header();
    const auto& segHdr = seg.header();

    if (glm::any(glm::epsilonNotEqual(imgHdr.origin(), segHdr.origin(), EPS))) {
      spdlog::warn("The origins of image ({}) and segmentation ({}) do not match",
                   glm::to_string(imgHdr.origin()), glm::to_string(segHdr.origin()));
    }

    if (glm::any(glm::epsilonNotEqual(imgHdr.spacing(), segHdr.spacing(), EPS))) {
      spdlog::warn("The voxel spacings of image ({}) and segmentation ({}) do not match",
                   glm::to_string(imgHdr.spacing()), glm::to_string(segHdr.spacing()));
    }

    if (!math::areMatricesEqual(imgHdr.directions(), segHdr.directions())) {
      spdlog::warn("The direction vectors of image ({}) and segmentation ({}) do not match",
                   glm::to_string(imgHdr.directions()), glm::to_string(segHdr.directions()));
    }

    if (imgHdr.pixelDimensions() != segHdr.pixelDimensions()) {
      spdlog::warn("The pixel dimensions of image ({}) and segmentation ({}) do not match",
                   glm::to_string(imgHdr.pixelDimensions()), glm::to_string(segHdr.pixelDimensions()));
    }

    char type = '\0';

    while (
      promptForChar("\nContinue loading the segmentation despite transformation mismatch? [y/n]", type)
    )
    {
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
    spdlog::error("The segmentation from file {} does not have unsigned integer pixel "
                  "component type and so will not be loaded.", fileName);
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

std::pair<std::optional<uuids::uuid>, bool>
EntropyApp::loadDeformationField(const fs::path& fileName)
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
    spdlog::error("The deformation field from file {} has fewer than three components per pixel "
                  "and so will not be loaded.", fileName);
    return noDefLoaded;
  }

  spdlog::info("Read deformation field image from file {}", fileName);

  std::ostringstream ss;
  def.metaData(ss);

  spdlog::trace("Meta data:\n{}", ss.str());
  spdlog::info("Header:\n{}", def.header());
  spdlog::info("Transformation:\n{}", def.transformations());
  spdlog::info("Settings:\n{}", def.settings());

  /// @todo Do check of deformation field header against the reference image header?

  if (const auto defUid = m_data.addDef(std::move(def))) {
    spdlog::info("Loaded deformation field image from file {} as {}", fileName, *defUid);
    return {*defUid, true};
  }

  return noDefLoaded;
}

bool EntropyApp::loadSerializedImage(const serialize::Image& serializedImage, bool isReferenceImage)
{
  constexpr size_t defaultImageColorMapIndex = 0;

  // Do NOT ignore images if they have already been loaded:
  // (i.e. load duplicate images again anyway):
  constexpr bool ignoreImageIfAlreadyLoaded = false;

  // Load image:
  std::optional<uuids::uuid> imageUid;
  bool isNewImage = false;

  try {
    spdlog::debug("Attempting to load image from {}", serializedImage.m_imageFileName);
    std::tie(imageUid, isNewImage) = loadImage(serializedImage.m_imageFileName, ignoreImageIfAlreadyLoaded);
  }
  catch (const std::exception& e) {
    spdlog::error("Exception loading image from {}: {}", serializedImage.m_imageFileName, e.what());
    return false;
  }

  if (!imageUid) {
    spdlog::error("Unable to load image from {}", serializedImage.m_imageFileName);
    return false;
  }

  if (!isNewImage)
  {
    spdlog::info("Image from {} already exists in this project as {}", serializedImage.m_imageFileName, *imageUid);

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

  spdlog::info("Loaded image from {} as {}", serializedImage.m_imageFileName, *imageUid);

  // Disable the initial affine and manual transformations for the reference image:
  image->transformations().set_enable_worldDef_T_affine(isReferenceImage ? false : true);
  image->transformations().set_enable_affine_T_subject(isReferenceImage ? false : true);

  // Lock all affine transformations to the reference image, which defines the World space:
  image->transformations().set_worldDef_T_affine_locked(true);

  // Load and set affine transformation from file (for non-reference images only):
  if (serializedImage.m_affineTxFileName)
  {
    glm::dmat4 affine_T_subject(1.0);

    if (isReferenceImage) {
      spdlog::warn("An affine transformation file ({}) was provided for the reference image. "
                   "It will be ignored, since the reference image defines the World coordinate "
                   "space, which cannot be transformed.", *serializedImage.m_affineTxFileName);

      image->transformations().set_affine_T_subject_fileName(std::nullopt);
    }
    else
    {
      if (!serialize::openAffineTxFile(affine_T_subject, *serializedImage.m_affineTxFileName)) {
        spdlog::error("Unable to read affine transformation from {} for image {}",
                      *serializedImage.m_affineTxFileName, *imageUid);

        image->transformations().set_affine_T_subject_fileName(std::nullopt);
      }

      image->transformations().set_affine_T_subject_fileName(serializedImage.m_affineTxFileName);
      image->transformations().set_affine_T_subject(glm::mat4{affine_T_subject});
    }
  }
  else
  {
    // No affine transformation provided:
    image->transformations().set_affine_T_subject_fileName(std::nullopt);
  }

  //    if ( serializedImage.m_deformationFileName && isReferenceImage )
  //    {
  //        spdlog::warn( "A deformable transformation file ({}) was provided for the reference image. "
  //                      "It will be ignored, since the reference image defines the World coordinate "
  //                      "space, which cannot be transformed.", *serializedImage.m_deformationFileName );
  //    }
  //    else
  if (serializedImage.m_deformationFileName)
  {
    std::optional<uuids::uuid> deformationUid;
    bool isDeformationNewImage = false;

    try {
      spdlog::debug("Attempting to load deformation field image from {}", *serializedImage.m_deformationFileName);
      std::tie(deformationUid, isDeformationNewImage) = loadDeformationField(*serializedImage.m_deformationFileName);
    }
    catch (const std::exception& e) {
      spdlog::error("Exception loading deformation field from {}: {}",
                    *serializedImage.m_deformationFileName, e.what());
    }

    do
    {
      if (!deformationUid) {
        spdlog::error("Unable to load deformation field from {} for image {}",
                      *serializedImage.m_deformationFileName, *imageUid);
        break;
      }

      if (!isDeformationNewImage) {
        spdlog::info("Deformation field from {} already exists in this project as image {}",
                     *serializedImage.m_deformationFileName, *deformationUid);
        break;
      }

      Image* deformation = m_data.def(*deformationUid);

      if (!deformation) {
        spdlog::error("Null deformation field image {}", *deformationUid);
        break;
      }

      deformation->settings().setDisplayName(deformation->settings().displayName() + " (deformation)");

      /// @todo Load this from project settings
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

    /// @todo Deformation field images are special:
    /// 1) no segmentation is created
    /// 2) no affine transformation can be applied: it copies the affine tx of its image
    /// 3) need warning when header tx doesn't match that of reference
    /// 4) even if all components are loaded as RGB texure, we should be able to view each component separately in a shader that takes
    /// in as a uniform the active component
  }

  // Set annotations from file:
  if (serializedImage.m_annotationsFileName)
  {
    std::vector<Annotation> annots;

    if (serialize::openAnnotationsFromJsonFile(annots, *serializedImage.m_annotationsFileName))
    {
      spdlog::info("Loaded annotations from JSON file {} for image {}",
                   *serializedImage.m_annotationsFileName, *imageUid);

      for (auto& annot : annots)
      {
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
      spdlog::error("Unable to open annotations from JSON file {} for image {}",
                    *serializedImage.m_annotationsFileName, *imageUid);
    }
  }

  const auto hueMinMax = std::make_pair(0.0f, 360.0f);
  const auto satMinMax = std::make_pair(0.6f, 1.0f);
  const auto valMinMax = std::make_pair(0.6f, 1.0f);

  // Set landmarks from file:
  for (const auto& lm : serializedImage.m_landmarkGroups)
  {
    std::map<size_t, PointRecord<glm::vec3> > landmarks;

    if (serialize::openLandmarkGroupCsvFile(landmarks, lm.m_csvFileName))
    {
      spdlog::info("Loaded landmarks from CSV file {} for image {}", lm.m_csvFileName, *imageUid);

      // Assign random colors to the landmarks. Make sure that landmarks with the same index
      // in different groups have the same color. This is done by seeding the random number
      // generator with the landmark index.
      for (auto& p : landmarks)
      {
        const auto colors = math::generateRandomHsvSamples(
          1, hueMinMax, satMinMax, valMinMax, static_cast<uint32_t>(p.first));

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

      for (const auto& p : landmarks) {
        spdlog::trace("Landmark {} ('{}') : {}", p.first, p.second.getName(),
                      glm::to_string(p.second.getPosition()));
      }

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
    std::optional<uuids::uuid> uid; // Seg UID assigned by AppData after the image is loaded from disk
    bool isNewSeg = false; // Is this a new segmentation (true) or a repeat of a previous one (false)?
    bool needsNewLabelColorTable = true; // Does the segmentation need a new label color table?
  };

  // Holds info about all segmentations being loaded:
  std::vector<SegInfo> allSegInfos;

  for (const auto& serializedSeg : serializedImage.m_segmentations)
  {
    SegInfo segInfo;

    try {
      spdlog::debug("Attempting to load segmentation image from {}", serializedSeg.m_segFileName);
      std::tie(segInfo.uid, segInfo.isNewSeg) = loadSegmentation(serializedSeg.m_segFileName, *imageUid);
    }
    catch (const std::exception& e) {
      spdlog::error("Exception loading segmentation from {}: {}", serializedSeg.m_segFileName, e.what());
      continue; // Skip this segmentation
    }

    if (segInfo.uid)
    {
      if (segInfo.isNewSeg) {
        spdlog::info("Loaded segmentation from file {} for image {} as {}",
                     serializedSeg.m_segFileName, *imageUid, *segInfo.uid);

        // New segmentation needs a new table
        segInfo.needsNewLabelColorTable = true;
      }
      else {
        spdlog::info("Segmentation from {} already exists as {}, so it was not loaded again. "
                     "This segmentation will be shared across all images that reference it.",
                     serializedSeg.m_segFileName, *segInfo.uid);

        // Existing segmentation does not need a new table
        segInfo.needsNewLabelColorTable = false;
      }

      allSegInfos.push_back(segInfo);
    }
  }

  if (allSegInfos.empty())
  {
    // No segmentation was loaded!
    spdlog::debug("No segmentation loaded for image {}; creating blank segmentation.", *imageUid);

    try
    {
      const std::string segDisplayName =
        std::string("Untitled segmentation for image '") + image->settings().displayName() + "'";

      SegInfo segInfo;
      segInfo.uid = m_callbackHandler.createBlankSeg(*imageUid, segDisplayName);
      segInfo.isNewSeg = true;
      segInfo.needsNewLabelColorTable = true;

      if (segInfo.uid) {
        spdlog::debug("Created blank segmentation {} ('{}') for image {}",
                      *segInfo.uid, segDisplayName, *imageUid);
      }
      else {
        // This is a problem that we can't recover from:
        spdlog::error("Error creating blank segmentation for image {}. "
                      "No segmentation will be assigned to the image.", *imageUid);
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

  /// @todo Put all this into the loadSeg function?
  for (const SegInfo& segInfo : allSegInfos)
  {
    Image* seg = m_data.seg(*segInfo.uid);

    if (!seg) {
      spdlog::error("Null segmentation {}", *segInfo.uid);
      m_data.removeSeg(*segInfo.uid);
      continue;
    }

    if (segInfo.needsNewLabelColorTable)
    {
      if (!data::createLabelColorTableForSegmentation(m_data, *segInfo.uid))
      {
        constexpr size_t defaultTableIndex = 0;

        spdlog::error("Unable to create label color table for segmentation {}. "
                      "Defaulting to table index {}.", *segInfo.uid, defaultTableIndex);

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

  /// @todo Load from project settings
  for (uint32_t i = 0; i < image->header().numComponentsPerPixel(); ++i) {
    image->settings().setColorMapIndex(i, defaultImageColorMapIndex);
  }

  return true;
}

void EntropyApp::loadImagesFromParams(const InputParams& params)
{
  spdlog::debug("Begin loading images from parameters");

  // The image loader function is called from a new thread
  auto projectLoader =
    [this](const serialize::EntropyProject& project,
           const std::function<void(bool projectLoadedSuccessfully)>& onProjectLoadingDone)
  {
    static constexpr size_t defaultReferenceImageIndex = 0;
    static constexpr size_t defaultActiveImageIndex = 1;

    // Set event processing mode to poll, so that we have continuous animation while loading
    m_glfw.setEventProcessingMode(EventProcessingMode::Poll);
    m_data.state().setAnimating(true);

    spdlog::debug("Begin loading images in new thread");

    if (m_imageLoadCancelled) {
      onProjectLoadingDone(false);
    }

    if (!loadSerializedImage(project.m_referenceImage, true))
    {
      spdlog::critical("Could not load reference image from {}", project.m_referenceImage.m_imageFileName);
      onProjectLoadingDone(false);
    }

    if (m_imageLoadCancelled) {
      onProjectLoadingDone(false);
    }

    for (const auto& additionalImage : project.m_additionalImages)
    {
      if (!loadSerializedImage(additionalImage, false)) {
        spdlog::error("Could not load additional image from {}; skipping it", additionalImage.m_imageFileName);
      }

      if (m_imageLoadCancelled) {
        onProjectLoadingDone(false);
      }
    }

    const auto refImageUid = m_data.imageUid(defaultReferenceImageIndex);

    if (refImageUid && m_data.setRefImageUid(*refImageUid)) {
      spdlog::info("Set {} as the reference image", *refImageUid);
    }
    else {
      spdlog::critical("Unable to set {} as the reference image", *refImageUid);
      onProjectLoadingDone(false);
    }

    const auto desiredActiveImageUid = (defaultActiveImageIndex < m_data.numImages())
      ? m_data.imageUid(defaultActiveImageIndex) : *refImageUid;

    if (desiredActiveImageUid && m_data.setActiveImageUid(*desiredActiveImageUid)) {
      spdlog::info("Set {} as the active image", *desiredActiveImageUid);
    }
    else {
      spdlog::error("Unable to set {} as the active image", *desiredActiveImageUid);
    }

    // Assign nice rainbow colors:
    m_data.setRainbowColorsForAllImages();
    m_data.setRainbowColorsForAllLandmarkGroups();

    // Show the tri-view layout:
    m_data.windowData().setCurrentLayoutIndex(1);

    onProjectLoadingDone(true);
  };

  auto onProjectLoadingDone = [this](bool projectLoadedSuccessfully)
  {
    if (projectLoadedSuccessfully) {
      m_imagesReady = true;
      m_imageLoadFailed = false;
      m_glfw.postEmptyEvent(); // Post an empty event to notify render thread
      spdlog::debug("Done loading images");
    }
    else {
      spdlog::critical("Failed to load images");
      m_imagesReady = true;
      m_imageLoadFailed = false;
    }
  };

  m_glfw.setWindowTitleStatus("Loading project...");
  m_data.setProject(serialize::createProjectFromInputParams(params));
  m_futureLoadProject = std::async(std::launch::async, projectLoader, m_data.project(), onProjectLoadingDone);
  spdlog::debug("Done loading images from parameters");
}

void EntropyApp::setCallbacks()
{
  m_glfw.setCallbacks(
    [this](std::chrono::time_point<std::chrono::steady_clock>& lastFrameTime) { m_rendering.framerateLimiter(lastFrameTime); },
    [this]() { m_rendering.render(); },
    [this]() { m_imgui.render(); }
  );

  m_imgui.setCallbacks(
    [this]() { m_glfw.postEmptyEvent(); },
    [this]()
    { resize(m_data.windowData().getWindowSize().x, m_data.windowData().getWindowSize().y); },

    [this](const uuids::uuid& viewUid)
    { m_callbackHandler.recenterView(m_data.state().recenteringMode(), viewUid); },

    [this](
      bool recenterCrosshairs,
      bool realignCrosshairs,
      bool recenterOnCurrentCrosshairsPosition,
      bool resetObliqueOrientation,
      bool resetZoom)
    {
      m_callbackHandler.recenterViews(
        m_data.state().recenteringMode(),
        recenterCrosshairs,
        realignCrosshairs,
        recenterOnCurrentCrosshairsPosition,
        resetObliqueOrientation,
        resetZoom);
    },

    [this]() { return m_callbackHandler.showOverlays(); },
    [this](bool show) { m_callbackHandler.setShowOverlays(show); },
    [this]() { m_rendering.updateImageUniforms(m_data.imageUidsOrdered()); },
    [this](const uuids::uuid& imageUid) { m_rendering.updateImageUniforms(imageUid); },
    [this](const uuids::uuid& imageUid) { m_rendering.updateImageInterpolation(imageUid); },
    [this](std::size_t cmapIndex) { m_rendering.updateImageColorMapInterpolation(cmapIndex); },
    [this](size_t labelColorTableIndex)
    { m_rendering.updateLabelColorTableTexture(labelColorTableIndex); },

    // moveCrosshairsToSegLabelCentroid
    [this](const uuids::uuid& imageUid, size_t labelIndex)
    { m_callbackHandler.moveCrosshairsToSegLabelCentroid(imageUid, labelIndex); },

    [this]() { m_rendering.updateMetricUniforms(); },
    [this]() { return m_data.state().worldCrosshairs().worldOrigin(); },

    // Get subject position:
    [this](size_t imageIndex) -> std::optional<glm::vec3>
    {
      const auto imageUid = m_data.imageUid(imageIndex);
      const Image* image = imageUid ? m_data.image(*imageUid) : nullptr;
      if (!image) {
        return std::nullopt;
      }

      const glm::vec4 subjectPos = image->transformations().subject_T_worldDef() *
                                   glm::vec4{m_data.state().worldCrosshairs().worldOrigin(), 1.0f};

      return glm::vec3{subjectPos / subjectPos.w};
    },

    [this](size_t imageIndex) { return data::getImageVoxelCoordsAtCrosshairs(m_data, imageIndex); },

    // Set subject position for image:
    [this](size_t imageIndex, const glm::vec3& subjectPos)
    {
      const auto imageUid = m_data.imageUid(imageIndex);
      const Image* image = imageUid ? m_data.image(*imageUid) : nullptr;
      if (!image) {
        return;
      }

      const glm::vec4 worldPos = image->transformations().worldDef_T_subject() * glm::vec4{subjectPos, 1.0f};

      m_data.state().setWorldCrosshairsPos(glm::vec3{worldPos / worldPos.w});
    },

    // Set voxel position for image:
    [this](size_t imageIndex, const glm::ivec3& voxelPos)
    {
      const auto imageUid = m_data.imageUid(imageIndex);
      const Image* image = imageUid ? m_data.image(*imageUid) : nullptr;
      if (!image) {
        return;
      }

      /// @todo Put this in CallbackHandler as separate function, because it is used frequently
      /// @todo All logic related to rounding crosshairs positions should be in one place!

      const glm::vec4 worldPos = image->transformations().worldDef_T_pixel() * glm::vec4{voxelPos, 1.0f};
      const glm::vec3 worldPosRounded = data::roundPointToNearestImageVoxelCenter(
        *image, glm::vec3{worldPos / worldPos.w});

      m_data.state().setWorldCrosshairsPos(worldPosRounded);
    },

    // This gets the image value using NN interpolation
    [this](size_t imageIndex, bool getOnlyActiveComponent) -> std::vector<double>
    {
      std::vector<double> values;

      const auto imageUid = m_data.imageUid(imageIndex);
      const Image* image = imageUid ? m_data.image(*imageUid) : nullptr;
      if (!image) {
        return values;
      }

      if (const auto coords = data::getImageVoxelCoordsAtCrosshairs(m_data, imageIndex))
      {
        if (getOnlyActiveComponent)
        {
          if (const auto a = image->value<double>(
                image->settings().activeComponent(), coords->x, coords->y, coords->z)) {
            // Return empty vector if component has undefined value
            values.push_back(*a);
          }
          else {
            return std::vector<double>{};
          }
        }
        else
        {
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
    },

    // This gets the image value using linear interpolation
    [this](size_t imageIndex, bool getOnlyActiveComponent) -> std::vector<double>
    {
      std::vector<double> values;

      const auto imageUid = m_data.imageUid(imageIndex);
      const Image* image = imageUid ? m_data.image(*imageUid) : nullptr;
      if (!image) {
        return values;
      }

      if (const auto coords = data::getImageVoxelCoordsContinuousAtCrosshairs(m_data, imageIndex))
      {
        if (getOnlyActiveComponent)
        {
          if (const auto a = image->valueLinear<double>(
                image->settings().activeComponent(), coords->x, coords->y, coords->z)) {
            // Return empty vector if component has undefined value
            values.push_back(*a);
          }
          else {
            return std::vector<double>{};
          }
        }
        else
        {
          for (uint32_t i = 0; i < image->header().numComponentsPerPixel(); ++i)
          {
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
    },

    // This gets the active segmentation value:
    [this](size_t imageIndex) -> std::optional<int64_t>
    {
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
    },

    [this](const uuids::uuid& matchingImageUid, const std::string& segDisplayName)
    {
      return m_callbackHandler.createBlankSegWithColorTableAndTextures(matchingImageUid, segDisplayName);
    },

    [this](const uuids::uuid& segUid) -> bool { return m_callbackHandler.clearSegVoxels(segUid); },

    [this](const uuids::uuid& segUid) -> bool
    {
      bool success = false;
      success |= m_data.removeSeg(segUid);
      success |= m_rendering.removeSegTexture(segUid);
      return success;
    },

    [this](const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType& segType) -> bool
    { return m_callbackHandler.executeGraphCutsSegmentation(imageUid, seedSegUid, segType); },

    [this](const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType& segType) -> bool
    { return m_callbackHandler.executePoissonSegmentation(imageUid, seedSegUid, segType); },

    [this](const uuids::uuid& imageUid, bool locked) -> bool
    { return m_callbackHandler.setLockManualImageTransformation(imageUid, locked); },

    [this]() { return m_callbackHandler.paintActiveSegmentationWithAnnotation(); }
  );
}

//void EntropyApp::broadcastCrosshairsPosition()
//{
//    // We need a reference image to move the crosshairs
//    const Image* refImg = m_data.refImage();
//    if ( ! refImg ) return;

//    const auto& refTx = refImg->transformations();

//    // Convert World to reference Subject position:
//    glm::vec4 refSubjectPos = refTx.subject_T_world() *
//        glm::vec4{ m_data.state().worldCrosshairs().worldOrigin(), 1.0f };

//    refSubjectPos /= refSubjectPos.w;

//    // Read the contents of shared memory into the local message object
//    IPCMessage message;
//    m_IPCHandler.Read( static_cast<void*>( &message ) );

////    spdlog::info( "cursor = {}, {}, {}", message.cursor[0], message.cursor[1], message.cursor[2] );

//    // Convert LPS to RAS
//    message.cursor[0] = -refSubjectPos[0];
//    message.cursor[1] = -refSubjectPos[1];
//    message.cursor[2] = refSubjectPos[2];

//    if ( ! m_IPCHandler.Broadcast( static_cast<void*>( &message ) ) )
//    {
//        spdlog::warn( "Failed to broadcast IPC" );
//    }
//}
