#include "rendering/gl/GLShader.h"
#include "rendering/helpers/UnderlyingEnumType.h"

#include "common/Exception.hpp"

#include <glad/glad.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <glm/glm.hpp>

#include <cstddef>
#include <iterator>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

class ShaderHandleGuard
{
public:
  explicit ShaderHandleGuard(const GLuint handle) : m_handle(handle) {}
  ~ShaderHandleGuard()
  {
    if (m_handle != 0u) {
      glDeleteShader(m_handle);
    }
  }

  ShaderHandleGuard(const ShaderHandleGuard&) = delete;
  ShaderHandleGuard& operator=(const ShaderHandleGuard&) = delete;

  GLuint release() noexcept
  {
    const GLuint handle = m_handle;
    m_handle = 0u;
    return handle;
  }

private:
  GLuint m_handle;
};

static const std::unordered_map<ShaderType, std::string> sk_shaderTypeStrings = {
  {ShaderType::Vertex, "vertex"},
  {ShaderType::Geometry, "geometry"},
  {ShaderType::TessControl, "tessControl"},
  {ShaderType::TessEvaluation, "tessEval"},
  {ShaderType::Fragment, "fragment"}};

} // namespace

GLShader::GLShader(std::string name, const ShaderType& type)
  : m_name(std::move(name)), m_type(type), m_handle(0u), m_isCompiled(false)
{
}

GLShader::GLShader(std::string name, const ShaderType& type, const char* source) : GLShader(std::move(name), type)
{
  compileFromString(source);
}

GLShader::GLShader(std::string name, const ShaderType& type, std::istream& source) : GLShader(std::move(name), type)
{
  const std::string sourceString(std::istreambuf_iterator<char>(source), {});
  compileFromString(sourceString.c_str());
}

// GLShader::GLShader( std::string name, const ShaderType& type,
//                     const std::vector< const char* >& sources )
//     : GLShader( std::move( name ), type )
//{
//     compileFromStrings( sources );
// }

GLShader::~GLShader()
{
  if (m_handle != 0u) {
    glDeleteShader(m_handle);
  }
}

const std::string& GLShader::name() const
{
  return m_name;
}

ShaderType GLShader::type() const
{
  return m_type;
}

GLuint GLShader::handle() const
{
  return m_handle;
}

bool GLShader::isValid() const
{
  return m_handle != 0u;
}

bool GLShader::isCompiled() const
{
  return m_isCompiled;
}

void GLShader::compileFromString(const char* source)
{
  if (source == nullptr) {
    throwDebug("Cannot compile a shader from a null source pointer");
  }
  if (m_handle != 0u) {
    glDeleteShader(m_handle);
    m_handle = 0u;
    m_isCompiled = false;
  }
  const GLuint handleLocal = glCreateShader(underlyingType(m_type));
  if (handleLocal == 0u) {
    throwDebug("OpenGL could not create a shader object");
  }
  ShaderHandleGuard handleGuard(handleLocal);

  glShaderSource(handleLocal, 1, &source, nullptr);
  glCompileShader(handleLocal);

  if (!checkShaderStatus(handleLocal)) {
    spdlog::error("Cannot compile shader '{}' due to failed status check", m_name);
    m_isCompiled = false;
  }
  else {
    m_isCompiled = true;
  }
  CHECK_GL_ERROR(m_errorChecker);
  m_handle = handleGuard.release();
}

// void GLShader::compileFromStrings( const std::vector< const char* >& sources )
//{
//     const GLuint handle = glCreateShader( underlyingType(m_type) );

//    glShaderSource( handle, static_cast<GLsizei>( sources.size() ), &sources[0], nullptr );
//    glCompileShader( handle );

//    if ( ! checkShaderStatus( handle ) )
//    {
//        throwDebug( "Cannot compile shader due to failed status check" );
//    }

//    m_handle = handle;

//    CHECK_GL_ERROR( m_errorChecker );
//}

void GLShader::setRegisteredUniforms(Uniforms uniforms)
{
  m_uniforms = std::move(uniforms);
}

const Uniforms& GLShader::getRegisteredUniforms() const
{
  return m_uniforms;
}

const std::string& GLShader::shaderTypeString(const ShaderType& type)
{
  return sk_shaderTypeStrings.at(type);
}

bool GLShader::checkShaderStatus(GLuint handleArg) const
{
  GLint status = 0;
  glGetShaderiv(handleArg, GL_COMPILE_STATUS, &status);

  if (GL_FALSE == status) {
    GLint logLength = 0;
    glGetShaderiv(handleArg, GL_INFO_LOG_LENGTH, &logLength);

    std::string logString;

    if (logLength > 0) {
      std::vector<GLchar> cLog(static_cast<size_t>(logLength));
      GLsizei actualLength = 0;
      glGetShaderInfoLog(handleArg, logLength, &actualLength, cLog.data());
      logString.assign(cLog.data(), static_cast<std::size_t>(actualLength));
    }

    spdlog::error("Compilation of shader '{}' failed:\n{}", m_name, logString);
    return false;
  }

  return true;
}
