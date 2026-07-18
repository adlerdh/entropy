#include "rendering/ShaderSourceSetup.h"

#include <cmrc/cmrc.hpp>

CMRC_DECLARE(shaders);

namespace
{

std::string loadShaderFile(const std::string& path)
{
  const auto filesystem = cmrc::shaders::get_filesystem();
  const cmrc::file data = filesystem.open(path);
  return {data.begin(), data.end()};
}

} // namespace

namespace rendering::shader_setup
{

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
    .helpers = loadShaderFile(shaderPath + "functions/Helpers.glsl"),
    .colorHelpers = loadShaderFile(shaderPath + "functions/ColorHelpers.glsl"),
    .doRender = loadShaderFile(shaderPath + "functions/DoRender.glsl"),
    .textureFloatingPointLinear3D = loadShaderFile(shaderPath + "functions/TextureLookup_FloatingPoint_Linear.glsl"),
    .textureLinear3D = loadShaderFile(shaderPath + "functions/TextureLookup_Linear.glsl"),
    .textureCubic3D = loadShaderFile(shaderPath + "functions/TextureLookup_Cubic.glsl"),
    .uintTextureLinear3D = loadShaderFile(shaderPath + "functions/UIntTextureLookup_Linear.glsl"),
    .textureFloatingPointLinear2D = loadShaderFile(shaderPath + "functions/TextureLookup_FloatingPoint_Linear_2D.glsl"),
    .textureLinear2D = loadShaderFile(shaderPath + "functions/TextureLookup_Linear_2D.glsl"),
    .textureCubic2D = loadShaderFile(shaderPath + "functions/TextureLookup_Cubic_2D.glsl"),
    .uintTextureLinear2D = loadShaderFile(shaderPath + "functions/UIntTextureLookup_Linear_2D.glsl"),
    .sampleTexCoordIdentity = loadShaderFile(shaderPath + "functions/SampleTexCoord_Identity.glsl"),
    .sampleTexCoordDeformation = loadShaderFile(shaderPath + "functions/SampleTexCoord_Deformation.glsl"),
    .metricSamplingIdentity = loadShaderFile(shaderPath + "functions/MetricSampling_Identity.glsl"),
    .metricSamplingDeformation = loadShaderFile(shaderPath + "functions/MetricSampling_Deformation.glsl"),
    .segValueNearest = loadShaderFile(shaderPath + "functions/SegValue_Nearest.glsl"),
    .segValueLinear = loadShaderFile(shaderPath + "functions/SegValue_Linear.glsl"),
    .segInteriorAlphaWithOutline = loadShaderFile(shaderPath + "functions/SegInteriorAlpha_WithOutline.glsl"),
    .edgeSobel = loadShaderFile(shaderPath + "functions/ComputeEdge_Sobel.glsl"),
    .intensityProjection = loadShaderFile(shaderPath + "functions/IntensityProjection.glsl")};
}

} // namespace rendering::shader_setup
