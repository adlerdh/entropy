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
  CHECK(mesh.find("albedo * u_lightingAmbient * ao") != std::string::npos);
  CHECK(mesh.find("direct * shadow") != std::string::npos);
  CHECK(mesh.find("vec3(u_lightingSpecular * specular)") != std::string::npos);
  CHECK(raycast.find("uniform vec3 u_ambient") == std::string::npos);
  CHECK(raycast.find("uniform vec3 u_diffuse") == std::string::npos);
  CHECK(raycast.find("uniform vec3 u_specular") == std::string::npos);
}

TEST_CASE("mesh PBR shading uses independent neutral lighting in opaque and DDP paths", "[rendering][shaders][pbr]")
{
  const std::array shaderPaths{"app/rendering/shaders/mesh/Mesh.fs", "app/rendering/shaders/mesh/MeshDdpPeel.fs"};

  for (const char* shaderPath : shaderPaths) {
    const std::string shader = shader_setup::loadEmbeddedShaderSource(shaderPath);
    CHECK(shader.find("albedo * kPbrAmbientStrength * u_ambientOcclusion * ao") != std::string::npos);
    CHECK(shader.find("diffuse * (kPi * kPbrDiffuseStrength)") != std::string::npos);
    CHECK(shader.find("specular * (kPi * kPbrSpecularStrength)") != std::string::npos);
    CHECK(shader.find("kPbrFillLightDirection") != std::string::npos);
    CHECK(shader.find("kPbrFillLightStrength") != std::string::npos);
    CHECK(shader.find("ambient + keyLighting + fillLighting") != std::string::npos);
    CHECK(shader.find("faceforward(normal, -viewDirection, normal)") != std::string::npos);
    CHECK(shader.find("0.14 * albedo") == std::string::npos);
  }
}

TEST_CASE("mesh flat shading uses geometric face normals in opaque and DDP paths", "[rendering][shaders][mesh]")
{
  const std::array shaderPaths{"app/rendering/shaders/mesh/Mesh.fs", "app/rendering/shaders/mesh/MeshDdpPeel.fs"};

  for (const char* shaderPath : shaderPaths) {
    const std::string shader = shader_setup::loadEmbeddedShaderSource(shaderPath);
    CHECK(shader.find("uniform bool u_flatShadingEnabled") != std::string::npos);
    CHECK(shader.find("!u_flatShadingEnabled && dot(v_worldNormal, v_worldNormal)") != std::string::npos);
    CHECK(shader.find("cross(dFdx(v_worldPosition), dFdy(v_worldPosition))") != std::string::npos);
  }
}

TEST_CASE("mesh SSAO uses reconstructed geometry and an edge-preserving filter", "[rendering][shaders][ssao]")
{
  const std::string resolve =
    shader_setup::loadEmbeddedShaderSource("app/rendering/shaders/mesh/AmbientOcclusionResolve.fs");
  const std::string filter =
    shader_setup::loadEmbeddedShaderSource("app/rendering/shaders/mesh/AmbientOcclusionFilter.fs");

  CHECK(resolve.find("u_camera_T_clip") != std::string::npos);
  CHECK(resolve.find("u_clip_T_camera") != std::string::npos);
  CHECK(resolve.find("u_camera_T_worldNormal") != std::string::npos);
  CHECK(resolve.find("u_sampleCount") != std::string::npos);
  CHECK(resolve.find("tangent_T_camera * hemisphere") != std::string::npos);
  CHECK(resolve.find("smoothstep(u_radiusMm * 0.5, u_radiusMm, separation)") != std::string::npos);
  CHECK(filter.find("normalWeight") != std::string::npos);
  CHECK(filter.find("depthWeight") != std::string::npos);
  CHECK(filter.find("pow(clamp(filtered, 0.0, 1.0), u_power)") != std::string::npos);
  CHECK(filter.find("(1.0 - powered) * u_contrast") != std::string::npos);
}

TEST_CASE("ASCII compositing addresses the full framebuffer from the render viewport", "[rendering][shaders][ascii]")
{
  const std::string post = shader_setup::loadEmbeddedShaderSource("app/rendering/shaders/AsciiPost.fs");
  const std::string spatial = shader_setup::loadEmbeddedShaderSource("app/rendering/shaders/AsciiPostSpatial.fs");

  CHECK(post.find("u_sceneOriginPx + v_uv * u_viewSizePx") != std::string::npos);
  CHECK(spatial.find("u_sceneOriginPx + v_uv * u_viewSizePx") != std::string::npos);
}

TEST_CASE("raycasting does not render the mesh-only 3D crosshairs", "[rendering][shaders][crosshairs]")
{
  const std::string raycast = shader_setup::loadEmbeddedShaderSource("app/rendering/shaders/RaycastIso.fs");

  CHECK(raycast.find("u_showCrosshairs3D") == std::string::npos);
  CHECK(raycast.find("raySphereFirstHit") == std::string::npos);
}

TEST_CASE("image plane DDP shaders apply the ordered coplanar depth tie-break", "[rendering][shaders][ddp]")
{
  const std::string init =
    shader_setup::loadEmbeddedShaderSource("app/rendering/shaders/mesh/MeshImagePlaneDdpInit.fs");
  const std::string peel =
    shader_setup::loadEmbeddedShaderSource("app/rendering/shaders/mesh/MeshImagePlaneDdpPeel.fs");

  CHECK(init.find("gl_FragCoord.z - u_ddpDepthBias") != std::string::npos);
  CHECK(peel.find("gl_FragCoord.z - u_ddpDepthBias") != std::string::npos);
}

TEST_CASE("image plane DDP borders use explicit polygon boundaries", "[rendering][shaders][ddp]")
{
  const std::string display =
    shader_setup::loadEmbeddedShaderSource("app/rendering/shaders/mesh/MeshImagePlaneDisplay.glsl");
  const std::string init =
    shader_setup::loadEmbeddedShaderSource("app/rendering/shaders/mesh/MeshImagePlaneDdpInit.fs");
  const std::string peel =
    shader_setup::loadEmbeddedShaderSource("app/rendering/shaders/mesh/MeshImagePlaneDdpPeel.fs");

  CHECK(init.find("u_boundaryWorldPositions") != std::string::npos);
  CHECK(peel.find("u_boundaryWorldPositions") != std::string::npos);
  CHECK(init.find("u_viewportOrigin") != std::string::npos);
  CHECK(peel.find("u_viewportOrigin") != std::string::npos);
  CHECK(init.find("imagePlaneBorderDistancePixels()") != std::string::npos);
  CHECK(peel.find("imagePlaneBorderDistancePixels()") != std::string::npos);
  CHECK(display.find("clipImagePlaneBoundarySegment") != std::string::npos);
  CHECK(display.find("gl_FragCoord.xy - u_viewportOrigin") != std::string::npos);
  CHECK(init.find("for (int axis = 0; axis < 3; ++axis)") == std::string::npos);
  CHECK(peel.find("for (int axis = 0; axis < 3; ++axis)") == std::string::npos);
}

TEST_CASE("3D image planes use the same multi-component display modes as 2D images", "[rendering][shaders][ddp]")
{
  const std::string display =
    shader_setup::loadEmbeddedShaderSource("app/rendering/shaders/mesh/MeshImagePlaneDisplay.glsl");
  const std::string init =
    shader_setup::loadEmbeddedShaderSource("app/rendering/shaders/mesh/MeshImagePlaneDdpInit.fs");
  const std::string peel =
    shader_setup::loadEmbeddedShaderSource("app/rendering/shaders/mesh/MeshImagePlaneDdpPeel.fs");

  CHECK(display.find("u_imgRgbaTex[4]") != std::string::npos);
  CHECK(display.find("colorImagePlaneColor") != std::string::npos);
  CHECK(display.find("vectorImagePlaneColor") != std::string::npos);
  CHECK(display.find("scalarImagePlaneColor") != std::string::npos);
  CHECK(display.find("u_imgSlopeInterceptRgba") != std::string::npos);
  CHECK(display.find("u_imgThresholdsRgba") != std::string::npos);
  CHECK(display.find("u_imgOpacityRgba") != std::string::npos);
  CHECK(init.find("displayedImagePlaneColor(sampleTc, fs_in.v_worldPos).a") != std::string::npos);
  CHECK(peel.find("displayedImagePlaneColor(sampleTc, fs_in.v_worldPos)") != std::string::npos);
}

TEST_CASE("mesh shaders reconstruct missing normals and filter shadow maps", "[rendering][shaders][mesh]")
{
  const std::string vertex = shader_setup::loadEmbeddedShaderSource("app/rendering/shaders/mesh/Mesh.vs");
  const std::string opaque = shader_setup::loadEmbeddedShaderSource("app/rendering/shaders/mesh/Mesh.fs");
  const std::string peel = shader_setup::loadEmbeddedShaderSource("app/rendering/shaders/mesh/MeshDdpPeel.fs");

  CHECK(
    vertex.find("u_hasVertexNormals ? normalize(u_world_T_meshNormal * a_normal) : vec3(0.0)") != std::string::npos);
  CHECK(opaque.find("cross(dFdx(v_worldPosition), dFdy(v_worldPosition))") != std::string::npos);
  CHECK(peel.find("cross(dFdx(v_worldPosition), dFdy(v_worldPosition))") != std::string::npos);
  CHECK(opaque.find("occludedSamples / 9.0") != std::string::npos);
  CHECK(peel.find("occludedSamples / 9.0") != std::string::npos);
  CHECK(opaque.find("u_shadowMapEnabled ? normalize(u_lightDirectionWorld) : viewDirection") != std::string::npos);
  CHECK(peel.find("u_shadowMapEnabled ? normalize(u_lightDirectionWorld) : viewDirection") != std::string::npos);
  CHECK(opaque.find("u_shadowDepthBias * mix(1.0, 3.0, normalOffset)") != std::string::npos);
  CHECK(peel.find("u_shadowDepthBias * mix(1.0, 3.0, normalOffset)") != std::string::npos);
  CHECK(opaque.find("textureSize(u_screenAmbientOcclusionTex, 0) - ivec2(1)") != std::string::npos);
  CHECK(peel.find("textureSize(u_screenAmbientOcclusionTex, 0) - ivec2(1)") != std::string::npos);
}
