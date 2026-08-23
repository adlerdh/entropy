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

uniform $$IMAGE_SAMPLER_TYPE$$ u_imgTex;
uniform $$SEG_SAMPLER_TYPE$$ u_segTex;
uniform sampler1D u_cmapTex;
uniform samplerBuffer u_segLabelCmapTex;
uniform sampler2D u_previousDepthBoundsTex;
uniform sampler2D u_previousFrontColorTex;

uniform vec2 u_imgSlopeIntercept;
uniform vec2 u_imgMinMax;
uniform vec2 u_imgThresholds;
uniform float u_imgOpacity;
uniform bool u_imagePlaneShadingEnabled;
uniform vec4 u_imagePlaneBorderColor;
uniform float u_imagePlaneBorderWidthPixels;
uniform vec3 u_cameraWorldPosition;
uniform float u_lightingAmbient;
uniform float u_lightingDiffuse;
uniform float u_lightingSpecular;
uniform float u_lightingSpecularPower;

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

layout(location = 0) out vec2 outDepthBounds;
layout(location = 1) out vec4 outFrontColor;
layout(location = 2) out vec4 outBackColor;

const float kMaxDepth = 1.0;
const float kDepthEpsilon = 0.000001;

$$HELPER_FUNCTIONS$$
$$COLOR_HELPER_FUNCTIONS$$
$$TEXTURE_LOOKUP_FUNCTION$$
$$UINT_TEXTURE_LOOKUP_FUNCTION$$
$$SAMPLE_TEX_COORD_FUNCTION$$
$$DO_RENDER_FUNCTION$$
$$IP_FUNCTION$$

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

vec4 getLabelColor(int label)
{
  label -= label * when_ge(label, textureSize(u_segLabelCmapTex));
  vec4 color = texelFetch(u_segLabelCmapTex, label);
  return color.a * color;
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

vec4 segmentationPlaneColor(vec3 sampleTc)
{
  if (!u_segVisible || !isInsideTexture(sampleTc)) {
    return vec4(0.0);
  }

  float interpOpacity = 1.0;
  uint seg = getSegValue(sampleTc, vec3(0.0), interpOpacity);
  if (seg == 0u) {
    return vec4(0.0);
  }

  float alpha = u_segOpacity * interpOpacity * getSegInteriorAlpha(sampleTc, seg);
  return alpha * getLabelColor(int(seg));
}

float blinnPhongImagePlaneLighting(vec3 worldPosition, vec3 worldNormal)
{
  if (!u_imagePlaneShadingEnabled) {
    return 1.0;
  }

  float normalLength = length(worldNormal);
  vec3 eyeVector = u_cameraWorldPosition - worldPosition;
  float eyeVectorLength = length(eyeVector);
  if (normalLength <= 0.0 || eyeVectorLength <= 0.0) {
    return 1.0;
  }

  vec3 normal = worldNormal / normalLength;
  vec3 viewDirection = eyeVector / eyeVectorLength;
  float diffuse = abs(dot(normal, viewDirection));
  float specular = pow(max(abs(dot(normal, viewDirection)), 0.0), max(u_lightingSpecularPower, 0.001));
  float lighting = u_lightingAmbient + u_lightingDiffuse * diffuse + u_lightingSpecular * specular;
  return clamp(lighting, 0.0, 1.0);
}

vec4 imagePlaneColor()
{
  if (!doRender(fs_in.v_clipPos, fs_in.v_checkerCoord)) {
    return vec4(0.0);
  }

  vec3 sampleTc = sampleTexCoord(fs_in.v_texCoord, fs_in.v_worldPos);
  float img = clamp(textureLookup(u_imgTex, sampleTc), u_imgMinMax[0], u_imgMinMax[1]);
  img = computeProjection(sampleTc, fs_in.v_worldPos, img);

  float imgNorm = clamp(u_imgSlopeIntercept[0] * img + u_imgSlopeIntercept[1], 0.0, 1.0);
  float cmapCoord = mix(
    floor(float(u_cmapQuantLevels) * imgNorm) / max(float(u_cmapQuantLevels - 1), 1.0),
    imgNorm,
    float(0 == u_cmapQuantLevels));
  cmapCoord = u_cmapSlopeIntercept[0] * cmapCoord + u_cmapSlopeIntercept[1];

  vec4 imgColorOrig = texture(u_cmapTex, cmapCoord);
  vec3 imgColorHsv = rgb2hsv(imgColorOrig.rgb);
  imgColorHsv.x += u_cmapHsvModFactors.x;
  imgColorHsv.yz *= u_cmapHsvModFactors.yz;

  float mask = float(isInsideTexture(sampleTc));
  float alpha = u_imgOpacity * mask * hardThreshold(img, u_imgThresholds) * imgColorOrig.a;

  vec3 mappedColor = mix(imgColorOrig.rgb, hsv2rgb(imgColorHsv), float(u_applyHsvMod));
  mappedColor *= blinnPhongImagePlaneLighting(fs_in.v_worldPos, fs_in.v_worldNormal);
  vec4 imageColor = alpha > 0.0 ? vec4(mappedColor * alpha, alpha) : vec4(0.0);
  vec4 segmentationColor = segmentationPlaneColor(sampleTc);
  vec4 contentColor = segmentationColor + imageColor * (1.0 - segmentationColor.a);

  float borderDistancePixels = 1e20;
  for (int axis = 0; axis < 3; ++axis) {
    float textureUnitsPerPixel = length(vec2(dFdx(fs_in.v_texCoord[axis]), dFdy(fs_in.v_texCoord[axis])));
    if (textureUnitsPerPixel > 1e-8) {
      borderDistancePixels =
        min(borderDistancePixels, min(fs_in.v_texCoord[axis], 1.0 - fs_in.v_texCoord[axis]) / textureUnitsPerPixel);
    }
  }
  float borderCoverage = 1.0 - smoothstep(
                                 max(u_imagePlaneBorderWidthPixels - 0.5, 0.0),
                                 u_imagePlaneBorderWidthPixels + 0.5,
                                 borderDistancePixels);
  float borderAlpha = u_imagePlaneBorderColor.a * borderCoverage;
  vec4 borderColor = vec4(u_imagePlaneBorderColor.rgb * borderAlpha, borderAlpha);
  return borderColor + contentColor * (1.0 - borderAlpha);
}

void main()
{
  ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
  vec2 previousDepthBounds = texelFetch(u_previousDepthBoundsTex, pixelCoord, 0).xy;
  vec4 previousFrontColor = texelFetch(u_previousFrontColorTex, pixelCoord, 0);
  float fragmentDepth = gl_FragCoord.z;
  float nearestDepth = -previousDepthBounds.x;
  float farthestDepth = previousDepthBounds.y;

  outDepthBounds = vec2(-kMaxDepth);
  outFrontColor = previousFrontColor;
  outBackColor = vec4(0.0);

  if (fragmentDepth < nearestDepth - kDepthEpsilon || fragmentDepth > farthestDepth + kDepthEpsilon) {
    return;
  }

  if (fragmentDepth > nearestDepth + kDepthEpsilon && fragmentDepth < farthestDepth - kDepthEpsilon) {
    outDepthBounds = vec2(-fragmentDepth, fragmentDepth);
    return;
  }

  vec4 premultipliedColor = imagePlaneColor();
  if (premultipliedColor.a <= 0.0) {
    discard;
  }

  if (fragmentDepth >= nearestDepth - kDepthEpsilon && fragmentDepth <= nearestDepth + kDepthEpsilon) {
    outFrontColor = previousFrontColor + premultipliedColor * (1.0 - previousFrontColor.a);
  }
  else {
    outBackColor = premultipliedColor;
  }
}
