#pragma once

#include "rendering/utility/gl/GLShader.h"
#include "rendering/utility/containers/Uniforms.h"

#include <glm/fwd.hpp>

#include <glad/glad.h>

#include <array>
#include <string>

/**
 * @brief Owns one linked OpenGL shader program and its registered uniforms.
 *
 * The class creates and deletes a GL program object, attaches compiled shaders, links the program, and provides typed
 * uniform upload helpers. It is move-disabled by omission and copy-disabled because the OpenGL handle has unique
 * ownership semantics.
 *
 * @todo Implement call for `glDetachShader()` once shader object lifetime is revisited.
 */
class GLShaderProgram
{
public:
  GLShaderProgram();
  explicit GLShaderProgram(std::string name);

  GLShaderProgram(const GLShaderProgram&) = delete;
  GLShaderProgram& operator=(const GLShaderProgram&) = delete;

  ~GLShaderProgram();

  const std::string& name() const;
  GLuint handle() const;

  /// Link all attached shaders into an executable program.
  bool link();

  bool isLinked() const;

  /// Attach a compiled shader object before linking.
  bool attachShader(const GLShader& shader);

  /**
   * @brief Validate the linked program against the current OpenGL state.
   *
   * This is meant to be called directly before a draw call with this program bound and all required VAO and texture
   * bindings already configured.
   */
  bool isValid();

  /// Bind this shader program for subsequent draw calls.
  void use();

  /// Unbind the current shader program.
  void stopUse();

  void bindAttribLocation(const std::string& name, GLuint location);
  void bindFragDataLocation(const std::string& name, GLuint location);

  bool setUniform(const std::string& name, GLboolean val);
  bool setUniform(const std::string& name, GLint val);
  bool setUniform(const std::string& name, GLuint val);
  bool setUniform(const std::string& name, GLfloat val);
  bool setUniform(const std::string& name, GLfloat x, GLfloat y, GLfloat z);
  bool setUniform(const std::string& name, const glm::ivec2& vec);
  bool setUniform(const std::string& name, const glm::vec2& vec);
  bool setUniform(const std::string& name, const glm::vec3& vec);
  bool setUniform(const std::string& name, const glm::vec4& vec);
  bool setUniform(const std::string& name, const glm::mat2& mat);
  bool setUniform(const std::string& name, const glm::mat3& mat);
  bool setUniform(const std::string& name, const glm::mat4& mat);
  bool setSamplerUniform(const std::string& name, GLint sampler);

  bool setSamplerUniform(const std::string& name, const Uniforms::SamplerIndexVectorType& samplers);
  bool setUniform(const std::string& name, const std::vector<float>& floats);
  bool setUniform(const std::string& name, const std::vector<glm::vec2>& vectors);
  bool setUniform(const std::string& name, const std::vector<glm::vec3>& vectors);
  bool setUniform(const std::string& name, const std::vector<glm::mat4>& matrices);

  /**
   * @tparam N Number of array elements
   */
  template<uint32_t N>
  bool setUniform(const std::string& name, const std::array<float, N>& a)
  {
    const GLint loc = getUniformLocation(name);
    if (loc < 0) {
      return false;
    }

    glUniform1fv(loc, static_cast<int>(N), a.data());
    return true;
  }

  /// Upload every dirty uniform in the supplied registry, then mark successfully uploaded uniforms clean.
  void applyUniforms(Uniforms& uniforms);

  /// Replace the registered uniform declarations copied from shader setup.
  void setRegisteredUniforms(const Uniforms& uniforms);

  /// Replace the registered uniform declarations moved from shader setup.
  void setRegisteredUniforms(Uniforms&& uniforms);

  /// Return registered uniform declarations and their most recently queried locations.
  const Uniforms& getRegisteredUniforms() const;

  /// Query an attribute location from the linked program.
  GLint getAttribLocation(const std::string& name);

  /// Query a uniform location from the linked program.
  GLint getUniformLocation(const std::string& name);

  void printActiveUniforms();
  void printActiveUniformBlocks();
  void printActiveAttribs();

private:
  std::string m_name;
  GLuint m_handle;
  bool m_linked;

  Uniforms m_registeredUniforms;

  /**
   * @brief Variant visitor that dispatches a stored `Uniforms::ValueType` to the matching GL upload call.
   */
  class UniformSetter
  {
  public:
    UniformSetter(GLShaderProgram& parent);
    ~UniformSetter() = default;

    void setLocation(GLint loc);

    void operator()(bool v);
    void operator()(int v);
    void operator()(unsigned int v);
    void operator()(float v);
    void operator()(const glm::vec2& v);
    void operator()(const glm::vec3& v);
    void operator()(const glm::vec4& v);
    void operator()(const glm::mat2& v);
    void operator()(const glm::mat3& v);
    void operator()(const glm::mat4& v);

    void operator()(const Uniforms::SamplerIndexType& v);
    void operator()(const Uniforms::SamplerIndexVectorType& samplers);
    void operator()(const std::vector<float>& floats);
    void operator()(const std::vector<glm::vec2>& vectors);
    void operator()(const std::vector<glm::vec3>& vectors);
    void operator()(const std::vector<glm::mat4>& matrices);

    void operator()(const std::array<float, 2>& a);
    void operator()(const std::array<float, 3>& a);
    void operator()(const std::array<float, 4>& a);
    void operator()(const std::array<float, 5>& a);
    void operator()(const std::array<uint32_t, 5>& a);
    void operator()(const std::array<glm::vec3, 8>& a);

  private:
    GLint m_loc = -1;
  };
};
