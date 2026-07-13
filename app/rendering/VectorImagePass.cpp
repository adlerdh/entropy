#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "logic/camera/CameraHelpers.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/utility/gl/GLTexture.h"
#include "windowing/View.h"

#include <glm/glm.hpp>

#include <algorithm>

namespace
{

const Uniforms::SamplerIndexVectorType msk_imgRgbaTexSamplers{{0, 1, 2, 3}};

float maxAbsVectorComponentValue(const ImageSettings& settings)
{
  float maxAbs = 1.0f;
  for (uint32_t component = 0; component < std::min<uint32_t>(3u, settings.numComponents()); ++component) {
    const auto [minValue, maxValue] = settings.minMaxImageRange(component);
    maxAbs = std::max(maxAbs, static_cast<float>(std::max(std::abs(minValue), std::abs(maxValue))));
  }
  return maxAbs;
}

glm::vec3 viewBackNormal_subject(const Image& image, const View& view)
{
  const glm::vec3 worldViewNormal = helper::worldDirection(view.camera(), Directions::View::Back);
  return glm::normalize(glm::mat3{image.transformations().subject_T_worldDef()} * worldViewNormal);
}

glm::vec3 viewDirection_subject(const Image& image, const View& view, Directions::View direction)
{
  const glm::vec3 worldViewDirection = helper::worldDirection(view.camera(), direction);
  return glm::normalize(glm::mat3{image.transformations().subject_T_worldDef()} * worldViewDirection);
}

} // namespace

void Rendering::renderVectorImageForImage(
  const View& view,
  const glm::vec3& worldOffsetXhairs,
  const ImgSegPair& imgSegPair,
  const Image& image,
  const RenderData::ImageUniforms& uniforms,
  const RenderData::PlanarTextureLayout& imageTextureLayout,
  const int displayModeUniform,
  const bool isFixedImage)
{
  const RenderData& renderData = m_appData.renderData();
  GLShaderProgram* program = nullptr;
  const bool signedNormalProjection =
    ComponentRenderMode::VectorSignedNormalProjection == image.settings().componentRenderMode();
  const bool planarProjection =
    ComponentRenderMode::VectorPlanarProjectionColor == image.settings().componentRenderMode();

  auto vectorShaderType = [&](const InterpolationMode interpolationMode) {
    if (signedNormalProjection) {
      return InterpolationMode::CubicBsplineConvolution == interpolationMode
               ? ShaderProgramType::VectorSignedNormalProjectionCubic
               : ShaderProgramType::VectorSignedNormalProjectionLinear;
    }
    if (planarProjection) {
      return InterpolationMode::CubicBsplineConvolution == interpolationMode
               ? ShaderProgramType::VectorPlanarProjectionColorCubic
               : ShaderProgramType::VectorPlanarProjectionColorLinear;
    }
    return InterpolationMode::CubicBsplineConvolution == interpolationMode
             ? ShaderProgramType::VectorDirectionColorCubic
             : ShaderProgramType::VectorDirectionColorLinear;
  };

  switch (image.settings().colorInterpolationMode()) {
    case InterpolationMode::NearestNeighbor:
    case InterpolationMode::Linear: {
      program = &shaderProgramForTextureDimension(
        m_shaderPrograms,
        m_shaderPrograms2D,
        vectorShaderType(InterpolationMode::Linear),
        imageTextureLayout.dimension);
      break;
    }
    case InterpolationMode::CubicBsplineConvolution: {
      program = &shaderProgramForTextureDimension(
        m_shaderPrograms,
        m_shaderPrograms2D,
        vectorShaderType(InterpolationMode::CubicBsplineConvolution),
        imageTextureLayout.dimension);
      break;
    }
  }

  const auto boundTextures = bindColorImageTextures(imgSegPair);
  program->use();
  {
    program->setSamplerUniform("u_imgTex", msk_imgRgbaTexSamplers);
    setTexture2DAxesUniforms(*program, imageTextureLayout);
    program->setUniform("u_numCheckers", static_cast<float>(renderData.m_numCheckerboardSquares));
    program->setUniform("u_tex_T_world", uniforms.imgTexture_T_world);
    program->setUniform("u_imgSlope_native_T_texture", uniforms.slope_native_T_texture);
    program->setUniform("u_imgOpacity", uniforms.imgOpacity);
    if (signedNormalProjection) {
      program->setUniform("u_projectionScale", maxAbsVectorComponentValue(image.settings()));
      program->setUniform("u_viewNormal_subject", viewBackNormal_subject(image, view));
    }
    else if (planarProjection) {
      program->setUniform("u_projectionScale", maxAbsVectorComponentValue(image.settings()));
      program->setUniform("u_viewRight_subject", viewDirection_subject(image, view, Directions::View::Right));
      program->setUniform("u_viewUp_subject", viewDirection_subject(image, view, Directions::View::Up));
      program->setUniform("u_signedColors", image.settings().vectorPlanarProjectionSignedColors());
    }
    program->setUniform("u_quadrants", renderData.m_quadrants);
    program->setUniform("u_showFix", isFixedImage); // ignored if not checkerboard or quadrants
    program->setUniform("u_renderMode", displayModeUniform);

    renderOneImage(view, worldOffsetXhairs, *program, CurrentImages{imgSegPair}, uniforms.showEdges);
  }
  program->stopUse();
  unbindTextures(boundTextures);
}
