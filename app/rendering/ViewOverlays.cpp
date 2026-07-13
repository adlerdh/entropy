#include "rendering/Rendering.h"

#include "common/MathFuncs.h"
#include "logic/app/Data.h"
#include "logic/app/DataHelper.h"
#include "logic/camera/CameraFrustumSlice.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/camera/MathUtility.h"
#include "logic/states/annotation/AnnotationStateHelpers.h"
#include "logic/states/FsmList.hpp"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/vector/FrustumOverlayDrawing.h"
#include "rendering/vector/LightboxOverlayDrawing.h"
#include "rendering/vector/ScaleBarDrawing.h"
#include "rendering/vector/ViewOverlayDrawing.h"
#include "rendering/vector/VectorDrawing.h"
#include "windowing/View.h"

#include <glm/glm.hpp>
#include <nanovg.h>

#include <cmath>
#include <limits>
#include <list>
#include <optional>

namespace
{
using namespace uuids;

bool hasVisibleIsosurfaceForView(const AppData& appData, const View& view)
{
  if (ViewType::ThreeD != view.viewType()) {
    return false;
  }

  for (const uuid& imageUid :
       rendering::raycastableImageUids(view.visibleImages(), appData.renderData().m_imageTextureLayouts))
  {
    const Image* image = appData.image(imageUid);
    if (!image) {
      continue;
    }

    const ImageSettings& settings = image->settings();
    if (!settings.globalVisibility() || !settings.visibility() || !settings.isosurfacesVisible()) {
      continue;
    }

    const uint32_t activeComponent = settings.activeComponent();
    for (const auto& surfaceUid : appData.isosurfaceUids(imageUid, activeComponent)) {
      const Isosurface* surface = appData.isosurface(imageUid, activeComponent, surfaceUid);
      if (surface && surface->visible && surface->opacity > 0.0f) {
        return true;
      }
    }
  }

  return false;
}

bool canDrawFrustumForThreeDView(const AppData& appData, const View& view)
{
  return ViewType::ThreeD == view.viewType() && ViewRenderMode::VolumeRender == view.renderMode() &&
         hasVisibleIsosurfaceForView(appData, view);
}

const View* activeThreeDFrustumSource(const AppData& appData)
{
  const WindowData& windowData = appData.windowData();
  const std::optional<uuid>& lastInteractedViewUid = appData.renderData().m_lastInteractedThreeDViewUid;
  if (lastInteractedViewUid) {
    const View* view = windowData.getCurrentView(*lastInteractedViewUid);
    if (view && canDrawFrustumForThreeDView(appData, *view)) {
      return view;
    }
  }

  for (const uuid& viewUid : windowData.currentViewUids()) {
    const View* view = windowData.getCurrentView(viewUid);
    if (view && canDrawFrustumForThreeDView(appData, *view)) {
      return view;
    }
  }

  return nullptr;
}

float lightboxOffsetUnitReferenceMm(const AppData& appData, const WindowData& windowData)
{
  float minNonzeroOffsetMm = std::numeric_limits<float>::max();
  for (const auto& viewUid : windowData.currentViewUids()) {
    const View* view = windowData.getCurrentView(viewUid);
    if (!view) {
      continue;
    }

    const float offsetMm = data::computeViewOffsetDistance(
      appData,
      view->offsetSetting(),
      helper::worldDirection(view->camera(), Directions::View::Front));
    const float absOffsetMm = std::abs(offsetMm);
    if (absOffsetMm > std::numeric_limits<float>::epsilon()) {
      minNonzeroOffsetMm = std::min(minNonzeroOffsetMm, absOffsetMm);
    }
  }
  return minNonzeroOffsetMm == std::numeric_limits<float>::max() ? 0.0f : minNonzeroOffsetMm;
}
} // namespace

void Rendering::renderVectorOverlays()
{
  if (!m_nvg) {
    return;
  }

  const WindowData& windowData = m_appData.windowData();
  const Viewport& windowVP = windowData.viewport();
  const auto& R = m_appData.renderData();

  if (ProjectLoadState::Loading == m_appData.state().projectLoadState()) {
    startNvgFrame(m_nvg, windowVP);
    drawLoadingOverlay(m_nvg, windowVP);
    endNvgFrame(m_nvg);
    return;

    /*
    nvgFontSize( m_nvg, 64.0f );
    const char* txt = "Text me up.";
    float bounds[4];
    nvgTextBounds( m_nvg, 10, 10, txt, NULL, bounds );
    nvgBeginPath( m_nvg );
    // nvgRoundedRect( m_nvg, bounds[0],bounds[1], bounds[2]-bounds[0], bounds[3]-bounds[1], 0 );
    nvgText( m_nvg, vp.width() / 2, vp.height() / 2, "Loading images...", NULL );
    nvgFill( m_nvg );
    */
  }

  if (ProjectLoadState::Loaded != m_appData.state().projectLoadState() || 0 == windowData.numLayouts()) {
    return;
  }

  startNvgFrame(m_nvg, windowVP);

  // Transformation used for anatomical labels in the view
  glm::mat4 world_T_refSubject(1.0f);

  if (m_appData.settings().lockAnatomicalCoordinateAxesWithReferenceImage()) {
    // Align to reference image:
    if (const Image* refImage = m_appData.refImage()) {
      world_T_refSubject = refImage->transformations().worldDef_T_subject();
    }
  }

  /// @note This is the transformation used for rotating the crosshairs.
  /// It was designed initialially to show the mapping of subject (usually reference subject)
  /// in the view/camera. However, it need not be a subject-to-world transformation.
  /// We can also use the xhairs/frame-to-world transformation!!!

  // Transformation used for crosshairs labels:
  // (If the view is set to NOT align with crosshairs, then it equals world_T_refSubject)
  const glm::mat4 world_T_crosshairsFrame =
    (ViewAlignmentMode::Crosshairs == m_appData.windowData().viewAlignmentMode())
      ? m_appData.state().worldCrosshairs().world_T_frame()
      : world_T_refSubject;

  const float lightboxOffsetUnitReference = (R.m_showLightboxOffsetLabels && windowData.currentLayout().isLightbox())
                                              ? lightboxOffsetUnitReferenceMm(m_appData, windowData)
                                              : 0.0f;
  const View* threeDFrustumSource =
    R.m_showThreeDCameraFrustumIn2DViews ? activeThreeDFrustumSource(m_appData) : nullptr;

  for (const auto& viewUid : windowData.currentViewUids()) {
    const View* view = windowData.getCurrentView(viewUid);
    if (!view) {
      continue;
    }

    // Bounds of the view frame in Miewport space:
    const auto miewportViewBounds =
      helper::computeMiewportFrameBounds(view->windowClipViewport(), windowVP.getAsVec4());
    const glm::vec3 worldViewFront = helper::worldDirection(view->camera(), Directions::View::Front);
    const glm::vec3 worldXhairsOffset =
      m_appData.state().worldCrosshairs().worldOrigin() +
      data::computeViewOffsetDistance(m_appData, view->offsetSetting(), worldViewFront) * worldViewFront;

    // Do not render vector overlays when view is disabled
    if (m_showOverlays && ViewRenderMode::Disabled != view->renderMode()) {
      // Label positions are based on the reference image transform (world_T_refSubject)
      const auto labelPosInfo_forLabels = math::computeAnatomicalLabelPosInfo(
        miewportViewBounds,
        windowVP,
        view->camera(),
        world_T_refSubject,
        view->windowClip_T_viewClip(),
        m_appData.state().worldCrosshairs().worldOrigin());

      if (
        ViewType::ThreeD != view->viewType() &&
        (ViewRenderMode::Image == view->renderMode() || ViewRenderMode::Checkerboard == view->renderMode() ||
         ViewRenderMode::Quadrants == view->renderMode() || ViewRenderMode::Flashlight == view->renderMode()))
      {
        const std::list<uuid>& vectorOverlayImages =
          ViewRenderMode::Image == view->renderMode() ? view->renderedImages() : view->metricImages();
        drawVectorFieldArrows(m_nvg, miewportViewBounds, worldXhairsOffset, m_appData, *view, vectorOverlayImages);
      }

      if (ViewType::ThreeD == view->viewType() && !hasVisibleIsosurfaceForView(m_appData, *view)) {
        drawEmptyThreeDViewHint(m_nvg, miewportViewBounds);
      }

      if (threeDFrustumSource && ViewType::ThreeD != view->viewType() && ViewRenderMode::Disabled != view->renderMode())
      {
        const glm::vec3 planeNormal = helper::worldDirection(view->camera(), Directions::View::Front);
        const camera3d::FrustumSliceOverlay overlay =
          camera3d::frustumSliceOverlay(threeDFrustumSource->threeDCamera(), worldXhairsOffset, planeNormal);
        if (!overlay.segments.empty()) {
          drawThreeDCameraFrustumOverlay(
            m_nvg,
            miewportViewBounds,
            windowVP,
            *view,
            overlay,
            R.m_threeDCameraFrustumColor);
        }
      }

      const bool showCrosshairsInCurrentLayout =
        R.m_showCrosshairs && (!windowData.currentLayout().isLightbox() || R.m_showCrosshairsInLightboxViews);

      // Do not render crosshairs in volume rendering mode
      if (showCrosshairsInCurrentLayout && ViewRenderMode::VolumeRender != view->renderMode()) {
        // If aligning views to crosshairs, then crosshairs are based on the crosshairs
        // transform (world_T_crosshairsFrame)
        const auto labelPosInfo_forXhairs =
          (ViewAlignmentMode::Crosshairs == m_appData.windowData().viewAlignmentMode())
            ? math::computeAnatomicalLabelPosInfo(
                miewportViewBounds,
                windowVP,
                view->camera(),
                world_T_crosshairsFrame,
                view->windowClip_T_viewClip(),
                m_appData.state().worldCrosshairs().worldOrigin())
            : labelPosInfo_forLabels;

        drawCrosshairs(m_nvg, miewportViewBounds, *view, R.m_crosshairsColor, labelPosInfo_forXhairs);
      }

      const bool allowAnatomicalLabelsInCurrentLayout =
        !windowData.currentLayout().isLightbox() || R.m_showAnatomicalLabelsInLightboxViews;
      if (
        R.m_showAnatomicalLabels && allowAnatomicalLabelsInCurrentLayout &&
        AnatomicalLabelType::Disabled != R.m_anatomicalLabelType)
      {
        const bool isOblique = ViewType::Oblique == view->viewType();
        drawAnatomicalLabels(
          m_nvg,
          miewportViewBounds,
          isOblique,
          R.m_anatomicalLabelColor,
          R.m_anatomicalLabelType,
          R.m_anatomicalLabelScale,
          labelPosInfo_forLabels);
      }

      const bool allowScaleBarsInCurrentLayout =
        !windowData.currentLayout().isLightbox() || R.m_showScaleBarsInLightboxViews;
      if (R.m_showScaleBars && allowScaleBarsInCurrentLayout && ViewRenderMode::VolumeRender != view->renderMode()) {
        drawScaleBar(
          m_nvg,
          miewportViewBounds,
          windowVP,
          *view,
          R.m_scaleBarColor,
          R.m_scaleBarPosition,
          R.m_scaleBarOrientation,
          R.m_scaleBarTicks,
          R.m_scaleBarTargetFraction,
          R.m_scaleBarMarginPx,
          static_cast<int>(m_appData.guiData().m_coordsPrecision));
      }

      if (
        R.m_showLightboxOffsetLabels && windowData.currentLayout().isLightbox() &&
        ViewRenderMode::VolumeRender != view->renderMode())
      {
        drawLightboxOffsetLabel(
          m_nvg,
          miewportViewBounds,
          m_appData,
          *view,
          lightboxOffsetUnitReference,
          R.m_lightboxOffsetLabelColor);
      }
    }

    ViewOutlineMode outlineMode = ViewOutlineMode::None;

    if (state::annot::isInStateWhereViewSelectionsVisible() && ASM::current_state_ptr) {
      const auto hoveredViewUid = ASM::current_state_ptr->hoveredViewUid();
      const auto selectedViewUid = ASM::current_state_ptr->selectedViewUid();

      if (selectedViewUid && (viewUid == *selectedViewUid)) {
        outlineMode = ViewOutlineMode::Selected;
      }
      else if (hoveredViewUid && (viewUid == *hoveredViewUid)) {
        outlineMode = ViewOutlineMode::Hovered;
      }
    }

    drawViewOutline(m_nvg, miewportViewBounds, outlineMode);
  }

  drawWindowOutline(m_nvg, windowVP);

  endNvgFrame(m_nvg);
}

bool Rendering::showVectorOverlays() const
{
  return m_showOverlays;
}

void Rendering::setShowVectorOverlays(bool show)
{
  m_showOverlays = show;
}
