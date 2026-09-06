#include "rendering/Rendering.h"

#include "image/Image.h"
#include "image/ImageSettings.h"
#include "logic/app/Data.h"
#include "rendering/RenderResources.h"
#include "rendering/RenderSettings.h"
#include "rendering/utility/containers/Uniforms.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/gl/GLTexture.h"
#include "windowing/View.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <optional>

namespace
{

const Uniforms::SamplerIndexType s_imgTexSampler{0};
const Uniforms::SamplerIndexType s_jumpTexSampler{1};

} // namespace

void Rendering::setRaycastIsoUniforms(
  GLShaderProgram& program,
  const ImgSegPair& imgSegPair,
  const Image& image,
  const rendering::RenderDerivedData::ImageUniforms& uniforms,
  const bool renderWarped,
  const std::optional<uuids::uuid>& deformationUid)
{
  const rendering::RenderSettings& settings = m_appData.renderSettings();
  const rendering::RenderDerivedData& derived = m_appData.renderDerivedData();

  program.setSamplerUniform("u_imgTex", s_imgTexSampler.index);
  program.setSamplerUniform("u_jumpTex", s_jumpTexSampler.index);

  program.setUniform("u_tex_T_world", uniforms.imgTexture_T_world);
  program.setUniform("u_world_T_tex", uniforms.world_T_imgTexture);
  program.setUniform("u_texGrads", uniforms.textureGradientStep);

  // Shader arrays are fixed at 8 isosurfaces.
  program.setUniform("u_numIsos", derived.isosurfaces.numIsos);
  program.setUniform("u_isoValues", derived.isosurfaces.values);
  program.setUniform("u_isoOpacities", derived.isosurfaces.opacities);
  program.setUniform("u_isoRimOpacityStrengths", derived.isosurfaces.rimOpacityStrengths);
  program.setUniform("u_isoRimEmissionStrengths", derived.isosurfaces.rimEmissionStrengths);
  program.setUniform("u_isoRimPowers", derived.isosurfaces.rimPowers);
  program.setUniform("u_isoColors", derived.isosurfaces.colors);
  program.setUniform("u_lightingAmbient", settings.m_lightingAmbient);
  program.setUniform("u_lightingDiffuse", settings.m_lightingDiffuse);
  program.setUniform("u_lightingSpecular", settings.m_lightingSpecular);
  program.setUniform("u_lightingSpecularPower", settings.m_lightingSpecularPower);

  program.setUniform("u_samplingFactor", std::clamp(settings.m_raycastSamplingFactor, 0.5f, 2.0f));
  program.setUniform("u_imgInvDims", 1.0f / glm::vec3{image.header().pixelDimensions()});
  program.setUniform("u_renderFrontFaces", settings.m_renderFrontFaces);
  program.setUniform("u_renderBackFaces", settings.m_renderBackFaces);
  program.setUniform("u_bgColor", settings.m_3dBackgroundColor.a * settings.m_3dBackgroundColor);
  program.setUniform("u_bgEdgeBrighteningEnabled", settings.m_raycastBackgroundEdgeBrighteningEnabled);
  program.setUniform("u_noHitTransparent", settings.m_3dTransparentIfNoHit);

  if (renderWarped && deformationUid && imgSegPair.first) {
    const auto sampleUniformsIt = derived.imageUniforms.find(*imgSegPair.first);
    const glm::mat4 sampleTex_T_world = sampleUniformsIt != derived.imageUniforms.end()
                                          ? sampleUniformsIt->second.imgTexture_T_world
                                          : uniforms.imgTexture_T_world;
    setDeformationUniforms(program, *imgSegPair.first, *deformationUid, sampleTex_T_world);
  }
}
