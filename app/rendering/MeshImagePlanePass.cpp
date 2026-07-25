#include "rendering/Rendering.h"

#include "image/Image.h"
#include "logic/app/Data.h"
#include "rendering/PrivateMethods.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/mesh/MeshGpuData.h"
#include "rendering/mesh/MeshImagePlaneRenderList.h"
#include "rendering/mesh/MeshViewContext.h"
#include "rendering/mesh/MeshViewViewport.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/containers/Uniforms.h"
#include "viewer/ViewModes.h"
#include "windowing/View.h"

#include <glad/glad.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <functional>
#include <list>
#include <optional>

namespace
{

constexpr Uniforms::SamplerIndexType sk_imgTexSampler{0};
constexpr Uniforms::SamplerIndexType sk_imgCmapTexSampler{1};

class ScopedImagePlaneBlendState
{
public:
  ScopedImagePlaneBlendState()
  {
    m_blendEnabled = glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_BLEND_SRC_RGB, &m_srcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &m_dstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &m_srcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &m_dstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &m_equationRgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &m_equationAlpha);
  }

  ScopedImagePlaneBlendState(const ScopedImagePlaneBlendState&) = delete;
  ScopedImagePlaneBlendState& operator=(const ScopedImagePlaneBlendState&) = delete;

  ~ScopedImagePlaneBlendState()
  {
    glBlendEquationSeparate(static_cast<GLenum>(m_equationRgb), static_cast<GLenum>(m_equationAlpha));
    glBlendFuncSeparate(
      static_cast<GLenum>(m_srcRgb),
      static_cast<GLenum>(m_dstRgb),
      static_cast<GLenum>(m_srcAlpha),
      static_cast<GLenum>(m_dstAlpha));
    m_blendEnabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
  }

private:
  GLboolean m_blendEnabled = GL_FALSE;
  GLint m_srcRgb = GL_ONE;
  GLint m_dstRgb = GL_ZERO;
  GLint m_srcAlpha = GL_ONE;
  GLint m_dstAlpha = GL_ZERO;
  GLint m_equationRgb = GL_FUNC_ADD;
  GLint m_equationAlpha = GL_FUNC_ADD;
};

class ScopedImagePlaneDepthState
{
public:
  ScopedImagePlaneDepthState()
  {
    m_depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &m_depthWriteEnabled);
  }

  ScopedImagePlaneDepthState(const ScopedImagePlaneDepthState&) = delete;
  ScopedImagePlaneDepthState& operator=(const ScopedImagePlaneDepthState&) = delete;

  ~ScopedImagePlaneDepthState()
  {
    glDepthMask(m_depthWriteEnabled);
    m_depthTestEnabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
  }

private:
  GLboolean m_depthTestEnabled = GL_FALSE;
  GLboolean m_depthWriteEnabled = GL_TRUE;
};

GLShaderProgram& shaderProgramForImagePlaneTextureDimension(
  GLShaderProgram& texture3dProgram,
  GLShaderProgram& texture2dProgram,
  const rendering::TextureDimension textureDimension)
{
  return rendering::TextureDimension::Texture2D == textureDimension ? texture2dProgram : texture3dProgram;
}

std::list<std::reference_wrapper<GLTexture>> bindImagePlaneTextures(
  AppData& appData,
  const uuids::uuid& imageUid,
  const uint32_t component,
  const RenderData::PlanarTextureLayout& textureLayout)
{
  auto& renderData = appData.renderData();
  const Image* image = appData.image(imageUid);
  std::list<std::reference_wrapper<GLTexture>> boundTextures;

  GLTexture& blankImageTexture = rendering::TextureDimension::Texture2D == textureLayout.dimension
                                   ? renderData.m_blankImageBlackTransparentTexture2D
                                   : renderData.m_blankImageBlackTransparentTexture;
  GLTexture* imageTexture = &blankImageTexture;

  const auto textureIt = renderData.m_imageTextures.find(imageUid);
  if (image && std::end(renderData.m_imageTextures) != textureIt && !textureIt->second.empty()) {
    const std::size_t componentTextureIndex =
      Image::MultiComponentBufferType::InterleavedImage == image->bufferType() && textureIt->second.size() == 1u
        ? 0u
        : std::min<std::size_t>(component, textureIt->second.size() - 1u);
    imageTexture = &textureIt->second.at(componentTextureIndex);
  }

  imageTexture->bind(sk_imgTexSampler.index);
  boundTextures.emplace_back(*imageTexture);

  const std::optional<uuids::uuid> cmapUid =
    image ? appData.imageColorMapUid(image->settings().colorMapIndex()) : std::nullopt;
  GLTexture& colorMapTexture =
    cmapUid ? renderData.m_colormapTextures.at(*cmapUid) : std::begin(renderData.m_colormapTextures)->second;
  colorMapTexture.bind(sk_imgCmapTexSampler.index);
  boundTextures.emplace_back(colorMapTexture);

  return boundTextures;
}

void setMeshImagePlaneUniforms(
  GLShaderProgram& program,
  const View& view,
  const rendering::mesh::MeshImagePlaneRenderable& renderable,
  const RenderData::ImageUniforms& uniforms,
  const RenderData::PlanarTextureLayout& textureLayout,
  const rendering::mesh::MeshDrawContext& context,
  const int checkerboardSquares)
{
  program.setSamplerUniform("u_imgTex", sk_imgTexSampler.index);
  program.setSamplerUniform("u_cmapTex", sk_imgCmapTexSampler.index);
  rendering::setTexture2DAxesUniforms(program, textureLayout);

  program.setUniform("u_clip_T_world", context.clip_T_world);
  program.setUniform("u_world_T_mesh", renderable.world_T_mesh);
  program.setUniform("u_aspectRatio", view.camera().aspectRatio());
  program.setUniform("u_numCheckers", checkerboardSquares);

  program.setUniform("u_imgSlopeIntercept", uniforms.slopeIntercept_normalized_T_texture);
  program.setUniform("u_applyHsvMod", false);
  program.setUniform("u_cmapHsvModFactors", uniforms.hsvModFactors);
  program.setUniform("u_cmapSlopeIntercept", uniforms.cmapSlopeIntercept);
  program.setUniform("u_cmapQuantLevels", uniforms.cmapQuantLevels);
  program.setUniform("u_imgThresholds", uniforms.thresholds);
  program.setUniform("u_imgMinMax", uniforms.minMax);
  program.setUniform("u_imgOpacity", uniforms.imgOpacity);

  // 3D image planes use the image shader's ordinary layer path. Comparison modes, flashlight masking, and intensity
  // projection are 2D-view concepts and remain disabled for this mesh pass.
  program.setUniform("u_renderMode", 0);
  program.setUniform("u_clipCrosshairs", glm::vec2{0.0f});
  program.setUniform("u_quadrants", glm::ivec2{0, 0});
  program.setUniform("u_showFix", true);
  program.setUniform("u_flashlightRadius", 0.0f);
  program.setUniform("u_flashlightMovingOnFixed", false);
  program.setUniform("u_mipMode", 0);
  program.setUniform("u_halfNumMipSamples", 0);
  program.setUniform("u_texSamplingDirZ", glm::vec3{0.0f});
  program.setUniform("u_worldSamplingDirZ", glm::vec3{0.0f});
}

void drawUploadedImagePlane(const rendering::mesh::MeshGpuData& gpuData)
{
  gpuData.vao().bind();
  gpuData.vao().drawElements(gpuData.drawParams());
  gpuData.vao().release();
}

} // namespace

void Rendering::drawMeshImagePlaneRenderListForView(
  const View& view,
  const rendering::mesh::MeshImagePlaneRenderList& list)
{
  if (list.imagePlanes.empty()) {
    return;
  }

  const rendering::mesh::ScopedMeshViewViewport scopedViewport{view, m_appData.windowData()};
  const rendering::mesh::MeshDrawContext context = rendering::mesh::meshDrawContextForView(m_meshGpuStore, view);
  if (!context.meshLookup) {
    return;
  }

  const ScopedImagePlaneDepthState scopedDepthState;
  const ScopedImagePlaneBlendState scopedBlendState;
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  for (const std::reference_wrapper<const rendering::mesh::MeshImagePlaneRenderable> imagePlaneRef : list.imagePlanes) {
    const rendering::mesh::MeshImagePlaneRenderable& imagePlane = imagePlaneRef.get();
    const rendering::mesh::MeshGpuData* gpuData = context.meshLookup(imagePlane.mesh);
    if (!gpuData || !gpuData->hasTextureCoords()) {
      continue;
    }

    const Image* image = m_appData.image(imagePlane.texture.imageUid);
    if (!image) {
      continue;
    }

    const auto uniformsIt = m_appData.renderData().m_uniforms.find(imagePlane.texture.imageUid);
    if (uniformsIt == std::end(m_appData.renderData().m_uniforms)) {
      continue;
    }

    const RenderData::PlanarTextureLayout textureLayout =
      rendering::textureLayoutOrDefault(m_appData.renderData().m_imageTextureLayouts, imagePlane.texture.imageUid);
    GLShaderProgram& program = shaderProgramForImagePlaneTextureDimension(
      m_meshImagePlaneGrayLinearProgram,
      m_meshImagePlaneGrayLinearTexture2DProgram,
      textureLayout.dimension);

    const auto boundTextures =
      bindImagePlaneTextures(m_appData, imagePlane.texture.imageUid, imagePlane.texture.component, textureLayout);

    program.use();
    {
      setMeshImagePlaneUniforms(
        program,
        view,
        imagePlane,
        uniformsIt->second,
        textureLayout,
        context,
        m_appData.renderData().m_numCheckerboardSquares);
      drawUploadedImagePlane(*gpuData);
    }
    program.stopUse();

    unbindTextures(boundTextures);
  }

  setupOpenGLState();
}
