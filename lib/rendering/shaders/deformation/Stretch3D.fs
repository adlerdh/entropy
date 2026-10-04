#version 330 core
#include "entropy/FIELD_SAMPLING.glsl"
uniform sampler3D u_first;
uniform int u_layer;
layout(location = 0) out vec4 result;
void main()
{
  vec3 p = vec3(floor(gl_FragCoord.xy), float(u_layer));
  result = vec4(0.0);
  vec4 left = sampleField(u_first, p - vec3(1, 0, 0));
  vec4 right = sampleField(u_first, p + vec3(1, 0, 0));
  vec4 down = sampleField(u_first, p - vec3(0, 1, 0));
  vec4 up = sampleField(u_first, p + vec3(0, 1, 0));
  vec4 back = sampleField(u_first, p - vec3(0, 0, 1));
  vec4 front = sampleField(u_first, p + vec3(0, 0, 1));
  if (left.w * right.w * down.w * up.w * back.w * front.w != 1.0) return;
  vec3 dx = (right.xyz - left.xyz) / (2.0 * u_spacing.x);
  vec3 dy = (up.xyz - down.xyz) / (2.0 * u_spacing.y);
  vec3 dz = (front.xyz - back.xyz) / (2.0 * u_spacing.z);
  mat3 jac = mat3(
    vec3(1.0 + dot(u_directions[0], dx), dot(u_directions[1], dx), dot(u_directions[2], dx)),
    vec3(dot(u_directions[0], dy), 1.0 + dot(u_directions[1], dy), dot(u_directions[2], dy)),
    vec3(dot(u_directions[0], dz), dot(u_directions[1], dz), 1.0 + dot(u_directions[2], dz)));
  float frob2 = dot(jac[0], jac[0]) + dot(jac[1], jac[1]) + dot(jac[2], jac[2]);
  float upper = sqrt(frob2) * 1.0001;
  // sigma_min >= 2*abs(det(J))/||J||_F^2 for a 3x3 matrix.
  float lower = frob2 > 0.0 ? 2.0 * abs(determinant(jac)) / frob2 * 0.9999 : 0.0;
  vec4 value = vec4(lower, upper, 0.0, 1.0);
  if (finite4(value)) result = value;
}
