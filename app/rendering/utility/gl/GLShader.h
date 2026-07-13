#pragma once

#include "rendering/utility/containers/Uniforms.h"
#include "rendering/utility/gl/GLErrorChecker.h"
#include "rendering/utility/gl/GLShaderType.h"

#include <glm/fwd.hpp>
#include <glad/glad.h>

#include <istream>
#include <string>

/**
 * @brief Owns one compiled OpenGL shader object.
 *
 * Construction compiles shader source immediately and records the compile status. The GL shader handle is unique to
 * this object and is deleted in the destructor.
 */
class GLShader final
{
public:
  /// Compile a shader from an in-memory source string.
  GLShader(std::string name, const ShaderType& type, const char* source);

  /// Compile a shader from the full contents of an input stream.
  GLShader(std::string name, const ShaderType& type, std::istream& source);

  GLShader(const GLShader&) = delete;
  GLShader& operator=(const GLShader&) = delete;

  ~GLShader();

  const std::string& name() const;
  ShaderType type() const;
  GLuint handle() const;
  bool isValid() const;
  bool isCompiled() const;

  /// Register the uniforms expected by shader setup for this shader variant.
  void setRegisteredUniforms(Uniforms uniforms);

  /// Return the uniforms declared by shader setup for this shader variant.
  const Uniforms& getRegisteredUniforms() const;

  static const std::string& shaderTypeString(const ShaderType& type);

private:
  std::string m_name;
  ShaderType m_type;
  GLuint m_handle;
  bool m_isCompiled;

  GLErrorChecker m_errorChecker;

  Uniforms m_uniforms;

  GLShader(std::string name, const ShaderType& type);

  void compileFromString(const char* source);

  bool checkShaderStatus(GLuint handle);
};
