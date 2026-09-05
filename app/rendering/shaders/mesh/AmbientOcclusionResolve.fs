#version 330 core

uniform sampler2D u_normalTex;
uniform sampler2D u_depthTex;
uniform vec2 u_viewportSize;
uniform mat4 u_camera_T_clip;
uniform mat4 u_clip_T_camera;
uniform mat3 u_camera_T_worldNormal;
uniform float u_radiusMm;
uniform float u_strength;
uniform int u_sampleCount;

layout(location = 0) out float outOcclusion;

const int kMaxSampleCount = 64;
const float kGoldenAngle = 2.39996323;

vec3 cameraPosition(vec2 uv, float depth)
{
  vec4 camera = u_camera_T_clip * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
  return camera.xyz / camera.w;
}

float pixelRotation(ivec2 pixel)
{
  return fract(sin(dot(vec2(pixel), vec2(12.9898, 78.233))) * 43758.5453) * 6.28318531;
}

void main()
{
  ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
  float centerDepth = texelFetch(u_depthTex, pixelCoord, 0).r;
  vec4 encodedNormal = texelFetch(u_normalTex, pixelCoord, 0);
  if (centerDepth >= 1.0 || encodedNormal.a <= 0.0) {
    outOcclusion = 1.0;
    return;
  }

  vec2 invViewport = 1.0 / max(u_viewportSize, vec2(1.0));
  vec2 centerUv = gl_FragCoord.xy * invViewport;
  vec3 center = cameraPosition(centerUv, centerDepth);
  vec3 normal = normalize(u_camera_T_worldNormal * (encodedNormal.xyz * 2.0 - 1.0));
  float rotation = pixelRotation(pixelCoord);
  vec3 helperAxis = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
  vec3 tangent = normalize(cross(helperAxis, normal));
  vec3 bitangent = cross(normal, tangent);
  mat3 tangent_T_camera = mat3(tangent, bitangent, normal);

  int count = clamp(u_sampleCount, 1, kMaxSampleCount);
  float occlusion = 0.0;
  for (int i = 0; i < kMaxSampleCount; ++i) {
    if (i >= count) break;
    float sequence = (float(i) + 0.5) / float(count);
    float z = mix(0.15, 1.0, sequence);
    float radial = sqrt(max(1.0 - z * z, 0.0));
    float angle = float(i) * kGoldenAngle + rotation;
    vec3 hemisphere = vec3(cos(angle) * radial, sin(angle) * radial, z);
    float scale = mix(0.1, 1.0, sequence * sequence);
    vec3 samplePosition = center + tangent_T_camera * hemisphere * (u_radiusMm * scale);

    vec4 sampleClip = u_clip_T_camera * vec4(samplePosition, 1.0);
    if (sampleClip.w <= 0.0) continue;
    vec3 sampleNdc = sampleClip.xyz / sampleClip.w;
    if (sampleNdc.z <= -1.0 || sampleNdc.z >= 1.0) continue;
    vec2 sampleUv = sampleNdc.xy * 0.5 + 0.5;
    if (any(lessThanEqual(sampleUv, vec2(0.0))) || any(greaterThanEqual(sampleUv, vec2(1.0)))) continue;

    float sampleDepth = texture(u_depthTex, sampleUv).r;
    if (sampleDepth >= 1.0) continue;
    vec3 sampledPosition = cameraPosition(sampleUv, sampleDepth);
    float separation = length(sampledPosition - center);
    float rangeWeight = 1.0 - smoothstep(u_radiusMm * 0.5, u_radiusMm, separation);
    float bias = max(0.002 * u_radiusMm, 0.001);
    occlusion += (sampledPosition.z >= samplePosition.z + bias ? 1.0 : 0.0) * rangeWeight;
  }

  // Background and off-screen samples are deliberately unoccluded; retaining them in the denominator prevents
  // silhouettes and viewport edges from becoming artificial dark halos.
  float normalized = occlusion / float(count);
  outOcclusion = clamp(1.0 - u_strength * normalized, 0.0, 1.0);
}
