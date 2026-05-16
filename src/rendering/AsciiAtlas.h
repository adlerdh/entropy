#pragma once

#include <glad/glad.h>
#include <glm/vec2.hpp>

#include <string>

/**
 * @brief Bakes a single-row, R8 OpenGL texture atlas from a TTF font.
 *
 * Glyphs are laid out horizontally (left-to-right), pre-sorted from darkest
 * (fewest pixels lit) to brightest (most pixels lit), matching the luminance
 * ordering expected by AsciiPost.fs.
 *
 * Texture unit used for the atlas is chosen by the caller and must not collide
 * with the units reserved in Rendering.cpp (0 = image, 1 = colormap/distmap).
 * We use unit 2 for the ASCII atlas and unit 3 for the scene texture.
 */
class AsciiAtlas
{
public:
  AsciiAtlas();
  ~AsciiAtlas();

  // Non-copyable, movable
  AsciiAtlas(const AsciiAtlas&) = delete;
  AsciiAtlas& operator=(const AsciiAtlas&) = delete;
  AsciiAtlas(AsciiAtlas&&) noexcept;
  AsciiAtlas& operator=(AsciiAtlas&&) noexcept;

  /**
   * @brief Build (or rebuild) the atlas from a TTF font stored in memory.
   *
   * @param ttfData    Pointer to TTF file bytes
   * @param ttfBytes   Number of bytes
   * @param charset    Characters to include, ordered darkest-to-brightest.
   *                   The order is measured by pixel fill density.
   * @param glyphPx    Glyph cell size in pixels {width, height}
   * @return true on success
   */
  bool build(const unsigned char* ttfData, int ttfBytes,
             const std::string& charset, glm::ivec2 glyphPx);

  /// OpenGL texture handle (0 if not built)
  GLuint textureId() const { return m_texId; }

  /// Number of glyphs in the atlas
  int glyphCount() const { return m_glyphCount; }

  /// Glyph cell size in pixels
  glm::ivec2 glyphSize() const { return m_glyphPx; }

private:
  void destroy();

  GLuint     m_texId      = 0;
  int        m_glyphCount = 0;
  glm::ivec2 m_glyphPx    = {8, 16};
};
