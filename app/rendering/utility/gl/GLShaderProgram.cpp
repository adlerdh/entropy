#include "rendering/utility/gl/GLShaderProgram.h"

#include "common/Exception.hpp"

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <utility>
#include <variant>

namespace
{

GLsizei uniformElementCount(const std::size_t count)
{
  if (count > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
    throwDebug("Uniform array exceeds the OpenGL element-count range");
  }
  return static_cast<GLsizei>(count);
}

} // namespace

GLShaderProgram::GLShaderProgram() : m_handle(0u), m_linked(false) {}

GLShaderProgram::GLShaderProgram(std::string name) : m_name(std::move(name)), m_handle(0u), m_linked(false) {}

GLShaderProgram::~GLShaderProgram()
{
  if (m_handle != 0u) {
    glDeleteProgram(m_handle);
  }
}

const std::string& GLShaderProgram::name() const
{
  return m_name;
}

GLuint GLShaderProgram::handle() const
{
  return m_handle;
}

bool GLShaderProgram::isLinked() const
{
  return m_linked;
}

bool GLShaderProgram::attachShader(const GLShader& shader)
{
  if (m_linked) {
    spdlog::error("Cannot attach shader '{}' after program '{}' has been linked", shader.name(), m_name);
    return false;
  }
  if (!shader.isValid() || !shader.isCompiled()) {
    spdlog::error("Shader '{}' did not compile; cannot attach it to program '{}'", shader.name(), m_name);
    return false;
  }

  if (!m_handle) {
    m_handle = glCreateProgram();
    if (!m_handle) {
      spdlog::error("Unable to create shader program '{}'", m_name);
      return false;
    }
  }

  glAttachShader(m_handle, shader.handle());
  CHECK_GL_ERROR(m_errorChecker);

  /// @internal Register shader's uniforms with the program
  m_registeredUniforms.insertUniforms(shader.getRegisteredUniforms());

  return true;
}

bool GLShaderProgram::link()
{
  if (!m_handle) {
    spdlog::error("Program '{}' has not been compiled", m_name);
    return false;
  }
  else if (m_linked) {
    spdlog::error("Program '{}' has already been linked", m_name);
    return false;
  }

  glLinkProgram(m_handle);
  CHECK_GL_ERROR(m_errorChecker);

  GLint status = 0;
  glGetProgramiv(m_handle, GL_LINK_STATUS, &status);

  if (GL_FALSE == status) {
    GLint logLength = 0;
    std::string logString;

    glGetProgramiv(m_handle, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength > 0) {
      std::vector<GLchar> cLog(static_cast<size_t>(logLength));
      GLsizei actualLength = 0;
      glGetProgramInfoLog(m_handle, logLength, &actualLength, cLog.data());
      logString.assign(cLog.data(), static_cast<std::size_t>(actualLength));
    }

    spdlog::error("Link of program '{}' failed: {}", m_name, logString);
    return false;
  }

  m_linked = true;

  auto locationGetter = [this](const std::string& name) -> GLint {
    return glGetUniformLocation(m_handle, name.c_str());
  };

  // A program with no active registered uniforms is still valid. Individual missing required uniforms are diagnosed
  // by the registry without incorrectly turning a successful OpenGL link into a failure.
  m_registeredUniforms.queryAndSetAllLocations(locationGetter);

  return true;
}

void GLShaderProgram::use()
{
  if (m_handle == 0u || !m_linked) {
    throwDebug("Cannot use unlinked shader program '" + m_name + "'");
  }
  glUseProgram(m_handle);
  CHECK_GL_ERROR(m_errorChecker);
}

void GLShaderProgram::stopUse()
{
  glUseProgram(0);
  CHECK_GL_ERROR(GLErrorChecker{});
}

GLint GLShaderProgram::getUniformLocation(const std::string& nameArg)
{
  if (m_handle == 0u || !m_linked) {
    throwDebug("Cannot query uniforms from unlinked shader program '" + m_name + "'");
  }
  if (const std::optional<GLint> locOpt = m_registeredUniforms.location(nameArg)) {
    return *locOpt;
  }
  else {
    const GLint loc = glGetUniformLocation(m_handle, nameArg.c_str());
    m_registeredUniforms.insertUniform(nameArg, Uniforms::Decl());
    m_registeredUniforms.setLocation(nameArg, loc);
    return loc;
  }
}

bool GLShaderProgram::setUniform(const std::string& nameArg, GLboolean val)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0) {
    return false;
  }

  glUniform1i(loc, val);
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, GLint val)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0) {
    return false;
  }

  glUniform1i(loc, val);
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, GLuint val)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0) {
    return false;
  }

  glUniform1ui(loc, val);
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, GLfloat val)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0) {
    return false;
  }

  glUniform1f(loc, val);
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const glm::ivec2& v)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0) {
    return false;
  }

  glUniform2iv(loc, 1, glm::value_ptr(v));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const glm::vec2& v)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0) {
    return false;
  }

  glUniform2fv(loc, 1, glm::value_ptr(v));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const glm::vec3& v)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0) {
    return false;
  }

  glUniform3fv(loc, 1, glm::value_ptr(v));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const glm::vec4& v)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0) {
    return false;
  }

  glUniform4fv(loc, 1, glm::value_ptr(v));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const glm::mat2& m)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0) {
    return false;
  }

  glUniformMatrix2fv(loc, 1, GL_FALSE, glm::value_ptr(m));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const glm::mat3& m)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0) {
    return false;
  }

  glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(m));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const glm::mat4& m)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0) {
    return false;
  }

  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m));
  return true;
}

bool GLShaderProgram::setSamplerUniform(const std::string& nameArg, GLint sampler)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0) {
    return false;
  }

  glUniform1i(loc, sampler);
  return true;
}

bool GLShaderProgram::setSamplerUniform(const std::string& nameArg, const Uniforms::SamplerIndexVectorType& samplers)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0 || samplers.indices.empty()) {
    return false;
  }

  glUniform1iv(loc, uniformElementCount(samplers.indices.size()), samplers.indices.data());
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const std::vector<glm::mat4>& matrices)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0 || matrices.empty()) {
    return false;
  }

  glUniformMatrix4fv(loc, uniformElementCount(matrices.size()), GL_FALSE, glm::value_ptr(matrices.front()));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const std::vector<glm::vec2>& vectors)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0 || vectors.empty()) {
    return false;
  }

  glUniform2fv(loc, uniformElementCount(vectors.size()), glm::value_ptr(vectors.front()));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const std::vector<glm::vec3>& vectors)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0 || vectors.empty()) {
    return false;
  }

  glUniform3fv(loc, uniformElementCount(vectors.size()), glm::value_ptr(vectors.front()));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const std::vector<glm::vec4>& vectors)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0 || vectors.empty()) {
    return false;
  }

  glUniform4fv(loc, uniformElementCount(vectors.size()), glm::value_ptr(vectors.front()));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const std::vector<float>& floats)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0 || floats.empty()) {
    return false;
  }

  glUniform1fv(loc, uniformElementCount(floats.size()), floats.data());
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const std::vector<GLint>& integers)
{
  const GLint loc = getUniformLocation(nameArg);
  if (loc < 0 || integers.empty()) {
    return false;
  }

  glUniform1iv(loc, uniformElementCount(integers.size()), integers.data());
  return true;
}

void GLShaderProgram::applyUniforms(Uniforms& uniforms)
{
  UniformSetter setter;

  for (const auto& uniform : uniforms()) {
    const Uniforms::Decl& u = uniform.second;
    if (u.m_isDirty) {
      setter.setLocation(u.m_location);
      std::visit(setter, u.m_value);

      uniforms.setDirty(uniform.first, false);
    }
  }
}

const Uniforms& GLShaderProgram::getRegisteredUniforms() const
{
  return m_registeredUniforms;
}

void GLShaderProgram::UniformSetter::setLocation(GLint loc)
{
  m_loc = loc;
}

void GLShaderProgram::UniformSetter::operator()(const Uniforms::SamplerIndexType& v) const
{
  glUniform1i(m_loc, v.index);
}

void GLShaderProgram::UniformSetter::operator()(bool v) const
{
  glUniform1i(m_loc, v);
}

void GLShaderProgram::UniformSetter::operator()(int v) const
{
  glUniform1i(m_loc, v);
}

void GLShaderProgram::UniformSetter::operator()(unsigned int v) const
{
  glUniform1ui(m_loc, v);
}

void GLShaderProgram::UniformSetter::operator()(float v) const
{
  glUniform1f(m_loc, v);
}

void GLShaderProgram::UniformSetter::operator()(const glm::vec2& v) const
{
  glUniform2fv(m_loc, 1, glm::value_ptr(v));
}

void GLShaderProgram::UniformSetter::operator()(const glm::vec3& v) const
{
  glUniform3fv(m_loc, 1, glm::value_ptr(v));
}

void GLShaderProgram::UniformSetter::operator()(const glm::vec4& v) const
{
  glUniform4fv(m_loc, 1, glm::value_ptr(v));
}

void GLShaderProgram::UniformSetter::operator()(const glm::mat2& m) const
{
  glUniformMatrix2fv(m_loc, 1, GL_FALSE, glm::value_ptr(m));
}

void GLShaderProgram::UniformSetter::operator()(const glm::mat3& m) const
{
  glUniformMatrix3fv(m_loc, 1, GL_FALSE, glm::value_ptr(m));
}

void GLShaderProgram::UniformSetter::operator()(const glm::mat4& m) const
{
  glUniformMatrix4fv(m_loc, 1, GL_FALSE, glm::value_ptr(m));
}

void GLShaderProgram::UniformSetter::operator()(const Uniforms::SamplerIndexVectorType& samplers) const
{
  if (!samplers.indices.empty()) {
    glUniform1iv(m_loc, uniformElementCount(samplers.indices.size()), samplers.indices.data());
  }
}

void GLShaderProgram::UniformSetter::operator()(const std::vector<float>& floats) const
{
  if (!floats.empty()) {
    glUniform1fv(m_loc, uniformElementCount(floats.size()), floats.data());
  }
}

void GLShaderProgram::UniformSetter::operator()(const std::vector<glm::vec2>& vectors) const
{
  if (!vectors.empty()) {
    glUniform2fv(m_loc, uniformElementCount(vectors.size()), glm::value_ptr(vectors.front()));
  }
}

void GLShaderProgram::UniformSetter::operator()(const std::vector<glm::vec3>& vectors) const
{
  if (!vectors.empty()) {
    glUniform3fv(m_loc, uniformElementCount(vectors.size()), glm::value_ptr(vectors.front()));
  }
}

void GLShaderProgram::UniformSetter::operator()(const std::vector<glm::mat4>& matrices) const
{
  if (!matrices.empty()) {
    glUniformMatrix4fv(m_loc, uniformElementCount(matrices.size()), GL_FALSE, glm::value_ptr(matrices.front()));
  }
}

void GLShaderProgram::UniformSetter::operator()(const std::array<float, 2>& a) const
{
  glUniform1fv(m_loc, 2, a.data());
}

void GLShaderProgram::UniformSetter::operator()(const std::array<float, 3>& a) const
{
  glUniform1fv(m_loc, 3, a.data());
}

void GLShaderProgram::UniformSetter::operator()(const std::array<float, 4>& a) const
{
  glUniform1fv(m_loc, 4, a.data());
}

void GLShaderProgram::UniformSetter::operator()(const std::array<float, 5>& a) const
{
  glUniform1fv(m_loc, 5, a.data());
}

void GLShaderProgram::UniformSetter::operator()(const std::array<uint32_t, 5>& a) const
{
  glUniform1uiv(m_loc, 5, a.data());
}

void GLShaderProgram::UniformSetter::operator()(const std::array<glm::vec3, 8>& a) const
{
  glUniform3fv(m_loc, 8, glm::value_ptr(a.at(0)));
}
