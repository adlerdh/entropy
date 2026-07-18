#include "rendering/Rendering.h"

#include "common/Types.h"
#include "logic/app/Data.h"
#include "logic/camera/CameraTypes.h"
#include "rendering/PrivateMethods.h"
#include "rendering/RenderData.h"
#include "rendering/vector/VectorDrawing.h"
#include "viewer/ViewModes.h"
#include "viewer/ViewTypes.h"
#include "windowing/Layout.h"
#include "windowing/View.h"
#include "windowing/WindowData.h"

#include <glm/glm.hpp>
#include <glm/vec3.hpp>

#include <utility>

namespace
{
using namespace uuids;
}

void Rendering::renderAllImageBordersForView(
  const View& view,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldOffsetXhairs)
{
  const RenderData& renderData = m_appData.renderData();
  const bool renderBordersInCurrentLayout =
    renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections &&
    (!m_appData.windowData().currentLayout().isLightbox() ||
     renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersectionsInLightboxViews);

  if (!m_nvg || !renderBordersInCurrentLayout) {
    return;
  }

  switch (getShaderGroup(view.renderMode())) {
    case ShaderGroup::Image: {
      const CurrentImages imageSegPairs = getImageAndSegUidsForImageShaders(view.renderedImages());
      for (const auto& imgSegPair : imageSegPairs) {
        drawImageViewIntersections(
          m_nvg,
          miewportViewBounds,
          worldOffsetXhairs,
          m_appData,
          view,
          CurrentImages{imgSegPair},
          true);
        setupOpenGLState();
      }
      break;
    }
    case ShaderGroup::Metric: {
      drawImageViewIntersections(
        m_nvg,
        miewportViewBounds,
        worldOffsetXhairs,
        m_appData,
        view,
        getImageAndSegUidsForMetricShaders(view.metricImages()),
        true);
      setupOpenGLState();
      break;
    }
    case ShaderGroup::Volume:
    case ShaderGroup::None:
    case ShaderGroup::NumElements:
      break;
  }
}

void Rendering::renderAllLandmarksForView(
  const View& view,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldOffsetXhairs)
{
  if (ViewType::ThreeD == view.viewType()) {
    return;
  }

  switch (view.renderMode()) {
    case ViewRenderMode::Image: {
      const CurrentImages imageSegPairs = getImageAndSegUidsForImageShaders(view.renderedImages());
      for (const auto& imgSegPair : imageSegPairs) {
        drawLandmarks(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, CurrentImages{imgSegPair});
        setupOpenGLState();
      }
      break;
    }
    case ViewRenderMode::Checkerboard:
    case ViewRenderMode::Quadrants:
    case ViewRenderMode::Flashlight: {
      const CurrentImages imageSegPairs = getImageAndSegUidsForMetricShaders(view.metricImages()); // guaranteed size 2
      for (const auto& imgSegPair : imageSegPairs) {
        drawLandmarks(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, CurrentImages{imgSegPair});
        setupOpenGLState();
      }
      break;
    }
    case ViewRenderMode::Disabled: {
      return;
    }
    default: {
      // This function guarantees that imageSegPairs has size at least 2:
      drawLandmarks(
        m_nvg,
        miewportViewBounds,
        worldOffsetXhairs,
        m_appData,
        view,
        getImageAndSegUidsForMetricShaders(view.metricImages()));
      setupOpenGLState();
    }
  }
}

void Rendering::renderAllAnnotationsForView(
  const View& view,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldOffsetXhairs)
{
  if (ViewType::ThreeD == view.viewType()) {
    return;
  }

  switch (view.renderMode()) {
    case ViewRenderMode::Image: {
      const CurrentImages imageSegPairs = getImageAndSegUidsForImageShaders(view.renderedImages());
      for (const auto& imgSegPair : imageSegPairs) {
        drawAnnotations(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, CurrentImages{imgSegPair});
        setupOpenGLState();
      }
      break;
    }
    case ViewRenderMode::Checkerboard:
    case ViewRenderMode::Quadrants:
    case ViewRenderMode::Flashlight: {
      const CurrentImages imageSegPairs = getImageAndSegUidsForMetricShaders(view.metricImages()); // guaranteed size 2
      for (const auto& imgSegPair : imageSegPairs) {
        drawAnnotations(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, CurrentImages{imgSegPair});
        setupOpenGLState();
      }
      break;
    }
    case ViewRenderMode::Disabled: {
      return;
    }
    default: {
      // This function guarantees that imageSegPairs has size at least 2:
      drawAnnotations(
        m_nvg,
        miewportViewBounds,
        worldOffsetXhairs,
        m_appData,
        view,
        getImageAndSegUidsForMetricShaders(view.metricImages()));
      setupOpenGLState();
    }
  }
}
