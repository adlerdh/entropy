#include "rendering/PixelEdgeRenderer.h"

#include "common/Exception.hpp"
#include "common/Expected.h"
#include "rendering/utility/containers/Uniforms.h"
#include "rendering/utility/gl/GLShader.h"
#include "rendering/utility/gl/GLTextureTypes.h"

#include <cmrc/cmrc.hpp>

#include <glad/glad.h>
#include <spdlog/spdlog.h>

#include <format>

CMRC_DECLARE(shaders);

namespace
{

const glm::vec2 k_zeroVec2{0.0f, 0.0f};
const glm::vec4 k_zeroVec4{0.0f, 0.0f, 0.0f, 0.0f};

entropy_expected::expected<std::unique_ptr<GLShaderProgram>, std::string> buildPixelEdgeShaderProgram()
{
  static const std::string shaderPath("app/rendering/shaders/");
  static const std::string vsName("AsciiPost.vs");
  static const std::string fsName("PixelEdgePost.fs");

  const auto filesystem = cmrc::shaders::get_filesystem();
  std::string vsSource;
  std::string fsSource;

  try {
    const cmrc::file vsData = filesystem.open(shaderPath + vsName);
    const cmrc::file fsData = filesystem.open(shaderPath + fsName);
    vsSource = std::string(vsData.begin(), vsData.end());
    fsSource = std::string(fsData.begin(), fsData.end());
  }
  catch (const std::exception& e) {
    return entropy_expected::unexpected(std::format("Exception loading pixel-edge shader: {}", e.what()));
  }

  Uniforms fsUniforms;
  fsUniforms.insertUniform("u_sceneTex", UniformType::Sampler, Uniforms::SamplerIndexType{3});
  fsUniforms.insertUniform("u_cmapTex", UniformType::Sampler, Uniforms::SamplerIndexType{1});
  fsUniforms.insertUniform("u_sceneSizePx", UniformType::Vec2, k_zeroVec2);
  fsUniforms.insertUniform("u_viewOriginPx", UniformType::Vec2, k_zeroVec2);
  fsUniforms.insertUniform("u_viewSizePx", UniformType::Vec2, k_zeroVec2);
  fsUniforms.insertUniform("u_thresholdEdges", UniformType::Bool, false);
  fsUniforms.insertUniform("u_thinEdges", UniformType::Bool, true);
  fsUniforms.insertUniform("u_edgeScale", UniformType::Float, 2.0f);
  fsUniforms.insertUniform("u_edgeThreshold", UniformType::Float, 0.2f);
  fsUniforms.insertUniform("u_cmapSlopeIntercept", UniformType::Vec2, k_zeroVec2);
  fsUniforms.insertUniform("u_colormapEdges", UniformType::Bool, false);
  fsUniforms.insertUniform("u_edgeColor", UniformType::Vec4, k_zeroVec4);
  fsUniforms.insertUniform("u_overlayEdges", UniformType::Bool, false);

  GLShader vs(vsName, ShaderType::Vertex, vsSource.c_str());
  GLShader fs(fsName, ShaderType::Fragment, fsSource.c_str());
  fs.setRegisteredUniforms(fsUniforms);

  auto program = std::make_unique<GLShaderProgram>(to_string(ShaderProgramType::PixelEdgePost));

  if (!program->attachShader(vs)) {
    return entropy_expected::unexpected(std::format("Unable to compile pixel-edge vertex shader {}", vsName));
  }
  if (!program->attachShader(fs)) {
    return entropy_expected::unexpected(std::format("Unable to compile pixel-edge fragment shader {}", fsName));
  }
  if (!program->link()) {
    return entropy_expected::unexpected("Failed to link pixel-edge shader program");
  }

  return program;
}

} // namespace

void PixelEdgeRenderer::init()
{
  m_postVao.generate();
}

void PixelEdgeRenderer::registerShaderPrograms(
  std::unordered_map<ShaderProgramType, std::unique_ptr<GLShaderProgram>>& programs)
{
  auto prog = buildPixelEdgeShaderProgram();
  if (prog) {
    programs.emplace(ShaderProgramType::PixelEdgePost, std::move(*prog));
  }
  else {
    spdlog::error(prog.error());
    throwDebug(std::format("Failed to create shader program {}", to_string(ShaderProgramType::PixelEdgePost)));
  }
}

void PixelEdgeRenderer::render(
  std::unordered_map<ShaderProgramType, std::unique_ptr<GLShaderProgram>>& shaderPrograms,
  glm::ivec4 defaultViewport,
  const ViewRect& viewRect,
  const RenderData::ImageUniforms& uniforms,
  const DrawImageFn& drawImage,
  const BindPostTexturesFn& bindPostTextures)
{
  const glm::ivec2 deviceSize{defaultViewport.z, defaultViewport.w};
  if (deviceSize.x <= 0 || deviceSize.y <= 0 || viewRect.width <= 0 || viewRect.height <= 0) {
    return;
  }

  ensureSceneFboSize(deviceSize);
  if (!m_sceneColorTex) {
    return;
  }

  GLboolean previousScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
  GLboolean previousDepthEnabled = glIsEnabled(GL_DEPTH_TEST);
  GLboolean previousStencilEnabled = glIsEnabled(GL_STENCIL_TEST);
  GLint previousViewport[4] = {0, 0, 0, 0};
  GLfloat previousClearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  glGetIntegerv(GL_VIEWPORT, previousViewport);
  glGetFloatv(GL_COLOR_CLEAR_VALUE, previousClearColor);

  m_sceneFbo.bind(fbo::TargetType::Draw);
  glViewport(0, 0, deviceSize.x, deviceSize.y);
  glEnable(GL_SCISSOR_TEST);
  glScissor(
    static_cast<GLint>(viewRect.sceneX),
    static_cast<GLint>(viewRect.sceneY),
    static_cast<GLsizei>(viewRect.width),
    static_cast<GLsizei>(viewRect.height));
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  drawImage();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(defaultViewport.x, defaultViewport.y, deviceSize.x, deviceSize.y);
  glScissor(
    static_cast<GLint>(viewRect.windowX),
    static_cast<GLint>(viewRect.windowY),
    static_cast<GLsizei>(viewRect.width),
    static_cast<GLsizei>(viewRect.height));
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);

  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, m_sceneColorTex->id());
  bindPostTextures();

  GLShaderProgram& program = *shaderPrograms.at(ShaderProgramType::PixelEdgePost);
  program.use();
  program.setSamplerUniform("u_sceneTex", 3);
  program.setSamplerUniform("u_cmapTex", 1);
  program.setUniform("u_sceneSizePx", glm::vec2{static_cast<float>(deviceSize.x), static_cast<float>(deviceSize.y)});
  program.setUniform(
    "u_viewOriginPx",
    glm::vec2{static_cast<float>(viewRect.sceneX), static_cast<float>(viewRect.sceneY)});
  program.setUniform(
    "u_viewSizePx",
    glm::vec2{static_cast<float>(viewRect.width), static_cast<float>(viewRect.height)});
  program.setUniform("u_thresholdEdges", uniforms.thresholdPixelEdges);
  program.setUniform("u_thinEdges", uniforms.thinPixelEdges);
  program.setUniform("u_edgeScale", uniforms.pixelEdgeScale);
  program.setUniform("u_edgeThreshold", uniforms.pixelEdgeThreshold);
  program.setUniform("u_cmapSlopeIntercept", uniforms.cmapSlopeIntercept);
  program.setUniform("u_colormapEdges", uniforms.colormapEdges);
  program.setUniform("u_edgeColor", uniforms.edgeColor);
  program.setUniform("u_overlayEdges", uniforms.overlayPixelEdges);

  m_postVao.bind();
  glDrawArrays(GL_TRIANGLES, 0, 3);
  m_postVao.release();
  program.stopUse();

  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_1D, 0);
  glActiveTexture(GL_TEXTURE0);
  glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
  glClearColor(previousClearColor[0], previousClearColor[1], previousClearColor[2], previousClearColor[3]);

  previousScissorEnabled ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
  previousDepthEnabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
  previousStencilEnabled ? glEnable(GL_STENCIL_TEST) : glDisable(GL_STENCIL_TEST);
}

void PixelEdgeRenderer::ensureSceneFboSize(glm::ivec2 deviceSize)
{
  if (deviceSize == m_sceneFboSize && m_sceneColorTex) {
    return;
  }
  m_sceneFboSize = deviceSize;

  using namespace tex;
  const glm::uvec3 texSize{static_cast<uint32_t>(deviceSize.x), static_cast<uint32_t>(deviceSize.y), 1u};

  if (!m_sceneColorTex) {
    m_sceneColorTex.emplace(tex::Target::Texture2D);
    m_sceneColorTex->generate();
    m_sceneColorTex->setMinificationFilter(tex::MinificationFilter::Nearest);
    m_sceneColorTex->setMagnificationFilter(tex::MagnificationFilter::Nearest);
    m_sceneColorTex->setWrapMode(tex::WrapMode::ClampToEdge);
  }

  m_sceneColorTex->setSize(texSize);
  m_sceneColorTex->bind(std::nullopt);
  m_sceneColorTex
    ->setData(0, SizedInternalFormat::RGBA8_UNorm, BufferPixelFormat::RGBA, BufferPixelDataType::UInt8, nullptr);
  m_sceneColorTex->unbind();

  if (m_sceneFbo.id() == 0) {
    m_sceneFbo.generate();
  }

  m_sceneFbo.bind(fbo::TargetType::DrawAndRead);
  m_sceneFbo.attach2DTexture(fbo::TargetType::Draw, fbo::AttachmentType::Color, *m_sceneColorTex, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
