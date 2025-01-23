#ifndef SHADER_PROGRAM_CONTAINER_H
#define SHADER_PROGRAM_CONTAINER_H

#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/gl/GLShaderType.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class GLShader;

class ShaderProgramContainer
{
public:
  ShaderProgramContainer();

  ShaderProgramContainer(const ShaderProgramContainer&) = delete;
  ShaderProgramContainer& operator=(const ShaderProgramContainer&) = delete;

  ShaderProgramContainer(ShaderProgramContainer&&) = default;
  ShaderProgramContainer& operator=(ShaderProgramContainer&&) = default;

  ~ShaderProgramContainer();

  void initializeGL();

  GLShaderProgram* getProgram(const std::string& name);
  GLShaderProgram* useProgram(const std::string& name);

  const Uniforms& getRegisteredUniforms(const std::string& name) const;

private:
  //    struct Impl;
  //    std::unique_ptr<Impl> m_impl;

  void generateShaders();
  void generateMeshVertexShader();
  void generateMeshFragmentShaders();

  void generatePrograms();
  void generateDualDepthPeelingPrograms();
  void generateFlatShadingProgram();
  void generateSimpleProgram();
  void generateBasicMeshPrograms();
  void generateMeshPrograms();
  void generatePolygonizerProgram();

  using ShaderSourceMap = std::unordered_map<ShaderType, const char*>;
  void generateProgram(const std::string& name, const ShaderSourceMap& shaderSources);

  using ShaderSet = std::unordered_set<std::shared_ptr<GLShader> >;
  void generateProgram(const std::string& name, const ShaderSet& shaders, bool gen = false);

  std::unordered_map<std::string, std::shared_ptr<GLShaderProgram> > m_programs;
  std::unordered_map<std::string, std::shared_ptr<GLShader> > m_shaders;

  bool m_validateBeforeUse;
};

#endif // SHADER_PROGRAM_CONTAINER_H
