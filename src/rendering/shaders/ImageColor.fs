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
  vec3 v_imgTexCoords;
  vec2 v_checkerCoord;
  vec2 v_clipPos;
} fs_in;

layout (location = 0) out vec4 o_color; // Output RGBA color (pre-multiplied alpha)

uniform sampler3D u_imgTex[4]; // Texture units 0, 1, 2, 3: Red, green, blue, alpha images

// Slope and intercept for mapping texture intensity to normalized intensity, accounting for window-leveling
uniform vec2 u_imgSlopeIntercept[4];

// Should alpha be forced to 1? Set to true for 3-component images, where no alpha is provided.
uniform bool u_alphaIsOne;

uniform float u_imgOpacity[4]; // Image opacities
uniform vec2 u_imgMinMax[4]; // Min and max image values
uniform vec2 u_imgThresholds[4]; // Image lower and upper thresholds, mapped to OpenGL texture intensity

uniform vec2 u_clipCrosshairs; // Crosshairs in Clip space

// Should comparison be done in x,y directions?
// If x is true, then compare along x; if y is true, then compare along y;
// if both are true, then compare along both.
uniform bvec2 u_quadrants;

// Should the fixed image be rendered (true) or the moving image (false):
uniform bool u_showFix;

// Render mode (0: normal, 1: checkerboard, 2: u_quadrants, 3: flashlight)
uniform int u_renderMode;

uniform float u_aspectRatio;

uniform float u_flashlightRadius;

// When true, the flashlight overlays the moving image on top of fixed image.
// When false, the flashlight replaces the fixed image with the moving image.
uniform bool u_flashlightOverlays;

float hardThreshold(float value, vec2 thresholds)
{
  return float(thresholds[0] <= value && value <= thresholds[1]);
}

// Check if inside texture coordinates
bool isInsideTexture(vec3 a)
{
  return (all(greaterThanEqual(a, MIN_IMAGE_TEXCOORD)) && all(lessThanEqual(a, MAX_IMAGE_TEXCOORD)));
}

// float textureLookup(sampler3D texture, vec3 texCoords);
{{TEXTURE_LOOKUP_FUNCTION}}

bool doRender()
{
  // Indicator of the quadrant of the crosshairs that the fragment is in:
  bvec2 Q = bvec2(fs_in.v_clipPos.x <= u_clipCrosshairs.x, fs_in.v_clipPos.y > u_clipCrosshairs.y);

  // Distance of the fragment from the crosshairs, accounting for aspect ratio:
  float flashlightDist = sqrt(pow(u_aspectRatio * (fs_in.v_clipPos.x - u_clipCrosshairs.x), 2.0) +
                              pow(fs_in.v_clipPos.y - u_clipCrosshairs.y, 2.0));

  // Flag indicating whether the fragment will be rendered:
  bool render = (IMAGE_RENDER_MODE == u_renderMode);

  // If in Checkerboard/Quadrants/Flashlight mode, then render the fragment?
  render = render || ((CHECKER_RENDER_MODE == u_renderMode) &&
    (u_showFix == bool(mod(floor(fs_in.v_checkerCoord.x) + floor(fs_in.v_checkerCoord.y), 2.0) > 0.5)));

  render = render || ((QUADRANTS_RENDER_MODE == u_renderMode) &&
    (u_showFix == ((! u_quadrants.x || Q.x) == (! u_quadrants.y || Q.y))));

  render = render || ((FLASHLIGHT_RENDER_MODE == u_renderMode) &&
    ((u_showFix == (flashlightDist > u_flashlightRadius)) || (u_flashlightOverlays && u_showFix)));

  return render;
}

void main()
{
  if (!doRender()) discard;

  // Look up the image values (after mapping to GL texture units):
  vec4 img = vec4(
    clamp(textureLookup(u_imgTex[0], fs_in.v_imgTexCoords), u_imgMinMax[0][0], u_imgMinMax[0][1]),
    clamp(textureLookup(u_imgTex[1], fs_in.v_imgTexCoords), u_imgMinMax[1][0], u_imgMinMax[1][1]),
    clamp(textureLookup(u_imgTex[2], fs_in.v_imgTexCoords), u_imgMinMax[2][0], u_imgMinMax[2][1]),
    clamp(textureLookup(u_imgTex[3], fs_in.v_imgTexCoords), u_imgMinMax[3][0], u_imgMinMax[3][1]));

  float forcedOpaque = float(u_alphaIsOne);

  float thresh[4];
  for (int i = 0; i <= 3; ++i) {
    thresh[i] = hardThreshold(img[i], u_imgThresholds[i]);
  }
  thresh[3] = mix(thresh[3], 1.0, forcedOpaque);

  // Apply window/level to normalize image values in [0.0, 1.0] range:
  vec4 imgNorm = vec4(0.0, 0.0, 0.0, 0.0);
  for (int i = 0; i <= 3; ++i) {
    imgNorm[i] = clamp(u_imgSlopeIntercept[i][0] * img[i] + u_imgSlopeIntercept[i][1], 0.0, 1.0);
  }
  imgNorm.a = mix(imgNorm.a, 1.0, forcedOpaque);

  // Apply opacity to each component:
  float mask = float(isInsideTexture(fs_in.v_imgTexCoords));
  for (int i = 0; i <= 3; ++i) {
    imgNorm[i] *= u_imgOpacity[i] * thresh[i] * mask;
  }

  vec4 imgLayer = imgNorm.a * vec4(imgNorm.rgb, 1.0);

  o_color = vec4(0.0, 0.0, 0.0, 0.0);
  o_color = imgLayer + (1.0 - imgLayer.a) * o_color;
}
