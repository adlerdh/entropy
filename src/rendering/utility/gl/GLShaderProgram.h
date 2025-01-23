#ifndef GLSHADERPROGRAM_H
#define GLSHADERPROGRAM_H

#include "rendering/utility/gl/GLShader.h"
//#include "rendering/utility/gl/GLErrorChecker.h"
#include "rendering/utility/containers/Uniforms.h"

#include <glm/fwd.hpp>

#include <glad/glad.h>

#include <array>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

/// @todo Implement call for glDetachShader()
class GLShaderProgram
{
public:
  explicit GLShaderProgram(std::string name);

  GLShaderProgram(const GLShaderProgram&) = delete;
  GLShaderProgram& operator=(const GLShaderProgram&) = delete;

  ~GLShaderProgram();

  const std::string& name() const;
  GLuint handle() const;

  bool link();
  bool isLinked() const;

  // this class shares ownership of the shader
  void attachShader(std::shared_ptr<GLShader> shader);

  /// meant to be called directly before a draw call with that shader bound and
  /// all the bindings (VAO, textures) set. Its purpose is to ensure that the shader
  /// can execute given the current GL state
  bool isValid();

  void use();
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
     * @tparam N Uniform index
     */
  template<GLint N>
  bool setUniform(const std::string& name, const std::array<float, N>& a)
  {
    const GLint loc = getUniformLocation(name);

    if (loc < 0)
    {
      return false;
    }

    glUniform1fv(loc, N, a.data());
    return true;
  }

  void applyUniforms(Uniforms& uniforms);

  void setRegisteredUniforms(const Uniforms& uniforms);
  void setRegisteredUniforms(Uniforms&& uniforms);
  const Uniforms& getRegisteredUniforms() const;

  GLint getAttribLocation(const std::string& name);
  GLint getUniformLocation(const std::string& name);

  void printActiveUniforms();
  void printActiveUniformBlocks();
  void printActiveAttribs();

private:
  const std::string m_name;
  GLuint m_handle;
  bool m_linked;

  //    GLErrorChecker m_errorChecker;

  std::unordered_set<std::shared_ptr<GLShader> > m_attachedShaders;

  Uniforms m_registeredUniforms;

  class UniformSetter // : public std::visitor<void>
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

#endif // GLSHADERPROGRAM_H
