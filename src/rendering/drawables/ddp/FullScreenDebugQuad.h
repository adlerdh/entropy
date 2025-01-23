#ifndef FULLSCREENDEBUGQUAD_H
#define FULLSCREENDEBUGQUAD_H

#include "rendering/common/ShaderProviderType.h"
#include "rendering/drawables/ddp/FullScreenQuad.h"

#include "common/ObjectCounter.hpp"

class GLTexture;

class FullScreenDebugQuad : public FullScreenQuad, public ObjectCounter<FullScreenDebugQuad>
{
public:
  FullScreenDebugQuad(
    const std::string& name,
    ShaderProgramActivatorType shaderProgramActivator,
    UniformsProviderType uniformsProvider
  );

  FullScreenDebugQuad(const FullScreenDebugQuad&) = delete;
  FullScreenDebugQuad& operator=(const FullScreenDebugQuad&) = delete;

  FullScreenDebugQuad(FullScreenDebugQuad&&) = default;
  FullScreenDebugQuad& operator=(FullScreenDebugQuad&&) = default;

  ~FullScreenDebugQuad() override = default;

  void setTexture(std::weak_ptr<GLTexture>);

private:
  void doRender(const RenderStage&) override;

  static const Uniforms::SamplerIndexType DebugTexSamplerIndex;

  ShaderProgramActivatorType m_shaderProgramActivator;
  UniformsProviderType m_uniformsProvider;

  Uniforms m_uniforms;

  std::weak_ptr<GLTexture> m_texture;
};

#endif // FULLSCREENDEBUGQUAD_H
