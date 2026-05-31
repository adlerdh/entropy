#version 330 core

in vec2 v_uv;
out vec4 o_color;

// Spatial glyph matching mode: uses per-cell 2×3 region luminance profiles
// and per-glyph region profiles, combined with cell-mean for colormap/alpha.
//
// Region convention (matches AsciiAtlasBaker.h and AsciiCellRegions.fs):
// 2 columns × 3 rows layout (glyphs are taller than wide):
//   Region 0: top-left    (x < W/2,   y >= 2H/3 in GL y-up)
//   Region 1: top-right   (x >= W/2,  y >= 2H/3)
//   Region 2: mid-left    (x < W/2,   H/3 <= y < 2H/3)
//   Region 3: mid-right   (x >= W/2,  H/3 <= y < 2H/3)
//   Region 4: bot-left    (x < W/2,   y < H/3)
//   Region 5: bot-right   (x >= W/2,  y < H/3)

uniform sampler2D u_cellRegionsTex;  // regions 0–3 in RGBA
uniform sampler2D u_cellRegionsTexB; // regions 4–5 in RG
uniform sampler2D u_cellMeanTex;
uniform sampler2D u_asciiAtlas;
uniform sampler2D u_asciiLumLut;
uniform vec2 u_viewSizePx;
uniform vec2 u_asciiCellSizePx;
uniform vec2 u_asciiSlotSizePx;
uniform int u_asciiGlyphCount;
uniform vec3 u_asciiFgColor;
uniform vec3 u_asciiBgColor;
uniform float u_asciiBgAlpha;
uniform bool u_asciiUseColormap;
uniform float u_asciiSdfPadding;
uniform float u_asciiPixDistScale;
uniform float u_asciiSpatialDensityWindow;
uniform vec4 u_regionMaxA;
uniform vec2 u_regionMaxB;
uniform float u_asciiSpatialExponent;

uniform vec4 u_glyphProfilesA[128];  // regions 0–3 per glyph
uniform vec4 u_glyphProfilesB[128];  // regions 4–5 in .xy, .zw unused
uniform int u_glyphRankToIndex[128]; // rank -> glyph index

$$ASCII_COMPOSITE_FUNCTIONS$$

void main()
{
  if (u_asciiGlyphCount <= 0) {
    discard;
  }

  vec2 fragPx = v_uv * u_viewSizePx;
  vec2 cellCoord = floor(fragPx / u_asciiCellSizePx);

  // Cell mean for alpha discard and colormap
  vec4 srcPM = texelFetch(u_cellMeanTex, ivec2(cellCoord), 0);
  if (srcPM.a < 0.001) {
    discard;
  }

  vec3 srcRgb = srcPM.rgb / max(srcPM.a, 1e-4);

  // Fetch per-cell region luminances (6 regions across two textures)
  vec4 cellProfileA = texelFetch(u_cellRegionsTex, ivec2(cellCoord), 0);
  vec2 cellProfileB = texelFetch(u_cellRegionsTexB, ivec2(cellCoord), 0).xy;
  cellProfileA = clamp(cellProfileA, 0.0, 1.0);
  cellProfileB = clamp(cellProfileB, 0.0, 1.0);

  cellProfileA /= max(u_regionMaxA, vec4(1e-6));
  cellProfileB /= max(u_regionMaxB, vec2(1e-6));

  {
    float localMax = max(
      max(max(cellProfileA.x, cellProfileA.y), max(cellProfileA.z, cellProfileA.w)),
      max(cellProfileB.x, cellProfileB.y));
    localMax = max(localMax, 1e-4);
    cellProfileA = pow(clamp(cellProfileA / localMax, vec4(0.0), vec4(1.0)), vec4(u_asciiSpatialExponent)) * localMax;
    cellProfileB = pow(clamp(cellProfileB / localMax, vec2(0.0), vec2(1.0)), vec2(u_asciiSpatialExponent)) * localMax;
  }

  // Luminance LUT gives density rank for this cell
  float lum = dot(srcRgb, vec3(0.299, 0.587, 0.114));
  float lutNorm = texture(u_asciiLumLut, vec2(lum, 0.5)).r;
  int baseRank = clamp(int(lutNorm * float(u_asciiGlyphCount - 1) + 0.5), 0, u_asciiGlyphCount - 1);
  int W = int(u_asciiSpatialDensityWindow);
  int rLo = max(0, baseRank - W);
  int rHi = min(u_asciiGlyphCount - 1, baseRank + W);

  // Density-windowed argmin: search only glyphs near expected density rank
  int gIdx = u_glyphRankToIndex[baseRank]; // fallback = LUT selection
  float bestD = 1.0e30;
  for (int r = rLo; r <= rHi; ++r) {
    int g = u_glyphRankToIndex[r];
    vec4 dA = cellProfileA - u_glyphProfilesA[g];
    vec2 dB = cellProfileB - u_glyphProfilesB[g].xy;
    float d2 = dot(dA, dA) + dot(dB, dB);
    if (d2 < bestD) {
      bestD = d2;
      gIdx = g;
    }
  }

  vec2 uvCell = (fragPx - cellCoord * u_asciiCellSizePx) / u_asciiCellSizePx;
  float glyph = sampleGlyphCoverage(
    u_asciiAtlas,
    gIdx,
    u_asciiGlyphCount,
    uvCell,
    u_asciiSlotSizePx,
    u_asciiSdfPadding,
    u_asciiPixDistScale,
    u_asciiCellSizePx);

  o_color = asciiComposite(glyph, srcPM, u_asciiFgColor, u_asciiBgColor, u_asciiBgAlpha, u_asciiUseColormap);
}
