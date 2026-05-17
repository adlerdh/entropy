#version 330 core

in vec2 v_uv;
out vec4 o_color;

// Spatial glyph matching mode: uses per-cell 3×2 region luminance profiles
// and per-glyph region profiles, combined with cell-mean for colormap/alpha.
//
// Region convention (matches AsciiAtlasBaker.h and AsciiCellRegions.fs):
// 3 columns × 2 rows layout:
//   Region 0: top-left      (x < W/3,        y >= H/2 in GL y-up)
//   Region 1: top-center    (W/3 <= x < 2W/3, y >= H/2)
//   Region 2: top-right     (x >= 2W/3,      y >= H/2)
//   Region 3: bottom-left   (x < W/3,        y < H/2)
//   Region 4: bottom-center (W/3 <= x < 2W/3, y < H/2)
//   Region 5: bottom-right  (x >= 2W/3,      y < H/2)

uniform sampler2D u_cellRegionsTex;   // regions 0–3 in RGBA
uniform sampler2D u_cellRegionsTexB;  // regions 4–5 in RG
uniform sampler2D u_cellMeanTex;
uniform sampler2D u_asciiAtlas;
uniform sampler2D u_asciiLumLut;
uniform vec2  u_viewSizePx;
uniform vec2  u_asciiCellSizePx;
uniform vec2  u_asciiSlotSizePx;
uniform int   u_asciiGlyphCount;
uniform vec3  u_asciiFgColor;
uniform vec3  u_asciiBgColor;
uniform float u_asciiBgAlpha;
uniform bool  u_asciiUseColormap;
uniform float u_asciiSdfPadding;
uniform float u_asciiPixDistScale;
uniform float u_asciiSpatialDensityWindow;
uniform vec4 u_regionMaxA;
uniform vec2 u_regionMaxB;
uniform float u_asciiSpatialExponent;

uniform vec4 u_glyphProfilesA[128];   // regions 0–3 per glyph
uniform vec4 u_glyphProfilesB[128];   // regions 4–5 in .xy, .zw unused
uniform int  u_glyphRankToIndex[128]; // rank -> glyph index


vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0/3.0, 2.0/3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0*d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main()
{
    if (u_asciiGlyphCount <= 0) { discard; }

    vec2 fragPx    = v_uv * u_viewSizePx;
    vec2 cellCoord = floor(fragPx / u_asciiCellSizePx);

    // Cell mean for alpha discard and colormap
    vec4 srcPM = texelFetch(u_cellMeanTex, ivec2(cellCoord), 0);
    if (srcPM.a < 0.001) { discard; }

    vec3 srcRgb = srcPM.rgb / max(srcPM.a, 1e-4);

    // Fetch per-cell region luminances (6 regions across two textures)
    vec4 cellProfileA = texelFetch(u_cellRegionsTex,  ivec2(cellCoord), 0);
    vec2 cellProfileB = texelFetch(u_cellRegionsTexB, ivec2(cellCoord), 0).xy;
    cellProfileA = clamp(cellProfileA, 0.0, 1.0);
    cellProfileB = clamp(cellProfileB, 0.0, 1.0);

    cellProfileA /= max(u_regionMaxA, vec4(1e-6));
    cellProfileB /= max(u_regionMaxB, vec2(1e-6));

    {
        float localMax = max(max(max(cellProfileA.x, cellProfileA.y),
                                 max(cellProfileA.z, cellProfileA.w)),
                             max(cellProfileB.x, cellProfileB.y));
        localMax = max(localMax, 1e-4);
        cellProfileA = pow(clamp(cellProfileA / localMax, vec4(0.0), vec4(1.0)), vec4(u_asciiSpatialExponent)) * localMax;
        cellProfileB = pow(clamp(cellProfileB / localMax, vec2(0.0), vec2(1.0)), vec2(u_asciiSpatialExponent)) * localMax;
    }

    // Luminance LUT gives density rank for this cell
    float lum      = dot(srcRgb, vec3(0.299, 0.587, 0.114));
    float lutNorm  = texture(u_asciiLumLut, vec2(lum, 0.5)).r;
    int   baseRank = clamp(int(lutNorm * float(u_asciiGlyphCount - 1) + 0.5), 0, u_asciiGlyphCount - 1);
    int   W        = int(u_asciiSpatialDensityWindow);
    int   rLo      = max(0, baseRank - W);
    int   rHi      = min(u_asciiGlyphCount - 1, baseRank + W);

    // Density-windowed argmin: search only glyphs near expected density rank
    int   gIdx  = u_glyphRankToIndex[baseRank];  // fallback = LUT selection
    float bestD = 1.0e30;
    for (int r = rLo; r <= rHi; ++r) {
        int  g  = u_glyphRankToIndex[r];
        vec4 dA = cellProfileA - u_glyphProfilesA[g];
        vec2 dB = cellProfileB - u_glyphProfilesB[g].xy;
        float d2 = dot(dA, dA) + dot(dB, dB);
        if (d2 < bestD) { bestD = d2; gIdx = g; }
    }

    vec2 uvCell  = (fragPx - cellCoord * u_asciiCellSizePx) / u_asciiCellSizePx;
    // Map cell UV [0,1] -> slot UV, skipping SDF padding on each side
    vec2 halfTexel = vec2(0.5) / u_asciiSlotSizePx;
    vec2 padFrac   = vec2(u_asciiSdfPadding) / u_asciiSlotSizePx + halfTexel;
    vec2 uvSlot    = padFrac + uvCell * (vec2(1.0) - 2.0 * padFrac);
    vec2 uvAtlas   = vec2((float(gIdx) + uvSlot.x) / float(u_asciiGlyphCount), uvSlot.y);
    float sdf      = texture(u_asciiAtlas, uvAtlas).r;

    float atlasTexelsPerScreenPx = u_asciiSlotSizePx.y / u_asciiCellSizePx.y;
    float sdfPerScreenPx = (u_asciiPixDistScale / 255.0) * atlasTexelsPerScreenPx;
    float soft    = clamp((atlasTexelsPerScreenPx - 2.0) * 0.5, 0.0, 1.0);
    float aaSharp = max(sdfPerScreenPx * 0.5, 1.0 / 255.0);
    float aaSoft  = 0.25;
    float aa      = mix(aaSharp, aaSoft, soft);
    float edge    = 128.0 / 255.0;
    float glyph   = smoothstep(edge - aa, edge + aa, sdf);

    vec3 fg;
    if (u_asciiUseColormap) {
        vec3 hsv = rgb2hsv(srcRgb);
        hsv.z = 1.0;
        fg = hsv2rgb(hsv);
    } else {
        fg = u_asciiFgColor;
    }
    vec3 rgb = mix(u_asciiBgColor, fg, glyph);
    float a  = mix(u_asciiBgAlpha, 1.0, glyph) * srcPM.a;
    o_color  = vec4(rgb * a, a);
}
