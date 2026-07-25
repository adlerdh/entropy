#version 330 core

uniform sampler2D u_normalTex;
uniform sampler2D u_depthTex;
uniform vec2 u_viewportSize;
uniform float u_radiusPixels;
uniform float u_strength;

layout(location = 0) out float outOcclusion;

const int kSampleCount = 8;
const vec2 kSampleDirections[kSampleCount] = vec2[](
  vec2(1.0, 0.0),
  vec2(-1.0, 0.0),
  vec2(0.0, 1.0),
  vec2(0.0, -1.0),
  vec2(0.70710678, 0.70710678),
  vec2(-0.70710678, 0.70710678),
  vec2(0.70710678, -0.70710678),
  vec2(-0.70710678, -0.70710678));

void main()
{
  ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
  float centerDepth = texelFetch(u_depthTex, pixelCoord, 0).r;
  vec4 encodedNormal = texelFetch(u_normalTex, pixelCoord, 0);
  if (centerDepth >= 1.0 || encodedNormal.a <= 0.0) {
    outOcclusion = 1.0;
    return;
  }

  vec2 texelStep = 1.0 / max(u_viewportSize, vec2(1.0));
  vec2 centerUv = gl_FragCoord.xy * texelStep;
  float occluded = 0.0;
  for (int i = 0; i < kSampleCount; ++i) {
    vec2 sampleUv = centerUv + kSampleDirections[i] * u_radiusPixels * texelStep;
    if (sampleUv.x < 0.0 || sampleUv.x > 1.0 || sampleUv.y < 0.0 || sampleUv.y > 1.0) {
      continue;
    }

    float sampleDepth = texture(u_depthTex, sampleUv).r;
    if (sampleDepth < centerDepth - 0.0005) {
      occluded += 1.0;
    }
  }

  float occlusion = occluded / float(kSampleCount);
  outOcclusion = clamp(1.0 - u_strength * occlusion, 0.0, 1.0);
}
