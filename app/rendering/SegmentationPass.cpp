#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/ImageDrawing.h"
#include "rendering/utility/gl/GLTexture.h"
#include "windowing/View.h"

#include <glm/glm.hpp>

#include <functional>
#include <list>
#include <optional>

namespace
{

const glm::vec4 sk_zeroVec4{0.0f, 0.0f, 0.0f, 0.0f};

const Uniforms::SamplerIndexType msk_segTexSampler{0};
const Uniforms::SamplerIndexType msk_segLabelTableTexSampler{1};

} // namespace

void Rendering::renderSegmentationForImage(
  const View& view,
  const glm::vec3& worldOffsetXhairs,
  const ImgSegPair& imgSegPair,
  const uuids::uuid& imageUid,
  const RenderData::ImageUniforms& uniforms,
  const bool renderWarped,
  const std::optional<uuids::uuid>& deformationUid,
  const int displayModeUniform,
  const bool isFixedImage)
{
  const auto& segUid = imgSegPair.second;
  const Image* seg = segUid ? m_appData.seg(*segUid) : nullptr;
  if (!seg) {
    return;
  }

  const RenderData& renderData = m_appData.renderData();
  const std::optional<uuids::uuid> referenceImageUid =
    renderWarped ? activeRenderableDeformationReferenceImageUid(imageUid) : std::nullopt;
  const Image* geometryImage = referenceImageUid ? m_appData.image(*referenceImageUid) : seg;
  const ShaderProgramType segShaderType =
    (InterpolationMode::NearestNeighbor == seg->settings().interpolationMode())
      ? (renderWarped ? ShaderProgramType::SegmentationNearestWarped : ShaderProgramType::SegmentationNearest)
      : (renderWarped ? ShaderProgramType::SegmentationLinearWarped : ShaderProgramType::SegmentationLinear);
  const RenderData::PlanarTextureLayout segTextureLayout =
    rendering::textureLayoutOrDefault(renderData.m_segTextureLayouts, segUid);
  GLShaderProgram& program =
    shaderProgramForTextureDimension(m_shaderPrograms, m_shaderPrograms2D, segShaderType, segTextureLayout.dimension);

  const auto boundTextures = bindSegTextures(imgSegPair);
  const auto boundDefTextures =
    renderWarped ? bindDeformationTextures(*deformationUid) : std::list<std::reference_wrapper<GLTexture>>{};
  const auto boundBufferTextures = bindSegBufferTextures(imgSegPair);

  program.use();
  {
    program.setSamplerUniform("u_segTex", msk_segTexSampler.index);
    program.setSamplerUniform("u_segLabelCmapTex", msk_segLabelTableTexSampler.index);
    setTexture2DAxesUniforms(program, segTextureLayout);

    program.setUniform("u_numCheckers", static_cast<float>(renderData.m_numCheckerboardSquares));
    program.setUniform("u_tex_T_world", uniforms.segTexture_T_world);
    program.setUniform(
      "u_segOpacity",
      uniforms.segOpacity * (renderData.m_modulateSegOpacityWithImageOpacity ? uniforms.imgOpacity : 1.0f));
    program.setUniform("u_useSegColorOverride", false);
    program.setUniform("u_segColorOverride", sk_zeroVec4);
    program.setUniform("u_quadrants", renderData.m_quadrants);
    program.setUniform("u_showFix", isFixedImage); // ignored if not checkerboard or quadrants
    program.setUniform("u_renderMode", displayModeUniform);
    if (renderWarped) {
      setDeformationUniforms(program, imageUid, *deformationUid, uniforms.segTexture_T_world);
    }

    drawSegQuad(
      program,
      renderData.m_quad,
      *seg,
      *(geometryImage ? geometryImage : seg),
      view,
      m_appData.windowData().viewport(),
      worldOffsetXhairs,
      renderData.m_flashlightRadius,
      renderData.m_flashlightOverlays,
      renderData.m_segOutlineStyle,
      renderData.m_segInteriorOpacity,
      renderData.m_segInterpCutoff);
  }
  program.stopUse();

  unbindBufferTextures(boundBufferTextures);
  unbindTextures(boundDefTextures);
  unbindTextures(boundTextures);
}
