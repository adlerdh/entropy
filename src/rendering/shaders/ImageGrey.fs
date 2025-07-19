#version 330 core

// 3D texture coordinates (s,t,p) are in [0.0, 1.0]^3
#define MIN_IMAGE_TEXCOORD vec3(0.0)
#define MAX_IMAGE_TEXCOORD vec3(1.0)

// Rendering modes:
#define IMAGE_RENDER_MODE 0
#define CHECKER_RENDER_MODE 1
#define QUADRANTS_RENDER_MODE 2
#define FLASHLIGHT_RENDER_MODE 3

// Intensity Projection modes:
#define NO_IP_MODE 0
#define MAX_IP_MODE 1
#define MEAN_IP_MODE 2
#define MIN_IP_MODE 3

// Maximum number of isosurfaces:
#define NISO 16

// Redeclared vertex shader outputs, which are now the fragment shader inputs
in VS_OUT
{
  vec3 v_imgTexCoords;
  vec2 v_checkerCoord;
  vec2 v_clipPos;
} fs_in;

layout (location = 0) out vec4 o_color; // Output RGBA color (pre-multiplied alpha)

uniform sampler3D u_imgTex; // Texture unit 0: image (scalar)
uniform sampler1D u_imgCmapTex; // Texture unit 1: image color map (non-pre-multiplied RGBA)

// Slope and intercept for mapping texture intensity to normalized intensity,
// while also accounting for window/leveling
uniform vec2 u_imgSlopeIntercept;

uniform vec2 u_imgCmapSlopeIntercept; // Slopes and intercepts for the image color maps
uniform int u_imgCmapQuantLevels; // Number of image color map quantization levels
uniform vec3 u_imgCmapHsvModFactors; // HSV modification factors for image color
uniform bool u_useHsv; // Flag that HSV modification is used

uniform vec2 u_imgMinMax; // Min and max image values (in texture intenstiy units)
uniform vec2 u_imgThresholds; // Image lower and upper thresholds (in texture intensity units)

uniform float u_imgOpacity; // Image opacity

uniform vec2 u_clipCrosshairs; // Crosshairs in Clip space

// Should quadrants comparison mode be done along the x, y directions?
// If x is true, then compare along x; if y is true, then compare along y.
// If both are true, then compare along both.
uniform bvec2 u_quadrants;

// Should the fixed image be rendered (true) or the moving image (false).
uniform bool u_showFix;

// Render mode (0: normal, 1: checkerboard, 2: u_quadrants, 3: flashlight)
uniform int u_renderMode;

uniform float u_aspectRatio; // View aspect ratio (width / height)

uniform float u_flashlightRadius;

// When true, the flashlight overlays the moving image on top of fixed image.
// When false, the flashlight replaces the fixed image with the moving image.
uniform bool u_flashlightOverlays;

// Intensity Projection (MIP) mode (0: none, 1: Max, 2: Mean, 3: Min, 4: X-ray)
uniform int u_mipMode;

// Half the number of samples for MIP. Set to 0 when no projection is used.
uniform int u_halfNumMipSamples;

// Z view camera direction, represented in texture sampling space
uniform vec3 u_texSamplingDirZ;

uniform float u_isoValues[NISO]; // Isosurface values
uniform float u_isoOpacities[NISO]; // Isosurface opacities
uniform vec3 u_isoColors[NISO]; // Isosurface colors
uniform float u_isoWidth; // Width of isosurface

// OPTIONS:
// 1) Image interpolation: linear, cubic (per image setting)
// 2) Image projection: none, enabled (per image setting)
// 3) Segmentation interpolation: nn, linear (global setting)
// 4) Outlining: solid, outline (global setting)

/// Copied from https://www.laurivan.com/rgb-to-hsv-to-rgb-for-shaders/
vec3 rgb2hsv(vec3 c)
{
  vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
  vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
  vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

  float d = q.x - min(q.w, q.y);
  float e = 1.0e-10;
  return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c)
{
  vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
  return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float hardThreshold(float value, vec2 thresholds)
{
  return float(thresholds[0] <= value && value <= thresholds[1]);
}

// Remapping the unit interval into the unit interval by expanding the
// sides and compressing the center, and keeping 1/2 mapped to 1/2,
// that can be done with the gain() function. This was a common function
// in RSL tutorials (the Renderman Shading Language). k=1 is the identity curve,
// k<1 produces the classic gain() shape, and k>1 produces "s" shaped curces.
// The curves are symmetric (and inverse) for k=a and k=1/a.

//float gain(float x, float k)
//{
//  float a = 0.5*pow(2.0*((x<0.5)?x:1.0-x), k);
//  return (x<0.5)?a:1.0-a;
//}

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
  return (all(greaterThanEqual(a, MIN_IMAGE_TEXCOORD)) && all(lessThanEqual(a, MAX_IMAGE_TEXCOORD)));
}

// float textureLookup(sampler3D texture, vec3 texCoords);
{{TEXTURE_LOOKUP_FUNCTION}}

/// Compute min/mean/max projection. Returns img when MIP is not used (i.e. u_halfNumMipSamples == 0)
float computeProjection(float img)
{
  // Number of samples used for computing the final image value:
  int numSamples = 1;

  // Accumulate intensity projection in forwards (+Z) and backwards (-Z) directions:
  for (int i = 1; i <= u_halfNumMipSamples; ++i)
  {
    for (int dir = -1; dir <= 1; dir += 2)
    {
      vec3 c = fs_in.v_imgTexCoords + dir * i * u_texSamplingDirZ;
      if (!isInsideTexture(c)) break;

      float a = clamp(textureLookup(u_imgTex, c), u_imgMinMax[0], u_imgMinMax[1]);

      img = float(MAX_IP_MODE == u_mipMode) * max(img, a) +
            float(MEAN_IP_MODE == u_mipMode) * (img + a) +
            float(MIN_IP_MODE == u_mipMode) * min(img, a);

      ++numSamples;
    }
  }

  // If using Mean Intensity Projection mode, then normalize by the number of samples:
  return img / mix(1.0, float(numSamples), float(MEAN_IP_MODE == u_mipMode));
}

bool doRender()
{
  // Indicator of the quadrant of the crosshairs that the fragment is in:
  bvec2 Q = bvec2(fs_in.v_clipPos.x <= u_clipCrosshairs.x, fs_in.v_clipPos.y > u_clipCrosshairs.y);

  // Distance of the fragment from the crosshairs, accounting for aspect ratio:
  float flashlightDist = sqrt(pow(u_aspectRatio * (fs_in.v_clipPos.x - u_clipCrosshairs.x), 2.0) +
                              pow(fs_in.v_clipPos.y - u_clipCrosshairs.y, 2.0));

  bool render = (IMAGE_RENDER_MODE == u_renderMode); // Flag indicating whether the fragment will be rendere

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

  float img = clamp(textureLookup(u_imgTex, fs_in.v_imgTexCoords), u_imgMinMax[0], u_imgMinMax[1]);
  img = computeProjection(img);

  // Apply window/level and normalize image values to [0.0, 1.0] range:
  float imgNorm = clamp(u_imgSlopeIntercept[0] * img + u_imgSlopeIntercept[1], 0.0, 1.0);

  // Compute coords into the image color map, accounting for quantization levels:
  float cmapCoord = mix(floor(float(u_imgCmapQuantLevels) * imgNorm) / float(u_imgCmapQuantLevels - 1),
                        imgNorm, float(0 == u_imgCmapQuantLevels));
  cmapCoord = u_imgCmapSlopeIntercept[0] * cmapCoord + u_imgCmapSlopeIntercept[1]; // normalize coords

  vec4 imgColorOrig = texture(u_imgCmapTex, cmapCoord); // image color (non-pre-mult. RGBA)

  // Apply HSV modification factors:
  vec3 imgColorHsv = rgb2hsv(imgColorOrig.rgb);
  imgColorHsv.x += u_imgCmapHsvModFactors.x;
  imgColorHsv.yz *= u_imgCmapHsvModFactors.yz;

  // Conditionally use HSV modified colors
  float mask = float(isInsideTexture(fs_in.v_imgTexCoords)); // image mask based on texture coords
  float alpha = u_imgOpacity * mask * hardThreshold(img, u_imgThresholds); // alpha = opacity * mask * threshold
  vec4 imgLayer = alpha * imgColorOrig.a * vec4(mix(imgColorOrig.rgb, hsv2rgb(imgColorHsv), float(u_useHsv)), 1.0);

  vec4 isoLayer = vec4(0.0, 0.0, 0.0, 0.0);
  for (int i = 0; i < NISO; ++i) {
    vec4 color = mask * u_isoOpacities[i] * cubicPulse(u_isoValues[i], u_isoWidth, img) * vec4(u_isoColors[i], 1.0);
    isoLayer = color + (1.0 - color.a) * isoLayer;
  }

  // Blend all layers in order (1) image, (2) isosurfaces
  o_color = vec4(0.0, 0.0, 0.0, 0.0);
  o_color = imgLayer + (1.0 - imgLayer.a) * o_color;
  o_color = isoLayer + (1.0 - isoLayer.a) * o_color;
  //o_color.rgb = pow(o_color.rgb, vec3(1.8));
}
