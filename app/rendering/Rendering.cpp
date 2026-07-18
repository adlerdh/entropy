#include "rendering/Rendering.h"

#include "common/Types.h"
#include "logic/app/Data.h"
#include "logic/app/DataHelper.h"
#include "logic/app/StackTrace.h"
#include "logic/camera/Camera3DControls.h"
#include "logic/camera/CameraHelpers.h"
#include "windowing/View.h"

#include <cmrc/cmrc.hpp>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <nanovg.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cmath>
#include <limits>
#include <list>
#include <thread>
#include <utility>

CMRC_DECLARE(fonts);
CMRC_DECLARE(shaders);

static_assert(static_cast<int>(RenderData::LocalNccPresentation::Dissimilarity) == 0);
static_assert(static_cast<int>(RenderData::LocalNccPresentation::Correlation) == 1);
static_assert(static_cast<int>(RenderData::LocalNccInvalidStyle::Transparent) == 0);
static_assert(static_cast<int>(RenderData::LocalNccInvalidStyle::Gray) == 1);

namespace
{
using namespace uuids;

void logOpenGLTextureLimits()
{
  GLint maxTextureSize = 0;
  GLint max3DTextureSize = 0;
  GLint maxArrayTextureLayers = 0;
  GLint maxTextureBufferSize = 0;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
  glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &max3DTextureSize);
  glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArrayTextureLayers);
  glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxTextureBufferSize);

  spdlog::debug(
    "OpenGL texture limits:\n"
    "  1D textures: max length = GL_MAX_TEXTURE_SIZE = {}\n"
    "  2D textures: max width and max height each = GL_MAX_TEXTURE_SIZE = {} (max {} x {})\n"
    "  3D textures: max width, max height, and max depth each = GL_MAX_3D_TEXTURE_SIZE = {} (max {} x {} x {})\n"
    "  Array textures: max layer count = GL_MAX_ARRAY_TEXTURE_LAYERS = {}\n"
    "  Buffer textures: max texels = GL_MAX_TEXTURE_BUFFER_SIZE = {}",
    maxTextureSize,
    maxTextureSize,
    maxTextureSize,
    maxTextureSize,
    max3DTextureSize,
    max3DTextureSize,
    max3DTextureSize,
    max3DTextureSize,
    maxArrayTextureLayers,
    maxTextureBufferSize);
}

} // namespace

Rendering::Rendering(AppData& appData)
  : m_appData(appData)
  , m_nvg(nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES /*| NVG_DEBUG*/))
  , m_asciiRenderer(appData)
  , m_pixelEdgeRenderer()
  , m_raycastIsoProgram("RaycastIsoSurfaceProgram")
  , m_raycastIsoWarpedProgram("RaycastIsoSurfaceWarpedProgram")
  , m_isAppDoneLoadingImages(false)
  , m_showOverlays(true)
{
  static const std::string ROBOTO_LIGHT("robotoLight");

  if (!m_nvg) {
    spdlog::error(
      "Could not initialize 'nanovg' vector graphics library. "
      "Proceeding without vector graphics.");
  }

  try {
    // Load the font for anatomical labels:
    auto filesystem = cmrc::fonts::get_filesystem();
    const cmrc::file robotoFont = filesystem.open("res/fonts/Roboto/Roboto-Light.ttf");

    const int robotoLightFont = nvgCreateFontMem(
      m_nvg,
      ROBOTO_LIGHT.c_str(),
      reinterpret_cast<uint8_t*>(const_cast<char*>(robotoFont.begin())),
      static_cast<int32_t>(robotoFont.size()),
      0);

    if (-1 == robotoLightFont) {
      spdlog::error("Could not load font {}", ROBOTO_LIGHT);
    }
  }
  catch (const std::exception& e) {
    spdlog::error("Exception when loading font file: {}", e.what());
  }

  logOpenGLTextureLimits();

  createShaderPrograms();
}

Rendering::~Rendering()
{
  if (m_nvg) {
    nvgDeleteGL3(m_nvg);
    m_nvg = nullptr;
  }
}
void Rendering::setupOpenGLState()
{
  glEnable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE);
  glDisable(GL_SCISSOR_TEST);
  glEnable(GL_STENCIL_TEST);

  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glFrontFace(GL_CCW);

  // This is the state touched by NanoVG:
  //    glEnable(GL_CULL_FACE);
  //    glCullFace(GL_BACK);
  //    glFrontFace(GL_CCW);
  //    glEnable(GL_BLEND);
  //    glDisable(GL_DEPTH_TEST);
  //    glDisable(GL_SCISSOR_TEST);
  //    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  //    glStencilMask(0xffffffff);
  //    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  //    glStencilFunc(GL_ALWAYS, 0, 0xffffffff);
  //    glActiveTexture(GL_TEXTURE0);
  //    glBindTexture(GL_TEXTURE_2D, 0);
}

void Rendering::init()
{
  nvgReset(m_nvg);
  m_asciiRenderer.init();
  m_pixelEdgeRenderer.init();
}

/// @todo Need to fix this to handle multicomponent images like
/// std::vector<uuid> createImageTextures( AppData& appData, uuid_range_t imageUids )
Rendering::CurrentImages Rendering::getImageAndSegUidsForMetricShaders(const std::list<uuid>& metricImageUids) const
{
  const RenderData& R = m_appData.renderData();
  CurrentImages imageSegPairs;

  for (const auto& imageUid : metricImageUids) {
    if (imageSegPairs.size() >= NUM_METRIC_IMAGES) {
      break; // Stop after NUM_METRIC_IMAGES images reached
    }

    if (const Image* image = m_appData.image(imageUid);
        image && ((image->settings().vectorArrowOverlayVisible() && !image->settings().vectorArrowOverlayOnImage()) ||
                  (image->settings().vectorWarpedGridVisible() && !image->settings().vectorWarpedGridOverlayOnImage())))
    {
      imageSegPairs.emplace_back();
      continue;
    }

    const uuid renderImageUid = m_appData.effectiveImageUidForRendering(imageUid);
    if (std::end(R.m_imageTextures) != R.m_imageTextures.find(renderImageUid)) {
      ImgSegPair imgSegPair;
      imgSegPair.first = renderImageUid; // The texture for this image exists

      // Find the segmentation that belongs to this image
      if (const auto segUid = m_appData.imageToActiveSegUid(imageUid)) {
        if (std::end(R.m_segTextures) != R.m_segTextures.find(*segUid)) {
          imgSegPair.second = *segUid; // The texture for this segmentation exists
        }
      }

      imageSegPairs.emplace_back(std::move(imgSegPair));
    }
  }

  // Always return at least two elements
  while (imageSegPairs.size() < Rendering::NUM_METRIC_IMAGES) {
    imageSegPairs.emplace_back();
  }

  return imageSegPairs;
}

Rendering::CurrentImages Rendering::getImageAndSegUidsForImageShaders(const std::list<uuid>& imageUids) const
{
  const RenderData& R = m_appData.renderData();
  CurrentImages imageSegPairs;

  for (const auto& imageUid : imageUids) {
    if (const Image* image = m_appData.image(imageUid);
        image && ((image->settings().vectorArrowOverlayVisible() && !image->settings().vectorArrowOverlayOnImage()) ||
                  (image->settings().vectorWarpedGridVisible() && !image->settings().vectorWarpedGridOverlayOnImage())))
    {
      continue;
    }

    const uuid renderImageUid = m_appData.effectiveImageUidForRendering(imageUid);
    if (std::end(R.m_imageTextures) != R.m_imageTextures.find(renderImageUid)) {
      std::pair<std::optional<uuid>, std::optional<uuid>> imgSegPair;
      imgSegPair.first = renderImageUid; // The texture for this image exists

      // Find the segmentation that belongs to this image
      if (const auto segUid = m_appData.imageToActiveSegUid(imageUid)) {
        if (std::end(R.m_segTextures) != R.m_segTextures.find(*segUid)) {
          imgSegPair.second = *segUid; // The texture for this segmentation exists
        }
      }

      imageSegPairs.emplace_back(std::move(imgSegPair));
    }
  }

  return imageSegPairs;
}

void Rendering::framerateLimiter(std::chrono::time_point<Clock>& lastFrameTime)
{
  if (!m_appData.renderData().m_manualFramerateLimiter) {
    return;
  }

  const double elapsed = std::chrono::duration<double>(Clock::now() - lastFrameTime).count();
  const double targetTime = m_appData.renderData().m_targetFrameTimeSeconds;

  if (elapsed < targetTime) {
    std::this_thread::sleep_for(std::chrono::duration<double>(targetTime - elapsed));
  }

  lastFrameTime = Clock::now();
}

void Rendering::render()
{
  // Rebuild ASCII atlas if the charset changed via the UI
  m_asciiRenderer.maybeRebuildAtlas();

  // Set up OpenGL state, because it changes after NanoVG calls in the render of the prior frame
  setupOpenGLState();

  // Set the OpenGL viewport in device units:
  const glm::ivec4 deviceViewport = m_appData.windowData().viewport().getDeviceAsVec4();
  glViewport(deviceViewport[0], deviceViewport[1], deviceViewport[2], deviceViewport[3]);

  const auto& bg = m_appData.renderData().m_2dBackgroundColor;
  glClearColor(bg.r, bg.g, bg.b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  try {
    renderImageData();
  }
  catch (const std::exception& e) {
    spdlog::error("Exception while rendering image data: {}\n{}", e.what(), stack_trace::current(1));
    throw;
  }

  try {
    renderVectorOverlays();
  }
  catch (const std::exception& e) {
    spdlog::error("Exception while rendering vector overlays: {}\n{}", e.what(), stack_trace::current(1));
    throw;
  }
}

std::optional<ClipboardPayload> Rendering::exportAsciiClipboardPayloadForView(const View& view)
{
  return m_asciiRenderer.exportClipboardPayloadForView(view);
}

void Rendering::renderImageData()
{
  if (ProjectLoadState::Loaded != m_appData.state().projectLoadState() || 0 == m_appData.windowData().numLayouts()) {
    return;
  }

  if (m_asciiRenderer.enabled()) {
    m_asciiRenderer.render(
      m_shaderPrograms,
      [this](const View& v, const FrameBounds& b, const glm::vec3& o) {
        renderAllImagesForView(v, b, o, false, false, false);
      },
      [this](const View& v, const FrameBounds& b, const glm::vec3& o) { renderAllImageBordersForView(v, b, o); },
      [this](const View& v, const FrameBounds& b, const glm::vec3& o) { renderAllLandmarksForView(v, b, o); },
      [this](const View& v, const FrameBounds& b, const glm::vec3& o) { renderAllAnnotationsForView(v, b, o); });
    return;
  }

  const auto& R = m_appData.renderData();

  const bool renderLandmarksOnTop = R.m_globalLandmarkParams.renderOnTopOfAllImagePlanes;
  const bool renderAnnotationsOnTop = R.m_globalAnnotationParams.renderOnTopOfAllImagePlanes;

  auto threeDSceneForView = [this](const View& view) {
    const ImageSelection selection =
      view.visibleImages().empty() ? ImageSelection::AllLoadedImages : ImageSelection::VisibleImagesInView;
    camera3d::SceneFrame scene =
      camera3d::sceneFrameFromAABB(data::computeWorldAABBoxEnclosingImages(m_appData, selection, &view));
    scene.m_voxelDiagonal = data::computeMinVoxelDiagonalForImages(m_appData, selection, &view);
    return scene;
  };

  // Render images for each view in the layout
  for (const auto& [viewUid, view] : m_appData.windowData().currentLayout().views()) {
    if (!view) {
      continue;
    }

    if (ViewType::ThreeD == view->viewType() && !view->threeDState().m_userMovedCamera) {
      const camera3d::SceneFrame scene = threeDSceneForView(*view);
      const glm::vec3 crosshairs = m_appData.state().worldCrosshairs().worldOrigin();
      const glm::vec3 orbitTarget =
        (camera3d::OrbitTargetMode::Crosshairs == view->threeDState().m_orbitTargetMode) ? crosshairs : scene.m_center;
      if (view->threeDState().m_viewPositionFollowsCrosshairs) {
        view->initializeThreeDCameraIfNeeded(scene);
        camera3d::recenterFollowing(view->threeDCamera(), view->threeDState(), scene, crosshairs, orbitTarget);
      }
      else {
        view->recenterThreeDCamera(scene, orbitTarget);
      }
    }
    else if (ViewType::ThreeD == view->viewType()) {
      const camera3d::SceneFrame scene = threeDSceneForView(*view);
      camera3d::Controller{view->threeDCamera(), view->threeDState()}.updateScene(scene);
      if (view->threeDState().m_viewPositionFollowsCrosshairs) {
        camera3d::followCrosshairs(
          view->threeDCamera(),
          view->threeDState(),
          m_appData.state().worldCrosshairs().worldOrigin());
      }
    }

    // Offset the crosshairs according to the image slice in the view
    const glm::vec3 worldXhairsOffset =
      view->updateImageSlice(m_appData, m_appData.state().worldCrosshairs().worldOrigin());

    const auto miewportViewBounds =
      helper::computeMiewportFrameBounds(view->windowClipViewport(), m_appData.windowData().viewport().getAsVec4());

    renderAllImagesForView(*view, miewportViewBounds, worldXhairsOffset);

    // Do not render landmarks and annotations in volume rendering mode
    if (ViewRenderMode::VolumeRender != view->renderMode()) {
      if (renderLandmarksOnTop) {
        renderAllLandmarksForView(*view, miewportViewBounds, worldXhairsOffset);
      }

      if (renderAnnotationsOnTop) {
        renderAllAnnotationsForView(*view, miewportViewBounds, worldXhairsOffset);
      }
    }
  }
}
