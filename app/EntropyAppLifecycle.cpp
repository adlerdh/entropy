#include "EntropyApp.h"
#include "BuildStamp.h"

#include "common/Exception.hpp"
#include "layout/LayoutFileSerialization.h"
#include "logic/app/AppPaths.h"
#include "logic/app/ProjectSnapshotSettings.h"
#include "logic/app/UserPreferences.h"
#include "logic/states/FsmList.hpp"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include "ui/dialogs/NativeMessageDialogs.h"

#include <glm/gtc/epsilon.hpp>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
struct WarpWarningAabb
{
  glm::vec3 min{std::numeric_limits<float>::max()};
  glm::vec3 max{std::numeric_limits<float>::lowest()};
};

constexpr float k_warpGeometryEpsilon = 1.0e-4f;
constexpr float k_warpDomainToleranceMm = 0.5f;

WarpWarningAabb warpWarningImageWorldAabb(const Image& image)
{
  WarpWarningAabb box;
  const glm::mat4 world_T_subject = image.transformations().worldDef_T_subject();
  for (const glm::vec3& corner : image.header().subjectBBoxCorners()) {
    const glm::vec3 worldCorner{world_T_subject * glm::vec4{corner, 1.0f}};
    box.min = glm::min(box.min, worldCorner);
    box.max = glm::max(box.max, worldCorner);
  }
  return box;
}

bool warpWarningAabbContains(const WarpWarningAabb& domain, const WarpWarningAabb& target)
{
  return glm::all(glm::lessThanEqual(domain.min, target.min + glm::vec3{k_warpDomainToleranceMm})) &&
         glm::all(glm::greaterThanEqual(domain.max, target.max - glm::vec3{k_warpDomainToleranceMm}));
}

bool warpWarningVec3NearlyEqual(const glm::vec3& a, const glm::vec3& b)
{
  return glm::all(glm::epsilonEqual(a, b, k_warpGeometryEpsilon));
}

std::vector<std::string> rawWarpAssignmentWarnings(const Image& field, const Image& target)
{
  std::vector<std::string> warnings;
  if (field.header().numComponentsPerPixel() < 3) {
    warnings.emplace_back("Fewer than three components per voxel");
  }
  if (field.header().pixelDimensions() != target.header().pixelDimensions()) {
    warnings.emplace_back("Grid dimensions differ");
  }
  if (!warpWarningVec3NearlyEqual(field.header().spacing(), target.header().spacing())) {
    warnings.emplace_back("Voxel spacing differs");
  }
  if (!warpWarningVec3NearlyEqual(field.header().origin(), target.header().origin())) {
    warnings.emplace_back("Origin differs");
  }
  if (
    !warpWarningVec3NearlyEqual(field.header().directions()[0], target.header().directions()[0]) ||
    !warpWarningVec3NearlyEqual(field.header().directions()[1], target.header().directions()[1]) ||
    !warpWarningVec3NearlyEqual(field.header().directions()[2], target.header().directions()[2]))
  {
    warnings.emplace_back("Direction matrix differs");
  }
  if (!warpWarningAabbContains(warpWarningImageWorldAabb(field), warpWarningImageWorldAabb(target))) {
    warnings.emplace_back("Physical domain does not fully cover the reference");
  }
  return warnings;
}

std::vector<std::string> inverseWarpAssignmentWarnings(const Image& field, const Image& referenceImage)
{
  std::vector<std::string> warnings = rawWarpAssignmentWarnings(field, referenceImage);
  if (!warnings.empty()) {
    warnings.insert(warnings.begin(), "Warning! The inverse warp field differs from the reference:");
  }
  return warnings;
}

std::vector<std::string>
forwardWarpAssignmentWarnings(const Image& field, const Image& movingImage, const Image* referenceImage)
{
  std::vector<std::string> movingWarnings = rawWarpAssignmentWarnings(field, movingImage);
  if (movingWarnings.empty()) {
    return {};
  }
  if (referenceImage && rawWarpAssignmentWarnings(field, *referenceImage).empty()) {
    return {};
  }
  movingWarnings.insert(
    movingWarnings.begin(),
    "The forward warp field does not match either the moving-image space or the reference-image space.");
  return movingWarnings;
}

std::string joinWarpAssignmentWarnings(const std::vector<std::string>& warnings)
{
  std::string text;
  for (std::size_t i = 0; i < warnings.size(); ++i) {
    if (!text.empty()) {
      text += "\n";
    }
    text += i == 0 ? warnings.at(i) : "- " + warnings.at(i);
  }
  return text;
}

bool confirmWarpAssignmentWarnings(const char* title, const std::vector<std::string>& warnings)
{
  if (warnings.empty()) {
    return true;
  }
  const auto result = native_dialog::showMessageDialog(
    {title,
     "The selected warp field may not match the expected image space.",
     joinWarpAssignmentWarnings(warnings),
     "Use warp field",
     "Cancel",
     ""});
  return !result || native_dialog::MessageDialogResult::FirstButton == *result;
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
{
  spdlog::debug("Begin constructing application");

  setCallbacks();

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
    if (!user_preferences::load(
          m_data.settings(),
          m_data.renderData(),
          m_data.guiData(),
          settingsFile,
          &preferencesError))
    {
      spdlog::warn("Using built-in settings after failing to load {}: {}", settingsFile, preferencesError);
    }
    m_imgui.setUserScaleOverride(m_data.settings().uiScaleOverride());
    m_imgui.requestFontReload();
    m_imgui.applyUiColorPreset(m_data.settings().uiColorPreset());
    m_imgui.applyUiDensityPreset(m_data.settings().uiDensityPreset());
    m_imgui.applyUiWindowBgOpacity(m_data.settings().uiWindowBgOpacity());
    project_snapshot::syncLayoutTabGuiData(m_data);
    user_preferences::markSavedAppSettingsState(m_data.settings(), m_data.renderData(), m_data.guiData());
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
  std::vector<uuids::uuid> pendingAddedImageUids = m_pendingAddedImageUids;
  const std::optional<fs::path> pendingLayoutsFile = m_pendingLayoutsFile;
  const std::optional<PendingWarpAssignment> pendingWarpAssignment = m_pendingWarpAssignment;
  m_preserveLayoutsOnImagesReady = false;
  m_pendingAddedImageUids.clear();
  m_pendingLayoutsFile = std::nullopt;
  m_pendingWarpAssignment = std::nullopt;

  spdlog::debug("Images are loaded.");

  const Image* refImg = m_data.refImage();
  if (!refImg) {
    // At a minimum, we need a reference image to do anything.
    // If the reference image is null, then image loading has failed.
    spdlog::critical("The reference image is null");
    throw_debug("The reference image is null")
  }

  if (pendingWarpAssignment) {
    const Image* image = m_data.image(pendingWarpAssignment->imageUid);
    const Image* warp = m_data.warpField(pendingWarpAssignment->warpUid);
    bool assigned = false;
    if (image && warp) {
      if (pendingWarpAssignment->forwardWarp) {
        const Image* referenceImage = m_data.refImageUid() ? m_data.image(*m_data.refImageUid()) : nullptr;
        if (confirmWarpAssignmentWarnings(
              "Forward warp warning",
              forwardWarpAssignmentWarnings(*warp, *image, referenceImage)))
        {
          assigned =
            m_data.assignForwardWarpUidToImage(pendingWarpAssignment->imageUid, pendingWarpAssignment->warpUid);
        }
      }
      else {
        const std::optional<uuids::uuid> referenceUid = pendingWarpAssignment->inverseWarpReferenceImageUid
                                                          ? pendingWarpAssignment->inverseWarpReferenceImageUid
                                                          : std::optional<uuids::uuid>{pendingWarpAssignment->imageUid};
        const Image* referenceImage = referenceUid ? m_data.image(*referenceUid) : nullptr;
        if (
          referenceImage &&
          confirmWarpAssignmentWarnings("Inverse warp warning", inverseWarpAssignmentWarnings(*warp, *referenceImage)))
        {
          assigned = m_data.assignInverseWarpUidToImage(
            pendingWarpAssignment->imageUid,
            pendingWarpAssignment->warpUid,
            referenceUid);
        }
      }
    }
    if (assigned) {
      if (pendingWarpAssignment->loaded) {
        pendingAddedImageUids.push_back(pendingWarpAssignment->warpUid);
      }
      m_data.setRainbowColorsForAllImages();
    }
    else if (pendingWarpAssignment->loaded) {
      m_data.removeDef(pendingWarpAssignment->warpUid);
    }
  }

  if (!preserveLayouts) {
    m_data.windowData().resetDefaultLayouts();
  }

  auto& renderData = m_data.renderData();
  renderData.m_imageTextures.clear();
  renderData.m_imageTextureLayouts.clear();
  renderData.m_distanceMapTextures.clear();
  renderData.m_segTextures.clear();
  renderData.m_segTextureLayouts.clear();
  renderData.m_labelBufferTextures.clear();
  renderData.m_colormapTextures.clear();
  renderData.m_uniforms.clear();

  m_rendering.initTextures();
  if (0 == m_data.numImages() || !m_data.refImage()) {
    spdlog::warn("Texture initialization removed all renderable project images; closing the project");
    closeProject();
    return;
  }

  m_rendering.updateImageUniforms(m_data.imageUidsOrdered());

  spdlog::debug("Textures and uniforms ready; rendering enabled");

  // Stop animation rendering (which plays during loading) and render only on events:
  m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
  updateWindowTitleStatus();

  m_data.guiData().m_visibleImageCountDuringLoad = std::nullopt;
  m_data.state().setAnimating(false);

  m_data.guiData().m_renderUiWindows = true;
  m_data.guiData().m_renderUiOverlays = m_data.settings().overlays();

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
    m_data.windowData().setCurrentLayoutToDefaultForImages(m_data);
    m_data.windowData().setDefaultRenderedImagesForAllLayouts(m_data);

    if (m_data.project().m_layoutsFileName) {
      layout::LayoutFile layoutFile;
      if (layout::open(layoutFile, *m_data.project().m_layoutsFileName)) {
        if (layoutFile.m_layouts.empty()) {
          spdlog::warn(
            "Referenced layout file {} contains no layouts; using the default layout",
            *m_data.project().m_layoutsFileName);
        }
        if (!m_data.windowData().applyLayoutPresets(m_data, layoutFile.m_layouts, layoutFile.m_currentLayoutIndex)) {
          spdlog::error("Could not apply referenced layout file {}", *m_data.project().m_layoutsFileName);
        }
      }
      else if (!m_data.project().m_layouts.empty()) {
        spdlog::warn("Falling back to inline project layouts after referenced layout file failed to load");
        m_data.windowData().applyProjectLayoutSnapshots(
          m_data.project().m_layouts,
          m_data.imageUidsOrdered(),
          m_data.project().m_currentLayoutIndex);
      }
    }
    else if (!m_data.project().m_layouts.empty()) {
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
  hideLoadingStatus();
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
