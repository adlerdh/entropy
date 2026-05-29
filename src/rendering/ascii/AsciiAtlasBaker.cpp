#include "AsciiAtlasBaker.h"

#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>

// Provide a local, file-scoped implementation of stb_truetype so that
// AsciiAtlasBaker does not inadvertently call the NanoVG-compiled copy of
// those functions. NanoVG overrides STBTT_malloc with fons__tmpalloc, which
// treats info->userdata as a FONScontext* — a contract we cannot satisfy here.
// STBTT_STATIC gives these functions internal linkage, avoiding duplicate-
// symbol conflicts with both NanoVG and ImGui compilation units.
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#define STBTT_malloc(sz, u) (static_cast<void>(u), std::malloc(sz))
#define STBTT_free(p, u) (static_cast<void>(u), std::free(p))
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include <imstb_truetype.h>
#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

float computeGlyphCoverage(
  const std::vector<uint8_t>& slotPixels,
  int slotW,
  int slotH,
  int padding,
  float pixDistScale,
  uint8_t onedgeValue,
  glm::vec2 cellSizePx)
{
  const float fSlotW = static_cast<float>(slotW);
  const float fSlotH = static_cast<float>(slotH);
  const float fPad = static_cast<float>(padding);

  // Match shader UV mapping exactly (including halfTexel inset)
  const float halfTexelX = 0.5f / fSlotW;
  const float halfTexelY = 0.5f / fSlotH;
  const float padFracX = fPad / fSlotW + halfTexelX;
  const float padFracY = fPad / fSlotH + halfTexelY;

  // Match shader soft-mode blend
  const float atlasTexelsPerScreenPx = fSlotH / cellSizePx.y;
  const float sdfPerScreenPx = (pixDistScale / 255.0f) * atlasTexelsPerScreenPx;
  const float soft = std::clamp((atlasTexelsPerScreenPx - 2.0f) * 0.5f, 0.0f, 1.0f);
  const float aaSharp = std::max(sdfPerScreenPx * 0.5f, 1.0f / 255.0f);
  const float aaSoft = 0.25f;
  const float aa = aaSharp + soft * (aaSoft - aaSharp); // mix()
  const float edge = static_cast<float>(onedgeValue) / 255.0f;
  const float lo = edge - aa;
  const float hi = edge + aa;

  // Sample grid: integer cell pixels
  const int sampleW = std::max(1, static_cast<int>(std::round(cellSizePx.x)));
  const int sampleH = std::max(1, static_cast<int>(std::round(cellSizePx.y)));

  float total = 0.0f;

  for (int sy = 0; sy < sampleH; ++sy) {
    const float uvCellY = (static_cast<float>(sy) + 0.5f) / static_cast<float>(sampleH);
    const float uvSlotY = padFracY + uvCellY * (1.0f - 2.0f * padFracY);
    const float slotPxYf = uvSlotY * fSlotH - 0.5f;
    const int r0 = std::clamp(static_cast<int>(slotPxYf), 0, slotH - 1);
    const int r1 = std::clamp(r0 + 1, 0, slotH - 1);
    const float fy = slotPxYf - static_cast<float>(r0);

    for (int sx = 0; sx < sampleW; ++sx) {
      const float uvCellX = (static_cast<float>(sx) + 0.5f) / static_cast<float>(sampleW);
      const float uvSlotX = padFracX + uvCellX * (1.0f - 2.0f * padFracX);
      const float slotPxXf = uvSlotX * fSlotW - 0.5f;
      const int c0 = std::clamp(static_cast<int>(slotPxXf), 0, slotW - 1);
      const int c1 = std::clamp(c0 + 1, 0, slotW - 1);
      const float fx = slotPxXf - static_cast<float>(c0);

      auto px = [&](int r, int c) -> float {
        return static_cast<float>(slotPixels[static_cast<size_t>(r * slotW + c)]) / 255.0f;
      };
      const float sdfVal = px(r0, c0) * (1.0f - fx) * (1.0f - fy) + px(r0, c1) * fx * (1.0f - fy) +
                           px(r1, c0) * (1.0f - fx) * fy + px(r1, c1) * fx * fy;

      // Smoothstep (same as shader)
      const float t = std::clamp((sdfVal - lo) / (hi - lo), 0.0f, 1.0f);
      total += t * t * (3.0f - 2.0f * t);
    }
  }

  return total / static_cast<float>(sampleW * sampleH);
}

std::vector<GlyphProfile> computeGlyphSpatialProfiles(
  const std::vector<std::vector<uint8_t>>& slotPixels,
  glm::ivec2 slotPx,
  int padding,
  float pixDistScale,
  uint8_t onedgeValue,
  glm::vec2 cellSizePx)
{
  const int slotW = slotPx.x;
  const int slotH = slotPx.y;
  const float fSlotW = static_cast<float>(slotW);
  const float fSlotH = static_cast<float>(slotH);
  const float fPad = static_cast<float>(padding);

  // Identical UV/AA setup as computeGlyphCoverage
  const float halfTexelX = 0.5f / fSlotW;
  const float halfTexelY = 0.5f / fSlotH;
  const float padFracX = fPad / fSlotW + halfTexelX;
  const float padFracY = fPad / fSlotH + halfTexelY;

  const float atlasTexelsPerScreenPx = fSlotH / cellSizePx.y;
  const float sdfPerScreenPx = (pixDistScale / 255.0f) * atlasTexelsPerScreenPx;
  const float soft = std::clamp((atlasTexelsPerScreenPx - 2.0f) * 0.5f, 0.0f, 1.0f);
  const float aaSharp = std::max(sdfPerScreenPx * 0.5f, 1.0f / 255.0f);
  const float aaSoft = 0.25f;
  const float aa = aaSharp + soft * (aaSoft - aaSharp);
  const float edge = static_cast<float>(onedgeValue) / 255.0f;
  const float lo = edge - aa;
  const float hi = edge + aa;

  const int sampleW = std::max(1, static_cast<int>(std::round(cellSizePx.x)));
  const int sampleH = std::max(1, static_cast<int>(std::round(cellSizePx.y)));

  const int N = static_cast<int>(slotPixels.size());
  std::vector<GlyphProfile> profiles(static_cast<size_t>(N));

  for (int gi = 0; gi < N; ++gi) {
    const auto& slot = slotPixels[static_cast<size_t>(gi)];

    std::array<float, 6> totals{};
    std::array<int, 6> counts{};

    for (int sy = 0; sy < sampleH; ++sy) {
      const float uvCellY = (static_cast<float>(sy) + 0.5f) / static_cast<float>(sampleH);
      const float uvSlotY = padFracY + uvCellY * (1.0f - 2.0f * padFracY);
      const float slotPxYf = uvSlotY * fSlotH - 0.5f;
      const int r0 = std::clamp(static_cast<int>(slotPxYf), 0, slotH - 1);
      const int r1 = std::clamp(r0 + 1, 0, slotH - 1);
      const float fy = slotPxYf - static_cast<float>(r0);

      for (int sx = 0; sx < sampleW; ++sx) {
        const float uvCellX = (static_cast<float>(sx) + 0.5f) / static_cast<float>(sampleW);
        const float uvSlotX = padFracX + uvCellX * (1.0f - 2.0f * padFracX);
        const float slotPxXf = uvSlotX * fSlotW - 0.5f;
        const int c0 = std::clamp(static_cast<int>(slotPxXf), 0, slotW - 1);
        const int c1 = std::clamp(c0 + 1, 0, slotW - 1);
        const float fx = slotPxXf - static_cast<float>(c0);

        auto px = [&](int r, int c) -> float {
          return static_cast<float>(slot[static_cast<size_t>(r * slotW + c)]) / 255.0f;
        };
        const float sdfVal = px(r0, c0) * (1.0f - fx) * (1.0f - fy) + px(r0, c1) * fx * (1.0f - fy) +
                             px(r1, c0) * (1.0f - fx) * fy + px(r1, c1) * fx * fy;

        const float t = std::clamp((sdfVal - lo) / (hi - lo), 0.0f, 1.0f);
        const float glyph = t * t * (3.0f - 2.0f * t);

        // 2-column × 3-row region layout:
        // col = 0 (left), 1 (right); row = 0 (top), 1 (mid), 2 (bot)
        // sy increases upward (GL convention: row 0 of slot = bottom)
        const int col = (sx < sampleW / 2) ? 0 : 1;
        const int row = (sy >= 2 * sampleH / 3) ? 0 : (sy >= sampleH / 3) ? 1 : 2;
        const int reg = row * 2 + col;
        totals[static_cast<size_t>(reg)] += glyph;
        counts[static_cast<size_t>(reg)] += 1;
      }
    }

    for (int k = 0; k < 6; ++k) {
      profiles[static_cast<size_t>(gi)].regionFill[static_cast<size_t>(k)] =
        (counts[static_cast<size_t>(k)] > 0)
          ? totals[static_cast<size_t>(k)] / static_cast<float>(counts[static_cast<size_t>(k)])
          : 0.0f;
    }
  }

  return profiles;
}

std::vector<int> buildCoverageRankOrder(const std::vector<float>& coverage)
{
  const int N = static_cast<int>(coverage.size());
  std::vector<int> order(static_cast<size_t>(N));
  std::iota(order.begin(), order.end(), 0);
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
    return coverage[static_cast<size_t>(a)] < coverage[static_cast<size_t>(b)];
  });
  return order;
}

std::vector<int> buildLumLut(const std::vector<float>& coverage)
{
  const int N = static_cast<int>(coverage.size());
  if (N == 0) return {};
  if (N == 1) return std::vector<int>(256, 0);
  if (N > 256) return std::vector<int>(256, N - 1); // unreachable: kMaxGlyphs=128

  const std::vector<int> order = buildCoverageRankOrder(coverage);

  std::vector<int> lut(256);
  for (int bin = 0; bin < 256; ++bin) {
    const int rank = static_cast<int>(std::round(static_cast<float>(bin) / 255.0f * static_cast<float>(N - 1)));
    lut[static_cast<size_t>(bin)] = order[rank];
  }
  return lut;
}

std::array<float, kSpatialK> computePerRegionMax(const std::vector<GlyphProfile>& profiles)
{
  std::array<float, kSpatialK> mx{};
  for (const auto& p : profiles)
    for (int i = 0; i < kSpatialK; i++)
      mx[static_cast<size_t>(i)] = std::max(mx[static_cast<size_t>(i)], p.regionFill[static_cast<size_t>(i)]);
  for (auto& v : mx)
    v = std::max(v, 1e-6f);
  return mx;
}

void normalizeGlyphProfilesInPlace(
  std::vector<GlyphProfile>& profiles,
  const std::array<float, kSpatialK>& perRegionMax)
{
  for (auto& p : profiles)
    for (int i = 0; i < kSpatialK; i++)
      p.regionFill[static_cast<size_t>(i)] /= perRegionMax[static_cast<size_t>(i)];
}

void shapeGlyphProfilesInPlace(std::vector<GlyphProfile>& profiles, float exponent)
{
  for (auto& p : profiles) {
    float localMax = *std::max_element(p.regionFill.begin(), p.regionFill.end());
    localMax = std::max(localMax, 1e-4f);
    for (auto& v : p.regionFill) {
      float norm = std::clamp(v / localMax, 0.0f, 1.0f);
      v = std::pow(norm, exponent) * localMax;
    }
  }
}

std::vector<BakedGlyph> bakeGlyphs(
  const uint8_t* ttfData,
  int ttfBytes,
  const std::string& charset,
  glm::ivec2 glyphPx,
  int padding,
  float pixDistScale,
  uint8_t onedgeValue)
{
  if (!ttfData || ttfBytes <= 0 || charset.empty()) return {};

  stbtt_fontinfo fontInfo{};
  if (!stbtt_InitFont(&fontInfo, ttfData, stbtt_GetFontOffsetForIndex(ttfData, 0))) return {};

  const int Gw = glyphPx.x;
  const int Gh = glyphPx.y;
  const int slotW = Gw + 2 * padding;
  const int slotH = Gh + 2 * padding;
  const float scale = stbtt_ScaleForPixelHeight(&fontInfo, static_cast<float>(Gh));

  // Font vertical metrics — for baseline alignment
  int ascent = 0, descent = 0, lineGap = 0;
  stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
  // Row (from top of slot, 0-indexed) where the baseline sits
  const int baselineRowFromTop = padding + static_cast<int>(std::round(static_cast<float>(ascent) * scale));

  std::vector<BakedGlyph> glyphs;
  glyphs.reserve(charset.size());

  for (char ch : charset) {
    const int codepoint = static_cast<unsigned char>(ch);

    // Fill fraction: use GetCodepointBitmap with cell area as denominator
    int bmpW = 0, bmpH = 0;
    uint8_t* bmp = stbtt_GetCodepointBitmap(&fontInfo, scale, scale, codepoint, &bmpW, &bmpH, nullptr, nullptr);
    int filled = 0;
    if (bmp) {
      for (int k = 0; k < bmpW * bmpH; ++k)
        if (bmp[k] > 127) ++filled;
      stbtt_FreeBitmap(bmp, nullptr);
    }
    const float fillFraction = (Gw * Gh > 0) ? static_cast<float>(filled) / static_cast<float>(Gw * Gh) : 0.0f;

    // SDF for rendering
    int sdfW = 0, sdfH = 0, xoff = 0, yoff = 0;
    uint8_t* sdf = stbtt_GetCodepointSDF(
      &fontInfo,
      scale,
      codepoint,
      padding,
      onedgeValue,
      pixDistScale,
      &sdfW,
      &sdfH,
      &xoff,
      &yoff);

    // Slot pixel buffer: initialize to 0 (fully outside SDF)
    std::vector<uint8_t> slotPixels(static_cast<size_t>(slotW * slotH), 0);

    if (sdf) {
      // Baseline-aligned placement:
      // xoff: horizontal offset from pen position to SDF top-left
      // yoff: vertical offset from baseline to SDF top (negative = above baseline)
      // dstCol: column in slot where SDF top-left lands
      // dstRowFromTop: row from top of slot where SDF top lands
      const int dstColTopLeft = padding + xoff;
      const int dstRowFromTop = baselineRowFromTop + yoff;

      for (int row = 0; row < sdfH; ++row) {
        const int slotRowFromTop = dstRowFromTop + row;
        // Row-flip: OpenGL UV.y=0 is bottom, so row 0 from top = slot bottom
        const int dstRow = slotH - 1 - slotRowFromTop;
        if (dstRow < 0 || dstRow >= slotH) continue;
        for (int col = 0; col < sdfW; ++col) {
          const int dstCol = dstColTopLeft + col;
          if (dstCol < 0 || dstCol >= slotW) continue;
          slotPixels[static_cast<size_t>(dstRow * slotW + dstCol)] = sdf[row * sdfW + col];
        }
      }
      stbtt_FreeSDF(sdf, nullptr);
    }

    glyphs.push_back(BakedGlyph{std::move(slotPixels), fillFraction, ch});
  }

  // Sort darkest (lowest fillFraction) to brightest
  std::stable_sort(glyphs.begin(), glyphs.end(), [](const BakedGlyph& a, const BakedGlyph& b) {
    return a.fillFraction < b.fillFraction;
  });

  return glyphs;
}
