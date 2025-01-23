#include "rendering/utility/gl/GLShaderProgram.h"

#include "common/Exception.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <fstream>
//#include <iostream>
//#include <sstream>
//#include <variant>

GLShaderProgram::GLShaderProgram(std::string name)
  : m_name(std::move(name))
  , m_handle(0u)
  , m_linked(false)
{
}

GLShaderProgram::~GLShaderProgram()
{
  if (!m_handle)
  {
    return;
  }

  GLint numAttachedShaders = 0;
  glGetProgramiv(m_handle, GL_ATTACHED_SHADERS, &numAttachedShaders);

  std::vector<GLuint> shaders(static_cast<size_t>(numAttachedShaders));
  GLsizei actualShaderCount = 0;
  glGetAttachedShaders(m_handle, numAttachedShaders, &actualShaderCount, &shaders[0]);

  for (int i = 0; i < actualShaderCount; ++i)
  {
    if (glIsShader(shaders[uint32_t(i)]))
    {
      glDetachShader(m_handle, shaders[uint32_t(i)]);
    }
  }

  if (glIsProgram(m_handle))
  {
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

void GLShaderProgram::attachShader(std::shared_ptr<GLShader> shader)
{
  if (!shader || !shader->isValid())
  {
    spdlog::error("Invalid shader; cannot attach to program '{}'", m_name);
    throw_debug("Invalid shader; cannot attach to program")
  }

  if (!m_handle)
  {
    m_handle = glCreateProgram();

    if (!m_handle)
    {
      spdlog::error("Unable to create shader program '{}'", m_name);
      throw_debug("Unable to create shader program");
    }
  }

  glAttachShader(m_handle, shader->handle());
  m_attachedShaders.insert(shader);

  /// @internal Register shader's uniforms with the program
  m_registeredUniforms.insertUniforms(shader->getRegisteredUniforms());

  m_linked = false;
}

bool GLShaderProgram::link()
{
  if (!m_handle)
  {
    spdlog::error("Program '{}' has not been compiled", m_name);
    return false;
  }
  else if (m_linked)
  {
    spdlog::error("Program '{}' has already been linked", m_name);
    return false;
  }

  glLinkProgram(m_handle);

  GLint status = 0;
  glGetProgramiv(m_handle, GL_LINK_STATUS, &status);

  if (GL_FALSE == status)
  {
    GLint logLength = 0;
    std::string logString;

    glGetProgramiv(m_handle, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength > 0)
    {
      std::vector<GLchar> cLog(static_cast<size_t>(logLength));
      GLsizei actualLength = 0;
      glGetProgramInfoLog(m_handle, logLength, &actualLength, &cLog[0]);
      logString = &cLog[0];
    }

    spdlog::error("Link of program '{}' failed: {}", m_name, logString);
    return false;
  }

  m_linked = true;

  auto locationGetter = [this](const std::string& name) -> GLint
  { return glGetUniformLocation(m_handle, name.c_str()); };

  /// Get locations for all of the program's registered uniforms
  const int ret = m_registeredUniforms.queryAndSetAllLocations(locationGetter);

  if (1 == ret)
  {
    spdlog::error("Setting uniform(s) in program '{}' failed", m_name);
    throw_debug("Error")
  }

  return true;
}

void GLShaderProgram::use()
{
  if (m_handle && m_linked)
  {
    glUseProgram(m_handle);
  }
  else
  {
    spdlog::error("Program '{}' is not valid", m_name);
    return;
  }
}

void GLShaderProgram::stopUse()
{
  glUseProgram(0);
}

void GLShaderProgram::bindAttribLocation(const std::string& name, GLuint location)
{
  glBindAttribLocation(m_handle, location, name.c_str());
  m_linked = false;
}

void GLShaderProgram::bindFragDataLocation(const std::string& name, GLuint location)
{
  glBindFragDataLocation(m_handle, location, name.c_str());
}

GLint GLShaderProgram::getAttribLocation(const std::string& name)
{
  return glGetAttribLocation(m_handle, name.c_str());
}

GLint GLShaderProgram::getUniformLocation(const std::string& name)
{
  if (const std::optional<GLint> locOpt = m_registeredUniforms.location(name))
  {
    return *locOpt;
  }
  else
  {
    const GLint loc = glGetUniformLocation(m_handle, name.c_str());
    m_registeredUniforms.insertUniform(name, Uniforms::Decl());
    m_registeredUniforms.setLocation(name, loc);
    return loc;
  }
}

bool GLShaderProgram::setUniform(const std::string& name, GLboolean val)
{
  const GLint loc = getUniformLocation(name);

  if (loc < 0)
  {
    return false;
  }

  glUniform1i(loc, val);
  return true;
}

bool GLShaderProgram::setUniform(const std::string& name, GLint val)
{
  const GLint loc = getUniformLocation(name);

  if (loc < 0)
  {
    return false;
  }

  glUniform1i(loc, val);
  return true;
}

bool GLShaderProgram::setUniform(const std::string& name, GLuint val)
{
  const GLint loc = getUniformLocation(name);

  if (loc < 0)
  {
    return false;
  }

  glUniform1ui(loc, val);
  return true;
}

bool GLShaderProgram::setUniform(const std::string& name, GLfloat val)
{
  const GLint loc = getUniformLocation(name);

  if (loc < 0)
  {
    return false;
  }

  glUniform1f(loc, val);
  return true;
}

bool GLShaderProgram::setUniform(const std::string& name, GLfloat x, GLfloat y, GLfloat z)
{
  const GLint loc = getUniformLocation(name);

  if (loc < 0)
  {
    return false;
  }

  glUniform3f(loc, x, y, z);
  return true;
}

bool GLShaderProgram::setUniform(const std::string& name, const glm::ivec2& v)
{
  const GLint loc = getUniformLocation(name);

  if (loc < 0)
  {
    return false;
  }

  glUniform2iv(loc, 1, glm::value_ptr(v));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& name, const glm::vec2& v)
{
  const GLint loc = getUniformLocation(name);

  if (loc < 0)
  {
    return false;
  }

  glUniform2fv(loc, 1, glm::value_ptr(v));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& name, const glm::vec3& v)
{
  const GLint loc = getUniformLocation(name);

  if (loc < 0)
  {
    return false;
  }

  glUniform3fv(loc, 1, glm::value_ptr(v));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& name, const glm::vec4& v)
{
  const GLint loc = getUniformLocation(name);

  if (loc < 0)
  {
    return false;
  }

  glUniform4fv(loc, 1, glm::value_ptr(v));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& name, const glm::mat2& m)
{
  const GLint loc = getUniformLocation(name);

  if (loc < 0)
  {
    return false;
  }

  glUniformMatrix2fv(loc, 1, GL_FALSE, glm::value_ptr(m));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& name, const glm::mat3& m)
{
  const GLint loc = getUniformLocation(name);

  if (loc < 0)
  {
    return false;
  }

  glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(m));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& name, const glm::mat4& m)
{
  const GLint loc = getUniformLocation(name);

  if (loc < 0)
  {
    return false;
  }

  glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m));
  return true;
}

bool GLShaderProgram::setSamplerUniform(const std::string& name, GLint sampler)
{
  const GLint loc = getUniformLocation(name);

  if (loc < 0)
  {
    return false;
  }

  glUniform1i(loc, sampler);
  return true;
}

bool GLShaderProgram::setSamplerUniform(
  const std::string& name, const Uniforms::SamplerIndexVectorType& samplers
)
{
  const GLint loc = getUniformLocation(name);

  if (loc < 0 || samplers.indices.empty())
  {
    return false;
  }

  glUniform1iv(loc, static_cast<GLint>(samplers.indices.size()), samplers.indices.data());
  return true;
}

bool GLShaderProgram::setUniform(const std::string& name, const std::vector<glm::mat4>& matrices)
{
  const GLint loc = getUniformLocation(name);

  if (loc < 0 || matrices.empty())
  {
    return false;
  }

  glUniformMatrix4fv(
    loc, static_cast<GLint>(matrices.size()), GL_FALSE, glm::value_ptr(matrices.at(0))
  );
  return true;
}

bool GLShaderProgram::setUniform(const std::string& name, const std::vector<glm::vec2>& vectors)
{
  const GLint loc = getUniformLocation(name);

  if (loc < 0 || vectors.empty())
  {
    return false;
  }

  glUniform2fv(loc, static_cast<GLint>(vectors.size()), glm::value_ptr(vectors.at(0)));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& name, const std::vector<glm::vec3>& vectors)
{
  const GLint loc = getUniformLocation(name);

  if (loc < 0 || vectors.empty())
  {
    return false;
  }

  glUniform3fv(loc, static_cast<GLint>(vectors.size()), glm::value_ptr(vectors.at(0)));
  return true;
}

bool GLShaderProgram::setUniform(const std::string& name, const std::vector<float>& floats)
{
  const GLint loc = getUniformLocation(name);

  if (loc < 0 || floats.empty())
  {
    return false;
  }

  glUniform1fv(loc, static_cast<GLint>(floats.size()), floats.data());
  return true;
}

void GLShaderProgram::applyUniforms(Uniforms& uniforms)
{
  UniformSetter setter(*this);

  for (auto& uniform : uniforms())
  {
    const Uniforms::Decl& u = uniform.second;
    if (u.m_isDirty)
    {
      setter.setLocation(u.m_location);
      std::visit(setter, u.m_value);

      uniforms.setDirty(uniform.first, false);
    }
  }
}

void GLShaderProgram::setRegisteredUniforms(const Uniforms& uniforms)
{
  m_registeredUniforms = uniforms;
}

void GLShaderProgram::setRegisteredUniforms(Uniforms&& uniforms)
{
  m_registeredUniforms = std::move(uniforms);
}

const Uniforms& GLShaderProgram::getRegisteredUniforms() const
{
  return m_registeredUniforms;
}

void GLShaderProgram::printActiveUniforms()
{
  GLint maxUniformNameLength;
  GLint numActiveUniforms;

  glGetProgramiv(m_handle, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformNameLength);
  glGetProgramiv(m_handle, GL_ACTIVE_UNIFORMS, &numActiveUniforms);

  std::vector<GLchar> nameData(static_cast<size_t>(maxUniformNameLength));

  spdlog::info("Active uniforms:");

  for (int i = 0; i < numActiveUniforms; ++i)
  {
    GLsizei actualLength;
    GLint arraySize;
    GLenum type;

    glGetActiveUniform(
      m_handle, GLuint(i), maxUniformNameLength, &actualLength, &arraySize, &type, &nameData[0]
    );

    const std::string name(&nameData[0], static_cast<size_t>(actualLength));
    const GLint location = glGetUniformLocation(m_handle, &nameData[0]);

    spdlog::info(
      "uniform {}: location = {}, name = {}, type = {}",
      i,
      location,
      name,
      Uniforms::getUniformTypeString(type)
    );
  }

#if 0
    // For OpenGL 4.3 and above, use glGetProgramResource
    GLint numUniforms = 0;
    glGetProgramInterfaceiv( handle, GL_UNIFORM, GL_ACTIVE_RESOURCES, &numUniforms);

    GLenum properties[] = {GL_NAME_LENGTH, GL_TYPE, GL_LOCATION, GL_BLOCK_INDEX};

    printf("Active uniforms:\n");
    for( int i = 0; i < numUniforms; ++i ) {
        GLint results[4];
        glGetProgramResourceiv(handle, GL_UNIFORM, i, 4, properties, 4, NULL, results);

        if( results[3] != -1 ) continue;  // Skip uniforms in blocks
        GLint nameBufSize = results[0] + 1;
        char * name = new char[nameBufSize];
        glGetProgramResourceName(handle, GL_UNIFORM, i, nameBufSize, NULL, name);
        printf("%-5d %s (%s)\n", results[2], name, getTypeString(results[1]));
        delete [] name;
    }
#endif
}

void GLShaderProgram::printActiveUniformBlocks()
{
  GLint maxUniformBlockNameLength;
  GLint numUniformBlocks;
  GLint maxUniformNameLength;

  glGetProgramiv(m_handle, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &maxUniformBlockNameLength);
  glGetProgramiv(m_handle, GL_ACTIVE_UNIFORM_BLOCKS, &numUniformBlocks);
  glGetProgramiv(m_handle, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformNameLength);

  std::vector<GLchar> uniformBlockNameData(static_cast<size_t>(maxUniformBlockNameLength));
  std::vector<GLchar> uniformNameData(static_cast<size_t>(maxUniformNameLength));

  spdlog::info("Active uniform blocks:");

  for (GLint i = 0; i < numUniformBlocks; ++i)
  {
    GLsizei actualLength;
    GLint binding;

    glGetActiveUniformBlockName(
      m_handle, i, maxUniformBlockNameLength, &actualLength, &uniformBlockNameData[0]
    );

    glGetActiveUniformBlockiv(m_handle, i, GL_UNIFORM_BLOCK_BINDING, &binding);

    const std::string uniformBlockName(&uniformBlockNameData[0], actualLength);

    spdlog::info("block {}: name = {}, binding = {}", i, uniformBlockName, binding);

    GLint numUniforms;
    glGetActiveUniformBlockiv(m_handle, i, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &numUniforms);

    std::vector<GLint> uniformIndices(numUniforms);
    glGetActiveUniformBlockiv(
      m_handle, i, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, &uniformIndices[0]
    );

    for (GLint u = 0; u < numUniforms; ++u)
    {
      GLint arraySize;
      GLenum type;

      glGetActiveUniform(
        m_handle,
        uniformIndices[u],
        maxUniformNameLength,
        &actualLength,
        &arraySize,
        &type,
        &uniformNameData[0]
      );

      const std::string uniformName(&uniformNameData[0], actualLength);
      const GLint location = glGetUniformLocation(m_handle, &uniformName[0]);

      spdlog::info(
        "uniform {}: location = {}, name = {}, type = {}",
        u,
        location,
        uniformName,
        Uniforms::getUniformTypeString(type)
      );
    }
  }

#if 0
    GLint numBlocks = 0;

    glGetProgramInterfaceiv(handle, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &numBlocks);
    GLenum blockProps[] = {GL_NUM_ACTIVE_VARIABLES, GL_NAME_LENGTH};
    GLenum blockIndex[] = {GL_ACTIVE_VARIABLES};
    GLenum props[] = {GL_NAME_LENGTH, GL_TYPE, GL_BLOCK_INDEX};

    for(int block = 0; block < numBlocks; ++block) {
        GLint blockInfo[2];
        glGetProgramResourceiv(handle, GL_UNIFORM_BLOCK, block, 2, blockProps, 2, NULL, blockInfo);
        GLint numUnis = blockInfo[0];

        char * blockName = new char[blockInfo[1]+1];
        glGetProgramResourceName(handle, GL_UNIFORM_BLOCK, block, blockInfo[1]+1, NULL, blockName);
        printf("Uniform block \"%s\":\n", blockName);
        delete [] blockName;

        GLint * unifIndexes = new GLint[numUnis];
        glGetProgramResourceiv(handle, GL_UNIFORM_BLOCK, block, 1, blockIndex, numUnis, NULL, unifIndexes);

        for( int unif = 0; unif < numUnis; ++unif ) {
            GLint uniIndex = unifIndexes[unif];
            GLint results[3];
            glGetProgramResourceiv(handle, GL_UNIFORM, uniIndex, 3, props, 3, NULL, results);

            GLint nameBufSize = results[0] + 1;
            char * name = new char[nameBufSize];
            glGetProgramResourceName(handle, GL_UNIFORM, uniIndex, nameBufSize, NULL, name);
            printf("    %s (%s)\n", name, getTypeString(results[1]));
            delete [] name;
        }

        delete [] unifIndexes;
    }
#endif
}

void GLShaderProgram::printActiveAttribs()
{
  GLint maxAttribNameLength;
  GLint numActiveAttribs;

  glGetProgramiv(m_handle, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxAttribNameLength);
  glGetProgramiv(m_handle, GL_ACTIVE_ATTRIBUTES, &numActiveAttribs);

  std::vector<GLchar> nameData(maxAttribNameLength);

  spdlog::info("Active attributes:");

  for (GLint i = 0; i < numActiveAttribs; ++i)
  {
    GLsizei actualLength;
    GLint arraySize;
    GLenum type;

    glGetActiveAttrib(
      m_handle, i, maxAttribNameLength, &actualLength, &arraySize, &type, &nameData[0]
    );

    const std::string name(&nameData[0], actualLength);
    const GLint location = glGetAttribLocation(m_handle, &nameData[0]);

    spdlog::info(
      "attribute {}: location = {}, name = {}, type = {}",
      i,
      location,
      name,
      Uniforms::getUniformTypeString(type)
    );
  }

#if 0
    // for OpenGL >= 4.3
    else
    {
        GLint numAttribs;
        glGetProgramInterfaceiv( handle, GL_PROGRAM_INPUT, GL_ACTIVE_RESOURCES, &numAttribs);

        GLenum properties[] = {GL_NAME_LENGTH, GL_TYPE, GL_LOCATION};

        printf("Active attributes:\n");
        for( int i = 0; i < numAttribs; ++i ) {
            GLint results[3];
            glGetProgramResourceiv(handle, GL_PROGRAM_INPUT, i, 3, properties, 3, NULL, results);

            GLint nameBufSize = results[0] + 1;
            char * name = new char[nameBufSize];
            glGetProgramResourceName(handle, GL_PROGRAM_INPUT, i, nameBufSize, NULL, name);
            printf("%-5d %s (%s)\n", results[2], name, getTypeString(results[1]));
            delete [] name;
        }
    }
#endif
}

bool GLShaderProgram::isValid()
{
  if (!m_handle)
  {
    spdlog::error("Program '{}' is not compiled", m_name);
    return false;
  }
  else if (!m_linked)
  {
    spdlog::error("Program '{}' is not linked", m_name);
    return false;
  }
  else if (!glIsProgram(m_handle))
  {
    spdlog::error("Handle '{}' is not a program", m_handle);
    return false;
  }

  GLint status;
  glValidateProgram(m_handle);
  glGetProgramiv(m_handle, GL_VALIDATE_STATUS, &status);

  if (GL_FALSE == status)
  {
    GLint logLength = 0;
    glGetProgramiv(m_handle, GL_INFO_LOG_LENGTH, &logLength);

    std::string logString;

    if (logLength > 0)
    {
      std::vector<GLchar> cLog(logLength);
      GLsizei actualLength = 0;
      glGetProgramInfoLog(m_handle, logLength, &actualLength, &cLog[0]);
      logString = &cLog[0];
    }

    spdlog::error("Program '{}' failed to validate: {}", m_name, logString);
    return false;
  }

  return true;
}

GLShaderProgram::UniformSetter::UniformSetter(GLShaderProgram& /*parent*/) {}

void GLShaderProgram::UniformSetter::setLocation(GLint loc)
{
  m_loc = loc;
}

void GLShaderProgram::UniformSetter::operator()(const Uniforms::SamplerIndexType& v)
{
  glUniform1i(m_loc, v.index);
}

void GLShaderProgram::UniformSetter::operator()(bool v)
{
  glUniform1i(m_loc, v);
}

void GLShaderProgram::UniformSetter::operator()(int v)
{
  glUniform1i(m_loc, v);
}

void GLShaderProgram::UniformSetter::operator()(unsigned int v)
{
  glUniform1ui(m_loc, v);
}

void GLShaderProgram::UniformSetter::operator()(float v)
{
  glUniform1f(m_loc, v);
}

void GLShaderProgram::UniformSetter::operator()(const glm::vec2& vec)
{
  glUniform2fv(m_loc, 1, glm::value_ptr(vec));
}

void GLShaderProgram::UniformSetter::operator()(const glm::vec3& vec)
{
  glUniform3fv(m_loc, 1, glm::value_ptr(vec));
}

void GLShaderProgram::UniformSetter::operator()(const glm::vec4& vec)
{
  glUniform4fv(m_loc, 1, glm::value_ptr(vec));
}

void GLShaderProgram::UniformSetter::operator()(const glm::mat2& mat)
{
  glUniformMatrix2fv(m_loc, 1, GL_FALSE, glm::value_ptr(mat));
}

void GLShaderProgram::UniformSetter::operator()(const glm::mat3& mat)
{
  glUniformMatrix3fv(m_loc, 1, GL_FALSE, glm::value_ptr(mat));
}

void GLShaderProgram::UniformSetter::operator()(const glm::mat4& mat)
{
  glUniformMatrix4fv(m_loc, 1, GL_FALSE, glm::value_ptr(mat));
}

void GLShaderProgram::UniformSetter::operator()(const Uniforms::SamplerIndexVectorType& samplers)
{
  if (!samplers.indices.empty())
  {
    glUniform1iv(m_loc, static_cast<GLint>(samplers.indices.size()), samplers.indices.data());
  }
}

void GLShaderProgram::UniformSetter::operator()(const std::vector<float>& floats)
{
  if (!floats.empty())
  {
    glUniform1fv(m_loc, static_cast<GLint>(floats.size()), floats.data());
  }
}

void GLShaderProgram::UniformSetter::operator()(const std::vector<glm::vec2>& vectors)
{
  if (!vectors.empty())
  {
    glUniform2fv(m_loc, static_cast<GLint>(vectors.size()), glm::value_ptr(vectors.at(0)));
  }
}

void GLShaderProgram::UniformSetter::operator()(const std::vector<glm::vec3>& vectors)
{
  if (!vectors.empty())
  {
    glUniform3fv(m_loc, static_cast<GLint>(vectors.size()), glm::value_ptr(vectors.at(0)));
  }
}

void GLShaderProgram::UniformSetter::operator()(const std::vector<glm::mat4>& matrices)
{
  if (!matrices.empty())
  {
    glUniformMatrix4fv(
      m_loc, static_cast<GLint>(matrices.size()), GL_FALSE, glm::value_ptr(matrices.at(0))
    );
  }
}

void GLShaderProgram::UniformSetter::operator()(const std::array<float, 2>& a)
{
  glUniform1fv(m_loc, 2, a.data());
}

void GLShaderProgram::UniformSetter::operator()(const std::array<float, 3>& a)
{
  glUniform1fv(m_loc, 3, a.data());
}

void GLShaderProgram::UniformSetter::operator()(const std::array<float, 4>& a)
{
  glUniform1fv(m_loc, 4, a.data());
}

void GLShaderProgram::UniformSetter::operator()(const std::array<float, 5>& a)
{
  glUniform1fv(m_loc, 5, a.data());
}

void GLShaderProgram::UniformSetter::operator()(const std::array<uint32_t, 5>& a)
{
  glUniform1uiv(m_loc, 5, a.data());
}

void GLShaderProgram::UniformSetter::operator()(const std::array<glm::vec3, 8>& a)
{
  glUniform3fv(m_loc, 8, glm::value_ptr(a.at(0)));
}
