#include "rendering_old/utility/containers/ShaderProgramContainer.h"
#include "rendering/utility/containers/Uniforms.h"
#include "rendering/utility/gl/GLShader.h"
#include "rendering_old/ShaderNames.h"

#include "common/Exception.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <regex>
#include <sstream>

namespace
{

static constexpr bool sk_isRequired = true;
static const float sk_shininess = 16.0f;
static const float sk_masterOpacity = 1.0f;
static const std::array<float, 5> sk_layerOpacities{1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
static const std::array<uint32_t, 5> sk_layerPermutation{0, 1, 2, 3, 4};

static const glm::mat4 sk_ident{1.0f};
static const glm::vec2 sk_thresh{0.0f, 1.0f};
static const glm::vec4 sk_zero{0.0f};
static const glm::vec3 sk_white{1.0f};
static const glm::vec3 sk_zAxis{0.0f, 0.0f, 1.0f};
static const glm::vec3 sk_origin{0.0f};

static const char* ks_meshVShaderName = "vsMesh";
static const char* ks_meshStdFShaderName = "fsMeshStd";
static const char* ks_meshPeelFShaderName = "fsMeshPeel";

} // namespace

ShaderProgramContainer::ShaderProgramContainer()
  : m_programs()
  , m_shaders()
  , m_validateBeforeUse(false)
{
}

void ShaderProgramContainer::initializeGL()
{
  //    initializeOpenGLFunctions();

  generateShaders();
  generatePrograms();
}

ShaderProgramContainer::~ShaderProgramContainer() = default;

void ShaderProgramContainer::generateShaders()
{
  generateMeshVertexShader();
  generateMeshFragmentShaders();
}

void ShaderProgramContainer::generatePrograms()
{
  generateFlatShadingProgram();
  generateSimpleProgram();
  generateBasicMeshPrograms();
  generateMeshPrograms();
  generateDualDepthPeelingPrograms();
  generatePolygonizerProgram();
}

GLShaderProgram* ShaderProgramContainer::getProgram(const std::string& name)
{
  const auto& itr = m_programs.find(name);

  if (std::end(m_programs) == itr)
  {
    std::ostringstream ss;
    ss << "Shader program " << name << " not found" << std::ends;
    throw_debug(ss.str());
  }

  if (!itr->second)
  {
    std::ostringstream ss;
    ss << "Invalid program " << name << std::ends;
    throw_debug(ss.str());
  }

  return itr->second.get();
}

GLShaderProgram* ShaderProgramContainer::useProgram(const std::string& name)
{
  const auto& itr = m_programs.find(name);

  if (std::end(m_programs) != itr)
  {
    if (auto program = itr->second)
    {
      if (m_validateBeforeUse && !program->isValid())
      {
        std::ostringstream ss;
        ss << "Invalid program " << name << std::ends;
        throw_debug(ss.str());
      }

      program->use();
      return program.get();
    }
    else
    {
      std::ostringstream ss;
      ss << "Unable to access shader program " << name << std::ends;
      throw_debug(ss.str());
    }
  }

  std::ostringstream ss;
  ss << "Unable to find program " << name << std::ends;
  throw_debug(ss.str());
}

const Uniforms& ShaderProgramContainer::getRegisteredUniforms(const std::string& name) const
{
  const auto& itr = m_programs.find(name);
  if (std::end(m_programs) == itr)
  {
    std::ostringstream ss;
    ss << "Unable to find uniforms for shader program " << name << std::ends;
    throw_debug(ss.str());
  }

  if (!itr->second)
  {
    std::ostringstream ss;
    ss << "Invalid shader program " << name << std::ends;
    throw_debug(ss.str());
  }

  return itr->second->getRegisteredUniforms();
}

void ShaderProgramContainer::generateFlatShadingProgram()
{
  const char* vsStdSource =
#include "rendering_old/shaders/Flat.vert"
    ;

  const char* fsStdSource =
#include "rendering_old/shaders/Flat.frag"
    ;

  const char* fsPeelSource =
#include "rendering_old/shaders/FlatPeel.frag"
    ;

  // std::cout << vsStdSource << std::endl;

  Uniforms vsStdUniforms;
  Uniforms fsCommonUniforms;
  {
    using namespace FlatProgram;

    vsStdUniforms.insertUniform(vert::world_O_model, UniformType::Mat4, sk_ident, sk_isRequired);
    vsStdUniforms.insertUniform(vert::camera_O_world, UniformType::Mat4, sk_ident, sk_isRequired);
    vsStdUniforms.insertUniform(vert::clip_O_camera, UniformType::Mat4, sk_ident, sk_isRequired);
    vsStdUniforms.insertUniform(vert::color, UniformType::Vec3, sk_white, sk_isRequired);

    fsCommonUniforms.insertUniform(frag::opacity, UniformType::Float, 1.0f, sk_isRequired);
  }

  Uniforms fsStdUniforms = fsCommonUniforms;
  fsStdUniforms.insertUniform(FlatProgram::frag::objectId, UniformType::UInt, 0, !sk_isRequired);

  Uniforms fsPeelUniforms;
  {
    using namespace FlatPeelProgram;

    fsPeelUniforms.insertUniforms(fsCommonUniforms);
    fsPeelUniforms.insertUniform(
      frag::depthBlenderTex, UniformType::Sampler, Uniforms::SamplerIndexType{0}, sk_isRequired
    );
    fsPeelUniforms.insertUniform(
      frag::frontBlenderTex, UniformType::Sampler, Uniforms::SamplerIndexType{1}, sk_isRequired
    );
  }

  auto vsStd = std::make_shared<GLShader>("vsStdFlat", ShaderType::Vertex, vsStdSource);
  auto fsStd = std::make_shared<GLShader>("fsStdFlat", ShaderType::Fragment, fsStdSource);

  vsStd->setRegisteredUniforms(std::move(vsStdUniforms));
  fsStd->setRegisteredUniforms(std::move(fsStdUniforms));

  auto vsPeel = vsStd;
  auto fsPeel = std::make_shared<GLShader>("fsPeelFlat", ShaderType::Fragment, fsPeelSource);

  fsPeel->setRegisteredUniforms(std::move(fsPeelUniforms));

  generateProgram(FlatProgram::name, ShaderSet{vsStd, fsStd});
  generateProgram(FlatPeelProgram::name, ShaderSet{vsPeel, fsPeel});
}

void ShaderProgramContainer::generateSimpleProgram()
{
  const char* vsStdSource =
#include "rendering_old/shaders/Simple.vert"
    ;

  const char* fsStdSource =
#include "rendering_old/shaders/Simple.frag"
    ;

  Uniforms vsUniforms;
  Uniforms fsUniforms;
  {
    using namespace SimpleProgram;

    vsUniforms.insertUniform(vert::world_O_model, UniformType::Mat4, sk_ident, sk_isRequired);
    vsUniforms.insertUniform(vert::camera_O_world, UniformType::Mat4, sk_ident, sk_isRequired);
    vsUniforms.insertUniform(vert::clip_O_camera, UniformType::Mat4, sk_ident, sk_isRequired);
    vsUniforms.insertUniform(vert::color, UniformType::Vec3, sk_white, sk_isRequired);

    fsUniforms
      .insertUniform(frag::tex2D, UniformType::Sampler, Uniforms::SamplerIndexType{0}, sk_isRequired);
    fsUniforms.insertUniform(frag::opacity, UniformType::Float, 1.0f, sk_isRequired);
    fsUniforms.insertUniform(frag::objectId, UniformType::UInt, 0, !sk_isRequired);
  }

  auto vsStd = std::make_shared<GLShader>("vsStdSimple", ShaderType::Vertex, vsStdSource);
  auto fsStd = std::make_shared<GLShader>("fsStdSimple", ShaderType::Fragment, fsStdSource);

  vsStd->setRegisteredUniforms(std::move(vsUniforms));
  fsStd->setRegisteredUniforms(std::move(fsUniforms));

  generateProgram(SimpleProgram::name, ShaderSet{vsStd, fsStd});
}

void ShaderProgramContainer::generateBasicMeshPrograms()
{
  const char* vsStdSource =
#include "rendering_old/shaders/BasicMesh.vert"
    ;

  const char* fsStdSource =
#include "rendering_old/shaders/BasicMesh.frag"
    ;

  const char* fsPeelSource =
#include "rendering_old/shaders/BasicMeshPeel.frag"
    ;

  Uniforms vsStdUniforms;
  Uniforms fsCommonUniforms;

  {
    using namespace BasicMeshProgram;

    vsStdUniforms.insertUniform(vert::world_O_model, UniformType::Mat4, sk_ident, sk_isRequired);
    vsStdUniforms.insertUniform(vert::camera_O_world, UniformType::Mat4, sk_ident, sk_isRequired);
    vsStdUniforms.insertUniform(vert::clip_O_camera, UniformType::Mat4, sk_ident, sk_isRequired);
    vsStdUniforms
      .insertUniform(vert::world_O_model_inv_trans, UniformType::Mat4, sk_ident, sk_isRequired);

    vsStdUniforms.insertUniform(vert::worldClipPlanes[0], UniformType::Vec4, sk_zero, sk_isRequired);
    vsStdUniforms.insertUniform(vert::worldClipPlanes[1], UniformType::Vec4, sk_zero, sk_isRequired);
    vsStdUniforms.insertUniform(vert::worldClipPlanes[2], UniformType::Vec4, sk_zero, sk_isRequired);

    fsCommonUniforms
      .insertUniform(frag::material_diffuse, UniformType::Vec3, sk_white, !sk_isRequired);
    fsCommonUniforms
      .insertUniform(frag::material_specular, UniformType::Vec3, sk_white, sk_isRequired);
    fsCommonUniforms
      .insertUniform(frag::material_shininess, UniformType::Float, sk_shininess, sk_isRequired);

    fsCommonUniforms
      .insertUniform(frag::simpleLight_position, UniformType::Vec3, sk_origin, sk_isRequired);
    fsCommonUniforms
      .insertUniform(frag::simpleLight_direction, UniformType::Vec3, sk_zAxis, sk_isRequired);
    fsCommonUniforms
      .insertUniform(frag::simpleLight_ambient, UniformType::Vec3, sk_white, sk_isRequired);
    fsCommonUniforms
      .insertUniform(frag::simpleLight_diffuse, UniformType::Vec3, sk_white, sk_isRequired);
    fsCommonUniforms
      .insertUniform(frag::simpleLight_specular, UniformType::Vec3, sk_white, sk_isRequired);

    fsCommonUniforms.insertUniform(frag::cameraPos, UniformType::Vec3, sk_origin, sk_isRequired);
    fsCommonUniforms.insertUniform(frag::cameraDir, UniformType::Vec3, sk_zAxis, sk_isRequired);
    fsCommonUniforms
      .insertUniform(frag::cameraIsOrthographic, UniformType::Bool, true, sk_isRequired);

    fsCommonUniforms.insertUniform(frag::objectId, UniformType::UInt, 0, !sk_isRequired);
    fsCommonUniforms.insertUniform(
      frag::masterOpacityMultiplier, UniformType::Float, sk_masterOpacity, sk_isRequired
    );
    fsCommonUniforms.insertUniform(
      frag::layerOpacities, UniformType::FloatArray5, sk_layerOpacities, sk_isRequired
    );

    fsCommonUniforms.insertUniform(frag::xrayMode, UniformType::Float, 0.0f, sk_isRequired);
    fsCommonUniforms.insertUniform(frag::xrayPower, UniformType::Float, 3.0f, sk_isRequired);
  }

  Uniforms fsStdUniforms = fsCommonUniforms;
  fsStdUniforms.insertUniform(FlatProgram::frag::objectId, UniformType::UInt, 0, !sk_isRequired);

  Uniforms fsPeelUniforms;

  {
    using namespace BasicMeshDualDepthPeelProgram;

    fsPeelUniforms.insertUniforms(fsCommonUniforms);
    fsPeelUniforms.insertUniform(
      frag::depthBlenderTex, UniformType::Sampler, Uniforms::SamplerIndexType{0}, sk_isRequired
    );
    fsPeelUniforms.insertUniform(
      frag::frontBlenderTex, UniformType::Sampler, Uniforms::SamplerIndexType{1}, sk_isRequired
    );
  }

  auto vsStd = std::make_shared<GLShader>("vsBasicMesh", ShaderType::Vertex, vsStdSource);
  auto fsStd = std::make_shared<GLShader>("fsBasicMesh", ShaderType::Fragment, fsStdSource);

  vsStd->setRegisteredUniforms(std::move(vsStdUniforms));
  fsStd->setRegisteredUniforms(std::move(fsStdUniforms));

  auto vsPeel = vsStd;
  auto fsPeel = std::make_shared<GLShader>("fsBasicMeshPeel", ShaderType::Fragment, fsPeelSource);

  fsPeel->setRegisteredUniforms(std::move(fsPeelUniforms));

  generateProgram(BasicMeshProgram::name, ShaderSet{vsStd, fsStd});
  generateProgram(BasicMeshDualDepthPeelProgram::name, ShaderSet{vsPeel, fsPeel});
}

void ShaderProgramContainer::generateMeshVertexShader()
{
  const char* vsStdSource =
#include "rendering_old/shaders/Mesh.vert"
    ;

  Uniforms vsStdUniforms;
  {
    using namespace MeshProgram;

    vsStdUniforms.insertUniform(vert::world_O_model, UniformType::Mat4, sk_ident, sk_isRequired);
    vsStdUniforms.insertUniform(vert::camera_O_world, UniformType::Mat4, sk_ident, sk_isRequired);
    vsStdUniforms.insertUniform(vert::clip_O_camera, UniformType::Mat4, sk_ident, sk_isRequired);
    vsStdUniforms
      .insertUniform(vert::world_O_model_inv_trans, UniformType::Mat4, sk_ident, sk_isRequired);

    vsStdUniforms
      .insertUniform(vert::imageTexCoords_O_world, UniformType::Mat4, sk_ident, sk_isRequired);
    vsStdUniforms
      .insertUniform(vert::labelTexCoords_O_world, UniformType::Mat4, sk_ident, sk_isRequired);

    vsStdUniforms.insertUniform(vert::worldClipPlanes[0], UniformType::Vec4, sk_zero, sk_isRequired);
    vsStdUniforms.insertUniform(vert::worldClipPlanes[1], UniformType::Vec4, sk_zero, sk_isRequired);
    vsStdUniforms.insertUniform(vert::worldClipPlanes[2], UniformType::Vec4, sk_zero, sk_isRequired);
  }

  auto vs = std::make_shared<GLShader>(ks_meshVShaderName, ShaderType::Vertex, vsStdSource);
  vs->setRegisteredUniforms(std::move(vsStdUniforms));

  m_shaders.insert(std::make_pair(vs->name(), vs));
}

void ShaderProgramContainer::generateMeshFragmentShaders()
{
  const char* fsStdSource =
#include "rendering_old/shaders/Mesh.frag"
    ;

  const char* fsPeelSource =
#include "rendering_old/shaders/MeshPeel.frag"
    ;

  Uniforms fsStdUniforms;
  {
    using namespace MeshProgram;

    fsStdUniforms.insertUniform(frag::material_diffuse, UniformType::Vec3, sk_white, !sk_isRequired);
    fsStdUniforms.insertUniform(frag::material_specular, UniformType::Vec3, sk_white, sk_isRequired);
    fsStdUniforms
      .insertUniform(frag::material_shininess, UniformType::Float, sk_shininess, sk_isRequired);

    fsStdUniforms
      .insertUniform(frag::simpleLight_position, UniformType::Vec3, sk_origin, sk_isRequired);
    fsStdUniforms
      .insertUniform(frag::simpleLight_direction, UniformType::Vec3, sk_zAxis, sk_isRequired);
    fsStdUniforms
      .insertUniform(frag::simpleLight_ambient, UniformType::Vec3, sk_white, sk_isRequired);
    fsStdUniforms
      .insertUniform(frag::simpleLight_diffuse, UniformType::Vec3, sk_white, sk_isRequired);
    fsStdUniforms
      .insertUniform(frag::simpleLight_specular, UniformType::Vec3, sk_white, sk_isRequired);

    fsStdUniforms.insertUniform(frag::cameraPos, UniformType::Vec3, sk_origin, sk_isRequired);
    fsStdUniforms.insertUniform(frag::cameraDir, UniformType::Vec3, sk_zAxis, sk_isRequired);
    fsStdUniforms.insertUniform(frag::cameraIsOrthographic, UniformType::Bool, true, sk_isRequired);

    fsStdUniforms.insertUniform(frag::objectId, UniformType::UInt, 0, !sk_isRequired);
    fsStdUniforms.insertUniform(
      frag::masterOpacityMultiplier, UniformType::Float, sk_masterOpacity, sk_isRequired
    );
    fsStdUniforms.insertUniform(
      frag::layerOpacities, UniformType::FloatArray5, sk_layerOpacities, sk_isRequired
    );
    fsStdUniforms.insertUniform(
      frag::layerPermutation, UniformType::UIntArray5, sk_layerPermutation, sk_isRequired
    );

    fsStdUniforms.insertUniform(
      frag::tex2D, UniformType::Sampler, Uniforms::SamplerIndexType{2}, !sk_isRequired
    );
    fsStdUniforms.insertUniform(
      frag::imageTex3D, UniformType::Sampler, Uniforms::SamplerIndexType{3}, !sk_isRequired
    );
    fsStdUniforms.insertUniform(
      frag::labelTex3D, UniformType::Sampler, Uniforms::SamplerIndexType{4}, !sk_isRequired
    );
    fsStdUniforms.insertUniform(
      frag::labelColormapTexture, UniformType::Sampler, Uniforms::SamplerIndexType{5}, !sk_isRequired
    );
    fsStdUniforms.insertUniform(
      frag::imageColorMapTexture, UniformType::Sampler, Uniforms::SamplerIndexType{6}, !sk_isRequired
    );

    fsStdUniforms.insertUniform(frag::slope, UniformType::Float, 1.0f, sk_isRequired);
    fsStdUniforms.insertUniform(frag::intercept, UniformType::Float, 0.0f, sk_isRequired);

    fsStdUniforms.insertUniform(frag::image2dThresholds, UniformType::Vec2, sk_thresh, sk_isRequired);
    fsStdUniforms.insertUniform(frag::thresholds, UniformType::Vec2, sk_thresh, sk_isRequired);

    fsStdUniforms.insertUniform(frag::cmapSlope, UniformType::Float, 1.0f, sk_isRequired);
    fsStdUniforms.insertUniform(frag::cmapIntercept, UniformType::Float, 0.0f, sk_isRequired);

    fsStdUniforms.insertUniform(frag::autoHidingMode, UniformType::Float, 0.0f, sk_isRequired);
    fsStdUniforms.insertUniform(frag::image3DThresholdMode, UniformType::Float, 0.0f, sk_isRequired);
    fsStdUniforms.insertUniform(frag::xrayMode, UniformType::Float, 0.0f, sk_isRequired);
    fsStdUniforms.insertUniform(frag::xrayPower, UniformType::Float, 3.0f, sk_isRequired);

    //        fsStdUniforms.insertUniform( frag::labelTexCoords_O_view, UniformType::Mat4, sk_ident, sk_isRequired );
  }

  Uniforms fsPeelUniforms;
  {
    using namespace MeshDDPPeelProgram;

    fsPeelUniforms.insertUniforms(fsStdUniforms);
    fsPeelUniforms.insertUniform(
      frag::depthBlenderTex, UniformType::Sampler, Uniforms::SamplerIndexType{0}, sk_isRequired
    );
    fsPeelUniforms.insertUniform(
      frag::frontBlenderTex, UniformType::Sampler, Uniforms::SamplerIndexType{1}, sk_isRequired
    );
  }

  /// @todo add these to uniform list when not optimized out
  //   "nrPointLights"
  //    uniform DirLight dirLight;
  //    uniform SpotLight spotLight;
  //    uniform PointLight pointLights[MAX_NR_POINT_LIGHTS];

  auto fragStdShader
    = std::make_shared<GLShader>(ks_meshStdFShaderName, ShaderType::Fragment, fsStdSource);
  auto fragPeelShader
    = std::make_shared<GLShader>(ks_meshPeelFShaderName, ShaderType::Fragment, fsPeelSource);

  fragStdShader->setRegisteredUniforms(fsStdUniforms);
  fragPeelShader->setRegisteredUniforms(fsPeelUniforms);

  m_shaders.insert(std::make_pair(ks_meshStdFShaderName, fragStdShader));
  m_shaders.insert(std::make_pair(ks_meshPeelFShaderName, fragPeelShader));
}

void ShaderProgramContainer::generateMeshPrograms()
{
  auto vsIter = m_shaders.find(ks_meshVShaderName);

  auto fsStdIter = m_shaders.find(ks_meshStdFShaderName);
  auto fsPeelIter = m_shaders.find(ks_meshPeelFShaderName);

  if (std::end(m_shaders) == vsIter || std::end(m_shaders) == fsStdIter || std::end(m_shaders) == fsPeelIter)
  {
    std::ostringstream ss;
    ss << "Required shader was not found" << std::ends;
    throw_debug(ss.str());
  }

  generateProgram(MeshProgram::name, ShaderSet{vsIter->second, fsStdIter->second});
  generateProgram(MeshDDPPeelProgram::name, ShaderSet{vsIter->second, fsPeelIter->second});
}

void ShaderProgramContainer::generateDualDepthPeelingPrograms()
{
  const char* vsInitSource =
#include "rendering_old/shaders/ddp/init.vert"
    ;

  const char* fsInitSource =
#include "rendering_old/shaders/ddp/InitializeDepths.frag"
    ;

  const char* vsBlendSource =
#include "rendering_old/shaders/ddp/blend.vert"
    ;

  const char* fsBlendSource =
#include "rendering_old/shaders/ddp/blend.frag"
    ;

  const char* fsFinalSource =
#include "rendering_old/shaders/ddp/final.frag"
    ;

  const char* vsDebugSource =
#include "rendering_old/shaders/Debug.vert"
    ;

  const char* fsDebugSource =
#include "rendering_old/shaders/Debug.frag"
    ;

  Uniforms vsInitUniforms;
  vsInitUniforms
    .insertUniform(DDPInitProgram::vert::world_O_model, UniformType::Mat4, sk_ident, sk_isRequired);
  vsInitUniforms
    .insertUniform(DDPInitProgram::vert::camera_O_world, UniformType::Mat4, sk_ident, sk_isRequired);
  vsInitUniforms
    .insertUniform(DDPInitProgram::vert::clip_O_camera, UniformType::Mat4, sk_ident, sk_isRequired);

  vsInitUniforms.insertUniform(
    DDPInitProgram::vert::worldClipPlanes[0], UniformType::Vec4, sk_zero, sk_isRequired
  );
  vsInitUniforms.insertUniform(
    DDPInitProgram::vert::worldClipPlanes[1], UniformType::Vec4, sk_zero, sk_isRequired
  );
  vsInitUniforms.insertUniform(
    DDPInitProgram::vert::worldClipPlanes[2], UniformType::Vec4, sk_zero, sk_isRequired
  );

  Uniforms fsInitUniforms;
  fsInitUniforms.insertUniform(
    DDPInitProgram::frag::opaqueDepthTex,
    UniformType::Sampler,
    Uniforms::SamplerIndexType{0},
    sk_isRequired
  );

  Uniforms vsBlendUniforms;
  Uniforms fsBlendUniforms;
  fsBlendUniforms.insertUniform(
    DDPBlendProgram::frag::tempTexture,
    UniformType::Sampler,
    Uniforms::SamplerIndexType{0},
    sk_isRequired
  );

  Uniforms fsFinalUniforms;
  fsFinalUniforms.insertUniform(
    DDPFinalProgram::frag::frontBlenderTexture,
    UniformType::Sampler,
    Uniforms::SamplerIndexType{0},
    sk_isRequired
  );
  fsFinalUniforms.insertUniform(
    DDPFinalProgram::frag::backBlenderTexture,
    UniformType::Sampler,
    Uniforms::SamplerIndexType{1},
    sk_isRequired
  );

  Uniforms vsDebugUniforms;
  Uniforms fsDebugUniforms;
  fsDebugUniforms.insertUniform(
    DebugProgram::frag::debugTexture,
    UniformType::Sampler,
    Uniforms::SamplerIndexType{0},
    sk_isRequired
  );

  auto vsInit = std::make_shared<GLShader>("vsMeshInit", ShaderType::Vertex, vsInitSource);
  auto fsInit = std::make_shared<GLShader>("fsMeshInit", ShaderType::Fragment, fsInitSource);

  auto vsBlend = std::make_shared<GLShader>("vsBlend", ShaderType::Vertex, vsBlendSource);
  auto fsBlend = std::make_shared<GLShader>("fsBlend", ShaderType::Fragment, fsBlendSource);

  auto vsFinal = vsBlend;
  auto fsFinal = std::make_shared<GLShader>("fsFinal", ShaderType::Fragment, fsFinalSource);

  auto vsDebug = std::make_shared<GLShader>("vsDebug", ShaderType::Vertex, vsDebugSource);
  auto fsDebug = std::make_shared<GLShader>("fsDebug", ShaderType::Fragment, fsDebugSource);

  vsInit->setRegisteredUniforms(std::move(vsInitUniforms));
  fsInit->setRegisteredUniforms(std::move(fsInitUniforms));

  vsBlend->setRegisteredUniforms(std::move(vsBlendUniforms));
  fsBlend->setRegisteredUniforms(std::move(fsBlendUniforms));

  fsFinal->setRegisteredUniforms(std::move(fsFinalUniforms));

  vsDebug->setRegisteredUniforms(std::move(vsDebugUniforms));
  fsDebug->setRegisteredUniforms(std::move(fsDebugUniforms));

  generateProgram(DDPInitProgram::name, ShaderSet{vsInit, fsInit});
  generateProgram(DDPBlendProgram::name, ShaderSet{vsBlend, fsBlend});
  generateProgram(DDPFinalProgram::name, ShaderSet{vsFinal, fsFinal});
  generateProgram(DebugProgram::name, ShaderSet{vsDebug, fsDebug});
}

void ShaderProgramContainer::generatePolygonizerProgram()
{
  const char* vsSource =
#include "rendering_old/shaders/Polygonizer.vert"
    ;

  const char* gsSource =
#include "rendering_old/shaders/Polygonizer.geom"
    ;

  // Default offsets from cube corner to corner
  static const glm::vec3 sk_step{1.0f, 1.0f, 1.0f};

  static const std::array<glm::vec3, 8> sk_defaultVertDecals = {
    glm::vec3{0.0, 0.0f, 0.0f},
    glm::vec3{sk_step.x, 0.0f, 0.0f},
    glm::vec3{sk_step.x, sk_step.y, 0.0f},
    glm::vec3{0.0f, sk_step.y, 0.0f},
    glm::vec3{0.0f, 0.0f, sk_step.z},
    glm::vec3{sk_step.x, 0.0f, sk_step.z},
    glm::vec3{sk_step.x, sk_step.y, sk_step.z},
    glm::vec3{0.0f, sk_step.y, sk_step.z}
  };

  static const glm::mat3 sk_gradDeltas{1.0f};

  using namespace PolygonizerProgram;

  Uniforms vsUniforms;
  vsUniforms.insertUniform(vert::tex_O_image, UniformType::Mat4, sk_ident, sk_isRequired);

  Uniforms gsUniforms;
  gsUniforms
    .insertUniform(geom::tex3D, UniformType::Sampler, Uniforms::SamplerIndexType{0}, sk_isRequired);
  gsUniforms.insertUniform(
    geom::triTableTex, UniformType::Sampler, Uniforms::SamplerIndexType{1}, sk_isRequired
  );
  gsUniforms.insertUniform(geom::isolevel, UniformType::Float, 0.0f, sk_isRequired);
  gsUniforms
    .insertUniform(geom::vertDecals, UniformType::Vec3Array8, sk_defaultVertDecals, sk_isRequired);
  gsUniforms.insertUniform(geom::gradDeltas, UniformType::Mat3, sk_gradDeltas, sk_isRequired);
  gsUniforms.insertUniform(geom::world_O_tex, UniformType::Mat4, sk_ident, sk_isRequired);

  auto vs = std::make_shared<GLShader>("vsVoxelizer", ShaderType::Vertex, vsSource);
  auto gs = std::make_shared<GLShader>("gsVoxelizer", ShaderType::Geometry, gsSource);

  vs->setRegisteredUniforms(std::move(vsUniforms));
  gs->setRegisteredUniforms(std::move(gsUniforms));

  generateProgram(PolygonizerProgram::name, ShaderSet{vs, gs}, true);
}

void ShaderProgramContainer::generateProgram(
  const std::string& name, const ShaderSourceMap& shaderSources
)
{
  auto program = std::make_shared<GLShaderProgram>(name.c_str());

  for (const auto& source : shaderSources)
  {
    const auto shaderType = source.first;

    std::ostringstream shaderName;
    shaderName << name << "_" << GLShader::shaderTypeString(shaderType) << std::ends;

    auto shader = std::make_shared<GLShader>(shaderName.str(), shaderType, source.second);
    program->attachShader(shader);
  }

  const bool linked = program->link();

  if (!linked)
  {
    std::ostringstream ss;
    ss << "Failed to link program " << name << std::ends;
    throw_debug(ss.str());
  }

  m_programs.insert(std::make_pair(name, program));
}

void ShaderProgramContainer::generateProgram(
  const std::string& name, const ShaderSet& shaders, bool set
)
{
  auto program = std::make_shared<GLShaderProgram>(name.c_str());

  for (const auto& shader : shaders)
  {
    program->attachShader(shader);
  }

  if (set)
  {
    // Feed back two varying variables:
    const GLchar* varyings[] = {"outPosition", "outNormal"};

    glTransformFeedbackVaryings(program->handle(), 2, varyings, GL_INTERLEAVED_ATTRIBS);
  }

  bool linked = program->link();

  if (!linked)
  {
    std::ostringstream ss;
    ss << "Failed to link program " << name << std::ends;
    throw_debug(ss.str());
  }

  m_programs.insert(std::make_pair(name, program));
}
