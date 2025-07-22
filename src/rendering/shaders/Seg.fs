#version 330 core

// 3D texture coordinates (s,t,p) are in [0.0, 1.0]^3
#define MIN_IMAGE_TEXCOORD vec3(0.0)
#define MAX_IMAGE_TEXCOORD vec3(1.0)

// Rendering modes:
#define IMAGE_RENDER_MODE 0
#define CHECKER_RENDER_MODE 1
#define QUADRANTS_RENDER_MODE 2
#define FLASHLIGHT_RENDER_MODE 3

// Redeclared vertex shader outputs, which are now the fragment shader inputs
in VS_OUT
{
  vec3 v_segTexCoords;
  vec3 v_segVoxCoords;
  vec2 v_checkerCoord;
  vec2 v_clipPos;
} fs_in;

layout (location = 0) out vec4 o_color; // Output RGBA color (pre-multiplied alpha)

uniform usampler3D u_segTex; // Texture unit 0: segmentation (scalar)
uniform samplerBuffer u_segLabelCmapTex; // Texutre unit 1: label color map (non-pre-multiplied RGBA)

uniform float u_segOpacity; // Segmentation opacity
uniform vec2 u_clipCrosshairs; // Crosshairs in Clip space
uniform float u_aspectRatio; // View aspect ratio (width / height)

// Should quadrants comparison mode be done along the x, y directions?
// If x is true, then compare along x; if y is true, then compare along y.
// If both are true, then compare along both.
uniform bvec2 u_quadrants;

// Should the fixed image be rendered (true) or the moving image (false)?
uniform bool u_showFix;

// Render mode (0: normal, 1: checkerboard, 2: u_quadrants, 3: flashlight)
uniform int u_renderMode;

// When true, the flashlight overlays the moving image on top of fixed image.
// When false, the flashlight replaces the fixed image with the moving image.
uniform bool u_flashlightOverlays;
uniform float u_flashlightRadius;

// Opacity of the interior of the segmentation
uniform float u_segInteriorOpacity;

// Texture sampling directions (horizontal and vertical) for calculating the seg outline
uniform vec3 u_texSamplingDirsForSegOutline[2];

// These are for linear only:

// For linear lookup:
uniform vec3 u_texSamplingDirsForSmoothSeg[2];

// Linear interpolation cut-off for segmentation (in [0, 1])
uniform float u_segInterpCutoff;

int when_lt(int x, int y)
{
  return max(sign(y - x), 0);
}

int when_ge(int x, int y)
{
  return (1 - when_lt(x, y));
}

// Remapping the unit interval into the unit interval by expanding the
// sides and compressing the center, and keeping 1/2 mapped to 1/2,
// that can be done with the gain() function. This was a common function
// in RSL tutorials (the Renderman Shading Language). k=1 is the identity curve,
// k<1 produces the classic gain() shape, and k>1 produces "s" shaped curces.
// The curves are symmetric (and inverse) for k=a and k=1/a.

float cubicPulse(float center, float width, float x)
{
  x = abs(x - center);
  if (x > width) return 0.0;
  x /= width;
  return 1.0 - x * x * (3.0 - 2.0 * x);
}

// Check if inside texture coordinates
bool isInsideTexture(vec3 a)
{
  return (all(greaterThanEqual(a, MIN_IMAGE_TEXCOORD)) &&
          all(lessThanEqual(a, MAX_IMAGE_TEXCOORD)));
}

vec4 computeLabelColor(int label)
{
  // Labels greater than the size of the segmentation labelc color texture are mapped to 0
  label -= label * when_ge(label, textureSize(u_segLabelCmapTex));

  vec4 color = texelFetch(u_segLabelCmapTex, label);
  return color.a * color; // pre-multiply by alpha
}

// float uintTextureLookup(sampler3D texture, vec3 texCoords);
{{UINT_TEXTURE_LOOKUP_FUNCTION}}

// Look up segmentation texture label value (after mapping to GL texture units):
// uint getSegValue(vec3 texOffset, out float opacity);
{{GET_SEG_VALUE_FUNCTION}}

// Look up alpha of segmentation interior:
// float getSegInteriorAlpha(uint seg)
{{GET_SEG_INTERIOR_ALPHA_FUNCTION}}

bool doRender()
{
  // Indicator of the quadrant of the crosshairs that the fragment is in:
  bvec2 Q = bvec2(fs_in.v_clipPos.x <= u_clipCrosshairs.x,
           fs_in.v_clipPos.y > u_clipCrosshairs.y);

  // Distance of the fragment from the crosshairs, accounting for aspect ratio:
  float flashlightDist = sqrt(
    pow(u_aspectRatio * (fs_in.v_clipPos.x - u_clipCrosshairs.x), 2.0) +
    pow(fs_in.v_clipPos.y - u_clipCrosshairs.y, 2.0));

  // Flag indicating whether the fragment will be rendered:
  bool render = (IMAGE_RENDER_MODE == u_renderMode);

  // If in Checkerboard mode, then render the fragment?
  render = render || ((CHECKER_RENDER_MODE == u_renderMode) &&
    (u_showFix == bool(mod(floor(fs_in.v_checkerCoord.x) +
                           floor(fs_in.v_checkerCoord.y), 2.0) > 0.5)));

  // If in Quadrants mode, then render the fragment?
  render = render || ((QUADRANTS_RENDER_MODE == u_renderMode) &&
    (u_showFix == ((!u_quadrants.x || Q.x) == (!u_quadrants.y || Q.y))));

  // If in Flashlight mode, then render the fragment?
  render = render || ((FLASHLIGHT_RENDER_MODE == u_renderMode) &&
    ((u_showFix == (flashlightDist > u_flashlightRadius)) ||
      (u_flashlightOverlays && u_showFix)));

  return render;
}

void main()
{
  if (!doRender()) discard;

  float segInterpOpacity = 1.0;
  uint seg = getSegValue(vec3(0, 0, 0), segInterpOpacity);

  // Segmentation mask based on texture coordinates:
  bool segMask = isInsideTexture(fs_in.v_segTexCoords);

  // Compute segmentation alpha based on opacity and mask:
  float segAlpha = u_segOpacity * segInterpOpacity * getSegInteriorAlpha(seg) * float(segMask);

  // Look up segmentation color:
  vec4 segLayerColor = computeLabelColor(int(seg)) * segAlpha;

  o_color = vec4(0.0, 0.0, 0.0, 0.0);
  o_color = segLayerColor + (1.0 - segLayerColor.a) * o_color;
}
