#pragma once

#include <glad/glad.h>

#include <array>
#include <cstdint>
#include <initializer_list>
#include <vector>

/**
 * @brief Restores mutable OpenGL context state after an isolated rendering pass.
 *
 * OpenGL state belongs to the context rather than to an individual renderer. This guard captures the common raster,
 * framebuffer, program, and vertex-array state that Entropy's off-screen passes modify. Texture bindings are captured
 * only for the explicitly listed unit/target pairs so a pass does not query every texture unit.
 */
class OpenGLStateGuard final
{
public:
  struct TextureBinding
  {
    uint32_t unit = 0u;
    GLenum target = GL_TEXTURE_2D;
  };

  OpenGLStateGuard();
  OpenGLStateGuard(std::initializer_list<TextureBinding> textureBindings);

  OpenGLStateGuard(const OpenGLStateGuard&) = delete;
  OpenGLStateGuard& operator=(const OpenGLStateGuard&) = delete;
  OpenGLStateGuard(OpenGLStateGuard&&) = delete;
  OpenGLStateGuard& operator=(OpenGLStateGuard&&) = delete;

  ~OpenGLStateGuard();

  /// Restore only the destination framebuffer, viewport, and scissor rectangle for an in-pass resolve operation.
  void restoreFramebufferAndViewport() const noexcept;

  [[nodiscard]] GLuint drawFramebuffer() const noexcept;

private:
  struct SavedTextureBinding
  {
    uint32_t unit = 0u;
    GLenum target = GL_TEXTURE_2D;
    GLint texture = 0;
  };

  static GLenum textureBindingQuery(GLenum target);
  static void setEnabled(GLenum capability, GLboolean enabled) noexcept;

  GLint m_drawFramebuffer = 0;
  GLint m_readFramebuffer = 0;
  std::array<GLint, 4> m_viewport{};
  std::array<GLint, 4> m_scissor{};
  std::array<GLfloat, 4> m_clearColor{};
  GLdouble m_clearDepth = 1.0;
  std::array<GLboolean, 4> m_colorMask{};
  GLboolean m_depthMask = GL_TRUE;

  GLint m_blendSrcRgb = GL_ONE;
  GLint m_blendDstRgb = GL_ZERO;
  GLint m_blendSrcAlpha = GL_ONE;
  GLint m_blendDstAlpha = GL_ZERO;
  GLint m_blendEquationRgb = GL_FUNC_ADD;
  GLint m_blendEquationAlpha = GL_FUNC_ADD;
  GLint m_depthFunction = GL_LESS;
  GLint m_cullFaceMode = GL_BACK;
  GLint m_frontFace = GL_CCW;
  GLint m_polygonMode = GL_FILL;
  GLfloat m_polygonOffsetFactor = 0.0f;
  GLfloat m_polygonOffsetUnits = 0.0f;

  struct StencilFaceState
  {
    GLint function = GL_ALWAYS;
    GLint reference = 0;
    GLint valueMask = -1;
    GLint writeMask = -1;
    GLint stencilFail = GL_KEEP;
    GLint depthFail = GL_KEEP;
    GLint depthPass = GL_KEEP;
  };
  StencilFaceState m_frontStencil;
  StencilFaceState m_backStencil;

  GLboolean m_blendEnabled = GL_FALSE;
  GLboolean m_scissorEnabled = GL_FALSE;
  GLboolean m_depthTestEnabled = GL_FALSE;
  GLboolean m_stencilTestEnabled = GL_FALSE;
  GLboolean m_cullFaceEnabled = GL_FALSE;
  GLboolean m_multisampleEnabled = GL_FALSE;
  GLboolean m_polygonOffsetFillEnabled = GL_FALSE;

  GLint m_activeTexture = GL_TEXTURE0;
  GLint m_program = 0;
  GLint m_vertexArray = 0;
  std::vector<SavedTextureBinding> m_textureBindings;
};
