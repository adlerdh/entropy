#include "ui/AboutIcon.h"

#include <glad/glad.h>

#define STBI_ONLY_PNG
#include <stb_image.h>

#include <cmrc/cmrc.hpp>
#include <spdlog/spdlog.h>

#include <exception>
#include <limits>
#include <memory>

CMRC_DECLARE(icons);

namespace
{

GLuint g_texture = 0u;

struct PixelUnpackState
{
  GLint alignment = 4;
  GLint rowLength = 0;
  GLint imageHeight = 0;
  GLint skipPixels = 0;
  GLint skipRows = 0;
  GLint skipImages = 0;
  GLboolean swapBytes = GL_FALSE;
  GLboolean lsbFirst = GL_FALSE;
};

PixelUnpackState capturePixelUnpackState()
{
  PixelUnpackState state;
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &state.alignment);
  glGetIntegerv(GL_UNPACK_ROW_LENGTH, &state.rowLength);
  glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT, &state.imageHeight);
  glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &state.skipPixels);
  glGetIntegerv(GL_UNPACK_SKIP_ROWS, &state.skipRows);
  glGetIntegerv(GL_UNPACK_SKIP_IMAGES, &state.skipImages);
  glGetBooleanv(GL_UNPACK_SWAP_BYTES, &state.swapBytes);
  glGetBooleanv(GL_UNPACK_LSB_FIRST, &state.lsbFirst);
  return state;
}

void applyPixelUnpackState(const PixelUnpackState& state)
{
  glPixelStorei(GL_UNPACK_ALIGNMENT, state.alignment);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, state.rowLength);
  glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, state.imageHeight);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, state.skipPixels);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, state.skipRows);
  glPixelStorei(GL_UNPACK_SKIP_IMAGES, state.skipImages);
  glPixelStorei(GL_UNPACK_SWAP_BYTES, state.swapBytes);
  glPixelStorei(GL_UNPACK_LSB_FIRST, state.lsbFirst);
}

void applyTightlyPackedPixelUnpackState()
{
  applyPixelUnpackState({.alignment = 1});
}

GLenum consumeOpenGLErrors() noexcept
{
  GLenum firstError = GL_NO_ERROR;
  for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError()) {
    if (firstError == GL_NO_ERROR) {
      firstError = error;
    }
  }
  return firstError;
}

GLuint loadTexture()
{
  cmrc::file icon;
  try {
    const auto filesystem = cmrc::icons::get_filesystem();
    icon = filesystem.open(ENTROPY_ABOUT_ICON_RESOURCE_PATH);
  }
  catch (const std::exception& error) {
    spdlog::error("Unable to open embedded About dialog icon '{}': {}", ENTROPY_ABOUT_ICON_RESOURCE_PATH, error.what());
    return 0;
  }

  int width = 0;
  int height = 0;
  int channels = 0;
  if (icon.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    spdlog::error("Embedded About dialog icon is too large to decode");
    return 0;
  }

  using StbiPixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;
  StbiPixels pixels{
    stbi_load_from_memory(
      reinterpret_cast<const stbi_uc*>(icon.begin()),
      static_cast<int>(icon.size()),
      &width,
      &height,
      &channels,
      4),
    &stbi_image_free};

  if (!pixels || width <= 0 || height <= 0) {
    spdlog::error("Unable to decode embedded About dialog icon '{}'", ENTROPY_ABOUT_ICON_RESOURCE_PATH);
    return 0;
  }

  GLint previousActiveTexture = GL_TEXTURE0;
  GLint previousTexture = 0;
  glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
  const PixelUnpackState previousPixelUnpackState = capturePixelUnpackState();

  if (const GLenum priorError = consumeOpenGLErrors(); priorError != GL_NO_ERROR) {
    spdlog::warn("Cleared a pending OpenGL error ({}) before uploading the About dialog icon", priorError);
  }

  GLuint texture = 0;
  glGenTextures(1, &texture);
  if (texture == 0u) {
    spdlog::error("Unable to create the About dialog icon texture");
    return 0u;
  }
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  applyTightlyPackedPixelUnpackState();
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());

  const GLenum uploadError = consumeOpenGLErrors();
  applyPixelUnpackState(previousPixelUnpackState);
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
  glActiveTexture(static_cast<GLenum>(previousActiveTexture));

  if (uploadError != GL_NO_ERROR) {
    glDeleteTextures(1, &texture);
    spdlog::error("Unable to upload the About dialog icon texture (OpenGL error {})", uploadError);
    return 0u;
  }

  return texture;
}

} // namespace

namespace about_entropy_icon
{

ImTextureID textureId()
{
  if (g_texture == 0u) {
    g_texture = loadTexture();
  }
  return static_cast<ImTextureID>(g_texture);
}

void releaseTexture() noexcept
{
  if (g_texture != 0u) {
    glDeleteTextures(1, &g_texture);
    g_texture = 0u;
  }
}

} // namespace about_entropy_icon
