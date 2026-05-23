#version 330 core

in vec2 v_uv;
out vec4 o_color;

uniform sampler2D u_cellMeanTex;
uniform sampler2D u_asciiAtlas;
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
uniform sampler2D u_asciiLumLut;

{{ASCII_COMPOSITE_FUNCTIONS}}

void main()
{
    if (u_asciiGlyphCount <= 0) { discard; }

    vec2 fragPx    = v_uv * u_viewSizePx;
    vec2 cellCoord = floor(fragPx / u_asciiCellSizePx);

    vec4 srcPM = texelFetch(u_cellMeanTex, ivec2(cellCoord), 0);
    if (srcPM.a < 0.001) { discard; }

    vec3 srcRgb = srcPM.rgb / max(srcPM.a, 1e-4);
    float lum   = dot(srcRgb, vec3(0.299, 0.587, 0.114));

    float lutNorm = texture(u_asciiLumLut, vec2(lum, 0.5)).r;
    int   gIdx    = clamp(int(lutNorm * float(u_asciiGlyphCount - 1) + 0.5),
                          0, u_asciiGlyphCount - 1);

    vec2 uvCell = (fragPx - cellCoord * u_asciiCellSizePx) / u_asciiCellSizePx;
    float glyph = sampleGlyphCoverage(u_asciiAtlas, gIdx, u_asciiGlyphCount,
                                      uvCell, u_asciiSlotSizePx,
                                      u_asciiSdfPadding, u_asciiPixDistScale,
                                      u_asciiCellSizePx);

    o_color = asciiComposite(glyph, srcPM,
                             u_asciiFgColor, u_asciiBgColor, u_asciiBgAlpha,
                             u_asciiUseColormap);
}
