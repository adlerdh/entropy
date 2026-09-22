#pragma once

#include "rendering/gl/Uniforms.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include <vector>

namespace rendering
{

/** Uniform declarations shared by the renderer and its shader-link test. */
inline Uniforms jointHistogramScatterUniforms()
{
  Uniforms uniforms;
  uniforms.insertUniform("u_fixedImage", UniformType::Sampler, Uniforms::SamplerIndexType{0});
  uniforms.insertUniform("u_movingImage", UniformType::Sampler, Uniforms::SamplerIndexType{1});
  uniforms.insertUniform("u_defTex0", UniformType::SamplerVector, Uniforms::SamplerIndexVectorType{{3, 4, 5}}, false);
  uniforms.insertUniform("u_defTex1", UniformType::SamplerVector, Uniforms::SamplerIndexVectorType{{6, 7, 8}}, false);
  uniforms.insertUniform("u_fixedSizeXY", UniformType::IVec2, glm::ivec2{1, 1});
  uniforms.insertUniform("u_fixedDepth", UniformType::Int, 1);
  uniforms.insertUniform("u_batchOffset", UniformType::Int, 0);
  uniforms.insertUniform("u_batchStride", UniformType::Int, 1);
  uniforms.insertUniform("u_tex2DAxes", UniformType::IVec2, glm::ivec2{0, 1}, false);
  uniforms.insertUniform("u_world_T_fixedTexture", UniformType::Mat4, glm::mat4{1.0f});
  uniforms.insertUniform("u_tex_T_world", UniformType::Mat4Vector, std::vector<glm::mat4>(2, glm::mat4{1.0f}));
  uniforms
    .insertUniform("u_defTex_T_world", UniformType::Mat4Vector, std::vector<glm::mat4>(2, glm::mat4{1.0f}), false);
  uniforms.insertUniform(
    "u_normalized_T_texture",
    UniformType::Vec2Vector,
    std::vector<glm::vec2>(2, glm::vec2{1.0f, 0.0f}));
  uniforms.insertUniform("u_textureComponents", UniformType::IVec2, glm::ivec2{0});
  uniforms.insertUniform("u_defSlope_native_T_texture", UniformType::FloatVector, std::vector<float>(2, 1.0f), false);
  uniforms.insertUniform("u_deformationStrength", UniformType::FloatVector, std::vector<float>(2, 0.0f), false);
  uniforms.insertUniform("u_defInterleaved", UniformType::Bool, false, false);
  uniforms.insertUniform("u_warpEnabled", UniformType::Bool, false, false);
  uniforms.insertUniform("u_bins", UniformType::Int, 512);
  uniforms.insertUniform("u_backgroundRows", UniformType::Int, 0);
  return uniforms;
}

inline Uniforms jointHistogramFixedSampleUniforms()
{
  Uniforms uniforms;
  uniforms.insertUniform("u_fixedImage", UniformType::Sampler, Uniforms::SamplerIndexType{0});
  uniforms.insertUniform("u_fixedDepth", UniformType::Int, 1);
  uniforms.insertUniform("u_fixedAxes", UniformType::IVec2, glm::ivec2{0, 1}, false);
  uniforms.insertUniform("u_normalization", UniformType::Vec2, glm::vec2{1.0f, 0.0f});
  uniforms.insertUniform("u_textureComponent", UniformType::Int, 0);
  return uniforms;
}

inline Uniforms jointHistogramFastScatterUniforms()
{
  Uniforms uniforms;
  uniforms.insertUniform("u_movingImage", UniformType::Sampler, Uniforms::SamplerIndexType{1});
  uniforms.insertUniform("u_moving_T_fixedTexture", UniformType::Mat4, glm::mat4{1.0f});
  uniforms.insertUniform("u_fixedValues", UniformType::Sampler, Uniforms::SamplerIndexType{10});
  uniforms.insertUniform("u_planeSize", UniformType::Int, 1);
  uniforms.insertUniform("u_fixedDepth", UniformType::Int, 1);
  uniforms.insertUniform("u_sampleOffset", UniformType::Int, 0);
  uniforms.insertUniform("u_sampleStride", UniformType::Int, 1);
  uniforms.insertUniform("u_movingAxes", UniformType::IVec2, glm::ivec2{0, 1}, false);
  uniforms.insertUniform("u_normalization", UniformType::Vec2, glm::vec2{1.0f, 0.0f});
  uniforms.insertUniform("u_textureComponent", UniformType::Int, 0);
  uniforms.insertUniform("u_bins", UniformType::Int, 512);
  uniforms.insertUniform("u_backgroundRows", UniformType::Int, 0);
  return uniforms;
}

inline Uniforms jointHistogramReductionUniforms()
{
  Uniforms uniforms;
  uniforms.insertUniform("u_source", UniformType::Sampler, Uniforms::SamplerIndexType{9});
  uniforms.insertUniform("u_sourceOffset", UniformType::IVec2, glm::ivec2{0});
  return uniforms;
}

inline Uniforms jointHistogramDisplayUniforms()
{
  Uniforms uniforms;
  uniforms.insertUniform("u_counts", UniformType::Sampler, Uniforms::SamplerIndexType{9});
  uniforms.insertUniform("u_histogramFraction", UniformType::Vec2, glm::vec2{1.0f});
  uniforms.insertUniform("u_colormap", UniformType::Sampler, Uniforms::SamplerIndexType{2});
  uniforms.insertUniform("u_referenceCount", UniformType::Float, 1.0f);
  uniforms.insertUniform("u_logarithmicScale", UniformType::Bool, true);
  uniforms.insertUniform("u_visibleMinimum", UniformType::Vec2, glm::vec2{0.0f});
  uniforms.insertUniform("u_visibleMaximum", UniformType::Vec2, glm::vec2{1.0f});
  uniforms.insertUniform("u_metricSlopeIntercept", UniformType::Vec2, glm::vec2{1.0f, 0.0f});
  uniforms.insertUniform("u_cmapSlopeIntercept", UniformType::Vec2, glm::vec2{1.0f, 0.0f});
  uniforms.insertUniform("u_cmapQuantizationLevels", UniformType::Int, 0);
  return uniforms;
}

} // namespace rendering
