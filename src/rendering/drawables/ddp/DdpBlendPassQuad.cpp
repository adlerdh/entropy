#include "rendering/drawables/ddp/DdpBlendPassQuad.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering_old/ShaderNames.h"

#include "common/Exception.hpp"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

namespace
{

static const Uniforms::SamplerIndexType s_tempTexSamplerIndex{0};

} // namespace

DdpBlendPassQuad::DdpBlendPassQuad(
  const std::string& name,
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  std::array<GLTexture, 2>& backTempTextures
)
  : FullScreenQuad(name)
  ,

  m_shaderProgramActivator(shaderProgramActivator)
  , m_uniformsProvider(uniformsProvider)
  , m_uniforms()
  ,

  m_backTempTextures(backTempTextures)
  , m_currentTextureId(0)
{
  if (m_uniformsProvider)
  {
    m_uniforms = m_uniformsProvider(DDPBlendProgram::name);
  }
  else
  {
    throw_debug("Unable to access UniformsProvider");
  }
}

void DdpBlendPassQuad::setCurrentTextureId(uint32_t i)
{
  m_currentTextureId = i;
}

void DdpBlendPassQuad::doRender(const RenderStage& /*stage*/)
{
  if (!m_shaderProgramActivator)
  {
    throw_debug("Unable to access ShaderProgramActivator");
  }

  if (auto program = m_shaderProgramActivator(DDPBlendProgram::name))
  {
    m_backTempTextures[m_currentTextureId].bind(s_tempTexSamplerIndex.index);
    m_uniforms.setValue(DDPBlendProgram::frag::tempTexture, s_tempTexSamplerIndex);

    program->applyUniforms(m_uniforms);

    drawVao();
  }
}
