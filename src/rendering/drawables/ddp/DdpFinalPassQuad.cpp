#include "rendering/drawables/ddp/DdpFinalPassQuad.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering_old/ShaderNames.h"

#include "common/Exception.hpp"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

namespace
{

static const Uniforms::SamplerIndexType s_frontTexSamplerIndex{0};
static const Uniforms::SamplerIndexType s_backTexSamplerIndex{1};

} // namespace

DdpFinalPassQuad::DdpFinalPassQuad(
  const std::string& name,
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  std::array<GLTexture, 2>& frontBlenderTextures,
  GLTexture& backBlenderTexture
)
  : FullScreenQuad(name)
  ,

  m_shaderProgramActivator(shaderProgramActivator)
  , m_uniformsProvider(uniformsProvider)
  , m_uniforms()
  ,

  m_frontBlenderTextures(frontBlenderTextures)
  , m_backBlenderTexture(backBlenderTexture)
  , m_currentTextureId(0)
{
  if (m_uniformsProvider)
  {
    m_uniforms = m_uniformsProvider(DDPFinalProgram::name);
  }
  else
  {
    spdlog::error("Unable to access UniformsProvider in '{}'", m_name);
    throw_debug("Unable to access UniformsProvider");
  }
}

void DdpFinalPassQuad::setCurrentTextureId(uint32_t i)
{
  m_currentTextureId = i;
}

void DdpFinalPassQuad::doRender(const RenderStage& /*stage*/)
{
  if (!m_shaderProgramActivator)
  {
    spdlog::error("Unable to access ShaderProgramActivator in '{}'", m_name);
    throw_debug("Unable to access ShaderProgramActivator");
  }

  if (auto program = m_shaderProgramActivator(DDPFinalProgram::name))
  {
    m_frontBlenderTextures[m_currentTextureId].bind(s_frontTexSamplerIndex.index);
    m_backBlenderTexture.bind(s_backTexSamplerIndex.index);

    m_uniforms.setValue(DDPFinalProgram::frag::frontBlenderTexture, s_frontTexSamplerIndex);
    m_uniforms.setValue(DDPFinalProgram::frag::backBlenderTexture, s_backTexSamplerIndex);

    program->applyUniforms(m_uniforms);

    // This prevents infinite loop during occlusion query
    glClear(GL_COLOR_BUFFER_BIT);

    drawVao();
  }
}
