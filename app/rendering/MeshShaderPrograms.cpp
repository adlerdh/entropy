#include "rendering/Rendering.h"

#include "common/Exception.hpp"
#include "rendering/RenderData.h"
#include "rendering/ShaderProgramSetup.h"
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
#include <utility>

CMRC_DECLARE(shaders);

namespace
{

static constexpr bool k_optionalUniform = false;

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
  uniforms.insertUniform("u_aspectRatio", UniformType::Float, 1.0f);
  uniforms.insertUniform("u_numCheckers", UniformType::Int, 1);
  return uniforms;
}

Uniforms meshFragmentUniforms()
{
  Uniforms uniforms;
  uniforms.insertUniform("u_baseColor", UniformType::Vec4, glm::vec4{0.8f, 0.8f, 0.8f, 1.0f});
  uniforms.insertUniform("u_metallic", UniformType::Float, 0.0f);
  uniforms.insertUniform("u_roughness", UniformType::Float, 0.55f);
  uniforms.insertUniform("u_ambientOcclusion", UniformType::Float, 1.0f);
  uniforms.insertUniform("u_shadingModel", UniformType::Int, 0);
  uniforms.insertUniform("u_hasVertexColors", UniformType::Bool, false);
  uniforms.insertUniform("u_cameraWorldPosition", UniformType::Vec3, glm::vec3{0.0f});
  uniforms.insertUniform("u_lightDirectionWorld", UniformType::Vec3, glm::vec3{0.4f, 0.6f, 0.7f});
  uniforms.insertUniform("u_shadowMapEnabled", UniformType::Bool, false);
  uniforms.insertUniform("u_shadowMapTex", UniformType::Sampler, 7, k_optionalUniform);
  uniforms.insertUniform("u_lightClip_T_world", UniformType::Mat4, glm::mat4{1.0f});
  uniforms.insertUniform("u_shadowStrength", UniformType::Float, 0.35f);
  uniforms.insertUniform("u_shadowDepthBias", UniformType::Float, 0.001f);
  uniforms.insertUniform("u_screenAmbientOcclusionEnabled", UniformType::Bool, false);
  uniforms.insertUniform("u_screenAmbientOcclusionTex", UniformType::Sampler, 6, k_optionalUniform);
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

bool createMeshImagePlaneProgram(GLShaderProgram& program, const RenderData::TextureDimension textureDimension)
{
  const rendering::shader_setup::ProgramSetup setup = rendering::shader_setup::buildProgramSetup();
  const rendering::shader_setup::ShaderInfo& imageShaderInfo = setup.shaderInfo.at(ShaderProgramType::ImageGrayLinear);
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
    "app/rendering/shaders/mesh/MeshAmbientOcclusionGeometry.fs",
    meshClipFragmentUniforms());
  return linkMeshProgram(program);
}

bool Rendering::createMeshAmbientOcclusionResolveProgram(GLShaderProgram& program)
{
  Uniforms fsUniforms;
  fsUniforms.insertUniform("u_normalTex", UniformType::Sampler, 0, k_optionalUniform);
  fsUniforms.insertUniform("u_depthTex", UniformType::Sampler, 1, k_optionalUniform);
  fsUniforms.insertUniform("u_viewportSize", UniformType::Vec2, glm::vec2{1.0f});
  fsUniforms.insertUniform("u_radiusPixels", UniformType::Float, 8.0f);
  fsUniforms.insertUniform("u_strength", UniformType::Float, 0.5f);
  return createFullscreenMeshProgram(
    program,
    "app/rendering/shaders/mesh/MeshAmbientOcclusionResolve.fs",
    std::move(fsUniforms));
}

bool Rendering::createMeshImagePlaneGrayLinearProgram(GLShaderProgram& program)
{
  return createMeshImagePlaneProgram(program, RenderData::TextureDimension::Texture3D);
}

bool Rendering::createMeshImagePlaneGrayLinearTexture2DProgram(GLShaderProgram& program)
{
  return createMeshImagePlaneProgram(program, RenderData::TextureDimension::Texture2D);
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
  return createFullscreenMeshDdpProgram(program, "app/rendering/shaders/mesh/MeshDdpResolve.fs", std::move(fsUniforms));
}
