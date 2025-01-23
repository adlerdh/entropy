#include "rendering/drawables/ddp/FullScreenDebugQuad.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering_old/ShaderNames.h"

#include "common/Exception.hpp"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

const Uniforms::SamplerIndexType FullScreenDebugQuad::DebugTexSamplerIndex{0};

FullScreenDebugQuad::FullScreenDebugQuad(
  const std::string& name,
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider
)
  : FullScreenQuad(name)
  ,

  m_shaderProgramActivator(shaderProgramActivator)
  , m_uniformsProvider(uniformsProvider)
  , m_uniforms()
  , m_texture()
{
  if (m_uniformsProvider)
  {
    m_uniforms = m_uniformsProvider(DebugProgram::name);
  }
  else
  {
    spdlog::error("Unable to access UniformsProvider in '{}'", m_name);
    throw_debug("Unable to access UniformsProvider");
  }
}

void FullScreenDebugQuad::setTexture(std::weak_ptr<GLTexture> texture)
{
  m_texture = texture;
}

void FullScreenDebugQuad::doRender(const RenderStage& /*stage*/)
{
  if (!m_shaderProgramActivator)
  {
    spdlog::error("Unable to access ShaderProgramActivator in '{}'", m_name);
    throw_debug("Unable to access ShaderProgramActivator");
  }

  if (auto program = m_shaderProgramActivator(DebugProgram::name))
  {
    if (auto texture = m_texture.lock())
    {
      texture->bind(DebugTexSamplerIndex.index);
      m_uniforms.setValue(DebugProgram::frag::debugTexture, DebugTexSamplerIndex);
      program->applyUniforms(m_uniforms);
      drawVao();
    }
    else
    {
      spdlog::error("Null DebugProgram shader program in '{}'", m_name);
      throw_debug("Null DebugProgram shader program");
    }
  }
}
