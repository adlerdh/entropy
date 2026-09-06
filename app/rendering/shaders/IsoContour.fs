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

// Redeclared vertex shader outputs, which are now the fragment shader inputs
in VS_OUT
{
  vec3 v_texCoord;
  vec3 v_worldPos;
  vec3 v_worldNormal;
  vec2 v_checkerCoord;
  vec2 v_clipPos;
}
fs_in;

layout(location = 0) out vec4 o_color; // Output RGBA color (premultiplied alpha)

uniform ${IMAGE_SAMPLER_TYPE} u_imgTex; // Texture unit 0: image (scalar)

uniform float u_isoValue;
uniform float u_fillOpacity;
uniform bool u_fillAboveIsovalue;
uniform float u_lineOpacity;
uniform vec3 u_color;
uniform float u_contourWidth; // pixels

uniform vec2 u_imgMinMax;     // Min and max image values (in texture intenstiy units)
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
uniform bool u_flashlightMovingOnFixed;

// Intensity Projection (MIP) mode (0: none, 1: Max, 2: Mean, 3: Min, 4: X-ray)
uniform int u_mipMode;

// Half the number of samples for MIP. Set to 0 when no projection is used.
uniform int u_halfNumMipSamples;

// Z view camera direction, represented in texture sampling space
uniform vec3 u_texSamplingDirZ;
uniform vec3 u_worldSamplingDirZ;

#include "entropy/HELPER_FUNCTIONS.glsl"
// float textureLookup(sampler3D texture, vec3 texCoords);
#include "entropy/TEXTURE_LOOKUP_FUNCTION.glsl"
/// vec3 sampleTexCoord(vec3 texCoord, vec3 worldPos);
#include "entropy/SAMPLE_TEX_COORD_FUNCTION.glsl"
/// bool doRender(vec2 clipPos, vec2 checkerCoord);
#include "entropy/DO_RENDER_FUNCTION.glsl"
/// float computeProjection(vec3 baseTc, vec3 baseWorldPos, float img);
#include "entropy/IP_FUNCTION.glsl"

bool isContourNeighborhoodInsideTexture(vec3 texCoord)
{
  // Screen-space contour width depends on derivatives over the fragment quad. Suppress the one-pixel image-domain
  // boundary neighborhood so derivative evaluation cannot turn texture clamp/discard behavior into a false contour.
  vec3 dx = dFdx(texCoord);
  vec3 dy = dFdy(texCoord);
  return isInsideTexture(texCoord - dx - dy) && isInsideTexture(texCoord + dx - dy) &&
         isInsideTexture(texCoord - dx + dy) && isInsideTexture(texCoord + dx + dy);
}

void main()
{
  vec3 sampleTc = sampleTexCoord(fs_in.v_texCoord, fs_in.v_worldPos);
  bool renderMask = doRender(fs_in.v_clipPos, fs_in.v_checkerCoord);
  bool contourNeighborhoodInside = isContourNeighborhoodInsideTexture(sampleTc);

  if (!renderMask || !contourNeighborhoodInside) {
    discard;
  }

  float img = clamp(textureLookup(u_imgTex, sampleTc), u_imgMinMax[0], u_imgMinMax[1]);
  img = computeProjection(sampleTc, fs_in.v_worldPos, img);

  /*
  // Optimization when using distance maps:
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

  float gradientPerPixel = length(vec2(dFdx(img), dFdy(img)));
  float lineCoverage = 0.0;
  float fillCoverage = float(img < u_isoValue);
  if (gradientPerPixel > 1.0e-12) {
    float signedDistancePx = (img - u_isoValue) / gradientPerPixel;
    float halfLineWidthPx = max(0.5 * u_contourWidth, 0.0);
    lineCoverage = 1.0 - smoothstep(halfLineWidthPx, halfLineWidthPx + 1.0, abs(signedDistancePx));
    fillCoverage = 1.0 - smoothstep(-0.5, 0.5, signedDistancePx);
  }
  if (u_fillAboveIsovalue) {
    fillCoverage = 1.0 - fillCoverage;
  }

  /// TODO: use thresholding?
  float alpha = hardThreshold(img, u_imgThresholds);
  vec4 lineColor = alpha * u_lineOpacity * lineCoverage * vec4(u_color, 1.0);
  vec4 fillColor = alpha * u_fillOpacity * fillCoverage * vec4(u_color, 1.0);

  // Draw line contour atop fill:
  o_color = fillColor;
  o_color = lineColor + (1.0 - lineColor.a) * o_color;
}
