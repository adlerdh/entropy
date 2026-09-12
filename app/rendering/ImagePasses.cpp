#include "rendering/Rendering.h"
#include "rendering/gl/OpenGLRenderState.h"

#include "common/Types.h"
#include "common/UuidUtility.h"
#include "image/Image.h"
#include "image/ImageSettings.h"
#include "logic/app/Data.h"
#include "logic/camera/CameraTypes.h"
#include "rendering/ImageDrawing.h"
#include "rendering/PrivateMethods.h"
#include "rendering/RenderResources.h"
#include "rendering/RenderSettings.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/gl/GLShaderProgram.h"
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
  bool showEdges,
  const bool metricUsesWorldSampling)
{
  auto getImage = [this](const std::optional<uuid>& imageUid) -> const Image* {
    return (imageUid ? m_appData.image(*imageUid) : nullptr);
  };

  const auto& resources = m_appData.renderResources();
  const auto& settings = m_appData.renderSettings();

  drawImageQuad(
    program,
    view.renderMode(),
    resources.m_quad,
    view,
    m_appData.windowData().viewport(),
    worldOffsetXhairs,
    settings.m_flashlightRadius,
    settings.m_flashlightOverlays,
    settings.m_intensityProjectionSlabThickness,
    settings.m_doMaxExtentIntensityProjection,
    settings.m_xrayIntensityWindow,
    settings.m_xrayIntensityLevel,
    imageSegPairs,
    getImage,
    showEdges,
    metricUsesWorldSampling);
}

void Rendering::renderOneImage_overlays(
  const View& view,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldOffsetXhairs,
  const CurrentImages& imageSegPairs,
  bool renderLandmarkAndAnnotationOverlays,
  bool renderImageBorders)
{
  const auto& renderSettings = m_appData.renderSettings();
  const bool allowLandmarksAndAnnotations = renderLandmarkAndAnnotationOverlays && ViewType::ThreeD != view.viewType();
  const bool renderBordersInCurrentLayout =
    renderImageBorders && renderSettings.m_globalSliceIntersectionParams.renderInactiveImageViewIntersections &&
    (!m_appData.windowData().currentLayout().isLightbox() ||
     renderSettings.m_globalSliceIntersectionParams.renderInactiveImageViewIntersectionsInLightboxViews);

  if (allowLandmarksAndAnnotations && !renderSettings.m_globalLandmarkParams.renderOnTopOfAllImagePlanes) {
    drawLandmarks(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, imageSegPairs);
    rendering::restoreOpenGLRenderState();
  }

  if (allowLandmarksAndAnnotations && !renderSettings.m_globalAnnotationParams.renderOnTopOfAllImagePlanes) {
    drawAnnotations(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, imageSegPairs);
    rendering::restoreOpenGLRenderState();
  }

  if (renderBordersInCurrentLayout) {
    drawImageViewIntersections(m_nvg, miewportViewBounds, worldOffsetXhairs, m_appData, view, imageSegPairs, true);
    rendering::restoreOpenGLRenderState();
  }
}

void Rendering::renderAllImagesForView(
  const View& view,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldOffsetXhairs,
  bool renderLandmarkAndAnnotationOverlays,
  bool renderImageBorders,
  bool allowScreenPixelEdgePostProcessing)
{
  const rendering::RenderResources& resources = m_appData.renderResources();

  if (ViewType::ThreeD == view.viewType()) {
    clearMeshViewBackgroundForView(view);

    const ThreeDSceneContents& contents = view.threeDSceneContents();
    const bool renderSegmentations = contents.contains(ThreeDSceneContent::Segmentations);
    const bool renderIsosurfaces = contents.contains(ThreeDSceneContent::Isosurfaces);
    const bool renderImportedMeshes = contents.contains(ThreeDSceneContent::ImportedMeshes);
    bool renderedSurface = false;

    if (renderImportedMeshes || (renderSegmentations && renderIsosurfaces)) {
      renderedSurface = renderCombinedSurfaceMeshesForView(view);
    }
    else if (renderIsosurfaces) {
      renderedSurface = renderVolumeImagesForView(view);
    }
    else if (renderSegmentations) {
      renderedSurface = renderSegmentationMeshesForView(view);
    }

    if (!renderedSurface) {
      renderMeshImagePlanesAndCrosshairsForView(view);
    }
    renderMeshLandmarksForView(view);
    return;
  }

  switch (getTwoDShaderGroup(view.renderMode())) {
    case ShaderGroup::Image: {
      CurrentImages imageSegPairs;
      CurrentImages sourceImages;

      const auto appendSourceImages = [&sourceImages](const auto& imageUids) {
        std::transform(imageUids.begin(), imageUids.end(), std::back_inserter(sourceImages), [](const auto& imageUid) {
          return ImgSegPair{imageUid, std::nullopt};
        });
      };

      int displayModeUniform = 0;

      if (ViewRenderMode::Image == view.renderMode()) {
        displayModeUniform = 0;
        imageSegPairs = getImageAndSegUidsForImageShaders(view.renderedImages());
        appendSourceImages(view.renderedImages());
      }
      else if (ViewRenderMode::Checkerboard == view.renderMode()) {
        displayModeUniform = 1;
        imageSegPairs = getImageAndSegUidsForMetricShaders(view.metricImages()); // guaranteed size 2
        appendSourceImages(view.metricImages());
      }
      else if (ViewRenderMode::Quadrants == view.renderMode()) {
        displayModeUniform = 2;
        imageSegPairs = getImageAndSegUidsForMetricShaders(view.metricImages());
        appendSourceImages(view.metricImages());
      }
      else if (ViewRenderMode::Flashlight == view.renderMode()) {
        displayModeUniform = 3;
        imageSegPairs = getImageAndSegUidsForMetricShaders(view.metricImages());
        appendSourceImages(view.metricImages());
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
          spdlog::error("Cannot render image {} because it is missing from application data", imgUid);
          return;
        }

        const rendering::RenderDerivedData::ImageUniforms& U = m_appData.renderDerivedData().imageUniforms.at(imgUid);
        const std::optional<uuid> deformationUid = activeRenderableDeformationUid(imgUid);
        const bool renderWarped = deformationUid.has_value();
        const rendering::PlanarTextureLayout imageTextureLayout =
          rendering::textureLayoutOrDefault(resources.m_imageTextureLayouts, imgSegPair.first);

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
            allowScreenPixelEdgePostProcessing);

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
            isFixedImage,
            allowScreenPixelEdgePostProcessing);
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

      renderImportedMeshIntersectionsForView(view, miewportViewBounds, worldOffsetXhairs, imageSegPairs);
      renderVectorWarpedGridOverlaysForView(view, worldOffsetXhairs, displayModeUniform, sourceImages);
      break;
    }

    case ShaderGroup::Metric: {
      renderMetricImagesForView(view, worldOffsetXhairs);
      renderImportedMeshIntersectionsForView(
        view,
        miewportViewBounds,
        worldOffsetXhairs,
        getImageAndSegUidsForMetricShaders(view.metricImages()));
      break;
    }

    case ShaderGroup::None:
    default: {
      return;
    }
  }
}
