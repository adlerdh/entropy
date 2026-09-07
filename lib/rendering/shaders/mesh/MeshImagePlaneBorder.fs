#version 330 core

uniform vec4 u_imagePlaneBorderColor;
uniform float u_imagePlaneBorderWidthPixels;
uniform int u_boundaryVertexCount;
uniform vec3 u_boundaryWorldPositions[6];
uniform vec2 u_viewportOrigin;
uniform vec2 u_viewportSize;
uniform mat4 u_clip_T_world;

layout(location = 0) out vec4 outColor;

float clipDistance(vec4 clipPosition, int planeIndex)
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

bool clipBoundarySegment(inout vec4 aClip, inout vec4 bClip)
{
  for (int planeIndex = 0; planeIndex < 6; ++planeIndex) {
    float aDistance = clipDistance(aClip, planeIndex);
    float bDistance = clipDistance(bClip, planeIndex);
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

void main()
{
  if (u_boundaryVertexCount < 2 || u_imagePlaneBorderWidthPixels <= 0.0 || u_imagePlaneBorderColor.a <= 0.0) {
    discard;
  }

  vec2 fragmentPixels = gl_FragCoord.xy - u_viewportOrigin;
  float nearestDistancePixels = 1.0e20;
  float nearestWindowDepth = 1.0;
  for (int i = 0; i < u_boundaryVertexCount; ++i) {
    int next = (i + 1) % u_boundaryVertexCount;
    vec4 aClip = u_clip_T_world * vec4(u_boundaryWorldPositions[i], 1.0);
    vec4 bClip = u_clip_T_world * vec4(u_boundaryWorldPositions[next], 1.0);
    if (!clipBoundarySegment(aClip, bClip)) {
      continue;
    }

    vec2 aPixels = 0.5 * (aClip.xy / aClip.w + vec2(1.0)) * u_viewportSize;
    vec2 bPixels = 0.5 * (bClip.xy / bClip.w + vec2(1.0)) * u_viewportSize;
    vec2 segment = bPixels - aPixels;
    float t = clamp(dot(fragmentPixels - aPixels, segment) / max(dot(segment, segment), 1.0e-8), 0.0, 1.0);
    float distancePixels = length(fragmentPixels - mix(aPixels, bPixels, t));
    if (distancePixels < nearestDistancePixels) {
      nearestDistancePixels = distancePixels;
      nearestWindowDepth = 0.5 * (mix(aClip.z / aClip.w, bClip.z / bClip.w, t) + 1.0);
    }
  }

  float halfWidthPixels = 0.5 * u_imagePlaneBorderWidthPixels;
  float aaRadiusPixels = 0.5 * clamp(fwidth(nearestDistancePixels), 1.0, 1.5);
  float coverage =
    1.0 -
    smoothstep(max(halfWidthPixels - aaRadiusPixels, 0.0), halfWidthPixels + aaRadiusPixels, nearestDistancePixels);
  float alpha = u_imagePlaneBorderColor.a * coverage;
  if (alpha <= 0.0) {
    discard;
  }

  gl_FragDepth = clamp(nearestWindowDepth, 0.0, 1.0);
  outColor = vec4(u_imagePlaneBorderColor.rgb * alpha, alpha);
}
