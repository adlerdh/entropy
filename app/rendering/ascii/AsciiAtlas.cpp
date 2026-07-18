#include "rendering/ascii/AsciiAtlas.h"
#include "rendering/ascii/AsciiAtlasBaker.h"

#include <glad/glad.h>

#include <spdlog/spdlog.h>

#include <cstring>
#include <vector>

std::vector<float> AsciiAtlas::computeRenderedCoverage(glm::vec2 cellSizePx) const
{
  if (m_slotPixels.empty()) return {};

  const int N = m_glyphCount;
  std::vector<float> coverage(static_cast<size_t>(N));

  for (int gi = 0; gi < N; ++gi) {
    coverage[static_cast<size_t>(gi)] = computeGlyphCoverage(
      m_slotPixels[static_cast<size_t>(gi)],
      m_slotPx.x,
      m_slotPx.y,
      kPadding,
      kPixDistScale,
      kOnedgeValue,
      cellSizePx);
  }

  return coverage;
}

std::vector<GlyphProfile> AsciiAtlas::computeRenderedSpatialProfiles(glm::vec2 cellSizePx) const
{
  if (m_slotPixels.empty()) return {};
  return computeGlyphSpatialProfiles(m_slotPixels, m_slotPx, kPadding, kPixDistScale, kOnedgeValue, cellSizePx);
}

AsciiAtlas::~AsciiAtlas()
{
  destroy();
}

AsciiAtlas::AsciiAtlas(AsciiAtlas&& o) noexcept
  : m_texId(o.m_texId)
  , m_glyphCount(o.m_glyphCount)
  , m_glyphPx(o.m_glyphPx)
  , m_slotPx(o.m_slotPx)
  , m_characters(std::move(o.m_characters))
  , m_fillFractions(std::move(o.m_fillFractions))
  , m_glyphMeta(std::move(o.m_glyphMeta))
  , m_slotPixels(std::move(o.m_slotPixels))
{
  o.m_texId = 0;
  o.m_glyphCount = 0;
}

AsciiAtlas& AsciiAtlas::operator=(AsciiAtlas&& o) noexcept
{
  if (this != &o) {
    destroy();
    m_texId = o.m_texId;
    m_glyphCount = o.m_glyphCount;
    m_glyphPx = o.m_glyphPx;
    m_slotPx = o.m_slotPx;
    m_characters = std::move(o.m_characters);
    m_fillFractions = std::move(o.m_fillFractions);
    m_glyphMeta = std::move(o.m_glyphMeta);
    m_slotPixels = std::move(o.m_slotPixels);
    o.m_texId = 0;
    o.m_glyphCount = 0;
  }
  return *this;
}

void AsciiAtlas::destroy()
{
  if (m_texId) {
    glDeleteTextures(1, &m_texId);
    m_texId = 0;
  }
  m_characters.clear();
  m_fillFractions.clear();
  m_glyphMeta.clear();
  m_slotPixels.clear();
}

bool AsciiAtlas::build(const unsigned char* ttfData, int ttfBytes, const std::string& charset, glm::ivec2 glyphPx)
{
  destroy();

  auto bakedGlyphs = bakeGlyphs(ttfData, ttfBytes, charset, glyphPx, kPadding, kPixDistScale, kOnedgeValue);
  if (bakedGlyphs.empty()) return false;

  const int Gw = glyphPx.x;
  const int Gh = glyphPx.y;
  const int slotW = Gw + 2 * kPadding;
  const int slotH = Gh + 2 * kPadding;
  const int N = static_cast<int>(bakedGlyphs.size());

  // Build flat atlas row (all slots concatenated horizontally)
  std::vector<uint8_t> atlasPixels(static_cast<size_t>(slotW * N * slotH), 0);
  m_fillFractions.resize(N);
  m_glyphMeta.resize(N);
  m_characters.clear();
  m_characters.reserve(static_cast<size_t>(N));

  for (int i = 0; i < N; ++i) {
    const auto& g = bakedGlyphs[i];
    m_fillFractions[i] = g.fillFraction;
    m_characters.push_back(g.character);
    m_glyphMeta[i].u0 = static_cast<float>(i) / static_cast<float>(N);
    m_glyphMeta[i].u1 = static_cast<float>(i + 1) / static_cast<float>(N);

    // Copy slot pixels into atlas at horizontal offset i*slotW
    for (int row = 0; row < slotH; ++row) {
      const int atlasRow = row;
      const int atlasColBase = i * slotW;
      std::memcpy(
        atlasPixels.data() + static_cast<size_t>(atlasRow * slotW * N + atlasColBase),
        g.slotPixels.data() + static_cast<size_t>(row * slotW),
        static_cast<size_t>(slotW));
    }
  }

  // Store baked pixel data for testing/coverage computation
  m_slotPixels.resize(N);
  for (int i = 0; i < N; ++i)
    m_slotPixels[i] = bakedGlyphs[i].slotPixels;

  // GL upload
  glGenTextures(1, &m_texId);
  glBindTexture(GL_TEXTURE_2D, m_texId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, slotW * N, slotH, 0, GL_RED, GL_UNSIGNED_BYTE, atlasPixels.data());
  glBindTexture(GL_TEXTURE_2D, 0);

  m_glyphCount = N;
  m_glyphPx = glyphPx;
  m_slotPx = {slotW, slotH};
  return true;
}
