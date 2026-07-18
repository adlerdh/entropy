#include "rendering/ShaderUniformSetup.h"

#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <vector>

namespace
{

using FloatVector = std::vector<float>;
using Mat4Vector = std::vector<glm::mat4>;
using Vec2Vector = std::vector<glm::vec2>;
using Vec3Vector = std::vector<glm::vec3>;

constexpr bool sk_optionalUniform = false;

const glm::mat4 sk_identMat4{1.0f};
const glm::vec2 sk_zeroVec2{0.0f, 0.0f};
const glm::vec3 sk_zeroVec3{0.0f, 0.0f, 0.0f};
const glm::vec4 sk_zeroVec4{0.0f, 0.0f, 0.0f, 0.0f};
const glm::ivec2 sk_zeroIVec2{0, 0};

const Uniforms::SamplerIndexType msk_imgTexSampler{0};
const Uniforms::SamplerIndexType msk_imgCmapTexSampler{1};
const Uniforms::SamplerIndexVectorType msk_imgRgbaTexSamplers{{0, 1, 2, 3}};
const Uniforms::SamplerIndexVectorType msk_defTexSamplers{{4, 5, 6}};
const Uniforms::SamplerIndexType msk_segTexSampler{0};
const Uniforms::SamplerIndexType msk_segLabelTableTexSampler{1};
const Uniforms::SamplerIndexVectorType msk_metricImgTexSamplers{{0, 1}};
const Uniforms::SamplerIndexType msk_metricCmapTexSampler{2};
const Uniforms::SamplerIndexVectorType msk_metricDefTexSamplers0{{3, 4, 5}};
const Uniforms::SamplerIndexVectorType msk_metricDefTexSamplers1{{6, 7, 8}};

} // namespace

namespace rendering::shader_setup
{

ShaderUniformSet buildShaderUniformSet()
{
  // All the vertex shader uniforms:
  Uniforms vsClipWorldUniforms;
  vsClipWorldUniforms.insertUniform("u_view_T_clip", UniformType::Mat4, sk_identMat4);
  vsClipWorldUniforms.insertUniform("u_world_T_clip", UniformType::Mat4, sk_identMat4);
  vsClipWorldUniforms.insertUniform("u_clipDepth", UniformType::Float, 0.0f);

  Uniforms vsViewModeUniforms;
  vsViewModeUniforms.insertUniform("u_aspectRatio", UniformType::Float, 1.0f);
  vsViewModeUniforms.insertUniform("u_numCheckers", UniformType::Int, 1);

  Uniforms vsImageUniforms;
  vsImageUniforms.insertUniforms(vsClipWorldUniforms);
  vsImageUniforms.insertUniform("u_tex_T_world", UniformType::Mat4, sk_identMat4);
  vsImageUniforms.insertUniforms(vsViewModeUniforms);

  Uniforms vsImageWarpedUniforms;
  vsImageWarpedUniforms.insertUniforms(vsClipWorldUniforms);
  vsImageWarpedUniforms.insertUniforms(vsViewModeUniforms);

  Uniforms vsSegUniforms;
  vsSegUniforms.insertUniforms(vsClipWorldUniforms);
  vsSegUniforms.insertUniform("u_tex_T_world", UniformType::Mat4, sk_identMat4);
  vsSegUniforms.insertUniforms(vsViewModeUniforms);

  Uniforms vsSegWarpedUniforms;
  vsSegWarpedUniforms.insertUniforms(vsClipWorldUniforms);
  vsSegWarpedUniforms.insertUniforms(vsViewModeUniforms);

  Uniforms vsMetricUniforms;
  vsMetricUniforms.insertUniform("u_view_T_clip", UniformType::Mat4, sk_identMat4);
  vsMetricUniforms.insertUniform("u_world_T_clip", UniformType::Mat4, sk_identMat4);
  vsMetricUniforms.insertUniform("u_clipDepth", UniformType::Float, 0.0f);
  vsMetricUniforms.insertUniform("u_tex_T_world", UniformType::Mat4Vector, Mat4Vector{sk_identMat4, sk_identMat4});

  Uniforms vsMetricWarpedUniforms;
  vsMetricWarpedUniforms.insertUniforms(vsClipWorldUniforms);

  // All the fragment shader uniforms:
  Uniforms fsImageAdjustmentUniforms;
  fsImageAdjustmentUniforms.insertUniform("u_imgSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
  fsImageAdjustmentUniforms.insertUniform("u_imgMinMax", UniformType::Vec2, sk_zeroVec2);
  fsImageAdjustmentUniforms.insertUniform("u_imgThresholds", UniformType::Vec2, sk_zeroVec2);
  fsImageAdjustmentUniforms.insertUniform("u_imgOpacity", UniformType::Float, 0.0f);

  Uniforms fsColorMapUniforms;
  fsColorMapUniforms.insertUniform("u_cmapSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
  fsColorMapUniforms.insertUniform("u_cmapQuantLevels", UniformType::Int, 0);
  fsColorMapUniforms.insertUniform("u_cmapHsvModFactors", UniformType::Vec3, glm::vec3{0.0f, 1.0f, 1.0f});
  fsColorMapUniforms.insertUniform("u_applyHsvMod", UniformType::Bool, false);

  Uniforms fsRenderModeUniforms;
  fsRenderModeUniforms.insertUniform(
    "u_renderMode",
    UniformType::Int,
    0); // 0: image, 1: checkerboard, 2: quadrants, 3: flashlight
  fsRenderModeUniforms.insertUniform("u_clipCrosshairs", UniformType::Vec2, sk_zeroVec2);
  fsRenderModeUniforms.insertUniform("u_quadrants", UniformType::IVec2,
                                     sk_zeroIVec2);                         // For quadrants
  fsRenderModeUniforms.insertUniform("u_showFix", UniformType::Bool, true); // For checkerboarding
  fsRenderModeUniforms.insertUniform("u_flashlightRadius", UniformType::Float, 0.5f);
  fsRenderModeUniforms.insertUniform("u_flashlightMovingOnFixed", UniformType::Bool, true);

  Uniforms fsIntensityProjectionUniforms;
  fsIntensityProjectionUniforms.insertUniform("u_mipMode", UniformType::Int, 0);
  fsIntensityProjectionUniforms.insertUniform("u_halfNumMipSamples", UniformType::Int, 0);
  fsIntensityProjectionUniforms.insertUniform("u_texSamplingDirZ", UniformType::Vec3, sk_zeroVec3, sk_optionalUniform);
  fsIntensityProjectionUniforms
    .insertUniform("u_worldSamplingDirZ", UniformType::Vec3, sk_zeroVec3, sk_optionalUniform);

  Uniforms fsDeformationUniforms;
  fsDeformationUniforms.insertUniform("u_defTex", UniformType::SamplerVector, msk_defTexSamplers);
  fsDeformationUniforms.insertUniform("u_defTex_T_world", UniformType::Mat4, sk_identMat4);
  fsDeformationUniforms.insertUniform("u_sampleTex_T_world", UniformType::Mat4, sk_identMat4);
  fsDeformationUniforms.insertUniform("u_defSlope_native_T_texture", UniformType::Float, 1.0f);
  fsDeformationUniforms.insertUniform("u_deformationStrength", UniformType::Float, 1.0f);
  fsDeformationUniforms.insertUniform("u_defInterleaved", UniformType::Bool, false);

  Uniforms fsImageGrayUniforms; // image gray FS
  fsImageGrayUniforms.insertUniforms(fsImageAdjustmentUniforms);
  fsImageGrayUniforms.insertUniforms(fsColorMapUniforms);
  fsImageGrayUniforms.insertUniforms(fsRenderModeUniforms);
  fsImageGrayUniforms.insertUniforms(fsIntensityProjectionUniforms);
  fsImageGrayUniforms.insertUniform("u_imgTex", UniformType::Sampler, msk_imgTexSampler);
  fsImageGrayUniforms.insertUniform("u_cmapTex", UniformType::Sampler, msk_imgCmapTexSampler);
  fsImageGrayUniforms.insertUniform("u_tex2DAxes[0]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  fsImageGrayUniforms.insertUniform("u_tex2DAxes[1]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  Uniforms fsImageGrayWarpedUniforms = fsImageGrayUniforms;
  fsImageGrayWarpedUniforms.insertUniforms(fsDeformationUniforms);

  Uniforms fsImageColorUniforms; // image color FS
  fsImageColorUniforms.insertUniforms(fsRenderModeUniforms);
  fsImageColorUniforms.insertUniform("u_imgTex", UniformType::SamplerVector, msk_imgRgbaTexSamplers);
  fsImageColorUniforms.insertUniform("u_imgSlopeIntercept", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2});
  fsImageColorUniforms.insertUniform("u_alphaIsOne", UniformType::Bool, true);
  fsImageColorUniforms.insertUniform("u_imgOpacity", UniformType::FloatVector, FloatVector{0.0f});
  fsImageColorUniforms.insertUniform("u_imgMinMax", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2});
  fsImageColorUniforms.insertUniform("u_imgThresholds", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2});
  fsImageColorUniforms.insertUniform("u_tex2DAxes[0]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  fsImageColorUniforms.insertUniform("u_tex2DAxes[1]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  fsImageColorUniforms.insertUniform("u_tex2DAxes[2]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  fsImageColorUniforms.insertUniform("u_tex2DAxes[3]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  Uniforms fsImageColorWarpedUniforms = fsImageColorUniforms;
  fsImageColorWarpedUniforms.insertUniforms(fsDeformationUniforms);

  Uniforms fsVectorDirectionColorUniforms;
  fsVectorDirectionColorUniforms.insertUniforms(fsRenderModeUniforms);
  fsVectorDirectionColorUniforms.insertUniform("u_imgTex", UniformType::SamplerVector, msk_imgRgbaTexSamplers);
  fsVectorDirectionColorUniforms.insertUniform("u_imgSlope_native_T_texture", UniformType::Float, 1.0f);
  fsVectorDirectionColorUniforms.insertUniform("u_imgOpacity", UniformType::Float, 0.0f);
  fsVectorDirectionColorUniforms.insertUniform("u_tex2DAxes[0]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  fsVectorDirectionColorUniforms.insertUniform("u_tex2DAxes[1]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);

  Uniforms fsVectorSignedNormalProjectionUniforms;
  fsVectorSignedNormalProjectionUniforms.insertUniforms(fsRenderModeUniforms);
  fsVectorSignedNormalProjectionUniforms.insertUniform("u_imgTex", UniformType::SamplerVector, msk_imgRgbaTexSamplers);
  fsVectorSignedNormalProjectionUniforms.insertUniform("u_imgSlope_native_T_texture", UniformType::Float, 1.0f);
  fsVectorSignedNormalProjectionUniforms.insertUniform("u_projectionScale", UniformType::Float, 1.0f);
  fsVectorSignedNormalProjectionUniforms.insertUniform("u_viewNormal_subject", UniformType::Vec3, sk_zeroVec3);
  fsVectorSignedNormalProjectionUniforms.insertUniform("u_imgOpacity", UniformType::Float, 0.0f);
  fsVectorSignedNormalProjectionUniforms
    .insertUniform("u_tex2DAxes[0]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  fsVectorSignedNormalProjectionUniforms
    .insertUniform("u_tex2DAxes[1]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);

  Uniforms fsVectorPlanarProjectionColorUniforms;
  fsVectorPlanarProjectionColorUniforms.insertUniforms(fsRenderModeUniforms);
  fsVectorPlanarProjectionColorUniforms.insertUniform("u_imgTex", UniformType::SamplerVector, msk_imgRgbaTexSamplers);
  fsVectorPlanarProjectionColorUniforms.insertUniform("u_imgSlope_native_T_texture", UniformType::Float, 1.0f);
  fsVectorPlanarProjectionColorUniforms.insertUniform("u_projectionScale", UniformType::Float, 1.0f);
  fsVectorPlanarProjectionColorUniforms.insertUniform("u_viewRight_subject", UniformType::Vec3, sk_zeroVec3);
  fsVectorPlanarProjectionColorUniforms.insertUniform("u_viewUp_subject", UniformType::Vec3, sk_zeroVec3);
  fsVectorPlanarProjectionColorUniforms.insertUniform("u_signedColors", UniformType::Bool, true);
  fsVectorPlanarProjectionColorUniforms.insertUniform("u_imgOpacity", UniformType::Float, 0.0f);
  fsVectorPlanarProjectionColorUniforms
    .insertUniform("u_tex2DAxes[0]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  fsVectorPlanarProjectionColorUniforms
    .insertUniform("u_tex2DAxes[1]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);

  Uniforms fsVectorWarpedGridUniforms;
  fsVectorWarpedGridUniforms.insertUniforms(fsRenderModeUniforms);
  fsVectorWarpedGridUniforms.insertUniform("u_imgTex", UniformType::SamplerVector, msk_imgRgbaTexSamplers);
  fsVectorWarpedGridUniforms.insertUniform("u_imgSlope_native_T_texture", UniformType::Float, 1.0f);
  fsVectorWarpedGridUniforms.insertUniform("u_subject_T_texture", UniformType::Mat4, sk_identMat4);
  fsVectorWarpedGridUniforms.insertUniform("u_viewRight_subject", UniformType::Vec3, sk_zeroVec3);
  fsVectorWarpedGridUniforms.insertUniform("u_viewUp_subject", UniformType::Vec3, sk_zeroVec3);
  fsVectorWarpedGridUniforms.insertUniform("u_gridSpacing_subject", UniformType::Float, 10.0f);
  fsVectorWarpedGridUniforms.insertUniform("u_lineThickness_px", UniformType::Float, 1.5f);
  fsVectorWarpedGridUniforms.insertUniform("u_warpScale", UniformType::Float, 1.0f);
  fsVectorWarpedGridUniforms.insertUniform("u_convention", UniformType::Int, 0);
  fsVectorWarpedGridUniforms.insertUniform(
    "u_foregroundColor",
    UniformType::Vec4,
    glm::vec4{209.0f / 255.0f, 79.0f / 255.0f, 1.0f, 1.0f});
  fsVectorWarpedGridUniforms.insertUniform("u_backgroundColor", UniformType::Vec4, sk_zeroVec4);
  fsVectorWarpedGridUniforms.insertUniform("u_imgOpacity", UniformType::Float, 1.0f);
  fsVectorWarpedGridUniforms.insertUniform("u_tex2DAxes[0]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  fsVectorWarpedGridUniforms.insertUniform("u_tex2DAxes[1]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);

  Uniforms fsEdgeUniforms;
  fsEdgeUniforms.insertUniforms(fsImageAdjustmentUniforms);
  fsEdgeUniforms.insertUniforms(fsRenderModeUniforms);
  fsEdgeUniforms.insertUniform("u_imgTex", UniformType::Sampler, msk_imgTexSampler);
  fsEdgeUniforms.insertUniform("u_cmapTex", UniformType::Sampler, msk_imgCmapTexSampler);
  fsEdgeUniforms.insertUniform("u_cmapSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
  fsEdgeUniforms.insertUniform("u_thresholdEdges", UniformType::Bool, true);
  fsEdgeUniforms.insertUniform("u_edgeMagnitude", UniformType::Float, 0.0f);
  // fsEdgeUniforms.insertUniform("u_useFreiChen", UniformType::Bool, 0.0f);
  fsEdgeUniforms.insertUniform("u_colormapEdges", UniformType::Bool, false);
  fsEdgeUniforms.insertUniform("u_edgeColor", UniformType::Vec4, sk_zeroVec4);
  fsEdgeUniforms.insertUniform("u_texelDirs", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3});
  fsEdgeUniforms.insertUniform("u_tex2DAxes[0]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  fsEdgeUniforms.insertUniform("u_tex2DAxes[1]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  Uniforms fsEdgeWarpedUniforms = fsEdgeUniforms;
  fsEdgeWarpedUniforms.insertUniforms(fsDeformationUniforms);

  Uniforms fsXrayUniforms;
  fsXrayUniforms.insertUniforms(fsImageAdjustmentUniforms);
  fsXrayUniforms.insertUniforms(fsColorMapUniforms);
  fsXrayUniforms.insertUniforms(fsRenderModeUniforms);
  fsXrayUniforms.insertUniform("u_imgTex", UniformType::Sampler, msk_imgTexSampler);
  fsXrayUniforms.insertUniform("u_cmapTex", UniformType::Sampler, msk_imgCmapTexSampler);
  fsXrayUniforms.insertUniform("u_imgSlope_native_T_texture", UniformType::Float, 1.0f);
  fsXrayUniforms.insertUniform("u_tex2DAxes[0]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  fsXrayUniforms.insertUniform("u_tex2DAxes[1]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);

  fsXrayUniforms.insertUniforms(fsIntensityProjectionUniforms);
  fsXrayUniforms.insertUniform("u_mipSamplingDistance_cm", UniformType::Float, 0.0f);
  fsXrayUniforms.insertUniform("u_waterAttenCoeff", UniformType::Float, 0.0f);
  fsXrayUniforms.insertUniform("u_airAttenCoeff", UniformType::Float, 0.0f);
  Uniforms fsXrayWarpedUniforms = fsXrayUniforms;
  fsXrayWarpedUniforms.insertUniforms(fsDeformationUniforms);

  Uniforms fsSegAdjustmentUniforms;
  fsSegAdjustmentUniforms.insertUniform("u_segOpacity", UniformType::Float, 0.0f);
  fsSegAdjustmentUniforms.insertUniform("u_segFillOpacity", UniformType::Float, 1.0f);
  fsSegAdjustmentUniforms.insertUniform("u_useSegColorOverride", UniformType::Bool, false);
  fsSegAdjustmentUniforms.insertUniform("u_segColorOverride", UniformType::Vec4, sk_zeroVec4);
  fsSegAdjustmentUniforms.insertUniform(
    "u_texSamplingDirsForSegOutline",
    UniformType::Vec3Vector,
    Vec3Vector{sk_zeroVec3});

  Uniforms fsSegNearestUniforms; // seg NN shader
  fsSegNearestUniforms.insertUniforms(fsRenderModeUniforms);
  fsSegNearestUniforms.insertUniforms(fsSegAdjustmentUniforms);
  fsSegNearestUniforms.insertUniform("u_segTex", UniformType::Sampler, msk_segTexSampler);
  fsSegNearestUniforms.insertUniform("u_segLabelCmapTex", UniformType::Sampler, msk_segLabelTableTexSampler);
  fsSegNearestUniforms.insertUniform("u_tex2DAxes[0]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  fsSegNearestUniforms.insertUniform("u_tex2DAxes[1]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  Uniforms fsSegNearestWarpedUniforms = fsSegNearestUniforms;
  fsSegNearestWarpedUniforms.insertUniforms(fsDeformationUniforms);

  Uniforms fsSegLinearUniforms; // seg linear shader
  fsSegLinearUniforms.insertUniforms(fsSegNearestUniforms);
  fsSegLinearUniforms.insertUniform("u_segInterpCutoff", UniformType::Float, 0.5f);
  fsSegLinearUniforms.insertUniform("u_texSamplingDirsForSmoothSeg", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3});
  Uniforms fsSegLinearWarpedUniforms = fsSegLinearUniforms;
  fsSegLinearWarpedUniforms.insertUniforms(fsDeformationUniforms);

  Uniforms fsIsoUniforms;
  fsIsoUniforms.insertUniforms(fsRenderModeUniforms);
  fsIsoUniforms.insertUniforms(fsIntensityProjectionUniforms);
  fsIsoUniforms.insertUniform("u_isoValue", UniformType::Float, 0.0f);
  fsIsoUniforms.insertUniform("u_fillOpacity", UniformType::Float, 0.0f);
  fsIsoUniforms.insertUniform("u_lineOpacity", UniformType::Float, 0.0f);
  fsIsoUniforms.insertUniform("u_color", UniformType::Vec3, sk_zeroVec3);
  fsIsoUniforms.insertUniform("u_contourWidth", UniformType::Float, 0.0f);
  fsIsoUniforms.insertUniform("u_imgMinMax", UniformType::Vec2, sk_zeroVec2);
  fsIsoUniforms.insertUniform("u_imgThresholds", UniformType::Vec2, sk_zeroVec2);
  fsIsoUniforms.insertUniform("u_imgTex", UniformType::Sampler, msk_imgTexSampler);
  fsIsoUniforms.insertUniform("u_tex2DAxes[0]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  fsIsoUniforms.insertUniform("u_tex2DAxes[1]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  Uniforms fsIsoWarpedUniforms = fsIsoUniforms;
  fsIsoWarpedUniforms.insertUniforms(fsDeformationUniforms);

  Uniforms fsMetricDeformationUniforms;
  fsMetricDeformationUniforms.insertUniform("u_defTex0", UniformType::SamplerVector, msk_metricDefTexSamplers0);
  fsMetricDeformationUniforms.insertUniform("u_defTex1", UniformType::SamplerVector, msk_metricDefTexSamplers1);
  fsMetricDeformationUniforms.insertUniform(
    "u_defTex_T_world",
    UniformType::Mat4Vector,
    Mat4Vector{sk_identMat4, sk_identMat4});
  fsMetricDeformationUniforms.insertUniform(
    "u_sampleTex_T_world",
    UniformType::Mat4Vector,
    Mat4Vector{sk_identMat4, sk_identMat4});
  fsMetricDeformationUniforms.insertUniform(
    "u_defSlope_native_T_texture",
    UniformType::FloatVector,
    FloatVector{1.0f, 1.0f});
  fsMetricDeformationUniforms.insertUniform("u_deformationStrength", UniformType::FloatVector, FloatVector{1.0f, 1.0f});
  fsMetricDeformationUniforms.insertUniform("u_worldSamplingDirX", UniformType::Vec3, sk_zeroVec3, sk_optionalUniform);
  fsMetricDeformationUniforms.insertUniform("u_worldSamplingDirY", UniformType::Vec3, sk_zeroVec3, sk_optionalUniform);
  fsMetricDeformationUniforms.insertUniform("u_worldSamplingDirZ", UniformType::Vec3, sk_zeroVec3, sk_optionalUniform);

  Uniforms fsDiffUniforms;
  fsDiffUniforms.insertUniforms(fsIntensityProjectionUniforms);
  fsDiffUniforms.insertUniform("u_imgTex", UniformType::SamplerVector, msk_metricImgTexSamplers);
  fsDiffUniforms.insertUniform("u_metricCmapTex", UniformType::Sampler, msk_metricCmapTexSampler);
  fsDiffUniforms.insertUniform("u_imgSlopeIntercept", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2, sk_zeroVec2});
  fsDiffUniforms.insertUniform("u_metricCmapSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
  fsDiffUniforms.insertUniform("u_metricSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
  fsDiffUniforms.insertUniform("u_useSquare", UniformType::Bool, true);
  fsDiffUniforms.insertUniform("img1Tex_T_img0Tex", UniformType::Mat4, sk_identMat4, sk_optionalUniform);
  fsDiffUniforms.insertUniform("u_tex2DAxes[0]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  fsDiffUniforms.insertUniform("u_tex2DAxes[1]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  Uniforms fsDiffWarpedUniforms = fsDiffUniforms;
  fsDiffWarpedUniforms.insertUniforms(fsMetricDeformationUniforms);

  Uniforms fsLocalNccUniforms;
  fsLocalNccUniforms.insertUniform("u_imgTex", UniformType::SamplerVector, msk_metricImgTexSamplers);
  fsLocalNccUniforms.insertUniform("u_metricCmapTex", UniformType::Sampler, msk_metricCmapTexSampler);
  fsLocalNccUniforms.insertUniform(
    "u_imgSlopeIntercept",
    UniformType::Vec2Vector,
    Vec2Vector{sk_zeroVec2, sk_zeroVec2});
  fsLocalNccUniforms.insertUniform("u_metricCmapSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
  fsLocalNccUniforms.insertUniform("u_metricSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
  fsLocalNccUniforms.insertUniform("img1Tex_T_img0Tex", UniformType::Mat4, sk_identMat4, sk_optionalUniform);
  fsLocalNccUniforms.insertUniform("u_tex0SamplingDirX", UniformType::Vec3, sk_zeroVec3, sk_optionalUniform);
  fsLocalNccUniforms.insertUniform("u_tex0SamplingDirY", UniformType::Vec3, sk_zeroVec3, sk_optionalUniform);
  fsLocalNccUniforms.insertUniform("u_texSamplingDirZ", UniformType::Vec3, sk_zeroVec3, sk_optionalUniform);
  fsLocalNccUniforms.insertUniform("u_patchRadius", UniformType::Int, 3);
  fsLocalNccUniforms.insertUniform("u_sampleSpacing", UniformType::Float, 1.0f);
  fsLocalNccUniforms.insertUniform("u_minValidFraction", UniformType::Float, 0.75f);
  fsLocalNccUniforms.insertUniform("u_varianceEpsilon", UniformType::Float, 1.0e-5f);
  fsLocalNccUniforms.insertUniform("u_ignoreNegativeCorrelation", UniformType::Bool, true);
  fsLocalNccUniforms.insertUniform("u_presentation", UniformType::Int, 0);
  fsLocalNccUniforms.insertUniform("u_invalidStyle", UniformType::Int, 0);
  fsLocalNccUniforms.insertUniform("u_tex2DAxes[0]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  fsLocalNccUniforms.insertUniform("u_tex2DAxes[1]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  Uniforms fsLocalNccWarpedUniforms = fsLocalNccUniforms;
  fsLocalNccWarpedUniforms.insertUniforms(fsMetricDeformationUniforms);

  Uniforms fsLocalLinearResidualUniforms;
  fsLocalLinearResidualUniforms.insertUniform("u_imgTex", UniformType::SamplerVector, msk_metricImgTexSamplers);
  fsLocalLinearResidualUniforms.insertUniform("u_metricCmapTex", UniformType::Sampler, msk_metricCmapTexSampler);
  fsLocalLinearResidualUniforms.insertUniform(
    "u_imgSlopeIntercept",
    UniformType::Vec2Vector,
    Vec2Vector{sk_zeroVec2, sk_zeroVec2});
  fsLocalLinearResidualUniforms.insertUniform("u_metricCmapSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
  fsLocalLinearResidualUniforms.insertUniform("u_metricSlopeIntercept", UniformType::Vec2, sk_zeroVec2);
  fsLocalLinearResidualUniforms.insertUniform("img1Tex_T_img0Tex", UniformType::Mat4, sk_identMat4, sk_optionalUniform);
  fsLocalLinearResidualUniforms.insertUniform("u_tex0SamplingDirX", UniformType::Vec3, sk_zeroVec3, sk_optionalUniform);
  fsLocalLinearResidualUniforms.insertUniform("u_tex0SamplingDirY", UniformType::Vec3, sk_zeroVec3, sk_optionalUniform);
  fsLocalLinearResidualUniforms.insertUniform("u_texSamplingDirZ", UniformType::Vec3, sk_zeroVec3, sk_optionalUniform);
  fsLocalLinearResidualUniforms.insertUniform("u_patchRadius", UniformType::Int, 3);
  fsLocalLinearResidualUniforms.insertUniform("u_sampleSpacing", UniformType::Float, 1.0f);
  fsLocalLinearResidualUniforms.insertUniform("u_minValidFraction", UniformType::Float, 0.75f);
  fsLocalLinearResidualUniforms.insertUniform("u_varianceEpsilon", UniformType::Float, 1.0e-5f);
  fsLocalLinearResidualUniforms.insertUniform("u_invalidStyle", UniformType::Int, 0);
  fsLocalLinearResidualUniforms.insertUniform("u_tex2DAxes[0]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  fsLocalLinearResidualUniforms.insertUniform("u_tex2DAxes[1]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  Uniforms fsLocalLinearResidualWarpedUniforms = fsLocalLinearResidualUniforms;
  fsLocalLinearResidualWarpedUniforms.insertUniforms(fsMetricDeformationUniforms);

  Uniforms fsOverlayUniforms;
  fsOverlayUniforms.insertUniform("u_imgTex", UniformType::SamplerVector, msk_metricImgTexSamplers);
  fsOverlayUniforms.insertUniform("u_imgSlopeIntercept", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2, sk_zeroVec2});
  fsOverlayUniforms.insertUniform("u_imgMinMax", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2, sk_zeroVec2});
  fsOverlayUniforms.insertUniform("u_imgThresholds", UniformType::Vec2Vector, Vec2Vector{sk_zeroVec2, sk_zeroVec2});
  fsOverlayUniforms.insertUniform("u_imgOpacity", UniformType::FloatVector, FloatVector{0.0f, 0.0f});
  fsOverlayUniforms.insertUniform("u_magentaCyan", UniformType::Bool, true);
  fsOverlayUniforms.insertUniform("img1Tex_T_img0Tex", UniformType::Mat4, sk_identMat4, sk_optionalUniform);
  fsOverlayUniforms.insertUniform("u_tex0SamplingDirX", UniformType::Vec3, sk_zeroVec3, sk_optionalUniform);
  fsOverlayUniforms.insertUniform("u_tex0SamplingDirY", UniformType::Vec3, sk_zeroVec3, sk_optionalUniform);
  fsOverlayUniforms.insertUniform("u_texSamplingDirZ", UniformType::Vec3, sk_zeroVec3, sk_optionalUniform);
  fsOverlayUniforms.insertUniform("u_tex2DAxes[0]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  fsOverlayUniforms.insertUniform("u_tex2DAxes[1]", UniformType::IVec2, sk_zeroIVec2, sk_optionalUniform);
  Uniforms fsOverlayWarpedUniforms = fsOverlayUniforms;
  fsOverlayWarpedUniforms.insertUniforms(fsMetricDeformationUniforms);

  return ShaderUniformSet{
    .vsClipWorldUniforms = vsClipWorldUniforms,
    .vsViewModeUniforms = vsViewModeUniforms,
    .vsImageUniforms = vsImageUniforms,
    .vsImageWarpedUniforms = vsImageWarpedUniforms,
    .vsSegUniforms = vsSegUniforms,
    .vsSegWarpedUniforms = vsSegWarpedUniforms,
    .vsMetricUniforms = vsMetricUniforms,
    .vsMetricWarpedUniforms = vsMetricWarpedUniforms,
    .fsImageAdjustmentUniforms = fsImageAdjustmentUniforms,
    .fsColorMapUniforms = fsColorMapUniforms,
    .fsRenderModeUniforms = fsRenderModeUniforms,
    .fsIntensityProjectionUniforms = fsIntensityProjectionUniforms,
    .fsDeformationUniforms = fsDeformationUniforms,
    .fsImageGrayUniforms = fsImageGrayUniforms,
    .fsImageGrayWarpedUniforms = fsImageGrayWarpedUniforms,
    .fsImageColorUniforms = fsImageColorUniforms,
    .fsImageColorWarpedUniforms = fsImageColorWarpedUniforms,
    .fsVectorDirectionColorUniforms = fsVectorDirectionColorUniforms,
    .fsVectorSignedNormalProjectionUniforms = fsVectorSignedNormalProjectionUniforms,
    .fsVectorPlanarProjectionColorUniforms = fsVectorPlanarProjectionColorUniforms,
    .fsVectorWarpedGridUniforms = fsVectorWarpedGridUniforms,
    .fsEdgeUniforms = fsEdgeUniforms,
    .fsEdgeWarpedUniforms = fsEdgeWarpedUniforms,
    .fsXrayUniforms = fsXrayUniforms,
    .fsXrayWarpedUniforms = fsXrayWarpedUniforms,
    .fsSegAdjustmentUniforms = fsSegAdjustmentUniforms,
    .fsSegNearestUniforms = fsSegNearestUniforms,
    .fsSegNearestWarpedUniforms = fsSegNearestWarpedUniforms,
    .fsSegLinearUniforms = fsSegLinearUniforms,
    .fsSegLinearWarpedUniforms = fsSegLinearWarpedUniforms,
    .fsIsoUniforms = fsIsoUniforms,
    .fsIsoWarpedUniforms = fsIsoWarpedUniforms,
    .fsMetricDeformationUniforms = fsMetricDeformationUniforms,
    .fsDiffUniforms = fsDiffUniforms,
    .fsDiffWarpedUniforms = fsDiffWarpedUniforms,
    .fsLocalNccUniforms = fsLocalNccUniforms,
    .fsLocalNccWarpedUniforms = fsLocalNccWarpedUniforms,
    .fsLocalLinearResidualUniforms = fsLocalLinearResidualUniforms,
    .fsLocalLinearResidualWarpedUniforms = fsLocalLinearResidualWarpedUniforms,
    .fsOverlayUniforms = fsOverlayUniforms,
    .fsOverlayWarpedUniforms = fsOverlayWarpedUniforms};
}

} // namespace rendering::shader_setup
