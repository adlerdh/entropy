#version 330 core

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

uniform sampler3D u_imgTex[4]; // image RGBA components

// Image adjustment uniforms:
uniform vec2 u_imgSlopeIntercept[4]; // map texture to normalized intensity [0, 1], plus window/leveling
uniform vec2 u_imgMinMax[4]; // min/max image values (texture intenstiy units)
uniform vec2 u_imgThresholds[4]; // lower/upper image thresholds (texture intensity units)
uniform float u_imgOpacity[4]; // image opacity
uniform bool u_alphaIsOne; // flag to force alpha to 1 (true for 3-component images)

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

/// float textureLookup(sampler3D texture, vec3 texCoord);
{{TEXTURE_LOOKUP_FUNCTION}}

/// bool doRender(vec2 clipPos, vec2 checkerCoord);
{{DO_RENDER_FUNCTION}}

void main()
{
  if (!doRender(fs_in.v_clipPos, fs_in.v_checkerCoord)) {
    discard;
  }

  // Look up the image values (after mapping to GL texture units):
  vec4 img = vec4(
    clamp(textureLookup(u_imgTex[0], fs_in.v_texCoord), u_imgMinMax[0][0], u_imgMinMax[0][1]),
    clamp(textureLookup(u_imgTex[1], fs_in.v_texCoord), u_imgMinMax[1][0], u_imgMinMax[1][1]),
    clamp(textureLookup(u_imgTex[2], fs_in.v_texCoord), u_imgMinMax[2][0], u_imgMinMax[2][1]),
    clamp(textureLookup(u_imgTex[3], fs_in.v_texCoord), u_imgMinMax[3][0], u_imgMinMax[3][1]));

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

  // Apply alpha to each component:
  float mask = float(isInsideTexture(fs_in.v_texCoord));
  for (int i = 0; i <= 3; ++i) {
    imgNorm[i] *= u_imgOpacity[i] * thresh[i] * mask;
  }

  o_color = imgNorm.a * vec4(imgNorm.rgb, 1.0); // premult. RGBA
}
