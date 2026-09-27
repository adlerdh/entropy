#include "rendering/Rendering.h"
#include "rendering/RaycastShaderUniforms.h"

#include "common/Exception.hpp"
#include "rendering/ShaderPreprocessor.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/gl/GLShader.h"

#include <cmrc/cmrc.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include <string>
#include <vector>

CMRC_DECLARE(shaders);

namespace
{

std::string loadFile(const std::string& path)
{
  const auto filesystem = cmrc::shaders::get_filesystem();
  const cmrc::file data = filesystem.open(path);
  return {data.begin(), data.end()};
}

} // namespace

bool Rendering::createRaycastIsoProgram(GLShaderProgram& program, bool warped)
{
  static const std::string vsFileName{"rendering/shaders/RaycastIso.vs"};
  static const std::string fsFileName{"rendering/shaders/RaycastIso.fs"};

  auto filesystem = cmrc::shaders::get_filesystem();
  std::string vsSource;
  std::string fsSource;

  try {
    cmrc::file vsData = filesystem.open(vsFileName);
    cmrc::file fsData = filesystem.open(fsFileName);

    vsSource = std::string(vsData.begin(), vsData.end());
    fsSource = std::string(fsData.begin(), fsData.end());
  }
  catch (const std::exception& e) {
    spdlog::critical("Could not load a raycasting shader: {}. Entropy cannot start", e.what());
    throwDebug("Unable to load shader");
  }

  const std::string shaderPath("rendering/shaders/functions/");
  const std::string sampleTexCoordIdentityRep = loadFile(shaderPath + "SampleTexCoord_Identity.glsl");
  const std::string sampleTexCoordDeformationRep = loadFile(shaderPath + "SampleTexCoord_Deformation.glsl");
  const std::string sampleImageValueIdentityRep = loadFile(shaderPath + "SampleImageValue_Identity.glsl");
  const std::string sampleImageValueDeformationRep = loadFile(shaderPath + "SampleImageValue_Deformation.glsl");
  const std::string jumpTextureRep = loadFile(shaderPath + "RaycastJumpDistance_Texture.glsl");
  const std::string jumpDisabledRep = loadFile(shaderPath + "RaycastJumpDistance_Disabled.glsl");
  fsSource = rendering::preprocessShaderSource(
    fsSource,
    {{"SAMPLE_TEX_COORD_FUNCTION", warped ? sampleTexCoordDeformationRep : sampleTexCoordIdentityRep},
     {"SAMPLE_IMAGE_VALUE_FUNCTION", warped ? sampleImageValueDeformationRep : sampleImageValueIdentityRep},
     {"RAYCAST_JUMP_DISTANCE_FUNCTION", warped ? jumpDisabledRep : jumpTextureRep}});

  {
    Uniforms vsUniforms = rendering::shader_setup::raycastVertexUniforms();

    GLShader vs("vsRaycast", ShaderType::Vertex, vsSource.c_str());
    vs.setRegisteredUniforms(std::move(vsUniforms));
    if (!program.attachShader(vs)) {
      return false;
    }

    spdlog::debug("Compiled vertex shader {}", vsFileName);
  }

  {
    Uniforms fsUniforms = rendering::shader_setup::raycastFragmentUniforms(warped);

    GLShader fs("fsRaycast", ShaderType::Fragment, fsSource.c_str());
    fs.setRegisteredUniforms(std::move(fsUniforms));
    if (!program.attachShader(fs)) {
      return false;
    }

    spdlog::debug("Compiled fragment shader {}", fsFileName);
  }

  if (!program.link()) {
    spdlog::critical("Failed to link raycasting shader program {}; Entropy cannot start", program.name());
    return false;
  }

  spdlog::debug("Linked shader program {}", program.name());
  return true;
}
