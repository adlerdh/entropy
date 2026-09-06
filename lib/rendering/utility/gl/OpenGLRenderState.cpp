#include "rendering/utility/gl/OpenGLRenderState.h"

#include <glad/glad.h>

namespace rendering
{

namespace
{

void clearTextureBindingsForAllUnits()
{
  GLint previousTextureUnit = GL_TEXTURE0;
  GLint maxTextureUnits = 0;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &previousTextureUnit);
  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTextureUnits);

  for (GLint unit = 0; unit < maxTextureUnits; ++unit) {
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
    glBindSampler(static_cast<GLuint>(unit), 0);
    glBindTexture(GL_TEXTURE_1D, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_3D, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glBindTexture(GL_TEXTURE_1D_ARRAY, 0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    glBindTexture(GL_TEXTURE_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 0);
  }
  glActiveTexture(static_cast<GLenum>(previousTextureUnit));
}

} // namespace

void restoreOpenGLRenderState()
{
  glEnable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE);
  glDisable(GL_SCISSOR_TEST);
  glEnable(GL_STENCIL_TEST);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LESS);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glPolygonOffset(0.0f, 0.0f);
  glStencilMask(0xffffffffu);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  glStencilFunc(GL_ALWAYS, 0, 0xffffffffu);
  glActiveTexture(GL_TEXTURE0);
}

void clearOpenGLBindingsForShutdown()
{
  glUseProgram(0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glBindVertexArray(0);
  clearTextureBindingsForAllUnits();
  glActiveTexture(GL_TEXTURE0);
}

} // namespace rendering
