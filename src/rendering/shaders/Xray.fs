#version 330 core

// Rendering modes:
#define IMAGE_RENDER_MODE 0
#define CHECKER_RENDER_MODE 1
#define QUADRANTS_RENDER_MODE 2
#define FLASHLIGHT_RENDER_MODE 3

#define XRAY_IP_MODE 4

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
uniform vec2 u_imgSlopeIntercept; // slope/intercept for window-leveling the attenuation value
uniform vec2 u_imgMinMax; // min/max image values (texture intenstiy units)
uniform vec2 u_imgThresholds; // lower/upper image thresholds (texture intensity units)
uniform float u_imgOpacity; // image opacity
uniform float u_imgSlope_native_T_texture; // slope to map texture intensity to native image intensity (NOT accounting for window-leveling)

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
uniform float u_mipSamplingDistance_cm;

// Photon mass attenuation coefficients [1/cm] of liquid water and dry air (at sea level):
uniform float u_waterAttenCoeff;
uniform float u_airAttenCoeff;

{{HELPER_FUNCTIONS}}
{{COLOR_HELPER_FUNCTIONS}}

/// float textureLookup(sampler3D texture, vec3 texCoord);
{{TEXTURE_LOOKUP_FUNCTION}}

/// bool doRender(vec2 clipPos, vec2 checkerCoord);
{{DO_RENDER_FUNCTION}}

/**
 * @brief Convert texture intensity to Hounsefield Units, then to Photon Mass Attenuation coefficient
 */
float convertTexToAtten(float img)
{
  float hu = u_imgSlope_native_T_texture * img; // Hounsefield units

  // Photon mass attenuation coefficient:
  return max((hu / 1000.0) * (u_waterAttenCoeff - u_airAttenCoeff) + u_waterAttenCoeff, 0.0);
}

void main()
{
  if (!doRender(fs_in.v_clipPos, fs_in.v_checkerCoord)) {
    discard;
  }

  // Look up the texture values and convert to mass attenuation coefficient.
  // Keep a running sum of attenuation for all samples.
  float img = clamp(textureLookup(u_imgTex, fs_in.v_texCoord), u_imgMinMax[0], u_imgMinMax[1]);
  float thresh = hardThreshold(img, u_imgThresholds);
  float atten = thresh * convertTexToAtten(img);
  float useXray = float(XRAY_IP_MODE == u_mipMode);

  // Accumulate intensity projection in forwards (+Z) and backwards (-Z) directions:
  for (int dir = -1; dir <= 1; dir += 2) // dir in {-1, 1}
  {
    for (int i = 1; i <= u_halfNumMipSamples; ++i)
    {
      vec3 tc = fs_in.v_texCoord + dir * i * u_texSamplingDirZ;
      if (!isInsideTexture(tc)) { break; }

      img = clamp(textureLookup(u_imgTex, tc), u_imgMinMax[0], u_imgMinMax[1]);
      thresh = hardThreshold(img, u_imgThresholds);
      atten += useXray * thresh * convertTexToAtten(img);
    }
  }

  // Inverse of the total photon attenuation, which is in range [0.0, 1):
  float invAtten = 1.0 - exp(-atten * u_mipSamplingDistance_cm);
  float invAttenWL = u_imgSlopeIntercept[0] * invAtten + u_imgSlopeIntercept[1]; // apply W/L

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
