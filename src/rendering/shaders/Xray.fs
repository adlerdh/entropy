#version 330 core

// 3D texture coordinates (s,t,p) are in [0.0, 1.0]^3
#define MIN_IMAGE_TEXCOORD vec3(0.0)
#define MAX_IMAGE_TEXCOORD vec3(1.0)

// Rendering modes:
#define IMAGE_RENDER_MODE 0
#define CHECKER_RENDER_MODE 1
#define QUADRANTS_RENDER_MODE 2
#define FLASHLIGHT_RENDER_MODE 3

in VS_OUT
{
  vec3 v_texCoord;
  vec2 v_checkerCoord;
  vec2 v_clipPos;
} fs_in;

layout (location = 0) out vec4 o_color; // output RGBA color (premultiplied alpha RGBA)

// Texture samplers:
uniform sampler3D u_imgTex; // image (scalar, red channel only)
uniform sampler1D u_cmapTex; // image color map (non-premultiplied RGBA)

// Image adjustment uniforms:
uniform float imgSlope_native_T_texture; // slope to map texture intensity to native image intensity (NOT accounting for window-leveling)
uniform vec2 slopeInterceptWindowLevel; // slope/intercept for window-leveling the attenuation value
uniform vec2 u_imgMinMax; // min/max image values (texture intenstiy units)
uniform vec2 u_imgThresholds; // lower/upper image thresholds (texture intensity units)
uniform float u_imgOpacity; // image opacity

// Image color map adjustment uniforms:
uniform vec2 u_cmapSlopeIntercept; // map texels to normalized range [0, 1]
uniform int u_cmapQuantLevels; // number of color map quantization levels
uniform vec3 u_cmapHsvModFactors; // HSV modification factors for color map
uniform bool u_applyHsvMod; // flag that HSV modification is applied

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

// Intensiy Projection mode uniforms:
uniform int u_mipMode; // MIP mode (0: none, 1: max, 2: mean, 3: min, 4: X-ray)
uniform int u_halfNumMipSamples; // half number of MIP samples (0 when no projection used)
uniform vec3 u_texSamplingDirZ; // Z view camera direction (in texture sampling space)

// Sampling distance in centimeters (used for X-ray IP mode)
uniform float mipSamplingDistance_cm;

// Z view camera direction, represented in texture sampling space
//uniform vec3 u_texSamplingDirZ;

// Photon mass attenuation coefficients [1/cm] of liquid water and dry air (at sea level):
uniform float waterAttenCoeff;
uniform float airAttenCoeff;

/// float textureLookup(sampler3D texture, vec3 texCoord);
{{TEXTURE_LOOKUP_FUNCTION}}

/**
 * @brief Check if coordinates are inside the image texture
 */
bool isInsideTexture(vec3 texCoord)
{
  return (all(greaterThanEqual(texCoord, MIN_IMAGE_TEXCOORD)) &&
          all(lessThanEqual(texCoord, MAX_IMAGE_TEXCOORD)));
}

/**
 * @brief Convert RGB to HSV color representation
 * @cite https://www.laurivan.com/rgb-to-hsv-to-rgb-for-shaders/
 */
vec3 rgb2hsv(vec3 c)
{
  vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
  vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
  vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

  float d = q.x - min(q.w, q.y);
  float e = 1.0e-10;
  return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

/**
 * @brief Convert HSV to RGB color representation
 * @cite https://www.laurivan.com/rgb-to-hsv-to-rgb-for-shaders/
 */
vec3 hsv2rgb(vec3 c)
{
  vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
  return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

/**
 * @brief Hard lower and upper thresholding
 */
float hardThreshold(float value, vec2 thresholds)
{
  return float(thresholds[0] <= value && value <= thresholds[1]);
}

/**
 * @brief Encapsulate logic for whether to render the fragment based on the view render mode
 */
bool doRender()
{
  // Indicator for which crosshairs quadrant the fragment is in:
  bvec2 quadrant = bvec2(fs_in.v_clipPos.x <= u_clipCrosshairs.x, fs_in.v_clipPos.y > u_clipCrosshairs.y);

  // Distance of the fragment from the crosshairs, accounting for aspect ratio:
  float flashlightDist = sqrt(pow(u_aspectRatio * (fs_in.v_clipPos.x - u_clipCrosshairs.x), 2.0) +
                              pow(fs_in.v_clipPos.y - u_clipCrosshairs.y, 2.0));

  // Flag indicating whether the fragment is rendered
  bool render = (IMAGE_RENDER_MODE == u_renderMode);

  // Check whether to render the fragment based on the mode (Checkerboard/Quadrants/Flashlight):
  render = render || ((CHECKER_RENDER_MODE == u_renderMode) &&
    (u_showFix == bool(mod(floor(fs_in.v_checkerCoord.x) + floor(fs_in.v_checkerCoord.y), 2.0) > 0.5)));

  render = render || ((QUADRANTS_RENDER_MODE == u_renderMode) &&
    (u_showFix == ((! u_quadrants.x || quadrant.x) == (! u_quadrants.y || quadrant.y))));

  render = render || ((FLASHLIGHT_RENDER_MODE == u_renderMode) &&
    ((u_showFix == (flashlightDist > u_flashlightRadius)) || (u_flashlightMovingOnFixed && u_showFix)));

  return render;
}

/**
 * @brief Convert texture intensity to Hounsefield Units, then to Photon Mass Attenuation coefficient
 */
float convertTexToAtten(float img)
{
  float hu = imgSlope_native_T_texture * img; // Hounsefield units

  // Photon mass attenuation coefficient:
  return max((hu / 1000.0) * (waterAttenCoeff - airAttenCoeff) + waterAttenCoeff, 0.0);
}

void main()
{
  if (!doRender()) { discard; }

  // Look up the texture values and convert to mass attenuation coefficient.
  // Keep a running sum of attenuation for all samples.
  float img = clamp(textureLookup(u_imgTex, fs_in.v_texCoord), u_imgMinMax[0], u_imgMinMax[1]);
  float thresh = hardThreshold(img, u_imgThresholds);
  float atten = thresh * convertTexToAtten(img);

  // Accumulate intensity projection in forwards (+Z) and backwards (-Z) directions:
  for (int dir = -1; dir <= 1; dir += 2) // dir in {-1, 1}
  {
    for (int i = 1; i <= u_halfNumMipSamples; ++i)
    {
      vec3 tc = fs_in.v_texCoord + dir * i * u_texSamplingDirZ;
      if (!isInsideTexture(tc)) { break; }

      img = clamp(textureLookup(u_imgTex, tc), u_imgMinMax[0], u_imgMinMax[1]);
      thresh = hardThreshold(img, u_imgThresholds);
      atten += thresh * convertTexToAtten(img);
    }
  }

  // Inverse of the total photon attenuation, which is in range [0.0, 1):
  float invAtten = 1.0 - exp(-atten * mipSamplingDistance_cm);
  float invAttenWL = slopeInterceptWindowLevel[0] * invAtten + slopeInterceptWindowLevel[1]; // apply W/L

  // Compute coords into the image color map, accounting for quantization levels:
  float cmapCoord = mix(floor(float(u_cmapQuantLevels) * invAttenWL) / float(u_cmapQuantLevels - 1),
                        invAttenWL, float(0 == u_cmapQuantLevels));
  cmapCoord = u_cmapSlopeIntercept[0] * cmapCoord + u_cmapSlopeIntercept[1]; // normalize coords

  vec4 imgColorOrig = texture(u_cmapTex, cmapCoord); // image color (non-premult.) before HSV

  // Apply HSV modification factors:
  vec3 imgColorHsv = rgb2hsv(imgColorOrig.rgb);
  imgColorHsv.x += u_cmapHsvModFactors.x;
  imgColorHsv.yz *= u_cmapHsvModFactors.yz;

  // Conditionally use HSV modified colors:
  float mask = float(isInsideTexture(fs_in.v_texCoord));
  float alpha = u_imgOpacity * mask;

  o_color = alpha * imgColorOrig.a * vec4(mix(imgColorOrig.rgb, hsv2rgb(imgColorHsv), float(u_applyHsvMod)), 1.0);
}
