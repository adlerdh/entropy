#include "rendering/ImageShaderCapabilities.h"

#include "rendering/ShaderProgramSetup.h"
#include "rendering/common/ShaderType.h"
#include "rendering/gl/GLUniformTypes.h"
#include "rendering/gl/Uniforms.h"

#include <catch2/catch_test_macros.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <stdexcept>

namespace shader_setup = rendering::shader_setup;

TEST_CASE("every main shader has a complete or absent intensity projection interface")
{
  const auto setup = shader_setup::buildProgramSetup();
  for (const ShaderProgramType type : setup.shaderTypes) {
    INFO("shader type: " << to_string(type));
    CHECK_NOTHROW(rendering::supportsIntensityProjection(setup.shaderInfo.at(type).fsUniforms));
  }
}

TEST_CASE("scalar image shaders advertise intensity projection support")
{
  const auto setup = shader_setup::buildProgramSetup();
  const std::array projectionPrograms{
    ShaderProgramType::ImageGrayLinear,
    ShaderProgramType::ImageGrayLinearFloating,
    ShaderProgramType::ImageGrayCubic,
    ShaderProgramType::ImageGrayLinearWarped,
    ShaderProgramType::ImageGrayLinearFloatingWarped,
    ShaderProgramType::ImageGrayCubicWarped,
    ShaderProgramType::XrayLinear,
    ShaderProgramType::XrayCubic,
    ShaderProgramType::XrayLinearWarped,
    ShaderProgramType::XrayCubicWarped,
    ShaderProgramType::IsoContourLinearFloating,
    ShaderProgramType::IsoContourLinearFixed,
    ShaderProgramType::IsoContourCubicFixed,
    ShaderProgramType::IsoContourLinearFloatingWarped,
    ShaderProgramType::IsoContourLinearFixedWarped,
    ShaderProgramType::IsoContourCubicFixedWarped,
    ShaderProgramType::DifferenceLinear,
    ShaderProgramType::DifferenceCubic,
    ShaderProgramType::DifferenceLinearWarped,
    ShaderProgramType::DifferenceCubicWarped};

  for (const ShaderProgramType type : projectionPrograms) {
    INFO("shader type: " << to_string(type));
    CHECK(rendering::supportsIntensityProjection(setup.shaderInfo.at(type).fsUniforms));
  }
}

TEST_CASE("color and vector image shaders do not advertise intensity projection support")
{
  const auto setup = shader_setup::buildProgramSetup();
  const std::array nonProjectionPrograms{
    ShaderProgramType::ImageColorLinear,
    ShaderProgramType::ImageColorCubic,
    ShaderProgramType::ImageColorLinearWarped,
    ShaderProgramType::ImageColorCubicWarped,
    ShaderProgramType::VectorDirectionColorLinear,
    ShaderProgramType::VectorDirectionColorCubic,
    ShaderProgramType::VectorSignedNormalProjectionLinear,
    ShaderProgramType::VectorSignedNormalProjectionCubic,
    ShaderProgramType::VectorPlanarProjectionColorLinear,
    ShaderProgramType::VectorPlanarProjectionColorCubic,
    ShaderProgramType::VectorWarpedGridLinear,
    ShaderProgramType::VectorWarpedGridCubic};

  for (const ShaderProgramType type : nonProjectionPrograms) {
    INFO("shader type: " << to_string(type));
    CHECK_FALSE(rendering::supportsIntensityProjection(setup.shaderInfo.at(type).fsUniforms));
  }
}

TEST_CASE("partial intensity projection interfaces are rejected")
{
  Uniforms missingSampleCount;
  missingSampleCount.insertUniform("u_mipMode", UniformType::Int, 0);
  CHECK_THROWS_AS(rendering::supportsIntensityProjection(missingSampleCount), std::logic_error);

  Uniforms missingSamplingDirection;
  missingSamplingDirection.insertUniform("u_mipMode", UniformType::Int, 0);
  missingSamplingDirection.insertUniform("u_halfNumMipSamples", UniformType::Int, 0);
  missingSamplingDirection.insertUniform("u_texSamplingDirZ", UniformType::Vec3, glm::vec3{0.0f});
  CHECK_THROWS_AS(rendering::supportsIntensityProjection(missingSamplingDirection), std::logic_error);
}

TEST_CASE("direct and warped image shaders expose mutually exclusive sampling transforms")
{
  const auto setup = shader_setup::buildProgramSetup();
  const std::array directPrograms{
    ShaderProgramType::ImageGrayLinear,
    ShaderProgramType::ImageGrayLinearFloating,
    ShaderProgramType::ImageGrayCubic,
    ShaderProgramType::ImageColorLinear,
    ShaderProgramType::ImageColorCubic,
    ShaderProgramType::EdgeSobelLinear,
    ShaderProgramType::EdgeSobelCubic,
    ShaderProgramType::XrayLinear,
    ShaderProgramType::XrayCubic,
    ShaderProgramType::SegmentationNearest,
    ShaderProgramType::SegmentationLinear,
    ShaderProgramType::IsoContourLinearFloating,
    ShaderProgramType::IsoContourLinearFixed,
    ShaderProgramType::IsoContourCubicFixed};
  const std::array warpedPrograms{
    ShaderProgramType::ImageGrayLinearWarped,
    ShaderProgramType::ImageGrayLinearFloatingWarped,
    ShaderProgramType::ImageGrayCubicWarped,
    ShaderProgramType::ImageColorLinearWarped,
    ShaderProgramType::ImageColorCubicWarped,
    ShaderProgramType::EdgeSobelLinearWarped,
    ShaderProgramType::EdgeSobelCubicWarped,
    ShaderProgramType::XrayLinearWarped,
    ShaderProgramType::XrayCubicWarped,
    ShaderProgramType::SegmentationNearestWarped,
    ShaderProgramType::SegmentationLinearWarped,
    ShaderProgramType::IsoContourLinearFloatingWarped,
    ShaderProgramType::IsoContourLinearFixedWarped,
    ShaderProgramType::IsoContourCubicFixedWarped};

  for (const ShaderProgramType type : directPrograms) {
    const auto& info = setup.shaderInfo.at(type);
    Uniforms uniforms = info.vsUniforms;
    uniforms.insertUniforms(info.fsUniforms);
    INFO("shader type: " << to_string(type));
    CHECK_NOTHROW(rendering::validateImageSamplingTransform(uniforms, rendering::ImageSamplingTransform::Direct));
    CHECK_THROWS_AS(
      rendering::validateImageSamplingTransform(uniforms, rendering::ImageSamplingTransform::Deformation),
      std::logic_error);
  }

  for (const ShaderProgramType type : warpedPrograms) {
    const auto& info = setup.shaderInfo.at(type);
    Uniforms uniforms = info.vsUniforms;
    uniforms.insertUniforms(info.fsUniforms);
    INFO("shader type: " << to_string(type));
    CHECK_NOTHROW(rendering::validateImageSamplingTransform(uniforms, rendering::ImageSamplingTransform::Deformation));
    CHECK_THROWS_AS(
      rendering::validateImageSamplingTransform(uniforms, rendering::ImageSamplingTransform::Direct),
      std::logic_error);
  }
}

TEST_CASE("partial or mixed image sampling-transform interfaces are rejected")
{
  Uniforms partialDeformation;
  partialDeformation.insertUniform("u_defTex", UniformType::Sampler, Uniforms::SamplerIndexType{0});
  CHECK_THROWS_AS(
    rendering::validateImageSamplingTransform(partialDeformation, rendering::ImageSamplingTransform::Deformation),
    std::logic_error);

  Uniforms mixed;
  mixed.insertUniform("u_tex_T_world", UniformType::Mat4, glm::mat4{1.0f});
  mixed.insertUniform("u_defTex", UniformType::Sampler, Uniforms::SamplerIndexType{0});
  mixed.insertUniform("u_defTex_T_world", UniformType::Mat4, glm::mat4{1.0f});
  mixed.insertUniform("u_sampleTex_T_world", UniformType::Mat4, glm::mat4{1.0f});
  mixed.insertUniform("u_defSlope_native_T_texture", UniformType::Float, 1.0f);
  mixed.insertUniform("u_deformationStrength", UniformType::Float, 1.0f);
  mixed.insertUniform("u_defInterleaved", UniformType::Bool, false);
  CHECK_THROWS_AS(
    rendering::validateImageSamplingTransform(mixed, rendering::ImageSamplingTransform::Direct),
    std::logic_error);
}

TEST_CASE("direct and warped metric shaders expose mutually exclusive sampling transforms")
{
  const auto setup = shader_setup::buildProgramSetup();
  const std::array directPrograms{
    ShaderProgramType::DifferenceLinear,
    ShaderProgramType::DifferenceCubic,
    ShaderProgramType::LocalNccLinear,
    ShaderProgramType::LocalNccCubic,
    ShaderProgramType::LocalLinearResidualLinear,
    ShaderProgramType::LocalLinearResidualCubic,
    ShaderProgramType::OverlapLinear,
    ShaderProgramType::OverlapCubic};
  const std::array warpedPrograms{
    ShaderProgramType::DifferenceLinearWarped,
    ShaderProgramType::DifferenceCubicWarped,
    ShaderProgramType::LocalNccLinearWarped,
    ShaderProgramType::LocalNccCubicWarped,
    ShaderProgramType::LocalLinearResidualLinearWarped,
    ShaderProgramType::LocalLinearResidualCubicWarped,
    ShaderProgramType::OverlapLinearWarped,
    ShaderProgramType::OverlapCubicWarped};

  for (const ShaderProgramType type : directPrograms) {
    const auto& info = setup.shaderInfo.at(type);
    Uniforms uniforms = info.vsUniforms;
    uniforms.insertUniforms(info.fsUniforms);
    INFO("shader type: " << to_string(type));
    CHECK_NOTHROW(rendering::validateMetricSamplingTransform(uniforms, rendering::ImageSamplingTransform::Direct));
    CHECK_THROWS_AS(
      rendering::validateMetricSamplingTransform(uniforms, rendering::ImageSamplingTransform::Deformation),
      std::logic_error);
  }

  for (const ShaderProgramType type : warpedPrograms) {
    const auto& info = setup.shaderInfo.at(type);
    Uniforms uniforms = info.vsUniforms;
    uniforms.insertUniforms(info.fsUniforms);
    INFO("shader type: " << to_string(type));
    CHECK_NOTHROW(rendering::validateMetricSamplingTransform(uniforms, rendering::ImageSamplingTransform::Deformation));
    CHECK_THROWS_AS(
      rendering::validateMetricSamplingTransform(uniforms, rendering::ImageSamplingTransform::Direct),
      std::logic_error);
  }
}
