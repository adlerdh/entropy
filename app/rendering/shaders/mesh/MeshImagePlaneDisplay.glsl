#define COMPONENT_RENDER_COLOR 1
#define COMPONENT_RENDER_VECTOR_DIRECTION 2
#define COMPONENT_RENDER_VECTOR_NORMAL_PROJECTION 3
#define COMPONENT_RENDER_VECTOR_PLANAR_PROJECTION 4

uniform $$IMAGE_SAMPLER_TYPE$$ u_imgRgbaTex[4];
uniform int u_componentRenderMode;
uniform vec2 u_imgSlopeInterceptRgba[4];
uniform vec2 u_imgMinMaxRgba[4];
uniform vec2 u_imgThresholdsRgba[4];
uniform float u_imgOpacityRgba[4];
uniform bool u_alphaIsOne;
uniform float u_imgSlope_native_T_texture;
uniform float u_projectionScale;
uniform vec3 u_planeNormal_subject;
uniform vec3 u_planeRight_subject;
uniform vec3 u_planeUp_subject;
uniform bool u_vectorSignedColors;

vec3 imagePlaneVector(vec3 sampleTc)
{
  return u_imgSlope_native_T_texture * vec3(
                                         textureLookup(u_imgRgbaTex[0], sampleTc, 0),
                                         textureLookup(u_imgRgbaTex[1], sampleTc, 1),
                                         textureLookup(u_imgRgbaTex[2], sampleTc, 2));
}

vec4 colorImagePlaneColor(vec3 sampleTc)
{
  vec4 img = vec4(
    clamp(textureLookup(u_imgRgbaTex[0], sampleTc, 0), u_imgMinMaxRgba[0].x, u_imgMinMaxRgba[0].y),
    clamp(textureLookup(u_imgRgbaTex[1], sampleTc, 1), u_imgMinMaxRgba[1].x, u_imgMinMaxRgba[1].y),
    clamp(textureLookup(u_imgRgbaTex[2], sampleTc, 2), u_imgMinMaxRgba[2].x, u_imgMinMaxRgba[2].y),
    clamp(textureLookup(u_imgRgbaTex[3], sampleTc, 3), u_imgMinMaxRgba[3].x, u_imgMinMaxRgba[3].y));
  vec4 imgNorm;
  vec4 threshold;
  for (int i = 0; i < 4; ++i) {
    imgNorm[i] = clamp(u_imgSlopeInterceptRgba[i].x * img[i] + u_imgSlopeInterceptRgba[i].y, 0.0, 1.0);
    threshold[i] = hardThreshold(img[i], u_imgThresholdsRgba[i]);
  }
  imgNorm.a = mix(imgNorm.a, 1.0, float(u_alphaIsOne));
  threshold.a = mix(threshold.a, 1.0, float(u_alphaIsOne));

  vec3 rgbOpacity = vec3(u_imgOpacityRgba[0], u_imgOpacityRgba[1], u_imgOpacityRgba[2]);
  vec3 color = imgNorm.rgb * rgbOpacity * threshold.rgb;
  float allRgbEnabled = step(1.0e-6, rgbOpacity.r) * step(1.0e-6, rgbOpacity.g) * step(1.0e-6, rgbOpacity.b);
  float allRgbThresholdedIn = threshold.r * threshold.g * threshold.b;
  float rgbVisibility =
    mix(step(1.0e-6, max(color.r, max(color.g, color.b))), 1.0, allRgbEnabled * allRgbThresholdedIn);
  float alpha = imgNorm.a * u_imgOpacityRgba[3] * threshold.a * rgbVisibility * float(isInsideTexture(sampleTc));
  return vec4(color * alpha, alpha);
}

vec4 vectorImagePlaneColor(vec3 sampleTc)
{
  vec3 vectorValue = imagePlaneVector(sampleTc);
  if (u_componentRenderMode == COMPONENT_RENDER_VECTOR_DIRECTION) {
    float magnitude = length(vectorValue);
    return magnitude > 0.0 ? u_imgOpacity * vec4(abs(vectorValue / magnitude), 1.0) : vec4(0.0);
  }

  if (u_componentRenderMode == COMPONENT_RENDER_VECTOR_NORMAL_PROJECTION) {
    float projection =
      clamp(dot(vectorValue, normalize(u_planeNormal_subject)) / max(u_projectionScale, 1.0e-6), -1.0, 1.0);
    vec3 negativeColor = vec3(0.10, 0.35, 1.00);
    vec3 zeroColor = vec3(0.92);
    vec3 positiveColor = vec3(1.00, 0.16, 0.10);
    vec3 color =
      projection < 0.0 ? mix(zeroColor, negativeColor, -projection) : mix(zeroColor, positiveColor, projection);
    float alpha = u_imgOpacity * mix(0.35, 1.0, abs(projection));
    return vec4(color * alpha, alpha);
  }

  vec2 planar = vec2(dot(vectorValue, normalize(u_planeRight_subject)), dot(vectorValue, normalize(u_planeUp_subject)));
  float magnitude = length(planar);
  if (magnitude <= 0.0) {
    return vec4(0.0);
  }
  vec2 normalizedPlanar = clamp(planar / max(u_projectionScale, 1.0e-6), vec2(-1.0), vec2(1.0));
  normalizedPlanar = mix(abs(normalizedPlanar), normalizedPlanar, float(u_vectorSignedColors));
  vec2 weights = abs(normalizedPlanar);
  vec3 rightColor = normalizedPlanar.x >= 0.0 ? vec3(1.00, 0.12, 0.10) : vec3(0.10, 0.78, 1.00);
  vec3 upColor = normalizedPlanar.y >= 0.0 ? vec3(0.15, 1.00, 0.20) : vec3(0.95, 0.10, 1.00);
  vec3 directionColor = (weights.x * rightColor + weights.y * upColor) / max(weights.x + weights.y, 1.0e-6);
  float strength = clamp(magnitude / max(u_projectionScale, 1.0e-6), 0.0, 1.0);
  vec3 color = mix(vec3(0.92), directionColor, strength);
  float alpha = u_imgOpacity * mix(0.35, 1.0, strength);
  return vec4(color * alpha, alpha);
}

vec4 scalarImagePlaneColor(vec3 sampleTc, vec3 worldPos)
{
  float img = clamp(textureLookup(u_imgTex, sampleTc), u_imgMinMax.x, u_imgMinMax.y);
  img = computeProjection(sampleTc, worldPos, img);
  float imgNorm = clamp(u_imgSlopeIntercept.x * img + u_imgSlopeIntercept.y, 0.0, 1.0);
  float cmapCoord = mix(
    floor(float(u_cmapQuantLevels) * imgNorm) / max(float(u_cmapQuantLevels - 1), 1.0),
    imgNorm,
    float(u_cmapQuantLevels == 0));
  cmapCoord = u_cmapSlopeIntercept.x * cmapCoord + u_cmapSlopeIntercept.y;
  vec4 originalColor = texture(u_cmapTex, cmapCoord);
  vec3 hsv = rgb2hsv(originalColor.rgb);
  hsv.x += u_cmapHsvModFactors.x;
  hsv.yz *= u_cmapHsvModFactors.yz;
  vec3 color = mix(originalColor.rgb, hsv2rgb(hsv), float(u_applyHsvMod));
  float alpha = u_imgOpacity * float(isInsideTexture(sampleTc)) * hardThreshold(img, u_imgThresholds) * originalColor.a;
  return vec4(color * alpha, alpha);
}

vec4 displayedImagePlaneColor(vec3 sampleTc, vec3 worldPos)
{
  if (u_componentRenderMode == COMPONENT_RENDER_COLOR) {
    return colorImagePlaneColor(sampleTc);
  }
  if (
    u_componentRenderMode >= COMPONENT_RENDER_VECTOR_DIRECTION &&
    u_componentRenderMode <= COMPONENT_RENDER_VECTOR_PLANAR_PROJECTION)
  {
    return vectorImagePlaneColor(sampleTc);
  }
  return scalarImagePlaneColor(sampleTc, worldPos);
}

float imagePlaneClipDistance(vec4 clipPosition, int planeIndex)
{
  switch (planeIndex) {
    case 0:
      return clipPosition.x + clipPosition.w;
    case 1:
      return clipPosition.w - clipPosition.x;
    case 2:
      return clipPosition.y + clipPosition.w;
    case 3:
      return clipPosition.w - clipPosition.y;
    case 4:
      return clipPosition.z + clipPosition.w;
    default:
      return clipPosition.w - clipPosition.z;
  }
}

bool clipImagePlaneBoundarySegment(inout vec4 aClip, inout vec4 bClip)
{
  for (int planeIndex = 0; planeIndex < 6; ++planeIndex) {
    float aDistance = imagePlaneClipDistance(aClip, planeIndex);
    float bDistance = imagePlaneClipDistance(bClip, planeIndex);
    bool aOutside = aDistance < 0.0;
    bool bOutside = bDistance < 0.0;
    if (aOutside && bOutside) {
      return false;
    }
    if (aOutside != bOutside) {
      vec4 intersection = mix(aClip, bClip, aDistance / (aDistance - bDistance));
      if (aOutside) {
        aClip = intersection;
      }
      else {
        bClip = intersection;
      }
    }
  }

  return aClip.w > 1.0e-6 && bClip.w > 1.0e-6;
}

float imagePlaneBorderDistancePixels()
{
  vec2 fragmentPixels = gl_FragCoord.xy - u_viewportOrigin;
  float borderDistancePixels = 1e20;
  for (int i = 0; i < u_boundaryVertexCount; ++i) {
    int next = (i + 1) % u_boundaryVertexCount;
    vec4 aClip = u_clip_T_world * vec4(u_boundaryWorldPositions[i], 1.0);
    vec4 bClip = u_clip_T_world * vec4(u_boundaryWorldPositions[next], 1.0);
    if (!clipImagePlaneBoundarySegment(aClip, bClip)) {
      continue;
    }

    vec2 a = 0.5 * (aClip.xy / aClip.w + vec2(1.0)) * u_viewportSize;
    vec2 b = 0.5 * (bClip.xy / bClip.w + vec2(1.0)) * u_viewportSize;
    vec2 ab = b - a;
    float t = clamp(dot(fragmentPixels - a, ab) / max(dot(ab, ab), 1.0e-8), 0.0, 1.0);
    borderDistancePixels = min(borderDistancePixels, length(fragmentPixels - (a + t * ab)));
  }
  return borderDistancePixels;
}
