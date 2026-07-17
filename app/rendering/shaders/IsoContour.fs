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
  vec2 v_checkerCoord;
  vec2 v_clipPos;
}
fs_in;

layout(location = 0) out vec4 o_color; // Output RGBA color (premultiplied alpha)

uniform $$IMAGE_SAMPLER_TYPE$$ u_imgTex; // Texture unit 0: image (scalar)

uniform float u_isoValue;
uniform float u_fillOpacity;
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

$$HELPER_FUNCTIONS$$

// float textureLookup(sampler3D texture, vec3 texCoords);
$$TEXTURE_LOOKUP_FUNCTION$$

/// vec3 sampleTexCoord(vec3 texCoord, vec3 worldPos);
$$SAMPLE_TEX_COORD_FUNCTION$$

/// bool doRender(vec2 clipPos, vec2 checkerCoord);
$$DO_RENDER_FUNCTION$$

/// Compute min/mean/max projection. Returns img when MIP is not used (i.e. u_halfNumMipSamples ==
/// 0)
float computeProjection(vec3 baseTc, vec3 baseWorldPos, float img)
{
  // Number of samples used for computing the final image value:
  int numSamples = 1;

  // Accumulate intensity projection in forwards (+Z) and backwards (-Z) directions:
  for (int i = 1; i <= u_halfNumMipSamples; ++i) {
    for (int dir = -1; dir <= 1; dir += 2) {
      vec3 c = baseTc + dir * i * u_texSamplingDirZ;
      vec3 worldPos = baseWorldPos + dir * i * u_worldSamplingDirZ;
      vec3 sampleTc = sampleTexCoord(c, worldPos);
      if (!isInsideTexture(sampleTc)) break;

      float a = clamp(textureLookup(u_imgTex, sampleTc), u_imgMinMax[0], u_imgMinMax[1]);

      img = float(MAX_IP_MODE == u_mipMode) * max(img, a) + float(MEAN_IP_MODE == u_mipMode) * (img + a) +
            float(MIN_IP_MODE == u_mipMode) * min(img, a);

      ++numSamples;
    }
  }

  // If using Mean Intensity Projection mode, then normalize by the number of samples:
  return img / mix(1.0, float(numSamples), float(MEAN_IP_MODE == u_mipMode));
}

void main()
{
  if (!doRender(fs_in.v_clipPos, fs_in.v_checkerCoord)) {
    discard;
  }

  vec3 sampleTc = sampleTexCoord(fs_in.v_texCoord, fs_in.v_worldPos);
  if (!isInsideTexture(sampleTc)) {
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

  /// TODO: use thresholding?
  float alpha = hardThreshold(img, u_imgThresholds);
  vec4 lineColor = alpha * u_lineOpacity * lineCoverage * vec4(u_color, 1.0);
  vec4 fillColor = alpha * u_fillOpacity * fillCoverage * vec4(u_color, 1.0);

  // Draw line contour atop fill:
  o_color = fillColor;
  o_color = lineColor + (1.0 - lineColor.a) * o_color;
}
