#include "rendering/ImageShaderCapabilities.h"

#include "rendering/gl/Uniforms.h"

#include <stdexcept>

namespace rendering
{

bool supportsIntensityProjection(const Uniforms& uniforms)
{
  const bool hasMode = uniforms.containsKey("u_mipMode");
  const bool hasSampleCount = uniforms.containsKey("u_halfNumMipSamples");
  if (hasMode != hasSampleCount) {
    throw std::logic_error("Image shader has an incomplete intensity-projection control interface");
  }
  if (!hasMode) {
    return false;
  }

  if (!uniforms.containsKey("u_texSamplingDirZ") || !uniforms.containsKey("u_worldSamplingDirZ")) {
    throw std::logic_error("Image shader has an incomplete intensity-projection sampling interface");
  }
  return true;
}

} // namespace rendering
