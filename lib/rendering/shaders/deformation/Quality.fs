#version 330 core
#include "entropy/FIELD_SAMPLING.glsl"
uniform sampler2D u_first;
uniform sampler2D u_second;
layout(location = 0) out vec4 result;
void main()
{
  vec2 p = floor(gl_FragCoord.xy);
  result = vec4(0.0);
  vec4 center = sampleField(u_first, p);
  vec4 left = sampleField(u_first, p - vec2(1, 0));
  vec4 right = sampleField(u_first, p + vec2(1, 0));
  vec4 down = sampleField(u_first, p - vec2(0, 1));
  vec4 up = sampleField(u_first, p + vec2(0, 1));
  if (center.w * left.w * right.w * down.w * up.w != 1.0) return;
  vec4 inverse = sampleField(u_second, p + (u_worldToIndex * center.xyz).xy);
  if (inverse.w != 1.0) return;
  vec3 dx = (right.xyz - left.xyz) / (2.0 * u_spacing.x);
  vec3 dy = (up.xyz - down.xyz) / (2.0 * u_spacing.y);
  mat2 jac = mat2(
    vec2(1.0 + dot(u_directions[0], dx), dot(u_directions[1], dx)),
    vec2(dot(u_directions[0], dy), 1.0 + dot(u_directions[1], dy)));
  vec3 residual = center.xyz + inverse.xyz;
  vec4 value = vec4(determinant(jac), length(residual), length((u_worldToIndex * residual).xy), 1.0);
  if (finite4(value)) result = value;
}
