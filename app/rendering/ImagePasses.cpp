#include "rendering/Rendering.h"

#include "common/Types.h"
#include "image/Image.h"
#include "image/ImageSettings.h"
#include "logic/app/Data.h"
#include "logic/camera/CameraTypes.h"
#include "rendering/ImageDrawing.h"
#include "rendering/PrivateMethods.h"
#include "rendering/RenderData.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/vector/VectorDrawing.h"
#include "viewer/ViewModes.h"
#include "viewer/ViewTypes.h"
#include "windowing/Layout.h"
#include "windowing/View.h"
#include "windowing/WindowData.h"

#include <glm/glm.hpp>
#include <uuid.h>
#include <spdlog/spdlog.h>

#include <functional>
#include <list>
#include <optional>
#include <unordered_map>
#include <utility>

namespace
{
using namespace uuids;

} // namespace

void Rendering::renderOneImage(
  const View& view,
  const glm::vec3& worldOffsetXhairs,
  GLShaderProgram& program,
  const CurrentImages& imageSegPairs,
  bool showEdges)
{
  auto getImage = [this](const std::optional<uuid>& imageUid) -> const Image* {
    return (imageUid ? m_appData.image(*imageUid) : nullptr);
  };

  auto& R = m_appData.renderData();

  drawImageQuad(
    program,
    view.renderMode(),
    R.m_quad,
    view,
    m_appData.windowData().viewport(),
    worldOffsetXhairs,
    R.m_flashlightRadius,
    R.m_flashlightOverlays,
    R.m_intensityProjectionSlabThickness,
    R.m_doMaxExtentIntensityProjection,
    R.m_xrayIntensityWindow,
    R.m_xrayIntensityLevel,
    imageSegPairs,
    getImage,
    showEdges);
}

void Rendering::renderOneImage_overlays(
  const View& view,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldOffsetXhairs,
  const CurrentImages& imageSegPairs,
  bool renderLandmarkAndAnnotationOverlays,
  bool renderImageBorders)
{
  auto& renderData = m_appData.renderData();
  const bool allowLandmarksAndAnnotations = renderLandmarkAndAnnotationOverlays && ViewType::ThreeD != view.viewType();
  const bool renderBordersInCurrentLayout =
    renderImageBorders && renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections &&
    (!m_appData.windowData().currentLayout().isLightbox() ||
     renderData.m_globalSliceIntersectionParams.renderInactiveImageViewIntersectionsInLightboxViews);

  if (allowLandmarksAndAnnotations && !renderData.m_globalLandmarkParams.renderOnTopOfAllImagePlanes) {
    drawLandmarks(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, imageSegPairs);
    setupOpenGLState();
  }

  if (allowLandmarksAndAnnotations && !renderData.m_globalAnnotationParams.renderOnTopOfAllImagePlanes) {
    drawAnnotations(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, imageSegPairs);
    setupOpenGLState();
  }

  if (renderBordersInCurrentLayout) {
    drawImageViewIntersections(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, imageSegPairs, true);
    setupOpenGLState();
  }
}

void Rendering::renderAllImagesForView(
  const View& view,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldOffsetXhairs,
  bool renderLandmarkAndAnnotationOverlays,
  bool renderImageBorders,
  bool allowImagePostProcessing)
{
  const RenderData& R = m_appData.renderData();

  switch (getShaderGroup(view.renderMode())) {
    case ShaderGroup::Image: {
      CurrentImages imageSegPairs;
      CurrentImages sourceImages;

      int displayModeUniform = 0;

      if (ViewRenderMode::Image == view.renderMode()) {
        displayModeUniform = 0;
        imageSegPairs = getImageAndSegUidsForImageShaders(view.renderedImages());
        for (const auto& imageUid : view.renderedImages()) {
          sourceImages.emplace_back(ImgSegPair{imageUid, std::nullopt});
        }
      }
      else if (ViewRenderMode::Checkerboard == view.renderMode()) {
        displayModeUniform = 1;
        imageSegPairs = getImageAndSegUidsForMetricShaders(view.metricImages()); // guaranteed size 2
        for (const auto& imageUid : view.metricImages()) {
          sourceImages.emplace_back(ImgSegPair{imageUid, std::nullopt});
        }
      }
      else if (ViewRenderMode::Quadrants == view.renderMode()) {
        displayModeUniform = 2;
        imageSegPairs = getImageAndSegUidsForMetricShaders(view.metricImages());
        for (const auto& imageUid : view.metricImages()) {
          sourceImages.emplace_back(ImgSegPair{imageUid, std::nullopt});
        }
      }
      else if (ViewRenderMode::Flashlight == view.renderMode()) {
        displayModeUniform = 3;
        imageSegPairs = getImageAndSegUidsForMetricShaders(view.metricImages());
        for (const auto& imageUid : view.metricImages()) {
          sourceImages.emplace_back(ImgSegPair{imageUid, std::nullopt});
        }
      }

      // The first image in the stack is the fixed one:
      bool isFixedImage = true;

      for (const auto& imgSegPair : imageSegPairs) {
        if (!imgSegPair.first) {
          isFixedImage = false;
          continue;
        }

        const uuid& imgUid = *imgSegPair.first;
        const Image* img = m_appData.image(imgUid);
        if (!img) {
          spdlog::error("Null image during render");
          return;
        }

        const RenderData::ImageUniforms& U = R.m_uniforms.at(imgUid);
        const std::optional<uuid> deformationUid = activeRenderableDeformationUid(imgUid);
        const bool renderWarped = deformationUid.has_value();
        const RenderData::PlanarTextureLayout imageTextureLayout =
          rendering::textureLayoutOrDefault(R.m_imageTextureLayouts, imgSegPair.first);

        if (
          ComponentRenderMode::VectorDirectionColor == img->settings().componentRenderMode() ||
          ComponentRenderMode::VectorSignedNormalProjection == img->settings().componentRenderMode() ||
          ComponentRenderMode::VectorPlanarProjectionColor == img->settings().componentRenderMode())
        {
          renderVectorImageForImage(
            view,
            worldOffsetXhairs,
            imgSegPair,
            *img,
            U,
            imageTextureLayout,
            displayModeUniform,
            isFixedImage);
        }
        else if (!img->settings().displayImageAsColor()) {
          renderGrayImageForImage(
            view,
            worldOffsetXhairs,
            imgSegPair,
            *img,
            imgUid,
            U,
            imageTextureLayout,
            renderWarped,
            deformationUid,
            displayModeUniform,
            isFixedImage,
            allowImagePostProcessing);

          renderIsoContoursForImage(
            view,
            worldOffsetXhairs,
            imgSegPair,
            *img,
            imgUid,
            U,
            imageTextureLayout,
            renderWarped,
            deformationUid,
            displayModeUniform,
            isFixedImage);
        }
        else {
          renderColorImageForImage(
            view,
            worldOffsetXhairs,
            imgSegPair,
            *img,
            imgUid,
            U,
            imageTextureLayout,
            renderWarped,
            deformationUid,
            displayModeUniform,
            isFixedImage);
        }

        renderSegmentationForImage(
          view,
          worldOffsetXhairs,
          imgSegPair,
          imgUid,
          U,
          renderWarped,
          deformationUid,
          displayModeUniform,
          isFixedImage);

        renderBrushPreview(view, worldOffsetXhairs, imgSegPair);

        // Render the annotation and landmark overlays:
        renderOneImage_overlays(
          view,
          miewportViewBounds,
          worldOffsetXhairs,
          CurrentImages{imgSegPair},
          renderLandmarkAndAnnotationOverlays,
          renderImageBorders);

        isFixedImage = false;
      }

      renderVectorWarpedGridOverlaysForView(view, worldOffsetXhairs, displayModeUniform, sourceImages);
      break;
    }

    case ShaderGroup::Metric: {
      renderMetricImagesForView(view, worldOffsetXhairs);
      break;
    }

    case ShaderGroup::Volume: {
      renderVolumeImagesForView(view);
      break;
    }

    case ShaderGroup::None:
    default: {
      return;
    }
  }
}
