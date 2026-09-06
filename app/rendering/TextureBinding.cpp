#include "rendering/Rendering.h"

#include "rendering/gl/GLTexture.h"

void Rendering::unbindTextures(const BoundTextures& textures)
{
  for (const BoundTexture<GLTexture>& binding : textures) {
    binding.texture.get().unbind(binding.unit);
  }
}
