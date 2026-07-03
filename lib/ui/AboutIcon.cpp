#include "ui/AboutIcon.h"

#include <glad/glad.h>

#define STBI_ONLY_PNG
#include <stb_image.h>

#include <cmrc/cmrc.hpp>
#include <exception>

CMRC_DECLARE(icons);

namespace
{

GLuint loadTexture()
{
  cmrc::file icon;
  try {
    const auto filesystem = cmrc::icons::get_filesystem();
    icon = filesystem.open(ENTROPY_ABOUT_ICON_RESOURCE_PATH);
  }
  catch (const std::exception&) {
    return 0;
  }

  int width = 0;
  int height = 0;
  int channels = 0;
  stbi_uc* pixels = stbi_load_from_memory(
    reinterpret_cast<const stbi_uc*>(icon.begin()),
    static_cast<int>(icon.size()),
    &width,
    &height,
    &channels,
    4);

  if (!pixels || width <= 0 || height <= 0) {
    stbi_image_free(pixels);
    return 0;
  }

  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  glBindTexture(GL_TEXTURE_2D, 0);
  stbi_image_free(pixels);

  return texture;
}

} // namespace

namespace about_entropy_icon
{

ImTextureID textureId()
{
  static const GLuint texture = loadTexture();
  return static_cast<ImTextureID>(texture);
}

} // namespace about_entropy_icon
