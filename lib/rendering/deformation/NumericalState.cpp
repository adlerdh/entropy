#include "rendering/deformation/NumericalState.h"

#include <cstddef>

namespace rendering::deformation::detail
{
namespace
{
constexpr std::array<GLenum, 16> pixelStore{
  GL_PACK_ALIGNMENT,
  GL_PACK_ROW_LENGTH,
  GL_PACK_IMAGE_HEIGHT,
  GL_PACK_SKIP_PIXELS,
  GL_PACK_SKIP_ROWS,
  GL_PACK_SKIP_IMAGES,
  GL_PACK_SWAP_BYTES,
  GL_PACK_LSB_FIRST,
  GL_UNPACK_ALIGNMENT,
  GL_UNPACK_ROW_LENGTH,
  GL_UNPACK_IMAGE_HEIGHT,
  GL_UNPACK_SKIP_PIXELS,
  GL_UNPACK_SKIP_ROWS,
  GL_UNPACK_SKIP_IMAGES,
  GL_UNPACK_SWAP_BYTES,
  GL_UNPACK_LSB_FIRST};
constexpr std::array<GLenum, 6> capabilities{
  GL_FRAMEBUFFER_SRGB,
  GL_DITHER,
  GL_RASTERIZER_DISCARD,
  GL_COLOR_LOGIC_OP,
  GL_SAMPLE_ALPHA_TO_COVERAGE,
  GL_SAMPLE_COVERAGE};
void enable(GLenum capability, GLboolean value)
{
  if (value == GL_TRUE) {
    glEnable(capability);
  }
  else {
    glDisable(capability);
  }
}
} // namespace

NumericalState::NumericalState()
{
  GLint count = 0;
  glGetIntegerv(GL_MAX_CLIP_DISTANCES, &count);
  // Allocate before changing state so allocation failure leaves the context alone.
  m_clipDistances.resize(static_cast<std::size_t>(count));
  glGetIntegerv(GL_MAX_DRAW_BUFFERS, &count);
  m_drawBuffers.resize(static_cast<std::size_t>(count));
  for (GLuint i = 0; i < m_drawBuffers.size(); ++i) {
    m_drawBuffers[i].blend = glIsEnabledi(GL_BLEND, i);
    glGetBooleani_v(GL_COLOR_WRITEMASK, i, m_drawBuffers[i].colorMask.data());
  }
  m_common.emplace(std::initializer_list<OpenGLStateGuard::TextureBinding>{
    {0, GL_TEXTURE_2D},
    {0, GL_TEXTURE_3D},
    {1, GL_TEXTURE_2D},
    {1, GL_TEXTURE_3D}});
  for (std::size_t i = 0; i < m_clipDistances.size(); ++i) {
    const auto cap = GL_CLIP_DISTANCE0 + static_cast<GLenum>(i);
    m_clipDistances[i] = glIsEnabled(cap);
    glDisable(cap);
  }
  for (std::size_t i = 0; i < capabilities.size(); ++i) {
    m_enabled[i] = glIsEnabled(capabilities[i]);
    glDisable(capabilities[i]);
  }
  for (std::size_t i = 0; i < pixelStore.size(); ++i) {
    glGetIntegerv(pixelStore[i], &m_pixelStore[i]);
    glPixelStorei(pixelStore[i], i == 0 || i == 8 ? 1 : 0);
  }
  glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &m_packBuffer);
  glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &m_unpackBuffer);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  for (GLuint i = 0; i < m_samplers.size(); ++i) {
    glGetIntegeri_v(GL_SAMPLER_BINDING, i, &m_samplers[i]);
    glBindSampler(i, 0);
  }
  for (const auto cap :
       {GL_BLEND,
        GL_SCISSOR_TEST,
        GL_DEPTH_TEST,
        GL_STENCIL_TEST,
        GL_CULL_FACE,
        GL_MULTISAMPLE,
        GL_POLYGON_OFFSET_FILL})
  {
    glDisable(static_cast<GLenum>(cap));
  }
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glActiveTexture(GL_TEXTURE0);
}

NumericalState::~NumericalState()
{
  // The common guard restores global blend/mask state. Restore per-buffer values
  // afterwards, because GL 3.3 permits those to differ between draw buffers.
  m_common.reset();
  for (GLuint i = 0; i < m_drawBuffers.size(); ++i) {
    if (m_drawBuffers[i].blend == GL_TRUE) {
      glEnablei(GL_BLEND, i);
    }
    else {
      glDisablei(GL_BLEND, i);
    }
    const auto& mask = m_drawBuffers[i].colorMask;
    glColorMaski(i, mask[0], mask[1], mask[2], mask[3]);
  }
  for (std::size_t i = 0; i < m_clipDistances.size(); ++i) {
    enable(GL_CLIP_DISTANCE0 + static_cast<GLenum>(i), m_clipDistances[i]);
  }
  for (std::size_t i = 0; i < capabilities.size(); ++i) {
    enable(capabilities[i], m_enabled[i]);
  }
  for (std::size_t i = 0; i < pixelStore.size(); ++i) {
    glPixelStorei(pixelStore[i], m_pixelStore[i]);
  }
  glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(m_packBuffer));
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(m_unpackBuffer));
  for (GLuint i = 0; i < m_samplers.size(); ++i) {
    glBindSampler(i, static_cast<GLuint>(m_samplers[i]));
  }
}
} // namespace rendering::deformation::detail
