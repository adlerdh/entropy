#version 330 core
#include "entropy/FIELD_SAMPLING.glsl"
uniform sampler3D u_first;
uniform sampler3D u_second;
uniform int u_layer;
layout(location = 0) out vec4 result;
void main()
{
  vec3 p = vec3(floor(gl_FragCoord.xy), float(u_layer));
  result = vec4(0.0);
  vec4 center = sampleField(u_first, p);
  vec4 left = sampleField(u_first, p - vec3(1, 0, 0));
  vec4 right = sampleField(u_first, p + vec3(1, 0, 0));
  vec4 down = sampleField(u_first, p - vec3(0, 1, 0));
  vec4 up = sampleField(u_first, p + vec3(0, 1, 0));
  vec4 back = sampleField(u_first, p - vec3(0, 0, 1));
  vec4 front = sampleField(u_first, p + vec3(0, 0, 1));
  if (center.w * left.w * right.w * down.w * up.w * back.w * front.w != 1.0) return;
  vec4 inverse = sampleField(u_second, p + u_worldToIndex * center.xyz);
  if (inverse.w != 1.0) return;
  vec3 dx = (right.xyz - left.xyz) / (2.0 * u_spacing.x);
  vec3 dy = (up.xyz - down.xyz) / (2.0 * u_spacing.y);
  vec3 dz = (front.xyz - back.xyz) / (2.0 * u_spacing.z);
  mat3 jac = mat3(
    vec3(1.0 + dot(u_directions[0], dx), dot(u_directions[1], dx), dot(u_directions[2], dx)),
    vec3(dot(u_directions[0], dy), 1.0 + dot(u_directions[1], dy), dot(u_directions[2], dy)),
    vec3(dot(u_directions[0], dz), dot(u_directions[1], dz), 1.0 + dot(u_directions[2], dz)));
  vec3 residual = center.xyz + inverse.xyz;
  vec4 value = vec4(determinant(jac), length(residual), length(u_worldToIndex * residual), 1.0);
  if (finite4(value)) result = value;
}
