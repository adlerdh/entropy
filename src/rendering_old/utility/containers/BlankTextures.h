#ifndef BLANK_TEXTURES_H
#define BLANK_TEXTURES_H

#include <memory>
#include <optional>

class BlankTextures
{
public:
  BlankTextures();

  BlankTextures(const BlankTextures&) = delete;
  BlankTextures& operator=(const BlankTextures&) = delete;

  BlankTextures(BlankTextures&&) = default;
  BlankTextures& operator=(BlankTextures&&) = default;

  ~BlankTextures();

  void initializeGL();

  void bindImageTexture2D(std::optional<uint32_t> textureUnit);
  void bindImageTexture3D(std::optional<uint32_t> textureUnit);
  void bindLabelTexture3D(std::optional<uint32_t> textureUnit);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

#endif // BLANK_TEXTURES_H
