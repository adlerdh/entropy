#include "rendering/ShaderSourceSetup.h"

#include <cmrc/cmrc.hpp>

CMRC_DECLARE(shaders);

namespace rendering::shader_setup
{

std::string loadEmbeddedShaderSource(const std::string& path)
{
  const auto filesystem = cmrc::shaders::get_filesystem();
  const cmrc::file data = filesystem.open(path);
  return {data.begin(), data.end()};
}

TextureLookupReplacementSources ShaderSourceSet::textureLookupReplacementSources() const
{
  return TextureLookupReplacementSources{
    .linear3D = textureLinear3D,
    .linear2D = textureLinear2D,
    .floatingPointLinear3D = textureFloatingPointLinear3D,
    .floatingPointLinear2D = textureFloatingPointLinear2D,
    .cubic3D = textureCubic3D,
    .cubic2D = textureCubic2D,
    .uintLinear2D = uintTextureLinear2D};
}

ShaderSourceSet buildShaderSourceSet()
{
  static const std::string shaderPath("app/rendering/shaders/");

  return ShaderSourceSet{
    .helpers = loadEmbeddedShaderSource(shaderPath + "functions/Helpers.glsl"),
    .colorHelpers = loadEmbeddedShaderSource(shaderPath + "functions/ColorHelpers.glsl"),
    .doRender = loadEmbeddedShaderSource(shaderPath + "functions/DoRender.glsl"),
    .textureFloatingPointLinear3D =
      loadEmbeddedShaderSource(shaderPath + "functions/TextureLookup_FloatingPoint_Linear.glsl"),
    .textureLinear3D = loadEmbeddedShaderSource(shaderPath + "functions/TextureLookup_Linear.glsl"),
    .textureCubic3D = loadEmbeddedShaderSource(shaderPath + "functions/TextureLookup_Cubic.glsl"),
    .uintTextureLinear3D = loadEmbeddedShaderSource(shaderPath + "functions/UIntTextureLookup_Linear.glsl"),
    .textureFloatingPointLinear2D =
      loadEmbeddedShaderSource(shaderPath + "functions/TextureLookup_FloatingPoint_Linear_2D.glsl"),
    .textureLinear2D = loadEmbeddedShaderSource(shaderPath + "functions/TextureLookup_Linear_2D.glsl"),
    .textureCubic2D = loadEmbeddedShaderSource(shaderPath + "functions/TextureLookup_Cubic_2D.glsl"),
    .uintTextureLinear2D = loadEmbeddedShaderSource(shaderPath + "functions/UIntTextureLookup_Linear_2D.glsl"),
    .sampleTexCoordIdentity = loadEmbeddedShaderSource(shaderPath + "functions/SampleTexCoord_Identity.glsl"),
    .sampleTexCoordDeformation = loadEmbeddedShaderSource(shaderPath + "functions/SampleTexCoord_Deformation.glsl"),
    .metricSamplingIdentity = loadEmbeddedShaderSource(shaderPath + "functions/MetricSampling_Identity.glsl"),
    .metricSamplingDeformation = loadEmbeddedShaderSource(shaderPath + "functions/MetricSampling_Deformation.glsl"),
    .segValueNearest = loadEmbeddedShaderSource(shaderPath + "functions/SegValue_Nearest.glsl"),
    .segValueLinear = loadEmbeddedShaderSource(shaderPath + "functions/SegValue_Linear.glsl"),
    .segInteriorAlphaWithOutline = loadEmbeddedShaderSource(shaderPath + "functions/SegInteriorAlpha_WithOutline.glsl"),
    .edgeSobel = loadEmbeddedShaderSource(shaderPath + "functions/ComputeEdge_Sobel.glsl"),
    .intensityProjection = loadEmbeddedShaderSource(shaderPath + "functions/IntensityProjection.glsl")};
}

} // namespace rendering::shader_setup
