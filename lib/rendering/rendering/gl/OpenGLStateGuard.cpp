#include "rendering/gl/OpenGLStateGuard.h"

#include "common/Exception.hpp"

#include <algorithm>

OpenGLStateGuard::OpenGLStateGuard() : OpenGLStateGuard({}) {}

OpenGLStateGuard::OpenGLStateGuard(const std::initializer_list<TextureBinding> textureBindings)
{
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_drawFramebuffer);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_readFramebuffer);
  glGetIntegerv(GL_VIEWPORT, m_viewport.data());
  glGetIntegerv(GL_SCISSOR_BOX, m_scissor.data());
  glGetFloatv(GL_COLOR_CLEAR_VALUE, m_clearColor.data());
  glGetDoublev(GL_DEPTH_CLEAR_VALUE, &m_clearDepth);
  glGetBooleanv(GL_COLOR_WRITEMASK, m_colorMask.data());
  glGetBooleanv(GL_DEPTH_WRITEMASK, &m_depthMask);

  glGetIntegerv(GL_BLEND_SRC_RGB, &m_blendSrcRgb);
  glGetIntegerv(GL_BLEND_DST_RGB, &m_blendDstRgb);
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &m_blendSrcAlpha);
  glGetIntegerv(GL_BLEND_DST_ALPHA, &m_blendDstAlpha);
  glGetIntegerv(GL_BLEND_EQUATION_RGB, &m_blendEquationRgb);
  glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &m_blendEquationAlpha);
  glGetIntegerv(GL_DEPTH_FUNC, &m_depthFunction);
  glGetIntegerv(GL_CULL_FACE_MODE, &m_cullFaceMode);
  glGetIntegerv(GL_FRONT_FACE, &m_frontFace);
  std::array<GLint, 2> polygonModes{};
  glGetIntegerv(GL_POLYGON_MODE, polygonModes.data());
  m_polygonMode = polygonModes[0];
  glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &m_polygonOffsetFactor);
  glGetFloatv(GL_POLYGON_OFFSET_UNITS, &m_polygonOffsetUnits);

  const auto captureStencilFace = [](const GLenum face, StencilFaceState& state) {
    const bool front = face == GL_FRONT;
    glGetIntegerv(front ? GL_STENCIL_FUNC : GL_STENCIL_BACK_FUNC, &state.function);
    glGetIntegerv(front ? GL_STENCIL_REF : GL_STENCIL_BACK_REF, &state.reference);
    glGetIntegerv(front ? GL_STENCIL_VALUE_MASK : GL_STENCIL_BACK_VALUE_MASK, &state.valueMask);
    glGetIntegerv(front ? GL_STENCIL_WRITEMASK : GL_STENCIL_BACK_WRITEMASK, &state.writeMask);
    glGetIntegerv(front ? GL_STENCIL_FAIL : GL_STENCIL_BACK_FAIL, &state.stencilFail);
    glGetIntegerv(front ? GL_STENCIL_PASS_DEPTH_FAIL : GL_STENCIL_BACK_PASS_DEPTH_FAIL, &state.depthFail);
    glGetIntegerv(front ? GL_STENCIL_PASS_DEPTH_PASS : GL_STENCIL_BACK_PASS_DEPTH_PASS, &state.depthPass);
  };
  captureStencilFace(GL_FRONT, m_frontStencil);
  captureStencilFace(GL_BACK, m_backStencil);

  m_blendEnabled = glIsEnabled(GL_BLEND);
  m_scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
  m_depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
  m_stencilTestEnabled = glIsEnabled(GL_STENCIL_TEST);
  m_cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
  m_multisampleEnabled = glIsEnabled(GL_MULTISAMPLE);
  m_polygonOffsetFillEnabled = glIsEnabled(GL_POLYGON_OFFSET_FILL);

  glGetIntegerv(GL_ACTIVE_TEXTURE, &m_activeTexture);
  glGetIntegerv(GL_CURRENT_PROGRAM, &m_program);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &m_vertexArray);

  GLint maxTextureUnits = 0;
  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
  // Validate the complete request before changing the active texture unit. This keeps construction exception-safe
  // when a caller supplies an unsupported target or an out-of-range unit.
  for (const TextureBinding& binding : textureBindings) {
    if (binding.unit >= static_cast<uint32_t>(std::max(maxTextureUnits, 0))) {
      throwDebug("OpenGL state guard texture unit exceeds the context limit");
    }
    static_cast<void>(textureBindingQuery(binding.target));
  }

  m_textureBindings.reserve(textureBindings.size());
  for (const TextureBinding& binding : textureBindings) {
    const GLenum bindingQuery = textureBindingQuery(binding.target);
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + binding.unit));
    GLint texture = 0;
    glGetIntegerv(bindingQuery, &texture);
    m_textureBindings.push_back({.unit = binding.unit, .target = binding.target, .texture = texture});
  }
  glActiveTexture(static_cast<GLenum>(m_activeTexture));
}

OpenGLStateGuard::~OpenGLStateGuard()
{
  restoreFramebufferAndViewport();
  glClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);
  glClearDepth(m_clearDepth);
  glColorMask(m_colorMask[0], m_colorMask[1], m_colorMask[2], m_colorMask[3]);
  glDepthMask(m_depthMask);

  glBlendEquationSeparate(static_cast<GLenum>(m_blendEquationRgb), static_cast<GLenum>(m_blendEquationAlpha));
  glBlendFuncSeparate(
    static_cast<GLenum>(m_blendSrcRgb),
    static_cast<GLenum>(m_blendDstRgb),
    static_cast<GLenum>(m_blendSrcAlpha),
    static_cast<GLenum>(m_blendDstAlpha));
  glDepthFunc(static_cast<GLenum>(m_depthFunction));
  glCullFace(static_cast<GLenum>(m_cullFaceMode));
  glFrontFace(static_cast<GLenum>(m_frontFace));
  glPolygonMode(GL_FRONT_AND_BACK, static_cast<GLenum>(m_polygonMode));
  glPolygonOffset(m_polygonOffsetFactor, m_polygonOffsetUnits);
  const auto restoreStencilFace = [](const GLenum face, const StencilFaceState& state) {
    glStencilFuncSeparate(
      face,
      static_cast<GLenum>(state.function),
      state.reference,
      static_cast<GLuint>(state.valueMask));
    glStencilMaskSeparate(face, static_cast<GLuint>(state.writeMask));
    glStencilOpSeparate(
      face,
      static_cast<GLenum>(state.stencilFail),
      static_cast<GLenum>(state.depthFail),
      static_cast<GLenum>(state.depthPass));
  };
  restoreStencilFace(GL_FRONT, m_frontStencil);
  restoreStencilFace(GL_BACK, m_backStencil);

  setEnabled(GL_BLEND, m_blendEnabled);
  setEnabled(GL_SCISSOR_TEST, m_scissorEnabled);
  setEnabled(GL_DEPTH_TEST, m_depthTestEnabled);
  setEnabled(GL_STENCIL_TEST, m_stencilTestEnabled);
  setEnabled(GL_CULL_FACE, m_cullFaceEnabled);
  setEnabled(GL_MULTISAMPLE, m_multisampleEnabled);
  setEnabled(GL_POLYGON_OFFSET_FILL, m_polygonOffsetFillEnabled);

  for (const SavedTextureBinding& binding : m_textureBindings) {
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + binding.unit));
    glBindTexture(binding.target, static_cast<GLuint>(binding.texture));
  }
  glActiveTexture(static_cast<GLenum>(m_activeTexture));
  glUseProgram(static_cast<GLuint>(m_program));
  glBindVertexArray(static_cast<GLuint>(m_vertexArray));
}

void OpenGLStateGuard::restoreFramebufferAndViewport() const noexcept
{
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(m_drawFramebuffer));
  glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(m_readFramebuffer));
  glViewport(m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);
  glScissor(m_scissor[0], m_scissor[1], m_scissor[2], m_scissor[3]);
}

GLuint OpenGLStateGuard::drawFramebuffer() const noexcept
{
  return static_cast<GLuint>(m_drawFramebuffer);
}

GLenum OpenGLStateGuard::textureBindingQuery(const GLenum target)
{
  switch (target) {
    case GL_TEXTURE_1D:
      return GL_TEXTURE_BINDING_1D;
    case GL_TEXTURE_2D:
      return GL_TEXTURE_BINDING_2D;
    case GL_TEXTURE_3D:
      return GL_TEXTURE_BINDING_3D;
    case GL_TEXTURE_CUBE_MAP:
      return GL_TEXTURE_BINDING_CUBE_MAP;
    case GL_TEXTURE_1D_ARRAY:
      return GL_TEXTURE_BINDING_1D_ARRAY;
    case GL_TEXTURE_2D_ARRAY:
      return GL_TEXTURE_BINDING_2D_ARRAY;
    case GL_TEXTURE_RECTANGLE:
      return GL_TEXTURE_BINDING_RECTANGLE;
    case GL_TEXTURE_BUFFER:
      return GL_TEXTURE_BINDING_BUFFER;
    case GL_TEXTURE_2D_MULTISAMPLE:
      return GL_TEXTURE_BINDING_2D_MULTISAMPLE;
    case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
      return GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY;
    default:
      throwDebug("Unsupported texture target in OpenGL state guard");
  }
}

void OpenGLStateGuard::setEnabled(const GLenum capability, const GLboolean enabled) noexcept
{
  enabled == GL_TRUE ? glEnable(capability) : glDisable(capability);
}
