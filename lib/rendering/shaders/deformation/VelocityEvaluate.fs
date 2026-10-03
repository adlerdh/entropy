#version 330 core
#include "entropy/FIELD_SAMPLING.glsl"
uniform sampler2D u_first; // unweighted cubic generator coefficients
uniform mat3 u_latticeFromWorld;
uniform vec3 u_latticeOrigin; // relative to output origin, subtracted in double on the CPU
uniform vec3 u_center;
uniform float u_radius;
uniform int u_protectionCount;
uniform vec4 u_protection[32]; // relative center, core radius
uniform float u_transition[32];
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
  vec2 index = floor(gl_FragCoord.xy);
  result = vec4(0.0);
  if (!covered(index)) return;
  vec3 p = u_directions * (vec3(index, 0.0) * u_spacing);
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
  vec2 q = (u_latticeFromWorld * (p - u_latticeOrigin)).xy;
  ivec2 size = textureSize(u_first, 0);
  // Reject unrepresentable lookups before converting coordinates to integers.
  if (any(isnan(q)) || any(isinf(q)) || any(lessThan(q, vec2(1.0))) || any(greaterThanEqual(q, vec2(size - 2)))) return;
  ivec2 base = ivec2(floor(q)) - 1;
  vec4 wx = cubic(fract(q.x));
  vec4 wy = cubic(fract(q.y));
  vec3 velocity = vec3(0.0);
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      ivec2 at = base + ivec2(x, y);
      if (any(lessThan(at, ivec2(0))) || any(greaterThanEqual(at, size))) return;
      velocity += wx[x] * wy[y] * texelFetch(u_first, at, 0).xyz;
    }
  }
  vec4 value = vec4(envelope * velocity, 1.0);
  if (validVector(value)) result = value;
}
