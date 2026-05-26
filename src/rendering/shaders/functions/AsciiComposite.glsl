// ---------------------------------------------------------------------------
// Shared SDF sampling, anti-aliasing, and colormap compositing for ASCII shaders.
// Injected via {{ASCII_COMPOSITE_FUNCTIONS}} placeholder.
// ---------------------------------------------------------------------------

vec3 rgb2hsv(vec3 c)
{
  vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
  vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
  vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
  float d = q.x - min(q.w, q.y);
  float e = 1e-10;
  return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c)
{
  vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
  return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// Sample the atlas SDF for glyph gIdx at cell UV uvCell,
// compute soft/sharp AA, and return the glyph ink coverage [0,1].
float sampleGlyphCoverage(
  sampler2D atlas,
  int gIdx,
  int glyphCount,
  vec2 uvCell,
  vec2 slotSizePx,
  float sdfPadding,
  float pixDistScale,
  vec2 cellSizePx)
{
  vec2 halfTexel = vec2(0.5) / slotSizePx;
  vec2 padFrac = vec2(sdfPadding) / slotSizePx + halfTexel;
  vec2 uvSlot = padFrac + uvCell * (vec2(1.0) - 2.0 * padFrac);
  vec2 uvAtlas = vec2((float(gIdx) + uvSlot.x) / float(glyphCount), uvSlot.y);
  float sdf = texture(atlas, uvAtlas).r;

  float atlasTexelsPerScreenPx = slotSizePx.y / cellSizePx.y;
  float sdfPerScreenPx = (pixDistScale / 255.0) * atlasTexelsPerScreenPx;
  float soft = clamp((atlasTexelsPerScreenPx - 2.0) * 0.5, 0.0, 1.0);
  float aaSharp = max(sdfPerScreenPx * 0.5, 1.0 / 255.0);
  float aaSoft = 0.25;
  float aa = mix(aaSharp, aaSoft, soft);
  float edge = 128.0 / 255.0;
  return smoothstep(edge - aa, edge + aa, sdf);
}

// Compute the final output color given ink coverage, source premultiplied color,
// foreground/background colors, and colormap flag.
vec4 asciiComposite(float glyph, vec4 srcPM, vec3 fgColor, vec3 bgColor, float bgAlpha, bool useColormap)
{
  vec3 srcRgb = srcPM.rgb / max(srcPM.a, 1e-4);
  vec3 fg;
  if (useColormap) {
    vec3 hsv = rgb2hsv(srcRgb);
    hsv.z = 1.0;
    fg = hsv2rgb(hsv);
  }
  else {
    fg = fgColor;
  }
  vec3 rgb = mix(bgColor, fg, glyph);
  float a = mix(bgAlpha, 1.0, glyph) * srcPM.a;
  return vec4(rgb * a, a);
}
