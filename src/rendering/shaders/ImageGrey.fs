#version 330 core

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
uniform vec2 u_imgSlopeIntercept; // map texture to normalized intensity [0, 1], plus window/leveling
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

{{HELPER_FUNCTIONS}}
{{COLOR_HELPER_FUNCTIONS}}

/// float textureLookup(sampler3D texture, vec3 texCoord);
{{TEXTURE_LOOKUP_FUNCTION}}

/// bool doRender(vec2 clipPos, vec2 checkerCoord);
{{DO_RENDER_FUNCTION}}

/// float computeProjection(vec3 baseTc, float img);
{{IP_FUNCTION}}

void main()
{
  if (!doRender(fs_in.v_clipPos, fs_in.v_checkerCoord)) {
    discard;
  }

  float img = clamp(textureLookup(u_imgTex, fs_in.v_texCoord), u_imgMinMax[0], u_imgMinMax[1]);
  img = computeProjection(fs_in.v_texCoord, img);

  // Apply window/level and normalize image values to [0.0, 1.0] range:
  float imgNorm = clamp(u_imgSlopeIntercept[0] * img + u_imgSlopeIntercept[1], 0.0, 1.0);

  // Compute color map coords, accounting for quantization levels:
  float cmapCoord = mix(floor(float(u_cmapQuantLevels) * imgNorm) / max(float(u_cmapQuantLevels - 1), 1.0),
                        imgNorm, float(0 == u_cmapQuantLevels));
  cmapCoord = u_cmapSlopeIntercept[0] * cmapCoord + u_cmapSlopeIntercept[1]; // normalize

  vec4 imgColorOrig = texture(u_cmapTex, cmapCoord); // image color (non-premult.) before HSV

  // Apply HSV modification factors:
  vec3 imgColorHsv = rgb2hsv(imgColorOrig.rgb);
  imgColorHsv.x += u_cmapHsvModFactors.x;
  imgColorHsv.yz *= u_cmapHsvModFactors.yz;

  // Conditionally use HSV modified colors:
  float mask = float(isInsideTexture(fs_in.v_texCoord)); // image mask based on texture coords
  float alpha = u_imgOpacity * mask * hardThreshold(img, u_imgThresholds); // alpha = opacity * mask * threshold

  // Output color (premult. RGBA)
  o_color = alpha * imgColorOrig.a * vec4(mix(imgColorOrig.rgb, hsv2rgb(imgColorHsv), float(u_applyHsvMod)), 1.0);
  //o_color.rgb = pow(o_color.rgb, vec3(1.8));
}
