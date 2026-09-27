#include "ui/ImGuiWrapper.h"

#include "logic/app/CallbackHandler.h"
#include "logic/app/Data.h"
#include "logic/camera/CameraHelpers.h"
#include "rendering/TextureSetup.h"
#include "ui/Helpers.h"
#include "ui/windows/Windows.h"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <utility>

namespace
{
const glm::quat k_identityRotation{1.0f, 0.0f, 0.0f, 0.0f};
const glm::vec3 k_zeroVec{0.0f, 0.0f, 0.0f};
} // namespace

void ImGuiWrapper::applyPresentationToMatchingViews(const uuids::uuid& viewUid)
{
  m_appData.windowData().applyPresentationToMatchingCurrentViews(viewUid);
}

void ImGuiWrapper::applyVisibleImageSelectionToMatchingViews(const uuids::uuid& viewUid)
{
  m_appData.windowData().applyVisibleImageSelectionToMatchingCurrentViews(viewUid);
}

glm::quat ImGuiWrapper::getViewCameraRotation(const uuids::uuid& viewUid)
{
  const View* view = m_appData.windowData().getCurrentView(viewUid);
  if (!view) return k_identityRotation;

  return helper::computeCameraRotationRelativeToWorld(view->camera());
}

void ImGuiWrapper::setViewCameraRotation(const uuids::uuid& viewUid, const glm::quat& camera_T_world_rotationDelta)
{
  m_callbackHandler.doCameraRotate3d(viewUid, camera_T_world_rotationDelta);
}

void ImGuiWrapper::setViewCameraDirection(const uuids::uuid& viewUid, const glm::vec3& worldFwdDirection)
{
  m_callbackHandler.handleSetViewForwardDirection(viewUid, worldFwdDirection);
}

glm::vec3 ImGuiWrapper::getViewNormal(const uuids::uuid& viewUid)
{
  View* view = m_appData.windowData().getCurrentView(viewUid);
  if (!view) return k_zeroVec;
  return helper::worldDirection(view->camera(), Directions::View::Back);
}

std::vector<glm::vec3> ImGuiWrapper::getObliqueViewDirections(const uuids::uuid& viewUidToExclude)
{
  std::vector<glm::vec3> obliqueViewDirections;

  for (std::size_t i = 0; i < m_appData.windowData().numLayouts(); ++i) {
    const Layout* layout = m_appData.windowData().layout(i);
    if (!layout) continue;

    for (const auto& view : layout->views()) {
      if (view.first == viewUidToExclude) continue;
      if (!view.second) continue;

      if (!helper::looksAlongOrthogonalAxis(view.second->camera())) {
        obliqueViewDirections.emplace_back(helper::worldDirection(view.second->camera(), Directions::View::Front));
      }
    }
  }

  return obliqueViewDirections;
}

bool ImGuiWrapper::renderViewOverlays()
{
  using namespace std::placeholders;
  const auto getNumImageColorMaps = std::bind_front(&ImGuiWrapper::getNumImageColorMaps, this);
  const auto getImageColorMap = std::bind_front(&ImGuiWrapper::getImageColorMap, this);
  const auto getImageIsVisibleSetting = std::bind_front(&ImGuiWrapper::getImageIsVisibleSetting, this);
  const auto getImageIsActive = std::bind_front(&ImGuiWrapper::getImageIsActive, this);
  const auto getImageIsReference = std::bind_front(&ImGuiWrapper::getImageIsReference, this);
  const auto getImageIdentificationColor = std::bind_front(&ImGuiWrapper::getImageIdentificationColor, this);
  const auto applyPresentationToMatchingViews = std::bind_front(&ImGuiWrapper::applyPresentationToMatchingViews, this);
  const auto applyVisibleImageSelectionToMatchingViews =
    std::bind_front(&ImGuiWrapper::applyVisibleImageSelectionToMatchingViews, this);
  const auto getViewCameraRotation = std::bind_front(&ImGuiWrapper::getViewCameraRotation, this);
  const auto setViewCameraRotation = std::bind_front(&ImGuiWrapper::setViewCameraRotation, this);
  const auto setViewCameraDirection = std::bind_front(&ImGuiWrapper::setViewCameraDirection, this);
  const auto getViewNormal = std::bind_front(&ImGuiWrapper::getViewNormal, this);
  const auto getObliqueViewDirections = std::bind_front(&ImGuiWrapper::getObliqueViewDirections, this);
  Layout& currentLayout = m_appData.windowData().currentLayout();

  const float wholeWindowHeight = static_cast<float>(m_appData.windowData().getWindowSize().y);
  const char* const boldFontPath = "res/fonts/Inter/Inter-Bold.ttf";
  const auto boldFontIt = m_appData.guiData().m_fonts.find(boldFontPath);
  ImFont* const popupHeadingFont = boldFontIt != m_appData.guiData().m_fonts.end() ? boldFontIt->second : nullptr;
  const auto renderComparisonModeSettings =
    [this, &getNumImageColorMaps, &getImageColorMap](const ViewRenderMode renderMode) {
      renderComparisonModeQuickSettings(
        renderMode,
        m_appData,
        getNumImageColorMaps,
        getImageColorMap,
        m_updateMetricUniforms);
    };
  bool viewOverlayControlExtentsChanged = false;
  const auto reportViewOverlayControlExtent =
    [this, &viewOverlayControlExtentsChanged](const uuids::uuid& frameUid, const glm::vec2& extent) {
      constexpr float measurementTolerance = 0.25f;
      auto& extents = m_appData.guiData().m_viewOverlayControlExtents;
      const auto previous = extents.find(frameUid);
      if (
        previous != extents.end() && std::abs(previous->second.x - extent.x) <= measurementTolerance &&
        std::abs(previous->second.y - extent.y) <= measurementTolerance)
      {
        return;
      }
      extents.insert_or_assign(frameUid, extent);
      viewOverlayControlExtentsChanged = true;
    };

  if (m_appData.guiData().m_renderUiOverlays && currentLayout.isLightbox()) {
    // Per-layout UI controls:

    const auto viewFrameBounds = helper::computeMindowFrameBounds(
      currentLayout.windowClipViewport(),
      m_appData.windowData().viewport().getAsVec4(),
      wholeWindowHeight);

    const ViewOverlayWindowContext overlayContext{
      currentLayout.uid(),
      viewFrameBounds,
      currentLayout.uiControls(),
      false,
      LayoutKind::Lightbox != currentLayout.kind(),
      0.5f,
      m_appData.state().worldCrosshairs(),
      m_uiScaleManager.effectiveScale(),
      [&reportViewOverlayControlExtent, layoutUid = currentLayout.uid()](const glm::vec2& extent) {
        reportViewOverlayControlExtent(layoutUid, extent);
      },
      popupHeadingFont};

    const bool useThreeDImageSelection = ViewType::ThreeD == currentLayout.viewType();
    auto canImageBeVolumeRendered = [this](std::size_t index) {
      const auto imageUid = m_appData.imageUid(index);
      if (!imageUid) {
        return false;
      }
      const auto layoutIt = m_appData.renderResources().m_imageTextureLayouts.find(*imageUid);
      return layoutIt == std::end(m_appData.renderResources().m_imageTextureLayouts) ||
             rendering::TextureDimension::Texture3D == layoutIt->second.dimension;
    };
    auto isLayoutVolumeImageRendered = [this, &currentLayout](std::size_t index) {
      const auto imageUid = m_appData.imageUid(index);
      if (!imageUid) {
        return false;
      }
      const auto& volumeImages = currentLayout.volumeRenderedImages();
      return !volumeImages.empty() && currentLayout.isImageVolumeRendered(*imageUid);
    };

    const ViewOverlayImageCallbacks imageCallbacks{
      m_appData.numImages(),
      [this, &currentLayout, useThreeDImageSelection, isLayoutVolumeImageRendered, canImageBeVolumeRendered](
        std::size_t index) {
        return useThreeDImageSelection ? canImageBeVolumeRendered(index) && isLayoutVolumeImageRendered(index)
                                       : currentLayout.isImageRendered(m_appData, index);
      },
      [this, &currentLayout, useThreeDImageSelection, canImageBeVolumeRendered](std::size_t index, bool visible) {
        if (useThreeDImageSelection) {
          currentLayout.setImageVolumeRendered(m_appData, index, visible && canImageBeVolumeRendered(index));
        }
        else {
          currentLayout.setImageRendered(m_appData, index, visible);
        }
      },
      nullptr,
      [this, &currentLayout](std::size_t index) { return currentLayout.isImageUsedForMetric(m_appData, index); },
      [this, &currentLayout](std::size_t index, bool visible) {
        currentLayout.setImageUsedForMetric(m_appData, index, visible);
      },
      std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),
      getImageIsVisibleSetting,
      getImageIsActive,
      getImageIsReference,
      canImageBeVolumeRendered,
      getImageIdentificationColor,
      [this, &currentLayout](std::size_t index) -> std::optional<std::size_t> {
        const auto imageUid = m_appData.imageUid(index);
        if (!imageUid) {
          return std::nullopt;
        }
        const auto& metricImages = currentLayout.metricImages();
        const auto it = std::find(metricImages.begin(), metricImages.end(), *imageUid);
        return it == metricImages.end()
                 ? std::nullopt
                 : std::optional{static_cast<std::size_t>(std::distance(metricImages.begin(), it))};
      }};

    const AnatomicalLabelResolution anatomicalLabels = m_appData.resolvedAnatomicalLabels();
    const ViewOverlayModeCallbacks modeCallbacks{
      .viewType = currentLayout.viewType(),
      .anatomicalLabelType = anatomicalLabels.type,
      .quadrupedBodyRegion = anatomicalLabels.quadrupedBodyRegion,
      .renderMode = currentLayout.renderMode(),
      .threeDSceneContents = currentLayout.threeDSceneContents(),
      .intensityProjectionMode = currentLayout.intensityProjectionMode(),
      .setViewType =
        [this](const ViewType& viewType) { m_appData.windowData().setCurrentLayoutViewType(m_appData, viewType); },
      .setRenderMode =
        [&currentLayout](const ViewRenderMode& renderMode) { return currentLayout.setRenderMode(renderMode); },
      .setThreeDSceneContents =
        [&currentLayout](ThreeDSceneContents contents) { currentLayout.setThreeDSceneContents(std::move(contents)); },
      .setIntensityProjectionMode =
        [&currentLayout](const IntensityProjectionMode& ipMode) {
          return currentLayout.setIntensityProjectionMode(ipMode);
        },
      .renderComparisonModeSettings = renderComparisonModeSettings,
      .applyPresentationToMatchingViews = nullptr,
      .isIsosurfacesPanelVisible = [this]() { return m_appData.guiData().m_showIsosurfacesWindow; },
      .showIsosurfacesPanel = [this]() { m_appData.guiData().m_showIsosurfacesWindow = true; },
      .hideIsosurfacesPanel = [this]() { m_appData.guiData().m_showIsosurfacesWindow = false; },
      .showIsosurfacesPanelForRaycastImage =
        [this, &currentLayout]() {
          m_appData.guiData().m_suppressIsosurfacesFocusOnNextAppearance = true;
          m_appData.guiData().m_showIsosurfacesWindow = true;
          if (!currentLayout.visibleImages().empty()) {
            m_appData.guiData().m_requestedIsosurfacesImageUid = currentLayout.visibleImages().front();
          }
        },
      .getThreeDCutawayEnabled = [this]() { return m_appData.renderSettings().m_meshCutawayEnabled; },
      .setThreeDCutawayEnabled = [this](bool enabled) { m_appData.renderSettings().m_meshCutawayEnabled = enabled; },
      .isThreeDRenderingSettingsVisible = [this]() { return m_appData.guiData().m_showSettingsWindow; },
      .openThreeDRenderingSettings =
        [this]() {
          m_appData.guiData().m_requestedSettingsTab = GuiData::SettingsTab::Rendering;
          m_appData.guiData().m_suppressSettingsFocusOnNextAppearance = true;
          m_appData.guiData().m_showSettingsWindow = true;
        },
      .hideThreeDRenderingSettings = [this]() { m_appData.guiData().m_showSettingsWindow = false; },
      .selectableViewTypes = LayoutKind::Lightbox == currentLayout.kind()
                               ? std::vector<ViewType>{ViewType::Axial, ViewType::Coronal, ViewType::Sagittal}
                               : std::vector<ViewType>{}};

    const ViewOverlayProjectionCallbacks projectionCallbacks{
      [this]() { return m_appData.renderSettings().m_intensityProjectionSlabThickness; },
      [this](float thickness) { m_appData.renderSettings().m_intensityProjectionSlabThickness = thickness; },
      [this]() { return m_appData.renderSettings().m_doMaxExtentIntensityProjection; },
      [this](bool set) { m_appData.renderSettings().m_doMaxExtentIntensityProjection = set; },
      [this]() { return m_appData.renderSettings().m_xrayIntensityWindow; },
      [this](float window) { m_appData.renderSettings().m_xrayIntensityWindow = window; },
      [this]() { return m_appData.renderSettings().m_xrayIntensityLevel; },
      [this](float level) { m_appData.renderSettings().m_xrayIntensityLevel = level; },
      [this]() { return m_appData.renderSettings().m_xrayEnergyKeV; },
      [this](float energy) {
        m_appData.renderSettings().setXrayEnergy(energy);
      }};

    renderViewSettingsComboWindow(overlayContext, imageCallbacks, modeCallbacks, projectionCallbacks);

    if (ViewRenderMode::JointHistogram != currentLayout.renderMode()) {
      renderViewOrientationToolWindow(
        overlayContext,
        {currentLayout.viewType(),
         anatomicalLabels.type,
         anatomicalLabels.quadrupedBodyRegion,
         [&getViewCameraRotation, &currentLayout]() { return getViewCameraRotation(currentLayout.uid()); },
         [&setViewCameraRotation, &currentLayout](const glm::quat& q) {
           return setViewCameraRotation(currentLayout.uid(), q);
         },
         [&setViewCameraDirection, &currentLayout](const glm::vec3& dir) {
           return setViewCameraDirection(currentLayout.uid(), dir);
         },
         [&getViewNormal, &currentLayout]() { return getViewNormal(currentLayout.uid()); },
         getObliqueViewDirections});
    }
  }
  else if (m_appData.guiData().m_renderUiOverlays && !currentLayout.isLightbox()) {
    // Per-view UI controls:

    const auto layoutFrameBounds = helper::computeMindowFrameBounds(
      currentLayout.windowClipViewport(),
      m_appData.windowData().viewport().getAsVec4(),
      wholeWindowHeight);

    for (const auto& viewUid : m_appData.windowData().currentViewUids()) {
      View* view = m_appData.windowData().getCurrentView(viewUid);
      if (!view) return false;

      auto synchronizeThreeDCamerasFromView = [this](const View* sourceView) {
        if (
          sourceView && m_appData.renderSettings().m_synchronizeThreeDCameras &&
          ViewType::ThreeD == sourceView->viewType())
        {
          m_appData.windowData().synchronizeCurrentLayoutThreeDCameras(sourceView->uid());
        }
      };

      auto setViewType = [this, view](const ViewType& viewType) {
        if (!view) {
          return;
        }

        std::optional<uuids::uuid> existingThreeDViewUid;
        if (ViewType::ThreeD == viewType && m_appData.renderSettings().m_synchronizeThreeDCameras) {
          for (const auto& candidateUid : m_appData.windowData().currentViewUids()) {
            const View* candidate = m_appData.windowData().getCurrentView(candidateUid);
            if (candidate && candidate != view && ViewType::ThreeD == candidate->viewType()) {
              existingThreeDViewUid = candidateUid;
              if (candidate->isThreeDCameraInitialized()) {
                break;
              }
            }
          }
        }

        view->setViewType(viewType);
        if (ViewType::ThreeD == viewType && m_appData.renderSettings().m_synchronizeThreeDCameras) {
          m_appData.windowData().synchronizeCurrentLayoutThreeDCameras(existingThreeDViewUid.value_or(view->uid()));
        }
      };

      auto setRenderMode = [view](const ViewRenderMode& renderMode) {
        if (view) view->setRenderMode(renderMode);
      };

      auto setIntensityProjectionMode = [view](const IntensityProjectionMode& ipMode) {
        if (view) view->setIntensityProjectionMode(ipMode);
      };

      const auto viewFrameBounds = helper::computeMindowFrameBounds(
        view->windowClipViewport(),
        m_appData.windowData().viewport().getAsVec4(),
        wholeWindowHeight);
      const float verticalTravel = layoutFrameBounds.bounds.height - viewFrameBounds.bounds.height;
      const float layoutVerticalPosition =
        verticalTravel > 0.5f
          ? std::clamp((viewFrameBounds.bounds.yoffset - layoutFrameBounds.bounds.yoffset) / verticalTravel, 0.0f, 1.0f)
          : 0.5f;

      const ViewOverlayWindowContext overlayContext{
        viewUid,
        viewFrameBounds,
        view->uiControls(),
        true,
        true,
        layoutVerticalPosition,
        m_appData.state().worldCrosshairs(),
        m_uiScaleManager.effectiveScale(),
        [&reportViewOverlayControlExtent, viewUid](const glm::vec2& extent) {
          reportViewOverlayControlExtent(viewUid, extent);
        },
        popupHeadingFont};

      const bool useThreeDImageSelection = ViewType::ThreeD == view->viewType();
      auto canImageBeVolumeRendered = [this](std::size_t index) {
        const auto imageUid = m_appData.imageUid(index);
        if (!imageUid) {
          return false;
        }
        const auto layoutIt = m_appData.renderResources().m_imageTextureLayouts.find(*imageUid);
        return layoutIt == std::end(m_appData.renderResources().m_imageTextureLayouts) ||
               rendering::TextureDimension::Texture3D == layoutIt->second.dimension;
      };
      auto isViewVolumeImageRendered = [this, view](std::size_t index) {
        const auto imageUid = m_appData.imageUid(index);
        if (!imageUid) {
          return false;
        }
        const auto& volumeImages = view->volumeRenderedImages();
        return !volumeImages.empty() && view->isImageVolumeRendered(*imageUid);
      };

      const ViewOverlayImageCallbacks imageCallbacks{
        m_appData.numImages(),
        [this, view, useThreeDImageSelection, isViewVolumeImageRendered, canImageBeVolumeRendered](std::size_t index) {
          return useThreeDImageSelection ? canImageBeVolumeRendered(index) && isViewVolumeImageRendered(index)
                                         : view->isImageRendered(m_appData, index);
        },
        [this, view, useThreeDImageSelection, canImageBeVolumeRendered](std::size_t index, bool visible) {
          if (useThreeDImageSelection) {
            view->setImageVolumeRendered(m_appData, index, visible && canImageBeVolumeRendered(index));
          }
          else {
            view->setImageRendered(m_appData, index, visible);
          }
        },
        applyVisibleImageSelectionToMatchingViews,
        [this, view](std::size_t index) { return view->isImageUsedForMetric(m_appData, index); },
        [this, view](std::size_t index, bool visible) { view->setImageUsedForMetric(m_appData, index, visible); },
        std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),
        getImageIsVisibleSetting,
        getImageIsActive,
        getImageIsReference,
        canImageBeVolumeRendered,
        getImageIdentificationColor,
        [this, view](std::size_t index) -> std::optional<std::size_t> {
          const auto imageUid = m_appData.imageUid(index);
          if (!imageUid) {
            return std::nullopt;
          }
          const auto& metricImages = view->metricImages();
          const auto it = std::find(metricImages.begin(), metricImages.end(), *imageUid);
          return it == metricImages.end()
                   ? std::nullopt
                   : std::optional{static_cast<std::size_t>(std::distance(metricImages.begin(), it))};
        }};

      const AnatomicalLabelResolution anatomicalLabels = m_appData.resolvedAnatomicalLabels();
      const ViewOverlayModeCallbacks modeCallbacks{
        .viewType = view->viewType(),
        .anatomicalLabelType = anatomicalLabels.type,
        .quadrupedBodyRegion = anatomicalLabels.quadrupedBodyRegion,
        .renderMode = view->renderMode(),
        .threeDSceneContents = view->threeDSceneContents(),
        .intensityProjectionMode = view->intensityProjectionMode(),
        .setViewType = setViewType,
        .setRenderMode = setRenderMode,
        .setThreeDSceneContents =
          [view](ThreeDSceneContents contents) { view->setThreeDSceneContents(std::move(contents)); },
        .setIntensityProjectionMode = setIntensityProjectionMode,
        .renderComparisonModeSettings = renderComparisonModeSettings,
        .applyPresentationToMatchingViews = applyPresentationToMatchingViews,
        .isIsosurfacesPanelVisible = [this]() { return m_appData.guiData().m_showIsosurfacesWindow; },
        .showIsosurfacesPanel = [this]() { m_appData.guiData().m_showIsosurfacesWindow = true; },
        .hideIsosurfacesPanel = [this]() { m_appData.guiData().m_showIsosurfacesWindow = false; },
        .showIsosurfacesPanelForRaycastImage =
          [this, view]() {
            m_appData.guiData().m_suppressIsosurfacesFocusOnNextAppearance = true;
            m_appData.guiData().m_showIsosurfacesWindow = true;
            if (!view->visibleImages().empty()) {
              m_appData.guiData().m_requestedIsosurfacesImageUid = view->visibleImages().front();
            }
          },
        .getThreeDProjectionType = [view]() { return view->threeDState().m_projectionType; },
        .setThreeDProjectionType =
          [view, synchronizeThreeDCamerasFromView](ProjectionType projectionType) {
            view->setThreeDProjectionType(projectionType);
            if (ProjectionType::Orthographic == projectionType) {
              view->threeDState().m_viewPositionFollowsCrosshairs = false;
            }
            synchronizeThreeDCamerasFromView(view);
          },
        .getThreeDFovAngleDegrees =
          [view]() {
            constexpr float k_radiansToDegrees = 57.29577951308232f;
            return k_radiansToDegrees * view->threeDCamera().angle();
          },
        .setThreeDFovAngleDegrees =
          [view, synchronizeThreeDCamerasFromView](float fovDegrees) {
            constexpr float k_defaultPerspectiveFovDegrees = 60.0f;
            constexpr float k_minPerspectiveFovDegrees = 0.5f;
            constexpr float k_maxPerspectiveFovDegrees = 150.0f;
            const float clampedFovDegrees =
              std::clamp(fovDegrees, k_minPerspectiveFovDegrees, k_maxPerspectiveFovDegrees);
            view->threeDCamera().setZoom(k_defaultPerspectiveFovDegrees / clampedFovDegrees);
            view->threeDState().m_perspectiveZoom = view->threeDCamera().getZoom();
            view->threeDState().m_userMovedCamera = true;
            synchronizeThreeDCamerasFromView(view);
          },
        .getThreeDViewPositionFollowsCrosshairs =
          [view]() { return view->threeDState().m_viewPositionFollowsCrosshairs; },
        .setThreeDViewPositionFollowsCrosshairs =
          [this, view, synchronizeThreeDCamerasFromView](bool followsCrosshairs) {
            view->threeDState().m_viewPositionFollowsCrosshairs = followsCrosshairs;
            if (followsCrosshairs) {
              view->threeDState().m_crosshairsFollowOffset = glm::vec3{0.0f};
              camera3d::followCrosshairs(
                view->threeDCamera(),
                view->threeDState(),
                glm::vec3{m_appData.state().worldCrosshairs().worldOrigin()});
            }
            synchronizeThreeDCamerasFromView(view);
          },
        .getThreeDOrbitTargetMode = [view]() { return view->threeDState().m_orbitTargetMode; },
        .setThreeDOrbitTargetMode =
          [view, synchronizeThreeDCamerasFromView](camera3d::OrbitTargetMode mode) {
            view->threeDState().m_orbitTargetMode = mode;
            view->threeDState().m_userMovedCamera = false;
            synchronizeThreeDCamerasFromView(view);
          },
        .areThreeDImagePlanesGloballyEnabled = [this]() { return m_appData.renderSettings().m_showImagePlanesIn3D; },
        .getThreeDImagePlanesVisible = [view]() { return view->threeDState().m_showImagePlanes; },
        .setThreeDImagePlanesVisible = [view](bool visible) { view->threeDState().m_showImagePlanes = visible; },
        .getThreeDImagePlaneOpacityFadeEnabled =
          [this]() { return m_appData.renderSettings().m_modulateImagePlaneOpacityWithViewAngle; },
        .setThreeDImagePlaneOpacityFadeEnabled =
          [this](bool enabled) { m_appData.renderSettings().m_modulateImagePlaneOpacityWithViewAngle = enabled; },
        .getThreeDPlaneSegmentationsVisible =
          [this]() { return m_appData.renderSettings().m_showSegmentationsOnImagePlanesIn3D; },
        .setThreeDPlaneSegmentationsVisible =
          [this](bool visible) { m_appData.renderSettings().m_showSegmentationsOnImagePlanesIn3D = visible; },
        .getThreeDPlaneIsocontoursVisible =
          [this]() { return m_appData.renderSettings().m_showIsocontoursOnImagePlanesIn3D; },
        .setThreeDPlaneIsocontoursVisible =
          [this](bool visible) { m_appData.renderSettings().m_showIsocontoursOnImagePlanesIn3D = visible; },
        .getThreeDCrosshairsVisible = [this]() { return m_appData.renderSettings().m_showCrosshairsIn3D; },
        .setThreeDCrosshairsVisible =
          [this](bool visible) { m_appData.renderSettings().m_showCrosshairsIn3D = visible; },
        .getThreeDImageVolumeBoundsVisible =
          [this]() { return m_appData.renderSettings().m_raycastBackgroundEdgeBrighteningEnabled; },
        .setThreeDImageVolumeBoundsVisible =
          [this](bool visible) { m_appData.renderSettings().m_raycastBackgroundEdgeBrighteningEnabled = visible; },
        .getThreeDCutawayEnabled = [this]() { return m_appData.renderSettings().m_meshCutawayEnabled; },
        .setThreeDCutawayEnabled = [this](bool enabled) { m_appData.renderSettings().m_meshCutawayEnabled = enabled; },
        .isThreeDRenderingSettingsVisible = [this]() { return m_appData.guiData().m_showSettingsWindow; },
        .openThreeDRenderingSettings =
          [this]() {
            m_appData.guiData().m_requestedSettingsTab = GuiData::SettingsTab::Rendering;
            m_appData.guiData().m_suppressSettingsFocusOnNextAppearance = true;
            m_appData.guiData().m_showSettingsWindow = true;
          },
        .hideThreeDRenderingSettings = [this]() { m_appData.guiData().m_showSettingsWindow = false; },
        .exportAsciiClipboardPayload =
          (m_appData.renderSettings().m_asciiEnabled && m_exportAsciiClipboardPayloadForView)
            ? std::function<std::optional<ClipboardPayload>()>(
                [this, viewUid]() { return m_exportAsciiClipboardPayloadForView(viewUid); })
            : std::function<std::optional<ClipboardPayload>()>{}};

      const ViewOverlayProjectionCallbacks projectionCallbacks{
        [this]() { return m_appData.renderSettings().m_intensityProjectionSlabThickness; },
        [this](float thickness) { m_appData.renderSettings().m_intensityProjectionSlabThickness = thickness; },
        [this]() { return m_appData.renderSettings().m_doMaxExtentIntensityProjection; },
        [this](bool set) { m_appData.renderSettings().m_doMaxExtentIntensityProjection = set; },
        [this]() { return m_appData.renderSettings().m_xrayIntensityWindow; },
        [this](float window) { m_appData.renderSettings().m_xrayIntensityWindow = window; },
        [this]() { return m_appData.renderSettings().m_xrayIntensityLevel; },
        [this](float level) { m_appData.renderSettings().m_xrayIntensityLevel = level; },
        [this]() { return m_appData.renderSettings().m_xrayEnergyKeV; },
        [this](float energy) {
          m_appData.renderSettings().setXrayEnergy(energy);
        }};

      renderViewSettingsComboWindow(overlayContext, imageCallbacks, modeCallbacks, projectionCallbacks);

      if (ViewRenderMode::JointHistogram != view->renderMode()) {
        renderViewOrientationToolWindow(
          overlayContext,
          {view->viewType(),
           anatomicalLabels.type,
           anatomicalLabels.quadrupedBodyRegion,
           [&getViewCameraRotation, &viewUid]() { return getViewCameraRotation(viewUid); },
           [&setViewCameraRotation, &viewUid](const glm::quat& q) { return setViewCameraRotation(viewUid, q); },
           [&setViewCameraDirection, &viewUid](const glm::vec3& dir) { return setViewCameraDirection(viewUid, dir); },
           [&getViewNormal, &viewUid]() { return getViewNormal(viewUid); },
           getObliqueViewDirections});
      }
    }
  }

  if (viewOverlayControlExtentsChanged) {
    m_callbackHandler.refreshTwoDViewOverlaySafeFraming();
    if (m_postEmptyGlfwEvent) {
      m_postEmptyGlfwEvent();
    }
  }

  return true;
}
