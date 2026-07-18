#include "rendering/Rendering.h"

#include "common/Exception.hpp"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/utility/gl/GLShader.h"

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

using FloatVector = std::vector<float>;
using Vec3Vector = std::vector<glm::vec3>;

const glm::mat4 sk_identMat3{1.0f};
const glm::mat4 sk_identMat4{1.0f};
const glm::vec3 sk_zeroVec3{0.0f, 0.0f, 0.0f};
const glm::vec4 sk_zeroVec4{0.0f, 0.0f, 0.0f, 0.0f};

const Uniforms::SamplerIndexType msk_imgTexSampler{0};
const Uniforms::SamplerIndexType msk_jumpTexSampler{1};
const Uniforms::SamplerIndexVectorType msk_defTexSamplers{{4, 5, 6}};

std::string loadFile(const std::string& path)
{
  const auto filesystem = cmrc::shaders::get_filesystem();
  const cmrc::file data = filesystem.open(path);
  return {data.begin(), data.end()};
}

} // namespace

bool Rendering::createRaycastIsoProgram(GLShaderProgram& program, bool warped)
{
  static const std::string vsFileName{"app/rendering/shaders/RaycastIso.vs"};
  static const std::string fsFileName{"app/rendering/shaders/RaycastIso.fs"};

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
    spdlog::critical("Exception when loading shader file: {}", e.what());
    throw_debug("Unable to load shader")
  }

  const std::string shaderPath("app/rendering/shaders/functions/");
  const std::string sampleTexCoordIdentityRep = loadFile(shaderPath + "SampleTexCoord_Identity.glsl");
  const std::string sampleTexCoordDeformationRep = loadFile(shaderPath + "SampleTexCoord_Deformation.glsl");
  const std::string sampleImageValueIdentityRep =
    "float sampleImageValue(vec3 texCoord)\n"
    "{\n"
    "  return getImageValue(u_imgTex, texCoord);\n"
    "}\n";
  const std::string sampleImageValueDeformationRep =
    "float sampleImageValue(vec3 texCoord)\n"
    "{\n"
    "  vec3 worldPos = vec3(u_world_T_tex * vec4(texCoord, 1.0));\n"
    "  vec3 sampleTc = sampleTexCoord(texCoord, worldPos);\n"
    "  return isInsideTexture(sampleTc) ? getImageValue(u_imgTex, sampleTc) : 0.0;\n"
    "}\n";
  const std::string jumpTextureRep =
    "float raycastJumpDistance(vec3 texCoord)\n"
    "{\n"
    "  return float(texture(u_jumpTex, texCoord).r);\n"
    "}\n";
  const std::string jumpDisabledRep =
    "float raycastJumpDistance(vec3 texCoord)\n"
    "{\n"
    "  return 0.0;\n"
    "}\n";
  fsSource = rendering::replacePlaceholders(
    fsSource,
    {{"$$SAMPLE_TEX_COORD_FUNCTION$$", warped ? sampleTexCoordDeformationRep : sampleTexCoordIdentityRep},
     {"$$SAMPLE_IMAGE_VALUE_FUNCTION$$", warped ? sampleImageValueDeformationRep : sampleImageValueIdentityRep},
     {"$$RAYCAST_JUMP_DISTANCE_FUNCTION$$", warped ? jumpDisabledRep : jumpTextureRep}});

  {
    Uniforms vsUniforms;
    vsUniforms.insertUniform("u_view_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_world_T_clip", UniformType::Mat4, sk_identMat4);
    vsUniforms.insertUniform("u_clipDepth", UniformType::Float, 0.0f);

    GLShader vs("vsRaycast", ShaderType::Vertex, vsSource.c_str());
    vs.setRegisteredUniforms(std::move(vsUniforms));
    program.attachShader(vs);

    spdlog::debug("Compiled vertex shader {}", vsFileName);
  }

  {
    Uniforms fsUniforms;

    fsUniforms.insertUniform("u_imgTex", UniformType::Sampler, msk_imgTexSampler);
    fsUniforms.insertUniform("u_jumpTex", UniformType::Sampler, msk_jumpTexSampler, !warped);

    fsUniforms.insertUniform("u_tex_T_world", UniformType::Mat4, sk_identMat4);
    fsUniforms.insertUniform("u_world_T_tex", UniformType::Mat4, sk_identMat4);
    fsUniforms.insertUniform("u_clip_T_imgTex", UniformType::Mat4, sk_identMat4);

    fsUniforms.insertUniform("u_texGrads", UniformType::Mat3, sk_identMat3);

    fsUniforms.insertUniform("u_numIsos", UniformType::Int, 0);
    fsUniforms.insertUniform("u_isoValues", UniformType::FloatVector, FloatVector{0.0f});
    fsUniforms.insertUniform("u_isoOpacities", UniformType::FloatVector, FloatVector{1.0f});
    fsUniforms.insertUniform("u_isoRimOpacityStrengths", UniformType::FloatVector, FloatVector{0.0f});
    fsUniforms.insertUniform("u_isoRimEmissionStrengths", UniformType::FloatVector, FloatVector{0.0f});
    fsUniforms.insertUniform("u_isoRimPowers", UniformType::FloatVector, FloatVector{2.0f});

    fsUniforms.insertUniform("u_ambient", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3});
    fsUniforms.insertUniform("u_diffuse", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3});
    fsUniforms.insertUniform("u_specular", UniformType::Vec3Vector, Vec3Vector{sk_zeroVec3});
    fsUniforms.insertUniform("u_shininess", UniformType::FloatVector, FloatVector{0.0f});

    fsUniforms.insertUniform("u_bgColor", UniformType::Vec4, sk_zeroVec4);
    fsUniforms.insertUniform("u_bgEdgeBrighteningEnabled", UniformType::Bool, true);
    fsUniforms.insertUniform("u_showCrosshairs3D", UniformType::Bool, false);
    fsUniforms.insertUniform("u_crosshairsWorldPos", UniformType::Vec3, sk_zeroVec3);
    fsUniforms.insertUniform("u_crosshairsColor", UniformType::Vec4, sk_zeroVec4);
    fsUniforms.insertUniform("u_crosshairsRadiusMm", UniformType::Float, 0.5f);

    fsUniforms.insertUniform("u_samplingFactor", UniformType::Float, 1.0f);
    fsUniforms.insertUniform("u_imgInvDims", UniformType::Vec3, glm::vec3{1.0f});

    fsUniforms.insertUniform("u_renderFrontFaces", UniformType::Bool, true);
    fsUniforms.insertUniform("u_renderBackFaces", UniformType::Bool, true);
    fsUniforms.insertUniform("u_noHitTransparent", UniformType::Bool, true);

    if (warped) {
      fsUniforms.insertUniform("u_defTex", UniformType::SamplerVector, msk_defTexSamplers);
      fsUniforms.insertUniform("u_defTex_T_world", UniformType::Mat4, sk_identMat4);
      fsUniforms.insertUniform("u_sampleTex_T_world", UniformType::Mat4, sk_identMat4);
      fsUniforms.insertUniform("u_defSlope_native_T_texture", UniformType::Float, 1.0f);
      fsUniforms.insertUniform("u_deformationStrength", UniformType::Float, 1.0f);
      fsUniforms.insertUniform("u_defInterleaved", UniformType::Bool, false);
    }

    GLShader fs("fsRaycast", ShaderType::Fragment, fsSource.c_str());
    fs.setRegisteredUniforms(std::move(fsUniforms));
    program.attachShader(fs);

    spdlog::debug("Compiled fragment shader {}", fsFileName);
  }

  if (!program.link()) {
    spdlog::critical("Failed to link shader program {}", program.name());
    return false;
  }

  spdlog::debug("Linked shader program {}", program.name());
  return true;
}
