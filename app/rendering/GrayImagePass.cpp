#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "logic/camera/CameraHelpers.h"
#include "rendering/helpers/ImageDrawingHelpers.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/ImageDrawing.h"
#include "rendering/PixelEdgeRenderer.h"
#include "rendering/utility/gl/GLTexture.h"
#include "windowing/View.h"

#include <glm/matrix.hpp>

#include <functional>
#include <list>
#include <optional>

namespace
{

const Uniforms::SamplerIndexType msk_imgTexSampler{0};
const Uniforms::SamplerIndexType msk_imgCmapTexSampler{1};
constexpr float k_imageFloatingPointInterpolationThresholdPxPerVoxel = 128.0f;

bool useFloatingPointLinearInterpolation(
  FloatingPointLinearInterpolationPolicy policy,
  const View& view,
  const Viewport& windowViewport,
  const Image& image,
  float thresholdPxPerVoxel)
{
  switch (policy) {
    case FloatingPointLinearInterpolationPolicy::FixedFunction:
      return false;
    case FloatingPointLinearInterpolationPolicy::FloatingPoint:
      return true;
    case FloatingPointLinearInterpolationPolicy::Automatic: {
      const glm::mat4 viewClip_T_voxel =
        glm::inverse(image.transformations().pixel_T_worldDef() * helper::world_T_clip(view.camera()));
      const float screenPixelsPerVoxel = rendering::image_drawing::maxScreenPixelsPerVoxelAxis(
        viewClip_T_voxel,
        view.windowClip_T_viewClip(),
        windowViewport);
      return rendering::image_drawing::automaticFloatingPointInterpolationEnabled(
        screenPixelsPerVoxel,
        thresholdPxPerVoxel);
    }
  }

  return false;
}

} // namespace

void Rendering::renderGrayImageForImage(
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
  const bool isFixedImage,
  const bool allowImagePostProcessing)
{
  const RenderData& renderData = m_appData.renderData();
  const glm::vec4 deviceViewport = m_appData.windowData().viewport().getDeviceAsVec4();
  const glm::ivec4 defaultViewport{
    static_cast<int>(deviceViewport.x),
    static_cast<int>(deviceViewport.y),
    static_cast<int>(deviceViewport.z),
    static_cast<int>(deviceViewport.w)};
  const bool doXray = IntensityProjectionMode::Xray == view.intensityProjectionMode();
  const std::optional<uuids::uuid> referenceImageUid =
    renderWarped ? activeRenderableDeformationReferenceImageUid(imageUid) : std::nullopt;
  const CurrentImages renderGeometryImages{
    referenceImageUid ? ImgSegPair{*referenceImageUid, std::nullopt} : imgSegPair};

  auto drawGrayImage = [&](const bool disableIntensityProjectionForEdges) {
    GLShaderProgram* program = nullptr;

    switch (image.settings().interpolationMode()) {
      case InterpolationMode::NearestNeighbor: {
        program = &shaderProgramForTextureDimension(
          m_shaderPrograms,
          m_shaderPrograms2D,
          doXray ? (renderWarped ? ShaderProgramType::XrayLinearWarped : ShaderProgramType::XrayLinear)
                 : (renderWarped ? ShaderProgramType::ImageGrayLinearWarped : ShaderProgramType::ImageGrayLinear),
          imageTextureLayout.dimension);
        break;
      }
      case InterpolationMode::Linear: {
        if (doXray) {
          program = &shaderProgramForTextureDimension(
            m_shaderPrograms,
            m_shaderPrograms2D,
            renderWarped ? ShaderProgramType::XrayLinearWarped : ShaderProgramType::XrayLinear,
            imageTextureLayout.dimension);
        }
        else {
          const bool useFloatingPoint = useFloatingPointLinearInterpolation(
            renderData.m_imageGrayFloatingPointInterpolationPolicy,
            view,
            m_appData.windowData().viewport(),
            image,
            k_imageFloatingPointInterpolationThresholdPxPerVoxel);
          const ShaderProgramType shaderType =
            useFloatingPoint
              ? (renderWarped ? ShaderProgramType::ImageGrayLinearFloatingWarped
                              : ShaderProgramType::ImageGrayLinearFloating)
              : (renderWarped ? ShaderProgramType::ImageGrayLinearWarped : ShaderProgramType::ImageGrayLinear);
          program = &shaderProgramForTextureDimension(
            m_shaderPrograms,
            m_shaderPrograms2D,
            shaderType,
            imageTextureLayout.dimension);
        }
        break;
      }
      case InterpolationMode::CubicBsplineConvolution: {
        program = &shaderProgramForTextureDimension(
          m_shaderPrograms,
          m_shaderPrograms2D,
          doXray ? (renderWarped ? ShaderProgramType::XrayCubicWarped : ShaderProgramType::XrayCubic)
                 : (renderWarped ? ShaderProgramType::ImageGrayCubicWarped : ShaderProgramType::ImageGrayCubic),
          imageTextureLayout.dimension);
        break;
      }
    }

    const auto boundTextures = bindScalarImageTextures(imgSegPair);
    const auto boundDefTextures =
      renderWarped ? bindDeformationTextures(*deformationUid) : std::list<std::reference_wrapper<GLTexture>>{};

    program->use();
    {
      program->setSamplerUniform("u_imgTex", msk_imgTexSampler.index);
      program->setSamplerUniform("u_cmapTex", msk_imgCmapTexSampler.index);
      setTexture2DAxesUniforms(*program, imageTextureLayout);

      program->setUniform("u_numCheckers", static_cast<float>(renderData.m_numCheckerboardSquares));
      program->setUniform("u_tex_T_world", uniforms.imgTexture_T_world);

      if (doXray) {
        program->setUniform("u_imgSlope_native_T_texture", uniforms.slope_native_T_texture);
        program->setUniform("u_waterAttenCoeff", renderData.m_waterMassAttenCoeff);
        program->setUniform("u_airAttenCoeff", renderData.m_airMassAttenCoeff);
      }
      else {
        program->setUniform("u_imgSlopeIntercept", uniforms.slopeIntercept_normalized_T_texture);
      }

      const bool useHsv =
        (uniforms.hsvModFactors.x != 0.0f) || (uniforms.hsvModFactors.y != 1.0f) || (uniforms.hsvModFactors.z != 1.0f);
      program->setUniform("u_applyHsvMod", useHsv);
      program->setUniform("u_cmapHsvModFactors", uniforms.hsvModFactors);
      program->setUniform("u_cmapSlopeIntercept", uniforms.cmapSlopeIntercept);
      program->setUniform("u_cmapQuantLevels", uniforms.cmapQuantLevels);
      program->setUniform("u_imgThresholds", uniforms.thresholds);
      program->setUniform("u_imgMinMax", uniforms.minMax);
      program->setUniform("u_imgOpacity", uniforms.imgOpacity);
      program->setUniform("u_quadrants", renderData.m_quadrants);
      program->setUniform("u_showFix", isFixedImage);
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

  if (uniforms.showPixelEdges && allowImagePostProcessing) {
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
      [&]() { drawGrayImage(false); },
      bindPixelEdgeColormap);
  }
  else if (uniforms.showPixelEdges || !uniforms.showEdges || (uniforms.showEdges && uniforms.overlayEdges)) {
    drawGrayImage(uniforms.showEdges);
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
      program->setSamplerUniform("u_imgTex", msk_imgTexSampler.index);
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
