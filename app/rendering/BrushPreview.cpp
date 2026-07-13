#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "rendering/ImageDrawing.h"
#include "rendering/utility/gl/GLTexture.h"
#include "windowing/View.h"

#include <functional>
#include <list>

namespace
{
using namespace uuids;

const Uniforms::SamplerIndexType s_segTexSampler{0};
const Uniforms::SamplerIndexType s_segLabelTableTexSampler{1};
} // namespace

void Rendering::renderBrushPreview(const View& view, const glm::vec3& worldOffsetXhairs, const ImgSegPair& imgSegPair)
{
  const auto& imageUid = imgSegPair.first;
  const auto& segUid = imgSegPair.second;
  if (!imageUid || !segUid) {
    return;
  }

  auto& R = m_appData.renderData();
  const auto previewIt = R.m_brushPreviews.find(*imageUid);
  if (previewIt == R.m_brushPreviews.end()) {
    return;
  }

  RenderData::BrushPreview& preview = previewIt->second;
  if (!preview.visible || preview.imageUid != *imageUid || preview.segUid != *segUid || !preview.texture) {
    return;
  }

  const std::optional<uuid> deformationUid = activeRenderableDeformationUid(*imageUid);
  const bool renderWarped = deformationUid.has_value();
  GLShaderProgram& program = *m_shaderPrograms.at(
    renderWarped ? ShaderProgramType::SegmentationNearestWarped : ShaderProgramType::SegmentationNearest);
  preview.texture->bind(s_segTexSampler.index);
  const auto boundBufferTextures = bindSegBufferTextures(imgSegPair);
  const auto boundDeformationTextures =
    renderWarped ? bindDeformationTextures(*deformationUid) : std::list<std::reference_wrapper<GLTexture>>{};

  program.use();
  {
    program.setSamplerUniform("u_segTex", s_segTexSampler.index);
    program.setSamplerUniform("u_segLabelCmapTex", s_segLabelTableTexSampler.index);

    program.setUniform("u_numCheckers", static_cast<float>(R.m_numCheckerboardSquares));
    program.setUniform("u_segOpacity", 1.0f);
    program.setUniform("u_useSegColorOverride", true);
    program.setUniform("u_segColorOverride", preview.color);
    program.setUniform("u_quadrants", R.m_quadrants);
    program.setUniform("u_showFix", false);
    program.setUniform("u_renderMode", static_cast<int>(ViewRenderMode::Image));

    if (renderWarped) {
      setDeformationUniforms(program, *imageUid, *deformationUid, preview.texture_T_world);
    }

    const float fillOpacity =
      (preview.allowFill && BrushPreviewStyle::OutlineAndFill == m_appData.settings().brushPreviewStyle())
        ? m_appData.settings().brushPreviewFillOpacity()
        : 0.0f;

    drawSegPreviewQuad(
      program,
      R.m_quad,
      preview.texture_T_world,
      preview.voxel_T_world,
      preview.textureCapacity,
      view,
      m_appData.windowData().viewport(),
      worldOffsetXhairs,
      R.m_flashlightRadius,
      R.m_flashlightOverlays,
      m_appData.settings().brushPreviewOutlineStyle(),
      fillOpacity);
  }
  program.stopUse();

  unbindTextures(boundDeformationTextures);
  unbindBufferTextures(boundBufferTextures);
  preview.texture->unbind();
}
