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

// Redeclared vertex shader outputs, which are now the fragment shader inputs
in VS_OUT
{
  vec3 v_imgTexCoords;
  vec2 v_checkerCoord;
  vec2 v_clipPos;
} fs_in;

layout (location = 0) out vec4 o_color; // Output RGBA color (pre-multiplied alpha)

uniform sampler3D u_imgTex; // Texture unit 0: image (scalar)

// Uniforms from vertex shader:
uniform mat4 u_imgTexture_T_world;
uniform mat4 u_world_T_clip;
uniform float u_clipDepth;

uniform float u_isoValue;
uniform float u_fillOpacity;
uniform float u_lineOpacity;
uniform vec3 u_color;
uniform float u_contourWidth; // pixels
uniform vec2 u_viewSize; // pixels

uniform vec2 u_imgMinMax; // Min and max image values (in texture intenstiy units)
uniform vec2 u_imgThresholds; // Image lower and upper thresholds (in texture intensity units)

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

float hardThreshold(float value, vec2 thresholds)
{
  return float(thresholds[0] <= value && value <= thresholds[1]);
}

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

  if (!isInsideTexture(fs_in.v_imgTexCoords)) {
    discard;
  }

  float img = clamp(textureLookup(u_imgTex, fs_in.v_imgTexCoords), u_imgMinMax[0], u_imgMinMax[1]);
  img = computeProjection(img);

  /*
  // Optimizationm when using distance maps:
  // Add option to apply distance map optimization that then does this early return...
  float voxelDiag = length(u_voxelSize);

  if (img > u_isoValue + voxelDiag) {
    discard; // outside boundary
  }
  else if (img < u_isoValue - voxelDiag) {
    o_color = u_fillOpacity * u_color; // inside boundary
    return;
  }
  */

  // Pixel deltas in clip space:
  float dx = 2.0 / u_viewSize.x;
  float dy = 2.0 / u_viewSize.y;

  vec2 p = fs_in.v_clipPos;
  vec2 pa = p + vec2(-dx, 0.0);
  vec2 pb = p + vec2(dx, 0.0);
  vec2 pc = p + vec2(0.0, -dy);
  vec2 pd = p + vec2(0.0, dy);

  mat4 texture_T_clip = u_imgTexture_T_world * u_world_T_clip;

  float a_v = textureLookup(u_imgTex, vec3(texture_T_clip * vec4(pa, u_clipDepth, 1.0)));
  float b_v = textureLookup(u_imgTex, vec3(texture_T_clip * vec4(pb, u_clipDepth, 1.0)));
  float c_v = textureLookup(u_imgTex, vec3(texture_T_clip * vec4(pc, u_clipDepth, 1.0)));
  float d_v = textureLookup(u_imgTex, vec3(texture_T_clip * vec4(pd, u_clipDepth, 1.0)));

  vec2 grad = vec2((b_v - a_v) / (2.0 * dx), (d_v - c_v) / (2.0 * dy));

  float de = (img - u_isoValue) / length(grad);
  float eps = u_contourWidth * min(dx, dy);

  // Feather the contour:
  //   -ve: [iso - eps, iso]
  //   +ve: (iso, iso + eps/2]
  float cneg = float(-eps <= de) * float(de <= 0.0) * max(1.0 - pow(de / (-1.0 * eps), 6.0), 0.0); // -ve side of u_isoValue
  float cpos = float(0.0 < de) * float(de <= 0.5 * eps) * max(1.0 - pow(de / (0.5 * eps), 2.0), 0.0); // +ve side of u_isoValue
  float c_feather = clamp(cneg + cpos, 0.0, 1.0);

  /// TODO: use thresholding?
  float alpha = hardThreshold(img, u_imgThresholds);
  vec4 lineColor = alpha * u_lineOpacity * c_feather * vec4(u_color, 1.0);
  vec4 fillColor = alpha * u_fillOpacity * float(img < u_isoValue) * vec4(u_color, 1.0);

  // Draw line contour atop fill:
  o_color = fillColor;
  o_color = lineColor + (1.0 - lineColor.a) * o_color;
}
