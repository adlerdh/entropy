#include "rendering/Rendering.h"

#include "common/Viewport.h"
#include "common/Types.h"
#include "image/Image.h"
#include "image/ImageHeader.h"
#include "image/ImageSettings.h"
#include "logic/app/Data.h"
#include "rendering/ImageDrawing.h"
#include "rendering/PixelEdgeRenderer.h"
#include "rendering/PrivateMethods.h"
#include "rendering/RenderData.h"
#include "rendering/common/ShaderType.h"
#include "rendering/geometry/PixelEdgeGeometry.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/utility/containers/Uniforms.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/gl/GLTexture.h"
#include "windowing/View.h"
#include "windowing/WindowData.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <uuid.h>

#include <functional>
#include <list>
#include <optional>
#include <string>
#include <vector>

namespace
{

const Uniforms::SamplerIndexVectorType msk_imgRgbaTexSamplers{{0, 1, 2, 3}};
const Uniforms::SamplerIndexType msk_imgCmapTexSampler{1};

} // namespace

void Rendering::renderColorImageForImage(
  const View& view,
  const glm::vec3& worldOffsetXhairs,
  const ImgSegPair& imgSegPair,
  const Image& image,
  const uuids::uuid& imageUid,
  const RenderData::ImageUniforms& uniforms,
  const RenderData::PlanarTextureLayout& imageTextureLayout,
  const bool renderWarped,
  const std::optional<uuids::uuid>& deformationUid,
  const int displayModeUniform,
  const bool isFixedImage)
{
  const RenderData& renderData = m_appData.renderData();
  const std::optional<uuids::uuid> referenceImageUid =
    renderWarped ? activeRenderableDeformationReferenceImageUid(imageUid) : std::nullopt;
  const CurrentImages renderGeometryImages{
    referenceImageUid ? ImgSegPair{*referenceImageUid, std::nullopt} : imgSegPair};
  const glm::vec4 deviceViewport = m_appData.windowData().viewport().getDeviceAsVec4();
  const glm::ivec4 defaultViewport{
    static_cast<int>(deviceViewport.x),
    static_cast<int>(deviceViewport.y),
    static_cast<int>(deviceViewport.z),
    static_cast<int>(deviceViewport.w)};

  auto drawColorImage = [&](const bool disableIntensityProjectionForEdges) {
    GLShaderProgram* program = nullptr;

    switch (image.settings().colorInterpolationMode()) {
      case InterpolationMode::NearestNeighbor:
      case InterpolationMode::Linear: {
        program = &shaderProgramForTextureDimension(
          m_shaderPrograms,
          m_shaderPrograms2D,
          renderWarped ? ShaderProgramType::ImageColorLinearWarped : ShaderProgramType::ImageColorLinear,
          imageTextureLayout.dimension);
        break;
      }
      case InterpolationMode::CubicBsplineConvolution: {
        program = &shaderProgramForTextureDimension(
          m_shaderPrograms,
          m_shaderPrograms2D,
          renderWarped ? ShaderProgramType::ImageColorCubicWarped : ShaderProgramType::ImageColorCubic,
          imageTextureLayout.dimension);
        break;
      }
    }

    const auto boundTextures = bindColorImageTextures(imgSegPair);
    const auto boundDefTextures =
      renderWarped ? bindDeformationTextures(*deformationUid) : std::list<std::reference_wrapper<GLTexture>>{};

    program->use();
    {
      program->setSamplerUniform("u_imgTex", msk_imgRgbaTexSamplers);
      program->setSamplerUniform("u_cmapTex", msk_imgCmapTexSampler.index);
      setTexture2DAxesUniforms(*program, imageTextureLayout);
      program->setUniform("u_tex2DAxes[2]", textureAxesForProgramSlot(imageTextureLayout));
      program->setUniform("u_tex2DAxes[3]", textureAxesForProgramSlot(imageTextureLayout));

      program->setUniform("u_numCheckers", static_cast<float>(renderData.m_numCheckerboardSquares));
      program->setUniform("u_tex_T_world", uniforms.imgTexture_T_world);
      program->setUniform("u_imgSlopeIntercept", uniforms.slopeInterceptRgba_normalized_T_texture);
      program->setUniform("u_imgThresholds", uniforms.thresholdsRgba);
      program->setUniform("u_imgMinMax", uniforms.minMaxRgba);

      const bool forceAlphaToOne = image.settings().ignoreAlpha() || 3 == image.header().numComponentsPerPixel();
      program->setUniform("u_alphaIsOne", forceAlphaToOne);
      program->setUniform("u_imgOpacity", uniforms.imgOpacityRgba);
      program->setUniform("u_quadrants", renderData.m_quadrants);
      program->setUniform("u_showFix", isFixedImage); // ignored if not checkerboard or quadrants
      program->setUniform("u_renderMode", displayModeUniform);
      if (renderWarped) {
        setDeformationUniforms(*program, imageUid, *deformationUid, uniforms.imgTexture_T_world);
      }

      renderOneImage(view, worldOffsetXhairs, *program, renderGeometryImages, disableIntensityProjectionForEdges);
    }
    program->stopUse();

    unbindTextures(boundDefTextures);
    unbindTextures(boundTextures);
  };

  if (uniforms.showPixelEdges) {
    const PixelEdgeRenderer::ViewRect viewRect =
      rendering::pixel_edge::computeViewRect(view.windowClipViewport(), deviceViewport);

    auto bindPixelEdgeColormap = [&]() {
      auto& mutableRenderData = m_appData.renderData();
      const auto cmapUid = m_appData.imageColorMapUid(image.settings().colorMapIndex());
      if (cmapUid) {
        mutableRenderData.m_colormapTextures.at(*cmapUid).bind(msk_imgCmapTexSampler.index);
      }
      else if (!mutableRenderData.m_colormapTextures.empty()) {
        mutableRenderData.m_colormapTextures.begin()->second.bind(msk_imgCmapTexSampler.index);
      }
    };

    m_pixelEdgeRenderer.render(
      m_shaderPrograms,
      defaultViewport,
      viewRect,
      uniforms,
      [&]() { drawColorImage(false); },
      bindPixelEdgeColormap);
  }
  else if (uniforms.showPixelEdges || !uniforms.showEdges || (uniforms.showEdges && uniforms.overlayEdges)) {
    drawColorImage(uniforms.showEdges);
  }

  if (!uniforms.showPixelEdges && uniforms.showEdges) {
    GLShaderProgram* program = nullptr;
    switch (image.settings().interpolationMode()) {
      case InterpolationMode::NearestNeighbor:
      case InterpolationMode::Linear: {
        program = &shaderProgramForTextureDimension(
          m_shaderPrograms,
          m_shaderPrograms2D,
          renderWarped ? ShaderProgramType::EdgeSobelLinearWarped : ShaderProgramType::EdgeSobelLinear,
          imageTextureLayout.dimension);
        break;
      }
      case InterpolationMode::CubicBsplineConvolution: {
        program = &shaderProgramForTextureDimension(
          m_shaderPrograms,
          m_shaderPrograms2D,
          renderWarped ? ShaderProgramType::EdgeSobelCubicWarped : ShaderProgramType::EdgeSobelCubic,
          imageTextureLayout.dimension);
        break;
      }
    }

    const auto boundTextures = bindScalarImageTextures(imgSegPair);
    const auto boundDefTextures =
      renderWarped ? bindDeformationTextures(*deformationUid) : std::list<std::reference_wrapper<GLTexture>>{};

    program->use();
    {
      program->setSamplerUniform("u_imgTex", 0);
      program->setSamplerUniform("u_cmapTex", msk_imgCmapTexSampler.index);
      setTexture2DAxesUniforms(*program, imageTextureLayout);

      program->setUniform("u_numCheckers", static_cast<float>(renderData.m_numCheckerboardSquares));
      program->setUniform("u_tex_T_world", uniforms.imgTexture_T_world);
      program->setUniform("u_imgSlopeIntercept", uniforms.largestSlopeIntercept);
      program->setUniform("u_imgThresholds", uniforms.thresholds);
      program->setUniform("u_imgMinMax", uniforms.minMax);
      program->setUniform("u_imgOpacity", uniforms.imgOpacity);
      program->setUniform("u_cmapSlopeIntercept", uniforms.cmapSlopeIntercept);
      program->setUniform("u_quadrants", renderData.m_quadrants);
      program->setUniform("u_showFix", isFixedImage);
      program->setUniform("u_renderMode", displayModeUniform);
      program->setUniform("u_thresholdEdges", uniforms.thresholdEdges);
      program->setUniform("u_edgeMagnitude", uniforms.edgeMagnitude);
      program->setUniform("u_colormapEdges", uniforms.colormapEdges);
      program->setUniform("u_edgeColor", uniforms.edgeColor);
      if (renderWarped) {
        setDeformationUniforms(*program, imageUid, *deformationUid, uniforms.imgTexture_T_world);
      }

      renderOneImage(view, worldOffsetXhairs, *program, renderGeometryImages, uniforms.showEdges);
    }
    program->stopUse();

    unbindTextures(boundDefTextures);
    unbindTextures(boundTextures);
  }
}
