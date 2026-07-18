#include "rendering/Rendering.h"

#include "rendering/utility/gl/GLTexture.h"

#include <functional>
#include <list>

void Rendering::unbindTextures(const std::list<std::reference_wrapper<GLTexture>>& textures)
{
  for (const auto& T : textures) {
    T.get().unbind();
  }
}
