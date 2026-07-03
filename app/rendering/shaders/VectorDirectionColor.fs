#version 330 core

// Rendering modes:
#define IMAGE_RENDER_MODE 0
#define CHECKER_RENDER_MODE 1
#define QUADRANTS_RENDER_MODE 2
#define FLASHLIGHT_RENDER_MODE 3

in VS_OUT
{
  vec3 v_texCoord;
  vec3 v_worldPos;
  vec2 v_checkerCoord;
  vec2 v_clipPos;
}
fs_in;

layout(location = 0) out vec4 o_color; // output RGBA color (premultiplied alpha)

uniform sampler3D u_imgTex[4]; // vector field components; only x/y/z are used

uniform float u_imgSlope_native_T_texture; // map texture value to native component units
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

  float magnitude = length(vectorValue);
  if (magnitude <= 0.0) {
    discard;
  }

  vec3 direction = normalize(vectorValue);
  vec3 color = abs(direction);

  o_color = u_imgOpacity * vec4(color, 1.0);
}
