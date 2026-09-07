#include "rendering/ImageShaderCapabilities.h"

#include "rendering/gl/Uniforms.h"

#include <stdexcept>
#include <string>

namespace rendering
{

namespace
{

bool hasCompleteDeformationInterface(const Uniforms& uniforms)
{
  const char* const deformationUniforms[]{
    "u_defTex",
    "u_defTex_T_world",
    "u_sampleTex_T_world",
    "u_defSlope_native_T_texture",
    "u_deformationStrength",
    "u_defInterleaved"};

  bool any = false;
  bool all = true;
  for (const char* name : deformationUniforms) {
    const bool present = uniforms.containsKey(name);
    any = any || present;
    all = all && present;
  }
  if (any && !all) {
    throw std::logic_error("Image shader has an incomplete deformation-sampling interface");
  }
  return all;
}

bool hasCompleteMetricDeformationInterface(const Uniforms& uniforms)
{
  const char* const deformationUniforms[]{
    "u_defTex0",
    "u_defTex1",
    "u_defTex_T_world",
    "u_sampleTex_T_world",
    "u_defSlope_native_T_texture",
    "u_deformationStrength",
    "u_warpEnabled[0]",
    "u_warpEnabled[1]",
    "u_defInterleaved[0]",
    "u_defInterleaved[1]"};

  bool any = false;
  bool all = true;
  for (const char* name : deformationUniforms) {
    const bool present = uniforms.containsKey(name);
    any = any || present;
    all = all && present;
  }
  if (any && !all) {
    throw std::logic_error("Metric shader has an incomplete deformation-sampling interface");
  }
  return all;
}

void validateSamplingTransform(
  const bool hasDirect,
  const bool hasDeformation,
  const ImageSamplingTransform expected,
  const char* shaderKind)
{
  if (hasDirect == hasDeformation) {
    throw std::logic_error(std::string{shaderKind} + " shader must expose exactly one sampling-transform interface");
  }

  const bool expectedInterfacePresent = ImageSamplingTransform::Direct == expected ? hasDirect : hasDeformation;
  if (!expectedInterfacePresent) {
    throw std::logic_error(
      std::string{shaderKind} + " shader sampling-transform interface does not match the selected render path");
  }
}

} // namespace

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

void validateImageSamplingTransform(const Uniforms& uniforms, const ImageSamplingTransform expected)
{
  const bool hasDirect = uniforms.containsKey("u_tex_T_world");
  const bool hasDeformation = hasCompleteDeformationInterface(uniforms);
  validateSamplingTransform(hasDirect, hasDeformation, expected, "Image");
}

void validateMetricSamplingTransform(const Uniforms& uniforms, const ImageSamplingTransform expected)
{
  const bool hasDirect = uniforms.containsKey("u_tex_T_world");
  const bool hasDeformation = hasCompleteMetricDeformationInterface(uniforms);
  validateSamplingTransform(hasDirect, hasDeformation, expected, "Metric");
}

} // namespace rendering
