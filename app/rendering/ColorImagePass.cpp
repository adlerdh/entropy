#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/ImageDrawing.h"
#include "rendering/utility/gl/GLTexture.h"
#include "windowing/View.h"

#include <functional>
#include <list>
#include <optional>

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

    renderOneImage(view, worldOffsetXhairs, *program, renderGeometryImages, uniforms.showEdges);
  }
  program->stopUse();

  unbindTextures(boundDefTextures);
  unbindTextures(boundTextures);
}
