#pragma once

#include <glad/glad.h>

#include <cstdint>

namespace fbo
{

/**
 * @brief Framebuffer binding targets.
 */
enum class TargetType : uint32_t
{
  Draw = GL_DRAW_FRAMEBUFFER,
  Read = GL_READ_FRAMEBUFFER,
  DrawAndRead = GL_FRAMEBUFFER
};

/**
 * @brief Framebuffer attachment points used by the renderer.
 */
enum class AttachmentType : uint32_t
{
  Color = GL_COLOR_ATTACHMENT0,
  Depth = GL_DEPTH_ATTACHMENT,
  Stencil = GL_STENCIL_ATTACHMENT,
  DepthStencil = GL_DEPTH_STENCIL_ATTACHMENT
};

} // namespace fbo
