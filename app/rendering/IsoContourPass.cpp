#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "logic/SurfaceUtility.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/ImageDrawing.h"
#include "rendering/utility/gl/GLTexture.h"
#include "windowing/View.h"

#include <spdlog/spdlog.h>

#include <functional>
#include <list>
#include <optional>

namespace
{

const Uniforms::SamplerIndexType msk_imgTexSampler{0};

} // namespace

void Rendering::renderIsoContoursForImage(
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
  const ImageSettings& imageSettings = image.settings();

  if (!imageSettings.isosurfacesVisible() || !imageSettings.showIsocontoursIn2D()) {
    return;
  }

  const auto& viewport = m_appData.windowData().viewport();
  const glm::vec2 windowSize{viewport.width(), viewport.height()};
  const glm::vec2 viewSize = 0.5f * glm::vec2{view.windowClipViewport()[2], view.windowClipViewport()[3]} * windowSize;
  const uint32_t activeComponent = imageSettings.activeComponent();

  GLShaderProgram* program = nullptr;
  switch (image.settings().interpolationMode()) {
    case InterpolationMode::NearestNeighbor: {
      // Fixed-point linear interpolation looks bad with nearest-neighbor image sampling. The floating-point
      // linear shader is the only useful isocontour fallback for this mode.
      program = &shaderProgramForTextureDimension(
        m_shaderPrograms,
        m_shaderPrograms2D,
        renderWarped ? ShaderProgramType::IsoContourLinearFloatingWarped : ShaderProgramType::IsoContourLinearFloating,
        imageTextureLayout.dimension);
      break;
    }
    case InterpolationMode::Linear: {
      const ShaderProgramType shaderType =
        renderData.m_isocontourFloatingPointInterpolation
          ? (renderWarped ? ShaderProgramType::IsoContourLinearFloatingWarped
                          : ShaderProgramType::IsoContourLinearFloating)
          : (renderWarped ? ShaderProgramType::IsoContourLinearFixedWarped : ShaderProgramType::IsoContourLinearFixed);
      program = &shaderProgramForTextureDimension(
        m_shaderPrograms,
        m_shaderPrograms2D,
        shaderType,
        imageTextureLayout.dimension);
      break;
    }
    case InterpolationMode::CubicBsplineConvolution: {
      program = &shaderProgramForTextureDimension(
        m_shaderPrograms,
        m_shaderPrograms2D,
        renderWarped ? ShaderProgramType::IsoContourCubicFixedWarped : ShaderProgramType::IsoContourCubicFixed,
        imageTextureLayout.dimension);
      break;
    }
  }

  const auto boundTextures = bindScalarImageTextures(imgSegPair);
  const auto boundDefTextures =
    renderWarped ? bindDeformationTextures(*deformationUid) : std::list<std::reference_wrapper<GLTexture>>{};

  program->use();
  for (const auto& surfaceUid : m_appData.isosurfaceUids(imageUid, activeComponent)) {
    const Isosurface* surface = m_appData.isosurface(imageUid, activeComponent, surfaceUid);
    if (!surface) {
      spdlog::warn("Null isosurface {} for image {}", surfaceUid, imageUid);
      continue;
    }

    if (!surface->visible || !surface->showIn2d) {
      continue;
    }

    static constexpr bool premultipliedAlpha = false;
    const glm::vec3 color =
      glm::vec3{getIsosurfaceColor(m_appData, *surface, imageSettings, activeComponent, premultipliedAlpha)};
    const float imageOpacity = renderData.m_modulateIsocontourOpacityWithImageOpacity ? uniforms.imgOpacity : 1.0f;
    const float isosurfaceOpacity = imageSettings.isosurfaceOpacityModulator() * imageOpacity;

    program->setSamplerUniform("u_imgTex", msk_imgTexSampler.index);
    setTexture2DAxesUniforms(*program, imageTextureLayout);

    program->setUniform("u_numCheckers", static_cast<float>(renderData.m_numCheckerboardSquares));
    program->setUniform("u_tex_T_world", uniforms.imgTexture_T_world);
    program->setUniform("u_isoValue", static_cast<float>(imageSettings.mapNativeIntensityToTexture(surface->value)));
    program->setUniform("u_fillOpacity", static_cast<float>(isosurfaceOpacity * surface->fillOpacity));
    program->setUniform("u_lineOpacity", static_cast<float>(isosurfaceOpacity * surface->opacity));
    program->setUniform("u_contourWidth", static_cast<float>(imageSettings.isoContourLineWidthIn2D()));
    program->setUniform("u_color", color);
    program->setUniform("u_viewSize", viewSize);
    program->setUniform("u_imgMinMax", uniforms.minMax);
    program->setUniform("u_imgThresholds", uniforms.thresholds);
    program->setUniform("u_quadrants", renderData.m_quadrants);
    program->setUniform("u_showFix", isFixedImage);
    program->setUniform("u_renderMode", displayModeUniform);
    if (renderWarped) {
      setDeformationUniforms(*program, imageUid, *deformationUid, uniforms.imgTexture_T_world);
    }

    renderOneImage(view, worldOffsetXhairs, *program, CurrentImages{imgSegPair}, false);
  }
  program->stopUse();

  unbindTextures(boundDefTextures);
  unbindTextures(boundTextures);
}
