#include "rendering/Rendering.h"

#include "common/Exception.hpp"
#include "rendering/RenderData.h"
#include "rendering/ShaderProgramSetup.h"
#include "rendering/ShaderSourceSetup.h"
#include "rendering/common/ShaderType.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/mesh/MeshDrawOptions.h"
#include "rendering/utility/gl/GLShader.h"

#include <cmrc/cmrc.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include <exception>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

CMRC_DECLARE(shaders);

namespace
{

static constexpr bool k_optionalUniform = false;

using Vec3Vector = std::vector<glm::vec3>;

std::string loadShaderFile(const std::string& path)
{
  const auto filesystem = cmrc::shaders::get_filesystem();
  const cmrc::file data = filesystem.open(path);
  return {data.begin(), data.end()};
}

bool attachShaderFile(GLShaderProgram& program, const ShaderType shaderType, const std::string& path, Uniforms uniforms)
{
  std::string source;
  try {
    source = loadShaderFile(path);
  }
  catch (const std::exception& e) {
    spdlog::critical("Exception when loading mesh shader file '{}': {}", path, e.what());
    throwDebug("Unable to load mesh shader");
  }

  GLShader shader(path, shaderType, source.c_str());
  shader.setRegisteredUniforms(std::move(uniforms));
  program.attachShader(shader);
  spdlog::debug("Compiled shader {}", path);
  return true;
}

bool attachShaderSource(
  GLShaderProgram& program,
  const ShaderType shaderType,
  const std::string& name,
  const std::string& source,
  Uniforms uniforms)
{
  GLShader shader(name, shaderType, source.c_str());
  shader.setRegisteredUniforms(std::move(uniforms));
  program.attachShader(shader);
  spdlog::debug("Compiled shader {}", name);
  return true;
}

bool linkMeshProgram(GLShaderProgram& program)
{
  if (!program.link()) {
    spdlog::critical("Failed to link shader program {}", program.name());
    return false;
  }

  spdlog::debug("Linked shader program {}", program.name());
  return true;
}

Uniforms meshVertexUniforms()
{
  Uniforms uniforms;
  uniforms.insertUniform("u_clip_T_world", UniformType::Mat4, glm::mat4{1.0f});
  uniforms.insertUniform("u_world_T_mesh", UniformType::Mat4, glm::mat4{1.0f});
  uniforms.insertUniform("u_world_T_meshNormal", UniformType::Mat3, glm::mat3{1.0f});
  uniforms.insertUniform("u_hasVertexNormals", UniformType::Bool, true);
  return uniforms;
}

Uniforms meshImagePlaneVertexUniforms()
{
  Uniforms uniforms;
  uniforms.insertUniform("u_clip_T_world", UniformType::Mat4, glm::mat4{1.0f});
  uniforms.insertUniform("u_world_T_mesh", UniformType::Mat4, glm::mat4{1.0f});
  uniforms.insertUniform("u_world_T_meshNormal", UniformType::Mat3, glm::mat3{1.0f});
  uniforms.insertUniform("u_hasVertexNormals", UniformType::Bool, true);
  uniforms.insertUniform("u_aspectRatio", UniformType::Float, 1.0f);
  uniforms.insertUniform("u_numCheckers", UniformType::Int, 1);
  return uniforms;
}

Uniforms meshFragmentUniforms()
{
  Uniforms uniforms;
  uniforms.insertUniform("u_baseColor", UniformType::Vec4, glm::vec4{0.8f, 0.8f, 0.8f, 1.0f});
  uniforms.insertUniform("u_metallic", UniformType::Float, 0.25f);
  uniforms.insertUniform("u_roughness", UniformType::Float, 0.5f);
  uniforms.insertUniform("u_ambientOcclusion", UniformType::Float, 1.0f);
  uniforms.insertUniform("u_shadingModel", UniformType::Int, 0);
  uniforms.insertUniform("u_lightingAmbient", UniformType::Float, 0.30f);
  uniforms.insertUniform("u_lightingDiffuse", UniformType::Float, 0.50f);
  uniforms.insertUniform("u_lightingSpecular", UniformType::Float, 0.20f);
  uniforms.insertUniform("u_lightingSpecularPower", UniformType::Float, 16.0f);
  uniforms.insertUniform("u_rimLightingEnabled", UniformType::Bool, false);
  uniforms.insertUniform("u_rimOpacityStrength", UniformType::Float, 1.0f);
  uniforms.insertUniform("u_rimEmissionStrength", UniformType::Float, 1.0f);
  uniforms.insertUniform("u_rimPower", UniformType::Float, 2.0f);
  uniforms.insertUniform("u_hasVertexColors", UniformType::Bool, false);
  uniforms.insertUniform("u_cameraWorldPosition", UniformType::Vec3, glm::vec3{0.0f});
  uniforms.insertUniform("u_shadowMapEnabled", UniformType::Bool, false);
  uniforms.insertUniform("u_shadowMapTex", UniformType::Sampler, 7, k_optionalUniform);
  uniforms.insertUniform("u_lightClip_T_world", UniformType::Mat4, glm::mat4{1.0f});
  uniforms.insertUniform("u_shadowStrength", UniformType::Float, 0.35f);
  uniforms.insertUniform("u_shadowDepthBias", UniformType::Float, 0.001f);
  uniforms.insertUniform("u_lightDirectionWorld", UniformType::Vec3, glm::vec3{0.0f, 0.0f, 1.0f});
  uniforms.insertUniform("u_screenAmbientOcclusionEnabled", UniformType::Bool, false);
  uniforms.insertUniform("u_screenAmbientOcclusionTex", UniformType::Sampler, 6, k_optionalUniform);
  uniforms.insertUniform("u_viewportOrigin", UniformType::IVec2, glm::ivec2{0});
  uniforms.insertUniform("u_clipPlaneCount", UniformType::Int, 0);
  for (int i = 0; i < rendering::mesh::MaxMeshClipPlanes; ++i) {
    uniforms.insertUniform(
      "u_clipPlanes[" + std::to_string(i) + "]",
      UniformType::Vec4,
      glm::vec4{0.0f, 0.0f, 1.0f, 0.0f});
  }
  return uniforms;
}

Uniforms meshClipFragmentUniforms()
{
  Uniforms uniforms;
  uniforms.insertUniform("u_clipPlaneCount", UniformType::Int, 0);
  for (int i = 0; i < rendering::mesh::MaxMeshClipPlanes; ++i) {
    uniforms.insertUniform(
      "u_clipPlanes[" + std::to_string(i) + "]",
      UniformType::Vec4,
      glm::vec4{0.0f, 0.0f, 1.0f, 0.0f});
  }
  return uniforms;
}

bool createFullscreenMeshDdpProgram(GLShaderProgram& program, const std::string& fsFileName, Uniforms fsUniforms)
{
  attachShaderFile(program, ShaderType::Vertex, "app/rendering/shaders/mesh/FullScreenTriangle.vs", Uniforms{});
  attachShaderFile(program, ShaderType::Fragment, fsFileName, std::move(fsUniforms));
  return linkMeshProgram(program);
}

bool createFullscreenMeshProgram(GLShaderProgram& program, const std::string& fsFileName, Uniforms fsUniforms)
{
  attachShaderFile(program, ShaderType::Vertex, "app/rendering/shaders/mesh/FullScreenTriangle.vs", Uniforms{});
  attachShaderFile(program, ShaderType::Fragment, fsFileName, std::move(fsUniforms));
  return linkMeshProgram(program);
}

bool createMeshImagePlaneProgram(
  GLShaderProgram& program,
  const ShaderProgramType shaderType,
  const RenderData::TextureDimension textureDimension)
{
  const rendering::shader_setup::ProgramSetup setup = rendering::shader_setup::buildProgramSetup();
  const rendering::shader_setup::ShaderInfo& imageShaderInfo = setup.shaderInfo.at(shaderType);
  std::string fsSource = loadShaderFile("app/rendering/shaders/" + imageShaderInfo.fsFileName);
  fsSource = rendering::replacePlaceholders(
    fsSource,
    rendering::shaderReplacementsForTextureDimension(
      imageShaderInfo.fsReplacements,
      textureDimension,
      setup.lookupReplacementSources));

  attachShaderFile(
    program,
    ShaderType::Vertex,
    "app/rendering/shaders/mesh/MeshImagePlane.vs",
    meshImagePlaneVertexUniforms());
  attachShaderSource(program, ShaderType::Fragment, imageShaderInfo.fsFileName, fsSource, imageShaderInfo.fsUniforms);
  return linkMeshProgram(program);
}

bool createMeshImagePlaneDdpProgram(
  GLShaderProgram& program,
  const std::string& fragmentShaderPath,
  const RenderData::TextureDimension textureDimension,
  const bool peelShader)
{
  const rendering::shader_setup::ProgramSetup setup = rendering::shader_setup::buildProgramSetup();
  const rendering::shader_setup::ShaderInfo& imageShaderInfo = setup.shaderInfo.at(ShaderProgramType::ImageGrayLinear);
  const rendering::shader_setup::ShaderSourceSet sources = rendering::shader_setup::buildShaderSourceSet();
  static const std::string sk_uintTextureLinear2DUsingExistingAxes = R"(
ivec3 segTextureSize()
{
  ivec2 texSize = textureSize(u_segTex, 0);
  ivec3 size = ivec3(1);
  size[u_tex2DAxes[0].x] = texSize.x;
  size[u_tex2DAxes[0].y] = texSize.y;
  return size;
}

uint uintTextureLookup(usampler2D tex, vec3 texCoord)
{
  return texture(tex, texCoord2D(texCoord, 0))[0];
}
)";
  std::unordered_map<std::string, std::string> replacements = imageShaderInfo.fsReplacements;
  replacements["$$UINT_TEXTURE_LOOKUP_FUNCTION$$"] = sources.uintTextureLinear3D;
  std::string fsSource = loadShaderFile(fragmentShaderPath);
  fsSource = rendering::replacePlaceholders(
    fsSource,
    {{"$$IMAGE_PLANE_DISPLAY_FUNCTIONS$$", loadShaderFile("app/rendering/shaders/mesh/MeshImagePlaneDisplay.glsl")}});
  std::unordered_map<std::string, std::string> dimensionReplacements =
    rendering::shaderReplacementsForTextureDimension(replacements, textureDimension, setup.lookupReplacementSources);
  if (RenderData::TextureDimension::Texture2D == textureDimension) {
    dimensionReplacements["$$UINT_TEXTURE_LOOKUP_FUNCTION$$"] = sk_uintTextureLinear2DUsingExistingAxes;
  }
  fsSource = rendering::replacePlaceholders(fsSource, dimensionReplacements);

  Uniforms fsUniforms = imageShaderInfo.fsUniforms;
  fsUniforms.insertUniform("u_imgRgbaTex", UniformType::SamplerVector, Uniforms::SamplerIndexVectorType{{0, 1, 2, 3}});
  fsUniforms.insertUniform("u_componentRenderMode", UniformType::Int, 0);
  fsUniforms.insertUniform("u_imgSlopeInterceptRgba", UniformType::Vec2Vector, std::vector<glm::vec2>(4));
  fsUniforms.insertUniform("u_imgThresholdsRgba", UniformType::Vec2Vector, std::vector<glm::vec2>(4));
  fsUniforms.insertUniform("u_imgMinMaxRgba", UniformType::Vec2Vector, std::vector<glm::vec2>(4));
  fsUniforms.insertUniform("u_imgOpacityRgba", UniformType::FloatVector, std::vector<float>(4));
  fsUniforms.insertUniform("u_alphaIsOne", UniformType::Bool, true);
  fsUniforms.insertUniform("u_tex2DAxes[2]", UniformType::IVec2, glm::ivec2{0}, k_optionalUniform);
  fsUniforms.insertUniform("u_tex2DAxes[3]", UniformType::IVec2, glm::ivec2{0}, k_optionalUniform);
  fsUniforms.insertUniform("u_imgSlope_native_T_texture", UniformType::Float, 1.0f);
  fsUniforms.insertUniform("u_projectionScale", UniformType::Float, 1.0f);
  fsUniforms.insertUniform("u_planeNormal_subject", UniformType::Vec3, glm::vec3{0.0f, 0.0f, 1.0f});
  fsUniforms.insertUniform("u_planeRight_subject", UniformType::Vec3, glm::vec3{1.0f, 0.0f, 0.0f});
  fsUniforms.insertUniform("u_planeUp_subject", UniformType::Vec3, glm::vec3{0.0f, 1.0f, 0.0f});
  fsUniforms.insertUniform("u_vectorSignedColors", UniformType::Bool, true);
  fsUniforms.insertUniform("u_segVisible", UniformType::Bool, false);
  fsUniforms.insertUniform("u_segTex", UniformType::Sampler, 5);
  fsUniforms.insertUniform("u_segLabelCmapTex", UniformType::Sampler, 6);
  fsUniforms.insertUniform("u_segOpacity", UniformType::Float, 0.0f);
  fsUniforms.insertUniform("u_segFillOpacity", UniformType::Float, 1.0f);
  fsUniforms.insertUniform("u_segInterpCutoff", UniformType::Float, 0.5f);
  fsUniforms.insertUniform("u_segLinearInterpolation", UniformType::Bool, false);
  fsUniforms.insertUniform("u_segOutlineUsesScreenPixels", UniformType::Bool, false);
  fsUniforms.insertUniform("u_texSamplingDirsForSegOutline", UniformType::Vec3Vector, Vec3Vector{glm::vec3{0.0f}});
  fsUniforms.insertUniform("u_texSamplingDirsForSmoothSeg", UniformType::Vec3Vector, Vec3Vector{glm::vec3{0.0f}});
  fsUniforms.insertUniform("u_boundaryVertexCount", UniformType::Int, 0);
  fsUniforms.insertUniform("u_boundaryWorldPositions", UniformType::Vec3Vector, Vec3Vector{glm::vec3{0.0f}});
  fsUniforms.insertUniform("u_viewportSize", UniformType::Vec2, glm::vec2{1.0f});
  fsUniforms.insertUniform("u_clip_T_world", UniformType::Mat4, glm::mat4{1.0f});
  if (peelShader) {
    fsUniforms.insertUniform("u_previousDepthBoundsTex", UniformType::Sampler, 7, k_optionalUniform);
    fsUniforms.insertUniform("u_previousFrontColorTex", UniformType::Sampler, 8, k_optionalUniform);
  }

  attachShaderFile(
    program,
    ShaderType::Vertex,
    "app/rendering/shaders/mesh/MeshImagePlane.vs",
    meshImagePlaneVertexUniforms());
  attachShaderSource(program, ShaderType::Fragment, fragmentShaderPath, fsSource, std::move(fsUniforms));
  return linkMeshProgram(program);
}

} // namespace

bool Rendering::createMeshProgram(GLShaderProgram& program)
{
  static const std::string vsFileName{"app/rendering/shaders/mesh/Mesh.vs"};
  static const std::string fsFileName{"app/rendering/shaders/mesh/Mesh.fs"};

  attachShaderFile(program, ShaderType::Vertex, vsFileName, meshVertexUniforms());
  attachShaderFile(program, ShaderType::Fragment, fsFileName, meshFragmentUniforms());
  return linkMeshProgram(program);
}

bool Rendering::createMeshShadowDepthProgram(GLShaderProgram& program)
{
  attachShaderFile(program, ShaderType::Vertex, "app/rendering/shaders/mesh/Mesh.vs", meshVertexUniforms());
  attachShaderFile(
    program,
    ShaderType::Fragment,
    "app/rendering/shaders/mesh/MeshShadowDepth.fs",
    meshClipFragmentUniforms());
  return linkMeshProgram(program);
}

bool Rendering::createMeshAmbientOcclusionGeometryProgram(GLShaderProgram& program)
{
  attachShaderFile(program, ShaderType::Vertex, "app/rendering/shaders/mesh/Mesh.vs", meshVertexUniforms());
  attachShaderFile(
    program,
    ShaderType::Fragment,
    "app/rendering/shaders/mesh/AmbientOcclusionGeometry.fs",
    meshClipFragmentUniforms());
  return linkMeshProgram(program);
}

bool Rendering::createMeshAmbientOcclusionResolveProgram(GLShaderProgram& program)
{
  Uniforms fsUniforms;
  fsUniforms.insertUniform("u_normalTex", UniformType::Sampler, 0, k_optionalUniform);
  fsUniforms.insertUniform("u_depthTex", UniformType::Sampler, 1, k_optionalUniform);
  fsUniforms.insertUniform("u_viewportSize", UniformType::Vec2, glm::vec2{1.0f});
  fsUniforms.insertUniform("u_camera_T_clip", UniformType::Mat4, glm::mat4{1.0f});
  fsUniforms.insertUniform("u_clip_T_camera", UniformType::Mat4, glm::mat4{1.0f});
  fsUniforms.insertUniform("u_camera_T_worldNormal", UniformType::Mat3, glm::mat3{1.0f});
  fsUniforms.insertUniform("u_radiusMm", UniformType::Float, 5.0f);
  fsUniforms.insertUniform("u_strength", UniformType::Float, 1.0f);
  fsUniforms.insertUniform("u_sampleCount", UniformType::Int, 24);
  return createFullscreenMeshProgram(
    program,
    "app/rendering/shaders/mesh/AmbientOcclusionResolve.fs",
    std::move(fsUniforms));
}

bool Rendering::createMeshAmbientOcclusionFilterProgram(GLShaderProgram& program)
{
  Uniforms fsUniforms;
  fsUniforms.insertUniform("u_occlusionTex", UniformType::Sampler, 2, k_optionalUniform);
  fsUniforms.insertUniform("u_normalTex", UniformType::Sampler, 0, k_optionalUniform);
  fsUniforms.insertUniform("u_depthTex", UniformType::Sampler, 1, k_optionalUniform);
  fsUniforms.insertUniform("u_viewportSize", UniformType::Vec2, glm::vec2{1.0f});
  fsUniforms.insertUniform("u_camera_T_clip", UniformType::Mat4, glm::mat4{1.0f});
  fsUniforms.insertUniform("u_radiusMm", UniformType::Float, 5.0f);
  fsUniforms.insertUniform("u_power", UniformType::Float, 1.5f);
  fsUniforms.insertUniform("u_contrast", UniformType::Float, 1.0f);
  return createFullscreenMeshProgram(
    program,
    "app/rendering/shaders/mesh/AmbientOcclusionFilter.fs",
    std::move(fsUniforms));
}

bool Rendering::createMeshImagePlaneGrayLinearProgram(GLShaderProgram& program)
{
  return createMeshImagePlaneProgram(
    program,
    ShaderProgramType::ImageGrayLinear,
    RenderData::TextureDimension::Texture3D);
}

bool Rendering::createMeshImagePlaneGrayLinearTexture2DProgram(GLShaderProgram& program)
{
  return createMeshImagePlaneProgram(
    program,
    ShaderProgramType::ImageGrayLinear,
    RenderData::TextureDimension::Texture2D);
}

bool Rendering::createMeshImagePlaneIsoContourProgram(GLShaderProgram& program)
{
  return createMeshImagePlaneProgram(
    program,
    ShaderProgramType::IsoContourLinearFloating,
    RenderData::TextureDimension::Texture3D);
}

bool Rendering::createMeshImagePlaneIsoContourTexture2DProgram(GLShaderProgram& program)
{
  return createMeshImagePlaneProgram(
    program,
    ShaderProgramType::IsoContourLinearFloating,
    RenderData::TextureDimension::Texture2D);
}

bool Rendering::createMeshImagePlaneDdpInitProgram(GLShaderProgram& program)
{
  return createMeshImagePlaneDdpProgram(
    program,
    "app/rendering/shaders/mesh/MeshImagePlaneDdpInit.fs",
    RenderData::TextureDimension::Texture3D,
    false);
}

bool Rendering::createMeshImagePlaneDdpInitTexture2DProgram(GLShaderProgram& program)
{
  return createMeshImagePlaneDdpProgram(
    program,
    "app/rendering/shaders/mesh/MeshImagePlaneDdpInit.fs",
    RenderData::TextureDimension::Texture2D,
    false);
}

bool Rendering::createMeshImagePlaneDdpPeelProgram(GLShaderProgram& program)
{
  return createMeshImagePlaneDdpProgram(
    program,
    "app/rendering/shaders/mesh/MeshImagePlaneDdpPeel.fs",
    RenderData::TextureDimension::Texture3D,
    true);
}

bool Rendering::createMeshImagePlaneDdpPeelTexture2DProgram(GLShaderProgram& program)
{
  return createMeshImagePlaneDdpProgram(
    program,
    "app/rendering/shaders/mesh/MeshImagePlaneDdpPeel.fs",
    RenderData::TextureDimension::Texture2D,
    true);
}

bool Rendering::createMeshDdpInitProgram(GLShaderProgram& program)
{
  attachShaderFile(program, ShaderType::Vertex, "app/rendering/shaders/mesh/Mesh.vs", meshVertexUniforms());
  attachShaderFile(
    program,
    ShaderType::Fragment,
    "app/rendering/shaders/mesh/MeshDdpInit.fs",
    meshClipFragmentUniforms());
  return linkMeshProgram(program);
}

bool Rendering::createMeshDdpPeelProgram(GLShaderProgram& program)
{
  Uniforms fsUniforms = meshFragmentUniforms();
  fsUniforms.insertUniform("u_previousDepthBoundsTex", UniformType::Sampler, 0, k_optionalUniform);
  fsUniforms.insertUniform("u_previousFrontColorTex", UniformType::Sampler, 1, k_optionalUniform);

  attachShaderFile(program, ShaderType::Vertex, "app/rendering/shaders/mesh/Mesh.vs", meshVertexUniforms());
  attachShaderFile(program, ShaderType::Fragment, "app/rendering/shaders/mesh/MeshDdpPeel.fs", std::move(fsUniforms));
  return linkMeshProgram(program);
}

bool Rendering::createMeshDdpBackBlendProgram(GLShaderProgram& program)
{
  Uniforms fsUniforms;
  fsUniforms.insertUniform("u_backTempTex", UniformType::Sampler, 0, k_optionalUniform);
  return createFullscreenMeshDdpProgram(
    program,
    "app/rendering/shaders/mesh/MeshDdpBackBlend.fs",
    std::move(fsUniforms));
}

bool Rendering::createMeshDdpResolveProgram(GLShaderProgram& program)
{
  Uniforms fsUniforms;
  fsUniforms.insertUniform("u_frontColorTex", UniformType::Sampler, 0, k_optionalUniform);
  fsUniforms.insertUniform("u_backColorTex", UniformType::Sampler, 1, k_optionalUniform);
  fsUniforms.insertUniform("u_viewportOrigin", UniformType::IVec2, glm::ivec2{0, 0});
  return createFullscreenMeshDdpProgram(program, "app/rendering/shaders/mesh/MeshDdpResolve.fs", std::move(fsUniforms));
}
