#pragma once

#include "rendering/helpers/PipelineHelpers.h"

#include <string>

namespace rendering::shader_setup
{

/**
 * @brief Embedded GLSL source snippets used to specialize shader templates.
 *
 * The strings are owned by this structure so replacement maps can safely refer to them while shader programs are being
 * created.
 */
struct ShaderSourceSet
{
  std::string helpers;
  std::string colorHelpers;
  std::string doRender;
  std::string textureFloatingPointLinear3D;
  std::string textureLinear3D;
  std::string textureCubic3D;
  std::string uintTextureLinear3D;
  std::string textureFloatingPointLinear2D;
  std::string textureLinear2D;
  std::string textureCubic2D;
  std::string uintTextureLinear2D;
  std::string sampleTexCoordIdentity;
  std::string sampleTexCoordDeformation;
  std::string metricSamplingIdentity;
  std::string metricSamplingDeformation;
  std::string segValueNearest;
  std::string segValueLinear;
  std::string segInteriorAlphaWithOutline;
  std::string edgeSobel;
  std::string intensityProjection;

  /**
   * @brief Return the texture lookup snippets needed to specialize shaders for 2D fallback textures.
   */
  TextureLookupReplacementSources textureLookupReplacementSources() const;
};

/**
 * @brief Load all reusable GLSL source snippets from embedded resources.
 */
ShaderSourceSet buildShaderSourceSet();

} // namespace rendering::shader_setup
