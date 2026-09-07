#include "EntropyApp.h"
#include "BuildStamp.h"

#include "common/Exception.hpp"
#include "layout/LayoutFileSerialization.h"
#include "logic/app/AppPaths.h"
#include "logic/app/ProjectSnapshotSettings.h"
#include "logic/app/UserPreferences.h"
#include "logic/states/FsmList.hpp"
#include "ui/dialogs/WarpFieldAssignment.h"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
void removeDefaultLayouts(WindowData& windowData, std::vector<std::size_t> removedLayoutIndices)
{
  std::ranges::sort(removedLayoutIndices, std::greater<>{});
  for (const std::size_t layoutIndex : removedLayoutIndices) {
    if (layoutIndex < windowData.numLayouts()) {
      windowData.removeLayout(layoutIndex);
    }
  }
}

void applyModifiedDefaultLayouts(
  WindowData& windowData,
  const std::vector<serialize::DefaultLayoutOverride>& overrides,
  const uuid_range_t& imageUids)
{
  for (const auto& override : overrides) {
    if (!windowData.replaceProjectLayoutSnapshot(override.m_index, override.m_layout, imageUids)) {
      spdlog::warn("Skipping project layout override for missing default layout {}", override.m_index);
    }
  }
}

} // namespace

void EntropyApp::init()
{
  spdlog::debug("Begin initializing application");

  // Start the annotation state machine
  state::annot::fsm_list::start();

  if (state::annot::fsm_list::current_state_ptr) {
    state::annot::AnnotationStateMachine::setAppData(&m_data);
    state::annot::AnnotationStateMachine::setCallbacks([this]() { m_imgui.render(); });
  }
  else {
    spdlog::error("Null annotation state machine");
    throwDebug("Null annotation state machine");
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
          m_data.renderSettings(),
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
    user_preferences::markSavedAppSettingsState(m_data.settings(), m_data.renderSettings(), m_data.guiData());
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
    throwDebug("The reference image is null");
  }

  if (pendingWarpAssignment) {
    const Image* image = m_data.image(pendingWarpAssignment->imageUid);
    const Image* warp = m_data.warpField(pendingWarpAssignment->warpUid);
    bool assigned = false;
    if (image && warp) {
      if (pendingWarpAssignment->forwardWarp) {
        const auto referenceUid = m_data.refImageUid();
        const Image* referenceImage = referenceUid ? m_data.image(*referenceUid) : nullptr;
        if (warp_field_assignment::confirm(
              "Forward warp warning",
              warp_field_assignment::forwardWarnings(*warp, *image, referenceImage)))
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
          referenceImage && warp_field_assignment::confirm(
                              "Inverse warp warning",
                              warp_field_assignment::inverseWarnings(*warp, *referenceImage)))
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

  m_data.renderResources().clearLoadedData();
  m_data.renderDerivedData().clear();

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

    if (const auto layoutsFileName = m_data.project().m_layoutsFileName) {
      layout::LayoutFile layoutFile;
      if (layout::open(layoutFile, *layoutsFileName)) {
        if (layoutFile.m_layouts.empty()) {
          spdlog::warn("Referenced layout file {} contains no layouts; using the default layout", *layoutsFileName);
        }
        if (!m_data.windowData().applyProjectLayoutSnapshots(
              layoutFile.m_layouts,
              m_data.imageUidsOrdered(),
              layoutFile.m_currentLayoutIndex))
        {
          spdlog::error("Could not apply referenced layout file {}", *layoutsFileName);
        }
      }
      else if (!m_data.project().m_layouts.empty()) {
        spdlog::warn("Falling back to inline project layouts after referenced layout file failed to load");
        applyModifiedDefaultLayouts(
          m_data.windowData(),
          m_data.project().m_modifiedDefaultLayouts,
          m_data.imageUidsOrdered());
        removeDefaultLayouts(m_data.windowData(), m_data.project().m_removedDefaultLayoutIndices);
        m_data.windowData().appendProjectLayoutSnapshots(
          m_data.project().m_layouts,
          m_data.imageUidsOrdered(),
          m_data.project().m_currentLayoutIndex);
      }
    }
    else {
      applyModifiedDefaultLayouts(
        m_data.windowData(),
        m_data.project().m_modifiedDefaultLayouts,
        m_data.imageUidsOrdered());
      removeDefaultLayouts(m_data.windowData(), m_data.project().m_removedDefaultLayoutIndices);
      if (!m_data.project().m_layouts.empty()) {
        m_data.windowData().appendProjectLayoutSnapshots(
          m_data.project().m_layouts,
          m_data.imageUidsOrdered(),
          m_data.project().m_currentLayoutIndex);
      }
    }

    if (
      m_data.project().m_currentLayoutIndex &&
      *m_data.project().m_currentLayoutIndex < m_data.windowData().numLayouts())
    {
      m_data.windowData().setCurrentLayoutIndex(*m_data.project().m_currentLayoutIndex);
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
  const GuiData::Margins margins = m_data.guiData().computeMargins();

  // This call sets the window size and viewport
  // app->resize( windowWidth, windowHeight );
  windowData().setWindowSize(windowWidth, windowHeight);

  if (const std::optional<glm::vec4>& renderViewport = m_data.guiData().m_renderViewport) {
    const GuiData::Margins toolbarMargins = m_data.guiData().computeToolbarMargins();
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
