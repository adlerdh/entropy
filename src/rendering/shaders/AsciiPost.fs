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

    vec2 fragPx      = v_uv * u_viewSizePx;
    vec2 cellCoord   = floor(fragPx / u_asciiCellSizePx);

    vec4 srcPM = texelFetch(u_cellMeanTex, ivec2(cellCoord), 0);
    if (srcPM.a < 0.001) { discard; }

    vec3 srcRgb = srcPM.rgb / max(srcPM.a, 1e-4);
    float lum   = dot(srcRgb, vec3(0.299, 0.587, 0.114));

    float lutNorm = texture(u_asciiLumLut, vec2(lum, 0.5)).r;
    int   gIdx    = clamp(int(lutNorm * float(u_asciiGlyphCount - 1) + 0.5),
                          0, u_asciiGlyphCount - 1);
    vec2 uvCell  = (fragPx - cellCoord * u_asciiCellSizePx) / u_asciiCellSizePx;
    // Map cell UV [0,1] -> slot UV, skipping SDF padding on each side
    // Half-texel inset prevents bilinear bleed across slot boundaries
    vec2 halfTexel = vec2(0.5) / u_asciiSlotSizePx;
    vec2 padFrac   = vec2(u_asciiSdfPadding) / u_asciiSlotSizePx + halfTexel;
    vec2 uvSlot    = padFrac + uvCell * (vec2(1.0) - 2.0 * padFrac);
    vec2 uvAtlas = vec2((float(gIdx) + uvSlot.x) / float(u_asciiGlyphCount), uvSlot.y);
    float sdf    = texture(u_asciiAtlas, uvAtlas).r;
    // One atlas-texel per screen pixel at current magnification:
    float atlasTexelsPerScreenPx = u_asciiSlotSizePx.y / u_asciiCellSizePx.y;
    // SDF changes by pixDistScale/255 per atlas texel:
    float sdfPerScreenPx = (u_asciiPixDistScale / 255.0) * atlasTexelsPerScreenPx;
    // Blend toward soft (wide) AA when atlas is heavily minified (small cells).
    // soft=0 below 2x minification; soft=1 at 4x+ minification.
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
