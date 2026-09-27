#pragma once
#include "rendering/gl/Uniforms.h"
namespace rendering::shader_setup
{
/// Create raycast vertex-stage uniform declarations with identity transforms and zero clip depth.
inline Uniforms raycastVertexUniforms()
{
  Uniforms vsUniforms;
  vsUniforms.insertUniform("u_view_T_clip", UniformType::Mat4, glm::mat4{1.0f});
  vsUniforms.insertUniform("u_world_T_clip", UniformType::Mat4, glm::mat4{1.0f});
  vsUniforms.insertUniform("u_clipDepth", UniformType::Float, 0.0f);

  return vsUniforms;
}
/// Create raycast fragment-stage uniform declarations and their initial values.
/// @param warped Include deformation-sampling uniforms for a warped-image shader.
/// @param distanceMap Require the jump-texture sampler for unwarped distance-map acceleration.
/// @return Uniform declarations; the jump sampler is optional when warped or distanceMap is false.
inline Uniforms raycastFragmentUniforms(bool warped, bool distanceMap = true)
{
  Uniforms fsUniforms;

  fsUniforms.insertUniform("u_imgTex", UniformType::Sampler, Uniforms::SamplerIndexType{0});
  fsUniforms.insertUniform("u_jumpTex", UniformType::Sampler, Uniforms::SamplerIndexType{1}, !warped && distanceMap);

  fsUniforms.insertUniform("u_tex_T_world", UniformType::Mat4, glm::mat4{1.0f});
  fsUniforms.insertUniform("u_world_T_tex", UniformType::Mat4, glm::mat4{1.0f});
  fsUniforms.insertUniform("u_clip_T_imgTex", UniformType::Mat4, glm::mat4{1.0f});

  fsUniforms.insertUniform("u_texGrads", UniformType::Mat3, glm::mat3{1.0f});

  fsUniforms.insertUniform("u_numIsos", UniformType::Int, 0);
  fsUniforms.insertUniform("u_isoValues", UniformType::FloatVector, std::vector<float>{0.0f});
  fsUniforms.insertUniform("u_isoOpacities", UniformType::FloatVector, std::vector<float>{1.0f});
  fsUniforms.insertUniform("u_isoRimOpacityStrengths", UniformType::FloatVector, std::vector<float>{0.0f});
  fsUniforms.insertUniform("u_isoRimEmissionStrengths", UniformType::FloatVector, std::vector<float>{0.0f});
  fsUniforms.insertUniform("u_isoRimPowers", UniformType::FloatVector, std::vector<float>{2.0f});

  fsUniforms.insertUniform("u_isoColors", UniformType::Vec3Vector, std::vector<glm::vec3>{glm::vec3{0.0f}});
  fsUniforms.insertUniform("u_lightingAmbient", UniformType::Float, 0.30f);
  fsUniforms.insertUniform("u_lightingDiffuse", UniformType::Float, 0.50f);
  fsUniforms.insertUniform("u_lightingSpecular", UniformType::Float, 0.20f);
  fsUniforms.insertUniform("u_lightingSpecularPower", UniformType::Float, 16.0f);

  fsUniforms.insertUniform("u_bgColor", UniformType::Vec4, glm::vec4{0.0f});
  fsUniforms.insertUniform("u_bgEdgeBrighteningEnabled", UniformType::Bool, true);

  fsUniforms.insertUniform("u_samplingFactor", UniformType::Float, 1.0f);
  fsUniforms.insertUniform("u_imgInvDims", UniformType::Vec3, glm::vec3{1.0f});

  fsUniforms.insertUniform("u_renderFrontFaces", UniformType::Bool, true);
  fsUniforms.insertUniform("u_renderBackFaces", UniformType::Bool, true);
  fsUniforms.insertUniform("u_noHitTransparent", UniformType::Bool, true);

  if (warped) {
    fsUniforms.insertUniform("u_defTex", UniformType::SamplerVector, Uniforms::SamplerIndexVectorType{{4, 5, 6}});
    fsUniforms.insertUniform("u_defTex_T_world", UniformType::Mat4, glm::mat4{1.0f});
    fsUniforms.insertUniform("u_sampleTex_T_world", UniformType::Mat4, glm::mat4{1.0f});
    fsUniforms.insertUniform("u_defSlope_native_T_texture", UniformType::Float, 1.0f);
    fsUniforms.insertUniform("u_deformationStrength", UniformType::Float, 1.0f);
    fsUniforms.insertUniform("u_defInterleaved", UniformType::Bool, false);
  }

  return fsUniforms;
}
} // namespace rendering::shader_setup
