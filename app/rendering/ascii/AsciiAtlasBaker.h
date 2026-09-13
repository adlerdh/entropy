#pragma once
#include <glm/vec2.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief Number of spatial regions used to describe a glyph.
 *
 * Profiles divide a glyph cell into two columns and three rows. Regions are
 * stored in row-major order from top-left to bottom-right, using the OpenGL
 * convention that larger y coordinates are nearer the top:
 *
 * @code
 * 0 1
 * 2 3
 * 4 5
 * @endcode
 *
 * This convention must remain synchronized with AsciiCellRegions.fs and
 * AsciiPostSpatial.fs.
 */
constexpr int kSpatialK = 6;

/** @brief Mean rendered ink coverage within each spatial glyph region. */
struct GlyphProfile
{
  std::array<float, kSpatialK> regionFill; //!< Region coverage in top-left-to-bottom-right order
};

/**
 * @brief Compute spatial coverage profiles for a sequence of baked glyphs.
 *
 * Simulates the atlas shader sampling and antialiasing used by
 * computeGlyphCoverage(), then averages coverage independently in each 2x3
 * region.
 *
 * @param slotPixels R8 SDF pixels for each glyph slot.
 * @param slotPx Width and height, in texels, of every slot.
 * @param padding SDF padding on each side of a glyph cell, in texels.
 * @param pixDistScale Distance scale used when generating the SDF.
 * @param onedgeValue SDF sample value representing the glyph boundary.
 * @param cellSizePx Screen-space glyph cell size used by the simulation.
 * @return One profile per input slot, in the same order as slotPixels.
 */
std::vector<GlyphProfile> computeGlyphSpatialProfiles(
  const std::vector<std::vector<uint8_t>>& slotPixels,
  glm::ivec2 slotPx,
  int padding,
  float pixDistScale,
  uint8_t onedgeValue,
  glm::vec2 cellSizePx);

/** @brief CPU representation of one glyph baked into an SDF atlas slot. */
struct BakedGlyph
{
  std::vector<uint8_t> slotPixels; //!< R8 SDF pixels, with the bottom row first
  float fillFraction = 0.0f;       //!< Fraction of the unpadded glyph cell covered by the glyph
  char character = '\0';           //!< Source character represented by this slot
};

/**
 * @brief Simulate the rendered coverage of one baked glyph.
 *
 * Reproduces the atlas UV mapping, bilinear sampling, adaptive antialiasing,
 * and smoothstep evaluation performed by the ASCII post-processing shader.
 *
 * @param slotPixels R8 SDF pixels for one glyph slot.
 * @param slotW Slot width in texels.
 * @param slotH Slot height in texels.
 * @param padding SDF padding on each side of the glyph cell, in texels.
 * @param pixDistScale Distance scale used when generating the SDF.
 * @param onedgeValue SDF sample value representing the glyph boundary.
 * @param cellSizePx Screen-space glyph cell size used by the simulation.
 * @return Mean rendered coverage in the range [0, 1].
 */
float computeGlyphCoverage(
  const std::vector<uint8_t>& slotPixels,
  int slotW,
  int slotH,
  int padding,
  float pixDistScale,
  uint8_t onedgeValue,
  glm::vec2 cellSizePx);

/**
 * @brief Build a luminance-bin lookup table from glyph coverage values.
 *
 * Glyphs are ranked from lowest to highest coverage, and the 256 luminance
 * bins are distributed uniformly across those ranks.
 *
 * @param coverage Rendered coverage for each glyph.
 * @return A 256-entry table of glyph indices, or an empty vector when
 * coverage is empty.
 */
std::vector<int> buildLumLut(const std::vector<float>& coverage);

/**
 * @brief Rank glyph indices by ascending rendered coverage.
 *
 * Equal-coverage glyphs retain their input order.
 *
 * @param coverage Rendered coverage for each glyph.
 * @return Glyph indices ordered from darkest to densest.
 */
std::vector<int> buildCoverageRankOrder(const std::vector<float>& coverage);

/**
 * @brief Find the maximum coverage represented in each spatial region.
 * @param profiles Glyph profiles to inspect.
 * @return Per-region maxima, each clamped to at least 1e-6.
 */
std::array<float, kSpatialK> computePerRegionMax(const std::vector<GlyphProfile>& profiles);

/**
 * @brief Normalize every profile component by its corresponding region maximum.
 * @param profiles Glyph profiles modified in place.
 * @param perRegionMax Divisors for the spatial regions; values must be nonzero.
 */
void normalizeGlyphProfilesInPlace(
  std::vector<GlyphProfile>& profiles,
  const std::array<float, kSpatialK>& perRegionMax);

/**
 * @brief Apply power-curve contrast shaping while preserving each profile maximum.
 * @param profiles Glyph profiles modified in place.
 * @param exponent Power applied to each component after local normalization.
 */
void shapeGlyphProfilesInPlace(std::vector<GlyphProfile>& profiles, float exponent);

/**
 * @brief Bake characters from an in-memory TrueType font into SDF slots.
 *
 * This CPU-only operation rasterizes each requested character, computes its
 * fill fraction, and returns the glyphs sorted from lowest to highest fill.
 *
 * @param ttfData Pointer to the TrueType font bytes.
 * @param ttfBytes Number of bytes available at ttfData.
 * @param charset Characters to rasterize.
 * @param glyphPx Width and height of the unpadded glyph cell, in pixels.
 * @param padding SDF padding on each side of the glyph cell, in pixels.
 * @param pixDistScale Distance scale passed to the SDF rasterizer.
 * @param onedgeValue SDF value assigned to the glyph boundary.
 * @return Baked glyphs sorted by ascending fill fraction, or an empty vector
 * if the inputs are invalid or the font cannot be initialized.
 */
std::vector<BakedGlyph> bakeGlyphs(
  const uint8_t* ttfData,
  int ttfBytes,
  const std::string& charset,
  glm::ivec2 glyphPx,
  int padding,
  float pixDistScale,
  uint8_t onedgeValue);
