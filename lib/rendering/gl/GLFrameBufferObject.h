#pragma once

#include "rendering/gl/GLFBOAttachmentTypes.h"
#include "rendering/gl/GLErrorChecker.h"
#include "rendering/gl/GLTexture.h"
#include "rendering/gl/GLTextureTypes.h"

#include <glad/glad.h>

#include <optional>
#include <string>

/**
 * @brief RAII wrapper around an OpenGL framebuffer object.
 *
 * The wrapper owns one framebuffer name and exposes the attachment operations currently needed by the renderer.
 * Attachment calls check framebuffer completeness after updating the target.
 */
class GLFrameBufferObject final
{
public:
  /// Create a named framebuffer wrapper for diagnostics.
  explicit GLFrameBufferObject(std::string name);

  GLFrameBufferObject(const GLFrameBufferObject&) = delete;
  GLFrameBufferObject& operator=(const GLFrameBufferObject&) = delete;

  GLFrameBufferObject(GLFrameBufferObject&& other) noexcept;
  GLFrameBufferObject& operator=(GLFrameBufferObject&& other) noexcept;

  /// Delete the framebuffer object if still owned.
  ~GLFrameBufferObject();

  /// Generate the framebuffer name if needed.
  void generate();

  /// Delete the framebuffer name and reset local state.
  void destroy();

  /// Bind the framebuffer to the requested draw, read, or draw/read target.
  void bind(const fbo::TargetType& target) const;

  /// Bind the default framebuffer to the requested target.
  static void unbind(const fbo::TargetType& target);

  /// Attach a 2D texture image to a framebuffer attachment point.
  void attach2DTexture(
    const fbo::TargetType& target,
    const fbo::AttachmentType& attachment,
    const GLTexture& texture,
    std::optional<int> colorAttachmentIndex = std::nullopt);

  /**
   * @brief Attach one allocated base-level slice of a 3D texture.
   * @details This framebuffer must already be bound to target. Checks the actual
   * storage depth, attachment index, and framebuffer completeness. Other slices
   * are untouched. Sampling any slice of this texture while drawing is forbidden.
   * @throws std::runtime_error For an invalid target, layer, binding, or incomplete framebuffer.
   */
  void attachTextureLayer(
    const fbo::TargetType& target,
    const fbo::AttachmentType& attachment,
    const GLTexture& texture,
    GLint layer,
    std::optional<int> colorAttachmentIndex = std::nullopt);

  /// Detach a 2D texture image from an attachment point.
  void detach2DTexture(
    const fbo::TargetType& target,
    const fbo::AttachmentType& attachment,
    std::optional<int> colorAttachmentIndex = std::nullopt);

  /// Attach one cube-map face to a framebuffer attachment point.
  void attachCubeMapTexture(
    const fbo::TargetType& target,
    const fbo::AttachmentType& attachment,
    const GLTexture& texture,
    const tex::CubeMapFace& cubeMapFace,
    GLint level,
    std::optional<int> colorAttachmentIndex = std::nullopt);

  GLuint id() const;

private:
  void requireBound(const fbo::TargetType& target) const;
  void checkStatus(const fbo::TargetType& target) const;

  GLErrorChecker m_errorChecker;
  std::string m_name;
  GLuint m_id;
};
