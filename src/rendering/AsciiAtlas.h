#pragma once

#include "rendering/AsciiAtlasBaker.h"

#include <glad/glad.h>
#include <glm/vec2.hpp>

#include <cstdint>
#include <string>
#include <vector>

struct GlyphMeta
{
  float u0, u1; // atlas UV x range [0,1]
};

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
  static constexpr int kMaxGlyphs = 128;
  static constexpr int kPadding = 4;
  static constexpr float kPixDistScale = 32.0f;
  static constexpr uint8_t kOnedgeValue = 128;

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
  bool build(const unsigned char* ttfData, int ttfBytes, const std::string& charset, glm::ivec2 glyphPx);

  /// OpenGL texture handle (0 if not built)
  GLuint textureId() const
  {
    return m_texId;
  }

  /// Number of glyphs in the atlas
  int glyphCount() const
  {
    return m_glyphCount;
  }

  /// Glyph cell size in pixels
  glm::ivec2 glyphSize() const
  {
    return m_glyphPx;
  }

  /// Fill fractions in sorted glyph order (darkest to brightest)
  const std::vector<float>& fillFractions() const
  {
    return m_fillFractions;
  }

  /// Per-glyph SDF metadata in sorted glyph order
  const std::vector<GlyphMeta>& glyphMeta() const
  {
    return m_glyphMeta;
  }

  /// Slot size in atlas pixels {slotW, slotH} (glyph cell + SDF padding on each side)
  glm::ivec2 slotSize() const
  {
    return m_slotPx;
  }

  /// Per-glyph raw slot pixels in sorted order (for testing and coverage computation)
  const std::vector<std::vector<uint8_t>>& slotPixels() const
  {
    return m_slotPixels;
  }

  /// CPU simulation of rendered coverage per glyph at the given screen cell size.
  /// Returns coverage fraction [0,1] in the same sorted order as fillFractions().
  /// Call when cell size changes; uses m_slotPixels (built at build() time).
  std::vector<float> computeRenderedCoverage(glm::vec2 cellSizePx) const;

  /// CPU simulation of 3x2 region spatial profiles per glyph at the given screen cell size.
  /// Returns N GlyphProfile entries in the same sorted order as fillFractions().
  std::vector<GlyphProfile> computeRenderedSpatialProfiles(glm::vec2 cellSizePx) const;

private:
  void destroy();

  GLuint m_texId = 0;
  int m_glyphCount = 0;
  glm::ivec2 m_glyphPx = {0, 0};
  glm::ivec2 m_slotPx = {0, 0};
  std::vector<float> m_fillFractions;
  std::vector<GlyphMeta> m_glyphMeta;
  std::vector<std::vector<uint8_t>> m_slotPixels; // per-glyph slot pixels, sorted order
};
