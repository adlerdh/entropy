#include "rendering/utility/gl/GLFrameBufferObject.h"
#include "rendering/utility/UnderlyingEnumType.h"

#include "common/Exception.hpp"

#include <glad/glad.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <sstream>
#include <utility>

namespace
{

const char* framebufferStatusName(const GLenum status) noexcept
{
  switch (status) {
    case GL_FRAMEBUFFER_COMPLETE:
      return "complete";
    case GL_FRAMEBUFFER_UNDEFINED:
      return "undefined";
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
      return "incomplete attachment";
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
      return "missing attachment";
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
      return "incomplete draw buffer";
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
      return "incomplete read buffer";
    case GL_FRAMEBUFFER_UNSUPPORTED:
      return "unsupported attachment combination";
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
      return "incomplete multisample configuration";
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
      return "incomplete layered targets";
    default:
      return "unknown status";
  }
}

} // namespace

GLFrameBufferObject::GLFrameBufferObject(std::string name) : m_name(std::move(name)), m_id(0u) {}

GLFrameBufferObject::GLFrameBufferObject(GLFrameBufferObject&& other) noexcept
  : m_name(std::move(other.m_name)), m_id(other.m_id)
{
  other.m_id = 0u;
}

GLFrameBufferObject& GLFrameBufferObject::operator=(GLFrameBufferObject&& other) noexcept
{
  if (this != &other) {
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
  if (m_id != 0u) {
    return;
  }
  glGenFramebuffers(1, &m_id);
  CHECK_GL_ERROR(m_errorChecker);
}

void GLFrameBufferObject::destroy()
{
  if (m_id != 0u) {
    glDeleteFramebuffers(1, &m_id);
  }
  m_id = 0u;
}

void GLFrameBufferObject::bind(const fbo::TargetType& target) const
{
  if (m_id == 0u) {
    throwDebug("Cannot bind a framebuffer before generating it");
  }
  glBindFramebuffer(underlyingType(target), m_id);
  CHECK_GL_ERROR(m_errorChecker);
}

void GLFrameBufferObject::unbind(const fbo::TargetType& target)
{
  glBindFramebuffer(underlyingType(target), 0u);
  CHECK_GL_ERROR(GLErrorChecker{});
}

void GLFrameBufferObject::attach2DTexture(
  const fbo::TargetType& target,
  const fbo::AttachmentType& attachment,
  const GLTexture& texture,
  std::optional<int> colorAttachmentIndex)
{
  if (m_id == 0u) {
    throwDebug("Cannot attach a texture before generating the framebuffer");
  }
  if (texture.id() == 0u) {
    throwDebug("Cannot attach an ungenerated texture to a framebuffer");
  }
  requireBound(target);

  if (
    tex::Target::Texture2D != texture.target() && tex::Target::Texture2DMultisample != texture.target() &&
    tex::Target::TextureRectangle != texture.target())
  {
    spdlog::error("Invalid texture target");
    throwDebug("Invalid texture target");
  }

  int index = 0;

  if (fbo::AttachmentType::Color == attachment) {
    if (colorAttachmentIndex) {
      // Get maximum color attachment point for the FBO
      GLint maxAttach = 0;
      glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAttach);

      if (*colorAttachmentIndex < 0 || maxAttach <= *colorAttachmentIndex) {
        spdlog::error("Invalid color attachment index {}", *colorAttachmentIndex);
        throwDebug("Invalid color attachment index");
      }

      index = *colorAttachmentIndex;
    }
    else {
      spdlog::error("No color attachment index specified");
      throwDebug("No color attachment index specified");
    }
  }
  else if (colorAttachmentIndex) {
    throwDebug("Color attachment indices are only valid for color attachments");
  }

  glFramebufferTexture2D(
    underlyingType(target),
    underlyingType(attachment) + static_cast<GLenum>(index),
    underlyingType(texture.target()),
    texture.id(),
    0);

  CHECK_GL_ERROR(m_errorChecker);
  checkStatus(target);
}

void GLFrameBufferObject::detach2DTexture(
  const fbo::TargetType& target,
  const fbo::AttachmentType& attachment,
  std::optional<int> colorAttachmentIndex)
{
  if (m_id == 0u) {
    throwDebug("Cannot detach a texture before generating the framebuffer");
  }
  requireBound(target);

  int index = 0;
  if (fbo::AttachmentType::Color == attachment) {
    if (!colorAttachmentIndex) {
      throwDebug("No color attachment index specified");
    }
    GLint maxAttachments = 0;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAttachments);
    if (*colorAttachmentIndex < 0 || *colorAttachmentIndex >= maxAttachments) {
      throwDebug("Invalid color attachment index");
    }
    index = *colorAttachmentIndex;
  }
  else if (colorAttachmentIndex) {
    throwDebug("Color attachment indices are only valid for color attachments");
  }

  glFramebufferTexture2D(
    underlyingType(target),
    underlyingType(attachment) + static_cast<GLenum>(index),
    GL_TEXTURE_2D,
    0u,
    0);
  CHECK_GL_ERROR(m_errorChecker);
  checkStatus(target);
}

// GLint maxDrawBuf = 0;
// glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuf);

void GLFrameBufferObject::attachCubeMapTexture(
  const fbo::TargetType& target,
  const fbo::AttachmentType& attachment,
  const GLTexture& texture,
  const tex::CubeMapFace& cubeMapFace,
  GLint level,
  std::optional<int> colorAttachmentIndex)
{
  if (level < 0) {
    throwDebug("Cube-map framebuffer attachment level cannot be negative");
  }
  if (tex::Target::TextureCubeMap != texture.target()) {
    throwDebug("Invalid cube-map texture target");
  }

  if (m_id == 0u) {
    throwDebug("Cannot attach a texture before generating the framebuffer");
  }
  if (texture.id() == 0u) {
    throwDebug("Cannot attach an ungenerated texture to a framebuffer");
  }
  requireBound(target);

  int index = 0;

  if (fbo::AttachmentType::Color == attachment) {
    if (!colorAttachmentIndex) {
      throwDebug("No color attachment index specified");
    }
    // Get maximum color attachment point for the FBO
    GLint maxAttach = 0;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAttach);

    if (*colorAttachmentIndex < 0 || maxAttach <= *colorAttachmentIndex) {
      spdlog::error("Invalid color attachment index {}", *colorAttachmentIndex);
      throwDebug("Invalid color attachment index");
    }

    index = *colorAttachmentIndex;
  }
  else if (colorAttachmentIndex) {
    throwDebug("Color attachment indices are only valid for color attachments");
  }

  glFramebufferTexture2D(
    underlyingType(target),
    underlyingType(attachment) + static_cast<GLenum>(index),
    underlyingType(cubeMapFace),
    texture.id(),
    level);

  CHECK_GL_ERROR(m_errorChecker);
  checkStatus(target);
}

GLuint GLFrameBufferObject::id() const
{
  return m_id;
}

void GLFrameBufferObject::requireBound(const fbo::TargetType& target) const
{
  const auto requireBinding = [this](const GLenum query) {
    GLint boundFramebuffer = 0;
    glGetIntegerv(query, &boundFramebuffer);
    if (static_cast<GLuint>(boundFramebuffer) != m_id) {
      throwDebug("Framebuffer '" + m_name + "' must be bound before attaching a texture");
    }
  };

  if (target == fbo::TargetType::Draw || target == fbo::TargetType::DrawAndRead) {
    requireBinding(GL_DRAW_FRAMEBUFFER_BINDING);
  }
  if (target == fbo::TargetType::Read || target == fbo::TargetType::DrawAndRead) {
    requireBinding(GL_READ_FRAMEBUFFER_BINDING);
  }
}

void GLFrameBufferObject::checkStatus(const fbo::TargetType& target) const
{
  const GLenum glTarget = underlyingType(target);
  const GLenum status = glCheckFramebufferStatus(glTarget);

  if (GL_FRAMEBUFFER_COMPLETE != status) {
    const std::string message = "Framebuffer '" + m_name + "' is incomplete: " + framebufferStatusName(status) + " (" +
                                std::to_string(status) + ")";
    spdlog::error("{}", message);
    throwDebug(message);
  }
}
