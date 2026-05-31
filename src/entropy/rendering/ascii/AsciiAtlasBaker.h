#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <glm/vec2.hpp>

// ---------------------------------------------------------------------------
// Spatial-glyph-matching constants
// ---------------------------------------------------------------------------
// kSpatialK == 6: the 2×3 region count (2 columns × 3 rows).
// Glyphs are taller than wide, so more vertical divisions give better discrimination.
//
// Region convention (must be consistent in baker, AsciiCellRegions.fs, and
// AsciiPostSpatial.fs):
//
//   Region 0: top-left    x < W/2,   y >= 2H/3  (GL y-up: top = high y)
//   Region 1: top-right   x >= W/2,  y >= 2H/3
//   Region 2: mid-left    x < W/2,   H/3 <= y < 2H/3
//   Region 3: mid-right   x >= W/2,  H/3 <= y < 2H/3
//   Region 4: bot-left    x < W/2,   y < H/3
//   Region 5: bot-right   x >= W/2,  y < H/3
//
//   col = (x < W/2) ? 0 : 1
//   row = (y >= 2H/3) ? 0 : (y >= H/3) ? 1 : 2
//   reg = row * 2 + col
// ---------------------------------------------------------------------------
constexpr int kSpatialK = 6;

struct GlyphProfile
{
  std::array<float, kSpatialK> regionFill; // mean SDF-rendered ink fraction per region
};

// Compute per-glyph 3x2 region profiles (same SDF+AA simulation as computeGlyphCoverage).
// Returns N profiles in the same sorted order as fillFractions().
std::vector<GlyphProfile> computeGlyphSpatialProfiles(
  const std::vector<std::vector<uint8_t>>& slotPixels,
  glm::ivec2 slotPx,
  int padding,
  float pixDistScale,
  uint8_t onedgeValue,
  glm::vec2 cellSizePx);

struct BakedGlyph
{
  std::vector<uint8_t> slotPixels; // slotW * slotH bytes, R8 SDF, row 0 = bottom (GL convention)
  float fillFraction;              // filledPixels / (Gw * Gh)
  char character;
};

/// CPU simulation of a single glyph's rendered coverage at a given screen cell size.
/// Replicates AsciiPost.fs UV + bilinear + softAA + smoothstep math exactly.
float computeGlyphCoverage(
  const std::vector<uint8_t>& slotPixels,
  int slotW,
  int slotH,
  int padding,
  float pixDistScale,
  uint8_t onedgeValue,
  glm::vec2 cellSizePx);

/// Build a 256-entry luminance-to-glyph LUT from per-glyph coverage values.
/// lut[b] = glyph index whose rendered coverage best matches luminance b/255,
/// where the luminance range [0,1] is mapped linearly onto [covMin, covMax].
/// Returns vector of size 256, values in [0, N-1].
std::vector<int> buildLumLut(const std::vector<float>& coverage);

// Returns the glyph indices sorted by coverage ascending (rank 0 = darkest, rank N-1 = densest).
// Input coverage is in the same order as fillFractions() / glyphProfiles.
std::vector<int> buildCoverageRankOrder(const std::vector<float>& coverage);

std::array<float, kSpatialK> computePerRegionMax(const std::vector<GlyphProfile>&);
void normalizeGlyphProfilesInPlace(std::vector<GlyphProfile>&, const std::array<float, kSpatialK>& perRegionMax);
void shapeGlyphProfilesInPlace(std::vector<GlyphProfile>& profiles, float exponent);

// Pure CPU function — no GL, no side effects.
// Returns glyphs in sorted order: darkest (lowest fillFraction) to brightest.
// ttfData: raw TTF bytes. charset: characters to include.
// glyphPx: {Gw, Gh} — base cell size in pixels.
// padding: SDF padding pixels on each side (= AsciiAtlas::kPadding = 4).
// pixDistScale: stbtt pixel_dist_scale (= 32.0f).
// onedgeValue: stbtt onedge_value (= 128).
// Returns empty vector on failure (font init failed or charset empty).
std::vector<BakedGlyph> bakeGlyphs(
  const uint8_t* ttfData,
  int ttfBytes,
  const std::string& charset,
  glm::ivec2 glyphPx,
  int padding,
  float pixDistScale,
  uint8_t onedgeValue);
