#include "rendering/gl/GLShaderProgram.h"

#include "common/Exception.hpp"

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string_view>
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

bool isSamplerType(const GLenum type)
{
  switch (type) {
    case GL_SAMPLER_1D:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_1D_ARRAY:
    case GL_SAMPLER_2D_ARRAY:
    case GL_SAMPLER_1D_ARRAY_SHADOW:
    case GL_SAMPLER_2D_ARRAY_SHADOW:
    case GL_SAMPLER_2D_MULTISAMPLE:
    case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_SAMPLER_CUBE_SHADOW:
    case GL_SAMPLER_BUFFER:
    case GL_SAMPLER_2D_RECT:
    case GL_SAMPLER_2D_RECT_SHADOW:
    case GL_INT_SAMPLER_1D:
    case GL_INT_SAMPLER_2D:
    case GL_INT_SAMPLER_3D:
    case GL_INT_SAMPLER_CUBE:
    case GL_INT_SAMPLER_1D_ARRAY:
    case GL_INT_SAMPLER_2D_ARRAY:
    case GL_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_INT_SAMPLER_BUFFER:
    case GL_INT_SAMPLER_2D_RECT:
    case GL_UNSIGNED_INT_SAMPLER_1D:
    case GL_UNSIGNED_INT_SAMPLER_2D:
    case GL_UNSIGNED_INT_SAMPLER_3D:
    case GL_UNSIGNED_INT_SAMPLER_CUBE:
    case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_BUFFER:
    case GL_UNSIGNED_INT_SAMPLER_2D_RECT:
      return true;
    default:
      return false;
  }
}

bool registeredTypeMatchesActiveType(const UniformType registered, const GLenum active)
{
  switch (registered) {
    case UniformType::Sampler:
    case UniformType::SamplerVector:
      return isSamplerType(active);
    case UniformType::FloatVector:
    case UniformType::FloatArray2:
    case UniformType::FloatArray3:
    case UniformType::FloatArray4:
    case UniformType::FloatArray5:
      return active == GL_FLOAT;
    case UniformType::Vec2Vector:
      return active == GL_FLOAT_VEC2;
    case UniformType::Vec3Vector:
    case UniformType::Vec3Array8:
      return active == GL_FLOAT_VEC3;
    case UniformType::Vec4Vector:
      return active == GL_FLOAT_VEC4;
    case UniformType::Mat4Vector:
      return active == GL_FLOAT_MAT4;
    case UniformType::UIntArray5:
      return active == GL_UNSIGNED_INT;
    case UniformType::IntVector:
      return active == GL_INT;
    case UniformType::Undefined:
      return true;
    default:
      return static_cast<GLenum>(registered) == active;
  }
}

std::string canonicalUniformName(const std::string_view name)
{
  constexpr std::string_view firstArrayElement{"[0]"};
  if (name.ends_with(firstArrayElement)) {
    return std::string{name.substr(0, name.size() - firstArrayElement.size())};
  }
  return std::string{name};
}

bool validateRegisteredUniformTypes(const GLuint program, const Uniforms& uniforms, const std::string& programName)
{
  GLint uniformCount = 0;
  GLint maxNameLength = 0;
  glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniformCount);
  glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxNameLength);
  if (uniformCount <= 0 || maxNameLength <= 0) {
    return true;
  }

  std::vector<GLchar> nameBuffer(static_cast<std::size_t>(maxNameLength));
  bool valid = true;
  for (GLint index = 0; index < uniformCount; ++index) {
    GLsizei nameLength = 0;
    GLint arraySize = 0;
    GLenum activeType = GL_NONE;
    glGetActiveUniform(
      program,
      static_cast<GLuint>(index),
      maxNameLength,
      &nameLength,
      &arraySize,
      &activeType,
      nameBuffer.data());
    const std::string activeName{nameBuffer.data(), static_cast<std::size_t>(nameLength)};
    // Some drivers expose built-in GLSL state such as gl_DepthRange through the active-uniform reflection API. These
    // values are owned by OpenGL rather than Entropy and must not have application-side registry declarations.
    if (activeName.starts_with("gl_")) {
      continue;
    }
    const std::string canonicalName = canonicalUniformName(activeName);
    const Uniforms::Decl* declaration = nullptr;
    if (uniforms.containsKey(activeName)) {
      declaration = &uniforms(activeName);
    }
    else if (uniforms.containsKey(canonicalName)) {
      declaration = &uniforms(canonicalName);
    }
    if (declaration == nullptr) {
      spdlog::error("Active uniform '{}' in program '{}' has no C++ registry declaration", activeName, programName);
      valid = false;
      continue;
    }
    if (registeredTypeMatchesActiveType(declaration->m_type, activeType)) {
      continue;
    }

    spdlog::error(
      "Uniform '{}' in program '{}' is active as {}[{}], but its registry type is {}",
      activeName,
      programName,
      Uniforms::getUniformTypeString(activeType),
      arraySize,
      Uniforms::getUniformTypeString(static_cast<GLenum>(declaration->m_type)));
    valid = false;
  }
  return valid;
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

  auto locationGetter = [this](const std::string& name) -> GLint {
    return glGetUniformLocation(m_handle, name.c_str());
  };

  // A program with no active registered uniforms is still valid. Individual missing required uniforms are diagnosed
  // by the registry without incorrectly turning a successful OpenGL link into a failure.
  m_registeredUniforms.queryAndSetAllLocations(locationGetter);

  if (!validateRegisteredUniformTypes(m_handle, m_registeredUniforms, m_name)) {
    spdlog::error("Linked shader program '{}' has incompatible registered uniform types", m_name);
    return false;
  }

  m_linked = true;

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

GLint GLShaderProgram::getUniformLocationForTypes(
  const std::string& nameArg,
  const std::initializer_list<UniformType> acceptedTypes)
{
  if (!m_registeredUniforms.containsKey(nameArg)) {
    throwDebug("Uniform '" + nameArg + "' is not registered for shader program '" + m_name + "'");
  }

  const UniformType registeredType = m_registeredUniforms(nameArg).m_type;
  if (std::find(acceptedTypes.begin(), acceptedTypes.end(), registeredType) == acceptedTypes.end()) {
    throwDebug(
      "Uniform '" + nameArg + "' in shader program '" + m_name + "' cannot be uploaded with this C++ value type");
  }
  return getUniformLocation(nameArg);
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const bool val)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::Bool});
  if (loc < 0) {
    return false;
  }

  glUniform1i(loc, val);
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, GLint val)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::Int});
  if (loc < 0) {
    return false;
  }

  glUniform1i(loc, val);
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, GLuint val)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::UInt});
  if (loc < 0) {
    return false;
  }

  glUniform1ui(loc, val);
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, GLfloat val)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::Float});
  if (loc < 0) {
    return false;
  }

  glUniform1f(loc, val);
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const glm::ivec2& v)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::IVec2, UniformType::BVec2});
  if (loc < 0) {
    return false;
  }

  glUniform2iv(loc, 1, glm::value_ptr(v));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const glm::vec2& v)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::Vec2});
  if (loc < 0) {
    return false;
  }

  glUniform2fv(loc, 1, glm::value_ptr(v));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const glm::vec3& v)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::Vec3});
  if (loc < 0) {
    return false;
  }

  glUniform3fv(loc, 1, glm::value_ptr(v));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const glm::vec4& v)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::Vec4});
  if (loc < 0) {
    return false;
  }

  glUniform4fv(loc, 1, glm::value_ptr(v));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const glm::mat2& m)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::Mat2});
  if (loc < 0) {
    return false;
  }

  glUniformMatrix2fv(loc, 1, GL_FALSE, glm::value_ptr(m));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const glm::mat3& m)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::Mat3});
  if (loc < 0) {
    return false;
  }

  glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(m));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const glm::mat4& m)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::Mat4});
  if (loc < 0) {
    return false;
  }

  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m));
  return true;
}

bool GLShaderProgram::setSamplerUniform(const std::string& nameArg, GLint sampler)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::Sampler});
  if (loc < 0) {
    return false;
  }

  glUniform1i(loc, sampler);
  return true;
}

bool GLShaderProgram::setSamplerUniform(const std::string& nameArg, const Uniforms::SamplerIndexVectorType& samplers)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::SamplerVector});
  if (loc < 0 || samplers.indices.empty()) {
    return false;
  }

  glUniform1iv(loc, uniformElementCount(samplers.indices.size()), samplers.indices.data());
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const std::vector<glm::mat4>& matrices)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::Mat4Vector});
  if (loc < 0 || matrices.empty()) {
    return false;
  }

  glUniformMatrix4fv(loc, uniformElementCount(matrices.size()), GL_FALSE, glm::value_ptr(matrices.front()));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const std::vector<glm::vec2>& vectors)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::Vec2Vector});
  if (loc < 0 || vectors.empty()) {
    return false;
  }

  glUniform2fv(loc, uniformElementCount(vectors.size()), glm::value_ptr(vectors.front()));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const std::vector<glm::vec3>& vectors)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::Vec3Vector});
  if (loc < 0 || vectors.empty()) {
    return false;
  }

  glUniform3fv(loc, uniformElementCount(vectors.size()), glm::value_ptr(vectors.front()));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const std::vector<glm::vec4>& vectors)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::Vec4Vector});
  if (loc < 0 || vectors.empty()) {
    return false;
  }

  glUniform4fv(loc, uniformElementCount(vectors.size()), glm::value_ptr(vectors.front()));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const std::vector<float>& floats)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::FloatVector});
  if (loc < 0 || floats.empty()) {
    return false;
  }

  glUniform1fv(loc, uniformElementCount(floats.size()), floats.data());
  return true;
}

bool GLShaderProgram::setUniform(const std::string& nameArg, const std::vector<GLint>& integers)
{
  const GLint loc = getUniformLocationForTypes(nameArg, {UniformType::IntVector});
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
    if (u.m_isDirty && u.m_location >= 0) {
      setter.setLocation(u.m_location);
      std::visit(setter, u.m_value);
    }
    uniforms.setDirty(uniform.first, false);
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

void GLShaderProgram::UniformSetter::operator()(const std::vector<glm::vec4>& vectors) const
{
  if (!vectors.empty()) {
    glUniform4fv(m_loc, uniformElementCount(vectors.size()), glm::value_ptr(vectors.front()));
  }
}

void GLShaderProgram::UniformSetter::operator()(const std::vector<int>& integers) const
{
  if (!integers.empty()) {
    glUniform1iv(m_loc, uniformElementCount(integers.size()), integers.data());
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
