#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/ImageDrawing.h"
#include "rendering/utility/gl/GLTexture.h"
#include "windowing/View.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <spdlog/spdlog.h>

#include <array>
#include <functional>
#include <list>
#include <optional>

namespace
{
using namespace uuids;

const Uniforms::SamplerIndexType s_segTexSampler{0};
const Uniforms::SamplerIndexType s_segLabelTableTexSampler{1};
const Uniforms::SamplerIndexVectorType s_metricImgTexSamplers{{0, 1}};
const Uniforms::SamplerIndexType s_metricCmapTexSampler{2};
const Uniforms::SamplerIndexVectorType s_metricDefTexSamplers0{{3, 4, 5}};
const Uniforms::SamplerIndexVectorType s_metricDefTexSamplers1{{6, 7, 8}};
} // namespace

void Rendering::renderMetricImagesForView(const View& view, const glm::vec3& worldOffsetXhairs)
{
  static const RenderData::ImageUniforms sk_defaultImageUniforms;
  const RenderData& R = m_appData.renderData();
  // This function guarantees that imageSegPairs has size at least 2:
  const CurrentImages imageSegPairs = getImageAndSegUidsForMetricShaders(view.metricImages());

  const std::array<Image*, 2> imgs{
    imageSegPairs[0].first ? m_appData.image(*imageSegPairs[0].first) : nullptr,
    imageSegPairs[1].first ? m_appData.image(*imageSegPairs[1].first) : nullptr};

  const std::array<Image*, 2> segs{
    imageSegPairs[0].second ? m_appData.seg(*imageSegPairs[0].second) : nullptr,
    imageSegPairs[1].second ? m_appData.seg(*imageSegPairs[1].second) : nullptr};

  const std::array<RenderData::ImageUniforms, 2> U{
    (imageSegPairs.size() >= 1 && imageSegPairs[0].first) ? R.m_uniforms.at(*imageSegPairs[0].first)
                                                          : sk_defaultImageUniforms,
    (imageSegPairs.size() >= 2 && imageSegPairs[1].first) ? R.m_uniforms.at(*imageSegPairs[1].first)
                                                          : sk_defaultImageUniforms};

  const bool useTricubic = imgs[0] && imgs[1] &&
                           (InterpolationMode::CubicBsplineConvolution == imgs[0]->settings().interpolationMode()) &&
                           (InterpolationMode::CubicBsplineConvolution == imgs[1]->settings().interpolationMode());
  const std::array<std::optional<uuid>, 2> deformationUids{
    (imageSegPairs[0].first ? activeRenderableDeformationUid(*imageSegPairs[0].first) : std::nullopt),
    (imageSegPairs[1].first ? activeRenderableDeformationUid(*imageSegPairs[1].first) : std::nullopt)};
  const bool renderWarpedMetric = deformationUids[0].has_value() || deformationUids[1].has_value();
  const std::array<RenderData::PlanarTextureLayout, 2> metricTextureLayouts{
    rendering::textureLayoutOrDefault(R.m_imageTextureLayouts, imageSegPairs[0].first),
    rendering::textureLayoutOrDefault(R.m_imageTextureLayouts, imageSegPairs[1].first)};
  const bool mixedMetricTextureDimensions = metricTextureLayouts[0].dimension != metricTextureLayouts[1].dimension;

  if (mixedMetricTextureDimensions) {
    spdlog::warn(
      "Metric rendering between mixed 2D-fallback and 3D textures is not supported yet; skipping metric view");
    return;
  }
  const RenderData::TextureDimension metricTextureDimension = metricTextureLayouts[0].dimension;

  const auto boundMetricTextures = bindMetricImageTextures(imageSegPairs, view.renderMode());
  std::list<std::reference_wrapper<GLTexture>> boundMetricDefTextures;
  if (deformationUids[0]) {
    auto boundDefTextures = bindDeformationTextures(*deformationUids[0], s_metricDefTexSamplers0);
    boundMetricDefTextures.splice(boundMetricDefTextures.end(), boundDefTextures);
  }
  if (deformationUids[1]) {
    auto boundDefTextures = bindDeformationTextures(*deformationUids[1], s_metricDefTexSamplers1);
    boundMetricDefTextures.splice(boundMetricDefTextures.end(), boundDefTextures);
  }

  auto setMetricWarpUniforms = [&](GLShaderProgram& program) {
    if (!renderWarpedMetric) {
      return;
    }

    program.setSamplerUniform("u_defTex0", s_metricDefTexSamplers0);
    program.setSamplerUniform("u_defTex1", s_metricDefTexSamplers1);
    program.setUniform("u_sampleTex_T_world", std::vector<glm::mat4>{U[0].imgTexture_T_world, U[1].imgTexture_T_world});
    program.setUniform("u_warpEnabled[0]", false);
    program.setUniform("u_warpEnabled[1]", false);
    program.setUniform("u_defInterleaved[0]", false);
    program.setUniform("u_defInterleaved[1]", false);

    if (deformationUids[0] && imageSegPairs[0].first) {
      setMetricDeformationUniforms(program, 0, *imageSegPairs[0].first, *deformationUids[0], U[0].imgTexture_T_world);
    }
    if (deformationUids[1] && imageSegPairs[1].first) {
      setMetricDeformationUniforms(program, 1, *imageSegPairs[1].first, *deformationUids[1], U[1].imgTexture_T_world);
    }
  };

  if (ViewRenderMode::Difference == view.renderMode()) {
    GLShaderProgram& program = shaderProgramForTextureDimension(
      m_shaderPrograms,
      m_shaderPrograms2D,
      useTricubic
        ? (renderWarpedMetric ? ShaderProgramType::DifferenceCubicWarped : ShaderProgramType::DifferenceCubic)
        : (renderWarpedMetric ? ShaderProgramType::DifferenceLinearWarped : ShaderProgramType::DifferenceLinear),
      metricTextureDimension);

    const auto& params = R.m_squaredDifferenceParams;

    program.use();
    {
      program.setSamplerUniform("u_imgTex", s_metricImgTexSamplers);
      program.setSamplerUniform("u_metricCmapTex", s_metricCmapTexSampler.index);
      setTexture2DAxesUniforms(program, metricTextureLayouts[0], metricTextureLayouts[1]);

      program.setUniform("u_tex_T_world", std::vector<glm::mat4>{U[0].imgTexture_T_world, U[1].imgTexture_T_world});
      program.setUniform("img1Tex_T_img0Tex", U[1].imgTexture_T_world * glm::inverse(U[0].imgTexture_T_world));
      program.setUniform(
        "u_imgSlopeIntercept",
        std::vector<glm::vec2>{U[0].largestSlopeIntercept, U[1].largestSlopeIntercept});
      program.setUniform("u_metricCmapSlopeIntercept", params.m_cmapSlopeIntercept);
      program.setUniform("u_metricSlopeIntercept", params.m_slopeIntercept);
      program.setUniform("u_useSquare", R.m_useSquare);
      setMetricWarpUniforms(program);

      renderOneImage(view, worldOffsetXhairs, program, imageSegPairs, false);
    }
    program.stopUse();
  }
  else if (ViewRenderMode::LocalNcc == view.renderMode()) {
    GLShaderProgram& program = shaderProgramForTextureDimension(
      m_shaderPrograms,
      m_shaderPrograms2D,
      useTricubic ? (renderWarpedMetric ? ShaderProgramType::LocalNccCubicWarped : ShaderProgramType::LocalNccCubic)
                  : (renderWarpedMetric ? ShaderProgramType::LocalNccLinearWarped : ShaderProgramType::LocalNccLinear),
      metricTextureDimension);

    const auto& params = R.m_localNccParams;

    program.use();
    {
      program.setSamplerUniform("u_imgTex", s_metricImgTexSamplers);
      program.setSamplerUniform("u_metricCmapTex", s_metricCmapTexSampler.index);
      setTexture2DAxesUniforms(program, metricTextureLayouts[0], metricTextureLayouts[1]);

      program.setUniform("u_tex_T_world", std::vector<glm::mat4>{U[0].imgTexture_T_world, U[1].imgTexture_T_world});
      program.setUniform("img1Tex_T_img0Tex", U[1].imgTexture_T_world * glm::inverse(U[0].imgTexture_T_world));
      program.setUniform(
        "u_imgSlopeIntercept",
        std::vector<glm::vec2>{U[0].largestSlopeIntercept, U[1].largestSlopeIntercept});
      program.setUniform("u_metricCmapSlopeIntercept", params.m_cmapSlopeIntercept);
      program.setUniform("u_metricSlopeIntercept", params.m_slopeIntercept);
      program.setUniform("u_patchRadius", R.m_localNccPatchRadius);
      program.setUniform("u_sampleSpacing", R.m_localNccSampleSpacing);
      program.setUniform("u_minValidFraction", R.m_localNccMinValidFraction);
      program.setUniform("u_varianceEpsilon", R.m_localNccVarianceEpsilon);
      program.setUniform("u_ignoreNegativeCorrelation", R.m_localNccIgnoreNegativeCorrelation);
      program.setUniform("u_presentation", static_cast<int>(R.m_localNccPresentation));
      program.setUniform("u_invalidStyle", static_cast<int>(R.m_localNccInvalidStyle));
      setMetricWarpUniforms(program);

      renderOneImage(view, worldOffsetXhairs, program, imageSegPairs, false);
    }
    program.stopUse();
  }
  else if (ViewRenderMode::LocalLinearResidual == view.renderMode()) {
    GLShaderProgram& program = shaderProgramForTextureDimension(
      m_shaderPrograms,
      m_shaderPrograms2D,
      useTricubic ? (renderWarpedMetric ? ShaderProgramType::LocalLinearResidualCubicWarped
                                        : ShaderProgramType::LocalLinearResidualCubic)
                  : (renderWarpedMetric ? ShaderProgramType::LocalLinearResidualLinearWarped
                                        : ShaderProgramType::LocalLinearResidualLinear),
      metricTextureDimension);

    const auto& params = R.m_localLinearResidualParams;

    program.use();
    {
      program.setSamplerUniform("u_imgTex", s_metricImgTexSamplers);
      program.setSamplerUniform("u_metricCmapTex", s_metricCmapTexSampler.index);
      setTexture2DAxesUniforms(program, metricTextureLayouts[0], metricTextureLayouts[1]);

      program.setUniform("u_tex_T_world", std::vector<glm::mat4>{U[0].imgTexture_T_world, U[1].imgTexture_T_world});
      program.setUniform("img1Tex_T_img0Tex", U[1].imgTexture_T_world * glm::inverse(U[0].imgTexture_T_world));
      program.setUniform(
        "u_imgSlopeIntercept",
        std::vector<glm::vec2>{U[0].largestSlopeIntercept, U[1].largestSlopeIntercept});
      program.setUniform("u_metricCmapSlopeIntercept", params.m_cmapSlopeIntercept);
      program.setUniform("u_metricSlopeIntercept", params.m_slopeIntercept);
      program.setUniform("u_patchRadius", R.m_localLinearResidualPatchRadius);
      program.setUniform("u_sampleSpacing", R.m_localLinearResidualSampleSpacing);
      program.setUniform("u_minValidFraction", R.m_localLinearResidualMinValidFraction);
      program.setUniform("u_varianceEpsilon", R.m_localLinearResidualVarianceEpsilon);
      program.setUniform("u_invalidStyle", static_cast<int>(R.m_localLinearResidualInvalidStyle));
      setMetricWarpUniforms(program);

      renderOneImage(view, worldOffsetXhairs, program, imageSegPairs, false);
    }
    program.stopUse();
  }
  else if (ViewRenderMode::Overlay == view.renderMode()) {
    GLShaderProgram& program = shaderProgramForTextureDimension(
      m_shaderPrograms,
      m_shaderPrograms2D,
      useTricubic ? (renderWarpedMetric ? ShaderProgramType::OverlapCubicWarped : ShaderProgramType::OverlapCubic)
                  : (renderWarpedMetric ? ShaderProgramType::OverlapLinearWarped : ShaderProgramType::OverlapLinear),
      metricTextureDimension);

    program.use();
    {
      program.setSamplerUniform("u_imgTex", s_metricImgTexSamplers);
      setTexture2DAxesUniforms(program, metricTextureLayouts[0], metricTextureLayouts[1]);

      program.setUniform("u_tex_T_world", std::vector<glm::mat4>{U[0].imgTexture_T_world, U[1].imgTexture_T_world});
      program.setUniform("img1Tex_T_img0Tex", U[1].imgTexture_T_world * glm::inverse(U[0].imgTexture_T_world));
      program.setUniform(
        "u_imgSlopeIntercept",
        std::vector<glm::vec2>{U[0].slopeIntercept_normalized_T_texture, U[1].slopeIntercept_normalized_T_texture});
      program.setUniform("u_imgMinMax", std::vector<glm::vec2>{U[0].minMax, U[1].minMax});
      program.setUniform("u_imgThresholds", std::vector<glm::vec2>{U[0].thresholds, U[1].thresholds});
      program.setUniform("u_imgOpacity", std::vector<float>{U[0].imgOpacity, U[1].imgOpacity});
      program.setUniform("u_magentaCyan", R.m_overlayMagentaCyan);
      setMetricWarpUniforms(program);

      renderOneImage(view, worldOffsetXhairs, program, imageSegPairs, false);
    }
    program.stopUse();
  }

  unbindTextures(boundMetricDefTextures);
  unbindTextures(boundMetricTextures);

  for (unsigned int i = 0; i < NUM_METRIC_IMAGES; ++i) {
    if (!segs[i]) {
      continue;
    }

    const RenderData::PlanarTextureLayout segTextureLayout =
      rendering::textureLayoutOrDefault(R.m_segTextureLayouts, imageSegPairs[i].second);
    GLShaderProgram& program = shaderProgramForTextureDimension(
      m_shaderPrograms,
      m_shaderPrograms2D,
      (InterpolationMode::NearestNeighbor == segs[i]->settings().interpolationMode())
        ? ShaderProgramType::SegmentationNearest
        : ShaderProgramType::SegmentationLinear,
      segTextureLayout.dimension);

    const auto boundTextures = bindSegTextures(imageSegPairs[i]);
    const auto boundBufferTextures = bindSegBufferTextures(imageSegPairs[i]);

    program.use();
    {
      program.setSamplerUniform("u_segTex", s_segTexSampler.index);
      program.setSamplerUniform("u_segLabelCmapTex", s_segLabelTableTexSampler.index);
      setTexture2DAxesUniforms(program, segTextureLayout);

      program.setUniform("u_numCheckers", 1.0f); // checkerboarding disabled
      program.setUniform("u_tex_T_world", U[i].segTexture_T_world);
      program.setUniform(
        "u_segOpacity",
        U[i].segOpacity * (R.m_modulateSegOpacityWithImageOpacity ? U[i].imgOpacity : 1.0f));
      program.setUniform("u_quadrants", glm::ivec2{0, 0});
      program.setUniform("u_showFix", true); // ignored if not checkerboard or quadrants
      program.setUniform("u_renderMode", 0); // disabled

      drawSegQuad(
        program,
        R.m_quad,
        *segs[i],
        view,
        m_appData.windowData().viewport(),
        worldOffsetXhairs,
        R.m_flashlightRadius,
        R.m_flashlightOverlays,
        R.m_segOutlineStyle,
        R.m_segInteriorOpacity,
        R.m_segInterpCutoff);
    }
    program.stopUse();

    unbindBufferTextures(boundBufferTextures);
    unbindTextures(boundTextures);
  }
}
