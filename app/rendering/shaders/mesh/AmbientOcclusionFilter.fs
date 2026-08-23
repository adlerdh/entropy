#version 330 core

uniform sampler2D u_occlusionTex;
uniform sampler2D u_normalTex;
uniform sampler2D u_depthTex;
uniform vec2 u_viewportSize;
uniform mat4 u_camera_T_clip;
uniform float u_radiusMm;

layout(location = 0) out float outOcclusion;

vec3 cameraPosition(vec2 uv, float depth)
{
  vec4 camera = u_camera_T_clip * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
  return camera.xyz / camera.w;
}

void main()
{
  ivec2 centerPixel = ivec2(gl_FragCoord.xy);
  float centerDepth = texelFetch(u_depthTex, centerPixel, 0).r;
  vec4 centerEncodedNormal = texelFetch(u_normalTex, centerPixel, 0);
  if (centerDepth >= 1.0 || centerEncodedNormal.a <= 0.0) {
    outOcclusion = 1.0;
    return;
  }

  vec2 invViewport = 1.0 / max(u_viewportSize, vec2(1.0));
  vec2 centerUv = gl_FragCoord.xy * invViewport;
  vec3 centerPosition = cameraPosition(centerUv, centerDepth);
  vec3 centerNormal = normalize(centerEncodedNormal.xyz * 2.0 - 1.0);
  float weightedOcclusion = 0.0;
  float totalWeight = 0.0;
  for (int y = -2; y <= 2; ++y) {
    for (int x = -2; x <= 2; ++x) {
      ivec2 pixel = clamp(centerPixel + ivec2(x, y), ivec2(0), ivec2(u_viewportSize) - ivec2(1));
      float depth = texelFetch(u_depthTex, pixel, 0).r;
      vec4 encodedNormal = texelFetch(u_normalTex, pixel, 0);
      if (depth >= 1.0 || encodedNormal.a <= 0.0) continue;
      vec2 uv = (vec2(pixel) + 0.5) * invViewport;
      vec3 position = cameraPosition(uv, depth);
      vec3 normal = normalize(encodedNormal.xyz * 2.0 - 1.0);
      float spatialWeight = exp(-0.5 * float(x * x + y * y) / 2.0);
      float normalWeight = pow(max(dot(centerNormal, normal), 0.0), 16.0);
      float depthWeight = exp(-8.0 * length(position - centerPosition) / max(u_radiusMm, 0.001));
      float weight = spatialWeight * normalWeight * depthWeight;
      weightedOcclusion += texelFetch(u_occlusionTex, pixel, 0).r * weight;
      totalWeight += weight;
    }
  }
  outOcclusion = totalWeight > 0.0 ? weightedOcclusion / totalWeight : 1.0;
}
