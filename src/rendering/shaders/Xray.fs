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

uniform sampler3D u_imgTex; // Texture unit 0: image
uniform sampler1D u_imgCmapTex; // Texture unit 1: image color map (pre-mult RGBA)

// Slope for mapping texture intensity to native image intensity, NOT accounting for window-leveling
uniform float imgSlope_native_T_texture;

uniform vec2 u_imgMinMax; // Min and max image values

// Low and high image thresholds, expressed in image texture values
uniform vec2 u_imgThresholds;

// Slope and intercept for window-leveling the attenuation value
uniform vec2 slopeInterceptWindowLevel;

uniform vec2 u_imgCmapSlopeIntercept; // Slopes and intercepts for the image color maps
uniform int u_imgCmapQuantLevels; // Number of image color map quantization levels
uniform vec3 u_imgCmapHsvModFactors; // HSV modification factors for image color
uniform bool u_useHsv; // Flag that HSV modification is used

uniform float u_imgOpacity; // Image opacities

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

// Half the number of samples for MIP. Set to 0 when no projection is used.
uniform int u_halfNumMipSamples;

// Sampling distance in centimeters (used for X-ray IP mode)
uniform float mipSamplingDistance_cm;

// Z view camera direction, represented in texture sampling space
uniform vec3 u_texSamplingDirZ;

// Photon mass attenuation coefficients [1/cm] of liquid water and dry air (at sea level):
uniform float waterAttenCoeff;
uniform float airAttenCoeff;

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

// Check if inside texture coordinates
bool isInsideTexture(vec3 a)
{
  return (all(greaterThanEqual(a, MIN_IMAGE_TEXCOORD)) &&
          all(lessThanEqual(a, MAX_IMAGE_TEXCOORD)));
}

// float textureLookup(sampler3D texture, vec3 texCoords);
{{TEXTURE_LOOKUP_FUNCTION}}

// Convert texture intensity to Hounsefield Units, then to Photon Mass Attenuation coefficient
float convertTexToAtten(float texValue)
{
  float hu = imgSlope_native_T_texture * texValue; // Hounsefield units

  // Photon mass attenuation coefficient:
  return max((hu / 1000.0) * (waterAttenCoeff - airAttenCoeff) + waterAttenCoeff, 0.0);
}

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
    (u_showFix == ((! u_quadrants.x || Q.x) == (! u_quadrants.y || Q.y))));

  // If in Flashlight mode, then render the fragment?
  render = render || ((FLASHLIGHT_RENDER_MODE == u_renderMode) &&
    ((u_showFix == (flashlightDist > u_flashlightRadius)) ||
             (u_flashlightOverlays && u_showFix)));

  return render;
}

float hardThreshold(float value, vec2 thresholds)
{
  return float(thresholds[0] <= value && value <= thresholds[1]);
}

void main()
{
  if (!doRender()) discard;

  // Look up the texture values and convert to mass attenuation coefficient.
  // Keep a running sum of attenuation for all samples.
  float texValue = clamp(textureLookup(u_imgTex, fs_in.v_imgTexCoords), u_imgMinMax[0], u_imgMinMax[1]);
  float thresh = hardThreshold(texValue, u_imgThresholds);
  float totalAtten = thresh * convertTexToAtten(texValue);

  // Accumulate intensity projection in forwards (+Z) direction:
  for (int i = 1; i <= u_halfNumMipSamples; ++i)
  {
    vec3 tc = fs_in.v_imgTexCoords + i * u_texSamplingDirZ;
    if (!isInsideTexture(tc)) break;

    texValue = clamp(textureLookup(u_imgTex, tc), u_imgMinMax[0], u_imgMinMax[1]);
    thresh = hardThreshold(texValue, u_imgThresholds);
    totalAtten += thresh * convertTexToAtten(texValue);
  }

  // Accumulate intensity projection in backwards (-Z) direction:
  for (int i = 1; i <= u_halfNumMipSamples; ++i)
  {
    vec3 tc = fs_in.v_imgTexCoords - i * u_texSamplingDirZ;
    if (!isInsideTexture(tc)) break;

    texValue = clamp(textureLookup(u_imgTex, tc), u_imgMinMax[0], u_imgMinMax[1]);
    thresh = hardThreshold(texValue, u_imgThresholds);
    totalAtten += thresh * convertTexToAtten(texValue);
  }

  // Compute inverse of the total photon attenuation, which is in range [0.0, 1):
  float invAtten = 1.0 - exp(-totalAtten * mipSamplingDistance_cm);

  // Apply window-leveling:
  float invAttenWL = slopeInterceptWindowLevel[0] * invAtten + slopeInterceptWindowLevel[1];

  // Compute coords into the image color map, accounting for quantization levels:
  float cmapCoord = mix(floor(float(u_imgCmapQuantLevels) * invAttenWL) / float(u_imgCmapQuantLevels - 1),
                        invAttenWL, float(0 == u_imgCmapQuantLevels));
  cmapCoord = u_imgCmapSlopeIntercept[0] * cmapCoord + u_imgCmapSlopeIntercept[1]; // normalize coords

  vec4 imgColorOrig = texture(u_imgCmapTex, cmapCoord); // image color (non-pre-mult. RGBA)

  // Apply HSV modification factors:
  vec3 imgColorHsv = rgb2hsv(imgColorOrig.rgb);
  imgColorHsv.x += u_imgCmapHsvModFactors.x;
  imgColorHsv.yz *= u_imgCmapHsvModFactors.yz;

  // Conditionally use HSV modified colors
  float mask = float(isInsideTexture(fs_in.v_imgTexCoords)); // image mask based on texture coords
  float alpha = u_imgOpacity * mask; // alpha = opacity * mask
  vec4 imgLayer = alpha * imgColorOrig.a * vec4(mix(imgColorOrig.rgb, hsv2rgb(imgColorHsv), float(u_useHsv)), 1.0);

  o_color = vec4(0.0, 0.0, 0.0, 0.0);
  o_color = imgLayer + (1.0 - imgLayer.a) * o_color;
}
