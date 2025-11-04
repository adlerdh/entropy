#version 330 core

// Rendering modes:
#define IMAGE_RENDER_MODE 0
#define CHECKER_RENDER_MODE 1
#define QUADRANTS_RENDER_MODE 2
#define FLASHLIGHT_RENDER_MODE 3

in VS_OUT
{
  vec3 v_texCoord;
  vec3 v_voxCoord;
  vec2 v_checkerCoord;
  vec2 v_clipPos;
} fs_in;

layout (location = 0) out vec4 o_color; // output RGBA color (premultiplied alpha)

uniform usampler3D u_segTex; // segmentation (scalar, red channel only)
uniform samplerBuffer u_segLabelCmapTex; // seg label color map (non-premultiplied RGBA)

// Segmentation adjustment uniforms:
uniform float u_segOpacity; // overall seg opacity
uniform float u_segFillOpacity; // opacity of the seg interior

uniform vec3 u_texSamplingDirsForSegOutline[2]; // sampling dirs (horiz/vert) for outlining in texture space
uniform vec3 u_texSamplingDirsForSmoothSeg[2]; // sampling dirs (linear lookup only)
uniform float u_segInterpCutoff; // linear interpolation cut-off in [0.5, 1.0]

// View render mode uniforms:
uniform int u_renderMode; // mode (0: normal, 1: checkerboard, 2: quadrants, 3: flashlight)
uniform vec2 u_clipCrosshairs; // crosshairs position in Clip space

// Should quadrants comparison mode be done along the x, y directions?
// If x is true, then compare along x; if y is true, then compare along y.
// If both are true, then compare along both.
uniform bvec2 u_quadrants;
uniform bool u_showFix; // flag that the either the fixed (true) or moving image is shown
uniform float u_aspectRatio; // view aspect ratio (width / height)
uniform float u_flashlightRadius; // flashlight circle radius
uniform bool u_flashlightMovingOnFixed; // overlay moving on fixed image (true) or opposite (false)

{{HELPER_FUNCTIONS}}

/// float uintTextureLookup(sampler3D texture, vec3 texCoord);
{{UINT_TEXTURE_LOOKUP_FUNCTION}}

/// Look up segmentation texture label value (after mapping to GL texture units):
/// uint getSegValue(vec3 texOffset, out float opacity);
{{GET_SEG_VALUE_FUNCTION}}

/// Look up alpha of segmentation interior:
/// float getSegInteriorAlpha(uint seg)
{{GET_SEG_INTERIOR_ALPHA_FUNCTION}}

/// bool doRender(vec2 clipPos, vec2 checkerCoord);
{{DO_RENDER_FUNCTION}}

int when_lt(int x, int y) {
  return max(sign(y - x), 0);
}

int when_ge(int x, int y) {
  return (1 - when_lt(x, y));
}

/**
 * @brief Get the label color as premultiplied RGBA
 */
vec4 getLabelColor(int label)
{
  // Labels greater than the size of the segmentation labelc color texture are mapped to 0
  label -= label * when_ge(label, textureSize(u_segLabelCmapTex));

  vec4 color = texelFetch(u_segLabelCmapTex, label);
  return color.a * color;
}

void main()
{
  if (!doRender(fs_in.v_clipPos, fs_in.v_checkerCoord)) {
    discard;
  }

  float interpOpacity = 1.0;
  uint seg = getSegValue(vec3(0, 0, 0), interpOpacity);
  float mask = float(isInsideTexture(fs_in.v_texCoord));
  float alpha = u_segOpacity * interpOpacity * getSegInteriorAlpha(seg) * mask;

  // Output color (premult. RGBA)
  o_color = alpha * getLabelColor(int(seg));
}
