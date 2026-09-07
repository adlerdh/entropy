#include "rendering/ImageShaderCapabilities.h"

#include "rendering/ShaderProgramSetup.h"
#include "rendering/common/ShaderType.h"
#include "rendering/gl/GLUniformTypes.h"
#include "rendering/gl/Uniforms.h"

#include <catch2/catch_test_macros.hpp>

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
