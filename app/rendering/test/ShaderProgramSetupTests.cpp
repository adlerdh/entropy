#include "rendering/ShaderProgramSetup.h"
#include "rendering/ShaderSourceSetup.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <unordered_set>

namespace shader_setup = rendering::shader_setup;

TEST_CASE("shader program setup registers every main renderer shader exactly once", "[rendering][shaders]")
{
  const auto setup = shader_setup::buildProgramSetup();

  const std::array expectedShaderTypes{
    ShaderProgramType::ImageGrayLinear,
    ShaderProgramType::ImageGrayLinearFloating,
    ShaderProgramType::ImageGrayCubic,
    ShaderProgramType::ImageGrayLinearWarped,
    ShaderProgramType::ImageGrayLinearFloatingWarped,
    ShaderProgramType::ImageGrayCubicWarped,
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
    ShaderProgramType::VectorWarpedGridCubic,
    ShaderProgramType::EdgeSobelLinear,
    ShaderProgramType::EdgeSobelCubic,
    ShaderProgramType::EdgeSobelLinearWarped,
    ShaderProgramType::EdgeSobelCubicWarped,
    ShaderProgramType::XrayLinear,
    ShaderProgramType::XrayCubic,
    ShaderProgramType::XrayLinearWarped,
    ShaderProgramType::XrayCubicWarped,
    ShaderProgramType::SegmentationNearest,
    ShaderProgramType::SegmentationLinear,
    ShaderProgramType::SegmentationNearestWarped,
    ShaderProgramType::SegmentationLinearWarped,
    ShaderProgramType::IsoContourLinearFloating,
    ShaderProgramType::IsoContourLinearFixed,
    ShaderProgramType::IsoContourCubicFixed,
    ShaderProgramType::IsoContourLinearFloatingWarped,
    ShaderProgramType::IsoContourLinearFixedWarped,
    ShaderProgramType::IsoContourCubicFixedWarped,
    ShaderProgramType::DifferenceLinear,
    ShaderProgramType::DifferenceCubic,
    ShaderProgramType::DifferenceLinearWarped,
    ShaderProgramType::DifferenceCubicWarped,
    ShaderProgramType::LocalNccLinear,
    ShaderProgramType::LocalNccCubic,
    ShaderProgramType::LocalNccLinearWarped,
    ShaderProgramType::LocalNccCubicWarped,
    ShaderProgramType::LocalLinearResidualLinear,
    ShaderProgramType::LocalLinearResidualCubic,
    ShaderProgramType::LocalLinearResidualLinearWarped,
    ShaderProgramType::LocalLinearResidualCubicWarped,
    ShaderProgramType::OverlapLinear,
    ShaderProgramType::OverlapCubic,
    ShaderProgramType::OverlapLinearWarped,
    ShaderProgramType::OverlapCubicWarped};

  REQUIRE(setup.shaderTypes.size() == expectedShaderTypes.size());
  REQUIRE(setup.shaderInfo.size() == expectedShaderTypes.size());

  std::unordered_set<ShaderProgramType> uniqueShaderTypes;
  for (const ShaderProgramType shaderType : setup.shaderTypes) {
    REQUIRE(uniqueShaderTypes.insert(shaderType).second);
    REQUIRE(setup.shaderInfo.contains(shaderType));
    REQUIRE_FALSE(setup.shaderInfo.at(shaderType).vsFileName.empty());
    REQUIRE_FALSE(setup.shaderInfo.at(shaderType).fsFileName.empty());
  }

  for (const ShaderProgramType shaderType : expectedShaderTypes) {
    REQUIRE(uniqueShaderTypes.contains(shaderType));
  }

  REQUIRE_FALSE(setup.shaderInfo.contains(ShaderProgramType::AsciiPost));
  REQUIRE_FALSE(setup.shaderInfo.contains(ShaderProgramType::AsciiCellMean));
  REQUIRE_FALSE(setup.shaderInfo.contains(ShaderProgramType::AsciiCellRegions));
  REQUIRE_FALSE(setup.shaderInfo.contains(ShaderProgramType::AsciiPostSpatial));
  REQUIRE_FALSE(setup.shaderInfo.contains(ShaderProgramType::PixelEdgePost));
}

TEST_CASE("shader program setup exposes complete texture lookup replacement sources", "[rendering][shaders]")
{
  const auto setup = shader_setup::buildProgramSetup();

  REQUIRE_FALSE(setup.lookupReplacementSources.linear3D.empty());
  REQUIRE_FALSE(setup.lookupReplacementSources.linear2D.empty());
  REQUIRE_FALSE(setup.lookupReplacementSources.floatingPointLinear3D.empty());
  REQUIRE_FALSE(setup.lookupReplacementSources.floatingPointLinear2D.empty());
  REQUIRE_FALSE(setup.lookupReplacementSources.cubic3D.empty());
  REQUIRE_FALSE(setup.lookupReplacementSources.cubic2D.empty());
  REQUIRE_FALSE(setup.lookupReplacementSources.uintLinear2D.empty());
}

TEST_CASE("raycast and mesh isosurfaces use matching simple lighting contributions", "[rendering][shaders]")
{
  const std::string raycast = shader_setup::loadEmbeddedShaderSource("app/rendering/shaders/RaycastIso.fs");
  const std::string mesh = shader_setup::loadEmbeddedShaderSource("app/rendering/shaders/mesh/Mesh.fs");

  CHECK(raycast.find("u_isoColors[i] * (u_lightingAmbient + u_lightingDiffuse * d)") != std::string::npos);
  CHECK(raycast.find("vec3(u_lightingSpecular * s)") != std::string::npos);
  CHECK(mesh.find("albedo * (u_lightingAmbient + u_lightingDiffuse * diffuse)") != std::string::npos);
  CHECK(mesh.find("vec3(u_lightingSpecular * specular)") != std::string::npos);
  CHECK(raycast.find("uniform vec3 u_ambient") == std::string::npos);
  CHECK(raycast.find("uniform vec3 u_diffuse") == std::string::npos);
  CHECK(raycast.find("uniform vec3 u_specular") == std::string::npos);
}
