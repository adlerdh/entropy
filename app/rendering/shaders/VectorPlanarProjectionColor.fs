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
}
fs_in;

layout(location = 0) out vec4 o_color; // output RGBA color (premultiplied alpha)

uniform sampler3D u_imgTex[4]; // vector field components; only x/y/z are used

uniform float u_imgSlope_native_T_texture; // map texture value to native component units
uniform float u_projectionScale;           // native-unit scale for mapping projected length to [0, 1]
uniform vec3 u_viewRight_subject;          // current view right axis in subject/LPS coordinates
uniform vec3 u_viewUp_subject;             // current view up axis in subject/LPS coordinates
uniform bool u_signedColors;               // preserve right/left and up/down color signs
uniform float u_imgOpacity;                // global vector field opacity

// View render mode uniforms:
uniform int u_renderMode;      // mode (0: normal, 1: checkerboard, 2: quadrants, 3: flashlight)
uniform vec2 u_clipCrosshairs; // crosshairs position in Clip space
uniform bvec2 u_quadrants;
uniform bool u_showFix;
uniform float u_aspectRatio;
uniform float u_flashlightRadius;
uniform bool u_flashlightMovingOnFixed;

$$HELPER_FUNCTIONS$$

/// float textureLookup(sampler3D texture, vec3 texCoord);
$$TEXTURE_LOOKUP_FUNCTION$$

/// bool doRender(vec2 clipPos, vec2 checkerCoord);
$$DO_RENDER_FUNCTION$$

void main()
{
  if (!doRender(fs_in.v_clipPos, fs_in.v_checkerCoord) || !isInsideTexture(fs_in.v_texCoord)) {
    discard;
  }

  vec3 vectorValue = u_imgSlope_native_T_texture * vec3(
                                                     textureLookup(u_imgTex[0], fs_in.v_texCoord),
                                                     textureLookup(u_imgTex[1], fs_in.v_texCoord),
                                                     textureLookup(u_imgTex[2], fs_in.v_texCoord));

  vec2 planar = vec2(dot(vectorValue, normalize(u_viewRight_subject)), dot(vectorValue, normalize(u_viewUp_subject)));
  float planarMagnitude = length(planar);
  if (planarMagnitude <= 0.0) {
    discard;
  }

  vec2 normalizedPlanar = clamp(planar / max(u_projectionScale, 1.0e-6), vec2(-1.0), vec2(1.0));
  if (!u_signedColors) {
    normalizedPlanar = abs(normalizedPlanar);
  }
  vec2 signedWeights = abs(normalizedPlanar);

  vec3 rightColor = normalizedPlanar.x >= 0.0 ? vec3(1.00, 0.12, 0.10) : vec3(0.10, 0.78, 1.00);
  vec3 upColor = normalizedPlanar.y >= 0.0 ? vec3(0.15, 1.00, 0.20) : vec3(0.95, 0.10, 1.00);
  vec3 zeroColor = vec3(0.92, 0.92, 0.92);

  float weightSum = max(signedWeights.x + signedWeights.y, 1.0e-6);
  vec3 directionColor = (signedWeights.x * rightColor + signedWeights.y * upColor) / weightSum;
  float strength = clamp(planarMagnitude / max(u_projectionScale, 1.0e-6), 0.0, 1.0);
  vec3 color = mix(zeroColor, directionColor, strength);

  float alpha = u_imgOpacity * mix(0.35, 1.0, strength);
  o_color = alpha * vec4(color, 1.0);
}
