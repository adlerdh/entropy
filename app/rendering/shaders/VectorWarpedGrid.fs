#version 330 core

// Rendering modes:
#define IMAGE_RENDER_MODE 0
#define CHECKER_RENDER_MODE 1
#define QUADRANTS_RENDER_MODE 2
#define FLASHLIGHT_RENDER_MODE 3

// Grid conventions:
#define SAMPLING_FIELD_CONVENTION 0
#define APPARENT_DEFORMATION_CONVENTION 1

in VS_OUT
{
  vec3 v_texCoord;
  vec3 v_worldPos;
  vec2 v_checkerCoord;
  vec2 v_clipPos;
}
fs_in;

layout(location = 0) out vec4 o_color; // output RGBA color (premultiplied alpha)

uniform $$IMAGE_SAMPLER_TYPE$$ u_imgTex[4]; // vector field components; only x/y/z are used

uniform float u_imgSlope_native_T_texture; // map texture value to native component units
uniform mat4 u_subject_T_texture;          // texture coordinates to subject/LPS coordinates
uniform vec3 u_viewRight_subject;          // current view right axis in subject/LPS coordinates
uniform vec3 u_viewUp_subject;             // current view up axis in subject/LPS coordinates
uniform float u_gridSpacing_subject;       // grid spacing in subject millimeters
uniform float u_lineThickness_px;          // line thickness in screen pixels
uniform float u_warpScale;                 // dimensionless displacement scale
uniform int u_convention;                  // 0: sampling field, 1: approximate apparent deformation
uniform vec4 u_foregroundColor;            // grid line color, straight alpha
uniform vec4 u_backgroundColor;            // grid background color, straight alpha
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

float gridLineAlpha(vec2 gridCoord, float lineThicknessPx)
{
  vec2 distanceToLine = min(fract(gridCoord), 1.0 - fract(gridCoord));
  vec2 derivativeWidth = max(fwidth(gridCoord), vec2(1.0e-6));
  vec2 halfLineWidth = 0.5 * max(lineThicknessPx, 0.0) * derivativeWidth;
  vec2 edgeWidth = derivativeWidth;
  vec2 axisAlpha = 1.0 - smoothstep(halfLineWidth, halfLineWidth + edgeWidth, distanceToLine);
  return clamp(max(axisAlpha.x, axisAlpha.y), 0.0, 1.0);
}

vec4 premultiply(vec4 color)
{
  return vec4(color.rgb * color.a, color.a);
}

void main()
{
  if (!doRender(fs_in.v_clipPos, fs_in.v_checkerCoord) || !isInsideTexture(fs_in.v_texCoord)) {
    discard;
  }

  vec3 vectorValue = u_imgSlope_native_T_texture * vec3(
                                                     textureLookup(u_imgTex[0], fs_in.v_texCoord),
                                                     textureLookup(u_imgTex[1], fs_in.v_texCoord),
                                                     textureLookup(u_imgTex[2], fs_in.v_texCoord));

  vec3 subjectPos = vec3(u_subject_T_texture * vec4(fs_in.v_texCoord, 1.0));
  vec3 warpedSubjectPos = subjectPos + u_warpScale * vectorValue;
  if (APPARENT_DEFORMATION_CONVENTION == u_convention) {
    // Approximate inverse visualization for sampling fields. This is intentionally not a true inverse warp.
    warpedSubjectPos = subjectPos - u_warpScale * vectorValue;
  }

  float spacing = max(u_gridSpacing_subject, 1.0e-6);
  vec2 gridCoord =
    vec2(dot(warpedSubjectPos, normalize(u_viewRight_subject)), dot(warpedSubjectPos, normalize(u_viewUp_subject))) /
    spacing;
  float lineAlpha = gridLineAlpha(gridCoord, u_lineThickness_px);

  vec4 background = premultiply(clamp(u_backgroundColor, vec4(0.0), vec4(1.0)));
  vec4 foreground = premultiply(clamp(u_foregroundColor, vec4(0.0), vec4(1.0)));
  vec4 color = mix(background, foreground, lineAlpha);
  color *= clamp(u_imgOpacity, 0.0, 1.0);

  if (color.a <= 0.0) {
    discard;
  }
  o_color = color;
}
