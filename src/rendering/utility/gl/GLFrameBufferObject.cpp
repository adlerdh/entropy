#include "rendering/utility/gl/GLFrameBufferObject.h"
#include "rendering/utility/UnderlyingEnumType.h"

#include "common/Exception.hpp"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <sstream>

GLFrameBufferObject::GLFrameBufferObject(const std::string& name)
  : m_name(name)
  , m_id(0u)
{
}

GLFrameBufferObject::GLFrameBufferObject(GLFrameBufferObject&& other) noexcept
  : m_name(other.m_name)
  , m_id(other.m_id)
{
  other.m_name = "";
  other.m_id = 0u;
}

GLFrameBufferObject& GLFrameBufferObject::operator=(GLFrameBufferObject&& other) noexcept
{
  if (this != &other)
  {
    destroy();

    std::swap(m_name, other.m_name);
    std::swap(m_id, other.m_id);
  }

  return *this;
}

GLFrameBufferObject::~GLFrameBufferObject()
{
  destroy();
}

void GLFrameBufferObject::generate()
{
  glGenFramebuffers(1, &m_id);
}

void GLFrameBufferObject::destroy()
{
  glDeleteFramebuffers(1, &m_id);
}

void GLFrameBufferObject::bind(const fbo::TargetType& target)
{
  glBindFramebuffer(underlyingType(target), m_id);
}

void GLFrameBufferObject::attach2DTexture(
  const fbo::TargetType& target,
  const fbo::AttachmentType& attachment,
  const GLTexture& texture,
  std::optional<int> colorAttachmentIndex
)
{
  if (fbo::TargetType::DrawAndRead == target)
  {
    spdlog::error("Invalid FBO target");
    throw_debug("Invalid FBO target")
  }

  if ( tex::Target::Texture2D != texture.target() &&
        tex::Target::Texture2DMultisample != texture.target() &&
        tex::Target::TextureRectangle != texture.target() )
  {
    spdlog::error("Invalid texture target");
    throw_debug("Invalid texture target")
  }

  int index = 0;

  if (fbo::AttachmentType::Color == attachment)
  {
    if (colorAttachmentIndex)
    {
      // Get maximum color attachment point for the FBO
      GLint maxAttach = 0;
      glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAttach);

      if (*colorAttachmentIndex < 0 || maxAttach <= *colorAttachmentIndex)
      {
        spdlog::error("Invalid color attachment index {}", *colorAttachmentIndex);
        throw_debug("Invalid color attachment index")
      }

      index = *colorAttachmentIndex;
    }
    else
    {
      spdlog::error("No color attachment index specified");
      throw_debug("No color attachment index specified")
    }
  }

  glFramebufferTexture2D(
    underlyingType(target),
    underlyingType(attachment) + static_cast<GLenum>(index),
    underlyingType(texture.target()),
    texture.id(),
    0
  );

  checkStatus();
}

//GLint maxDrawBuf = 0;
//glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuf);

void GLFrameBufferObject::attachCubeMapTexture(
  const fbo::TargetType& target,
  const fbo::AttachmentType& attachment,
  const GLTexture& texture,
  const tex::CubeMapFace& cubeMapFace,
  GLint level,
  std::optional<int> colorAttachmentIndex
)
{
  if (tex::Target::TextureCubeMap != texture.target())
  {
    throw_debug("Invalid FBO target")
  }

  int index = 0;

  if (fbo::AttachmentType::Color == attachment && colorAttachmentIndex)
  {
    // Get maximum color attachment point for the FBO
    GLint maxAttach = 0;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAttach);

    if (*colorAttachmentIndex < 0 || maxAttach <= *colorAttachmentIndex)
    {
      spdlog::error("Invalid color attachment index {}", *colorAttachmentIndex);
      throw_debug("Invalid color attachment index")
    }

    index = *colorAttachmentIndex;
  }

  glFramebufferTexture2D(
    underlyingType(target),
    underlyingType(attachment) + static_cast<GLenum>(index),
    underlyingType(cubeMapFace),
    texture.id(),
    level
  );

  checkStatus();
}

GLuint GLFrameBufferObject::id() const
{
  return m_id;
}

void GLFrameBufferObject::checkStatus()
{
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

  if (GL_FRAMEBUFFER_COMPLETE != status)
  {
    spdlog::error(
      "Framebuffer object '{}' not complete: {}", m_name, glCheckFramebufferStatus(GL_FRAMEBUFFER)
    );
    throw_debug("Framebuffer object not complete")
  }
}
