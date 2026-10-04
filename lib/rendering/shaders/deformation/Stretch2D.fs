#version 330 core
#include "entropy/FIELD_SAMPLING.glsl"
uniform sampler2D u_first;
layout(location = 0) out vec4 result;
void main()
{
  vec2 p = floor(gl_FragCoord.xy);
  result = vec4(0.0);
  vec4 left = sampleField(u_first, p - vec2(1, 0));
  vec4 right = sampleField(u_first, p + vec2(1, 0));
  vec4 down = sampleField(u_first, p - vec2(0, 1));
  vec4 up = sampleField(u_first, p + vec2(0, 1));
  if (left.w * right.w * down.w * up.w != 1.0) return;
  vec3 dx = (right.xyz - left.xyz) / (2.0 * u_spacing.x);
  vec3 dy = (up.xyz - down.xyz) / (2.0 * u_spacing.y);
  mat2 jac = mat2(
    vec2(1.0 + dot(u_directions[0], dx), dot(u_directions[1], dx)),
    vec2(dot(u_directions[0], dy), 1.0 + dot(u_directions[1], dy)));
  float frob2 = dot(jac[0], jac[0]) + dot(jac[1], jac[1]);
  float upper = sqrt(frob2) * 1.0001;
  float lower = upper > 0.0 ? abs(determinant(jac)) / upper * 0.9999 : 0.0;
  vec4 value = vec4(lower, upper, 0.0, 1.0);
  if (finite4(value)) result = value;
}
