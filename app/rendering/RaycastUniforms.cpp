#include "rendering/Rendering.h"

#include "logic/app/Data.h"
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
  const View& view,
  const ImgSegPair& imgSegPair,
  const Image& image,
  const RenderData::ImageUniforms& uniforms,
  const bool renderWarped,
  const std::optional<uuids::uuid>& deformationUid)
{
  const RenderData& renderData = m_appData.renderData();

  program.setSamplerUniform("u_imgTex", s_imgTexSampler.index);
  program.setSamplerUniform("u_jumpTex", s_jumpTexSampler.index);

  program.setUniform("u_tex_T_world", uniforms.imgTexture_T_world);
  program.setUniform("u_world_T_tex", uniforms.world_T_imgTexture);
  program.setUniform("u_texGrads", uniforms.textureGradientStep);

  // Shader arrays are fixed at 8 isosurfaces.
  program.setUniform("u_numIsos", renderData.m_isosurfaceData.numIsos);
  program.setUniform("u_isoValues", renderData.m_isosurfaceData.values);
  program.setUniform("u_isoOpacities", renderData.m_isosurfaceData.opacities);
  program.setUniform("u_isoRimOpacityStrengths", renderData.m_isosurfaceData.rimOpacityStrengths);
  program.setUniform("u_isoRimEmissionStrengths", renderData.m_isosurfaceData.rimEmissionStrengths);
  program.setUniform("u_isoRimPowers", renderData.m_isosurfaceData.rimPowers);
  program.setUniform("u_ambient", renderData.m_isosurfaceData.ambient);
  program.setUniform("u_diffuse", renderData.m_isosurfaceData.diffuse);
  program.setUniform("u_specular", renderData.m_isosurfaceData.specular);
  program.setUniform("u_shininess", renderData.m_isosurfaceData.shininesses);

  program.setUniform("u_samplingFactor", std::clamp(renderData.m_raycastSamplingFactor, 0.5f, 2.0f));
  program.setUniform("u_imgInvDims", 1.0f / glm::vec3{image.header().pixelDimensions()});
  program.setUniform("u_renderFrontFaces", renderData.m_renderFrontFaces);
  program.setUniform("u_renderBackFaces", renderData.m_renderBackFaces);
  program.setUniform("u_bgColor", renderData.m_3dBackgroundColor.a * renderData.m_3dBackgroundColor);
  program.setUniform("u_bgEdgeBrighteningEnabled", renderData.m_raycastBackgroundEdgeBrighteningEnabled);
  program.setUniform("u_noHitTransparent", renderData.m_3dTransparentIfNoHit);
  program.setUniform(
    "u_showCrosshairs3D",
    renderData.m_showCrosshairsIn3D && !view.threeDState().m_viewPositionFollowsCrosshairs);
  program.setUniform("u_crosshairsWorldPos", m_appData.state().worldCrosshairs().worldOrigin());
  program.setUniform("u_crosshairsColor", renderData.m_crosshairsColor);
  program.setUniform(
    "u_crosshairsRadiusMm",
    0.5f * renderData.m_crosshairs3DGlyphDiameterVoxelDiagonals * glm::length(image.header().spacing()));

  if (renderWarped && deformationUid && imgSegPair.first) {
    setDeformationUniforms(program, *imgSegPair.first, *deformationUid, uniforms.imgTexture_T_world);
  }
}
