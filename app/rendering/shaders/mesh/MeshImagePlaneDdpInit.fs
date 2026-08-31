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
  vec3 v_worldPos;
  vec3 v_worldNormal;
  vec2 v_checkerCoord;
  vec2 v_clipPos;
}
fs_in;

layout(location = 0) out vec2 outDepthBounds;

uniform $$IMAGE_SAMPLER_TYPE$$ u_imgTex;
uniform $$SEG_SAMPLER_TYPE$$ u_segTex;
uniform sampler1D u_cmapTex;
uniform samplerBuffer u_segLabelCmapTex;

uniform vec2 u_imgSlopeIntercept;
uniform vec2 u_imgMinMax;
uniform vec2 u_imgThresholds;
uniform float u_imgOpacity;
uniform bool u_imagePlaneShadingEnabled;
uniform vec4 u_imagePlaneBorderColor;
uniform float u_imagePlaneBorderWidthPixels;
uniform float u_ddpDepthBias;
uniform int u_boundaryVertexCount;
uniform vec3 u_boundaryWorldPositions[6];
uniform vec2 u_viewportOrigin;
uniform vec2 u_viewportSize;
uniform mat4 u_clip_T_world;
uniform vec3 u_cameraWorldPosition;

uniform vec2 u_cmapSlopeIntercept;
uniform int u_cmapQuantLevels;
uniform vec3 u_cmapHsvModFactors;
uniform bool u_applyHsvMod;

uniform int u_renderMode;
uniform vec2 u_clipCrosshairs;
uniform bvec2 u_quadrants;
uniform bool u_showFix;
uniform float u_aspectRatio;
uniform float u_flashlightRadius;
uniform bool u_flashlightMovingOnFixed;

uniform int u_mipMode;
uniform int u_halfNumMipSamples;
uniform vec3 u_texSamplingDirZ;
uniform vec3 u_worldSamplingDirZ;

uniform bool u_segVisible;
uniform float u_segOpacity;
uniform float u_segFillOpacity;
uniform float u_segInterpCutoff;
uniform bool u_segLinearInterpolation;
uniform bool u_segOutlineUsesScreenPixels;
uniform vec3 u_texSamplingDirsForSegOutline[2];
uniform vec3 u_texSamplingDirsForSmoothSeg[2];

$$HELPER_FUNCTIONS$$
$$COLOR_HELPER_FUNCTIONS$$
$$TEXTURE_LOOKUP_FUNCTION$$
$$UINT_TEXTURE_LOOKUP_FUNCTION$$
$$SAMPLE_TEX_COORD_FUNCTION$$
$$DO_RENDER_FUNCTION$$
$$IP_FUNCTION$$
$$IMAGE_PLANE_DISPLAY_FUNCTIONS$$

int when_lt(int x, int y)
{
  return max(sign(y - x), 0);
}

int when_ge(int x, int y)
{
  return 1 - when_lt(x, y);
}

uint safeSegLookup(vec3 texCoord)
{
  return isInsideTexture(texCoord) ? uintTextureLookup(u_segTex, texCoord) : 0u;
}

bool isLabelVisible(int label)
{
  label -= label * when_ge(label, textureSize(u_segLabelCmapTex));
  return texelFetch(u_segLabelCmapTex, label).a > 0.0;
}

float labelPremultipliedAlpha(int label)
{
  label -= label * when_ge(label, textureSize(u_segLabelCmapTex));
  vec4 color = texelFetch(u_segLabelCmapTex, label);
  return (color.a * color).a;
}

uint getNearestSegValue(vec3 texCoord, vec3 texOffset, out float opacity)
{
  opacity = 1.0;
  return safeSegLookup(texCoord + texOffset);
}

const uvec3 kSegNeighbors[8] = uvec3[8](
  uvec3(0, 0, 0),
  uvec3(0, 0, 1),
  uvec3(0, 1, 0),
  uvec3(0, 1, 1),
  uvec3(1, 0, 0),
  uvec3(1, 0, 1),
  uvec3(1, 1, 0),
  uvec3(1, 1, 1));

uint getLinearSegValue(vec3 texCoord, vec3 texOffset, out float opacity)
{
  opacity = 1.0;

  vec3 baseTc = texCoord + texOffset;
  if (!isInsideTexture(baseTc)) {
    return 0u;
  }

  vec3 baseVoxCoord = baseTc * vec3(segTextureSize());
  vec3 c = floor(baseVoxCoord);
  vec3 d = pow(vec3(segTextureSize()), vec3(-1));
  vec3 t = vec3(c.x * d.x, c.y * d.y, c.z * d.z) + 0.5 * d;

  uint neighborCenterLabels[8];
  for (int i = 0; i < 8; ++i) {
    neighborCenterLabels[i] = safeSegLookup(t + kSegNeighbors[i] * d);
  }

  vec3 fracPart = baseVoxCoord - c;
  vec3 w[2] = vec3[2](vec3(1.0) - fracPart, fracPart);

  float maxInterp = 0.0;
  for (int i = 0; i <= 8; ++i) {
    int neighborIndex = int(mod(i + 4, 9));
    float row = float(mod(neighborIndex, 3) - 1);
    float col = float(floor(float(neighborIndex / 3)) - 1);
    vec3 texPos = row * u_texSamplingDirsForSmoothSeg[0] + col * u_texSamplingDirsForSmoothSeg[1];
    uint label = safeSegLookup(baseTc + texPos);

    float interp = 0.0;
    for (int j = 0; j <= 7; ++j) {
      interp += float(neighborCenterLabels[j] == label) * w[kSegNeighbors[j].x].x * w[kSegNeighbors[j].y].y *
                w[kSegNeighbors[j].z].z;
    }

    if (interp > maxInterp && interp >= u_segInterpCutoff && isLabelVisible(int(label))) {
      maxInterp = interp;
      return label;
    }
  }

  return 0u;
}

uint getSegValue(vec3 texCoord, vec3 texOffset, out float opacity)
{
  return u_segLinearInterpolation ? getLinearSegValue(texCoord, texOffset, opacity)
                                  : getNearestSegValue(texCoord, texOffset, opacity);
}

float getSegInteriorAlpha(vec3 texCoord, uint seg)
{
  if (u_segFillOpacity >= 1.0) {
    return 1.0;
  }

  // Screen-pixel outlines must be measured at the actual 3D plane depth. CPU-side near-plane offsets change with the
  // camera and can make segmentation interiors flicker between fill and edge opacity.
  vec3 screenPixelDirs[2] = vec3[2](dFdx(texCoord), dFdy(texCoord));

  for (int i = 0; i <= 8; ++i) {
    float row = float(mod(i, 3) - 1);
    float col = float(floor(float(i / 3)) - 1);
    vec3 texPosOffset = u_segOutlineUsesScreenPixels
                          ? row * screenPixelDirs[0] + col * screenPixelDirs[1]
                          : row * u_texSamplingDirsForSegOutline[0] + col * u_texSamplingDirsForSegOutline[1];

    float ignore;
    if (seg != getSegValue(texCoord, texPosOffset, ignore)) {
      return 1.0;
    }
  }

  return u_segFillOpacity;
}

float segmentationPlaneAlpha(vec3 sampleTc)
{
  if (!u_segVisible || !isInsideTexture(sampleTc)) {
    return 0.0;
  }

  float interpOpacity = 1.0;
  uint seg = getSegValue(sampleTc, vec3(0.0), interpOpacity);
  if (seg == 0u) {
    return 0.0;
  }

  return u_segOpacity * interpOpacity * getSegInteriorAlpha(sampleTc, seg) * labelPremultipliedAlpha(int(seg));
}

float imagePlaneAlpha()
{
  if (!doRender(fs_in.v_clipPos, fs_in.v_checkerCoord)) {
    return 0.0;
  }

  vec3 sampleTc = sampleTexCoord(fs_in.v_texCoord, fs_in.v_worldPos);
  float imageAlpha = displayedImagePlaneColor(sampleTc, fs_in.v_worldPos).a;
  float segmentationAlpha = segmentationPlaneAlpha(sampleTc);
  float borderDistancePixels = imagePlaneBorderDistancePixels();
  float borderAlpha = u_imagePlaneBorderColor.a * (1.0 - smoothstep(
                                                           max(u_imagePlaneBorderWidthPixels - 0.5, 0.0),
                                                           u_imagePlaneBorderWidthPixels + 0.5,
                                                           borderDistancePixels));
  float contentAlpha = segmentationAlpha + imageAlpha * (1.0 - segmentationAlpha);
  return borderAlpha + contentAlpha * (1.0 - borderAlpha);
}

void main()
{
  if (imagePlaneAlpha() <= 0.0) {
    discard;
  }

  float orderedDepth = clamp(gl_FragCoord.z - u_ddpDepthBias, 0.0, 1.0);
  outDepthBounds = vec2(-orderedDepth, orderedDepth);
}
