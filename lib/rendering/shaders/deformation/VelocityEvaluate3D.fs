#version 330 core
#include "entropy/FIELD_SAMPLING.glsl"
uniform sampler3D u_first; // unweighted cubic generator coefficients
uniform mat3 u_latticeFromWorld;
uniform vec3 u_latticeOrigin;
uniform vec3 u_center;
uniform float u_radius;
uniform int u_protectionCount;
uniform vec4 u_protection[32]; // relative center, core radius
uniform float u_transition[32];
uniform int u_layer;
layout(location = 0) out vec4 result;

vec4 cubic(float t)
{
  float s = 1.0 - t;
  return vec4(
           s * s * s,
           3.0 * t * t * t - 6.0 * t * t + 4.0,
           -3.0 * t * t * t + 3.0 * t * t + 3.0 * t + 1.0,
           t * t * t) /
         6.0;
}
void main()
{
  vec3 index = vec3(floor(gl_FragCoord.xy), float(u_layer));
  result = vec4(0.0);
  if (!covered(index)) return;
  vec3 p = u_directions * (index * u_spacing);
  float r = length(p - u_center) / u_radius;
  float envelope = 0.0;
  if (r < 1.0) {
    float s = 1.0 - r;
    envelope = s * s * s * s * (4.0 * r + 1.0);
  }
  for (int i = 0; i < u_protectionCount; ++i) {
    float t = clamp((length(p - u_protection[i].xyz) - u_protection[i].w) / u_transition[i], 0.0, 1.0);
    envelope *= t * t * t * (10.0 + t * (-15.0 + 6.0 * t));
  }
  if (envelope == 0.0) {
    result = vec4(0.0, 0.0, 0.0, 1.0);
    return;
  }
  vec3 q = u_latticeFromWorld * (p - u_latticeOrigin);
  ivec3 size = textureSize(u_first, 0);
  if (any(isnan(q)) || any(isinf(q)) || any(lessThan(q, vec3(1.0))) || any(greaterThanEqual(q, vec3(size - 2)))) return;
  ivec3 base = ivec3(floor(q)) - 1;
  vec4 wx = cubic(fract(q.x));
  vec4 wy = cubic(fract(q.y));
  vec4 wz = cubic(fract(q.z));
  vec3 velocity = vec3(0.0);
  for (int z = 0; z < 4; ++z) {
    for (int y = 0; y < 4; ++y) {
      for (int x = 0; x < 4; ++x) {
        ivec3 at = base + ivec3(x, y, z);
        if (any(lessThan(at, ivec3(0))) || any(greaterThanEqual(at, size))) return;
        velocity += wx[x] * wy[y] * wz[z] * texelFetch(u_first, at, 0).xyz;
      }
    }
  }
  vec4 value = vec4(envelope * velocity, 1.0);
  if (validVolumeVector(value)) result = value;
}
