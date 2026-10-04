#version 330 core
#include "entropy/FIELD_SAMPLING.glsl"
uniform sampler3D u_first;
uniform sampler3D u_second;
uniform int u_mode; // 0: grid center, 1: cell center
uniform int u_layer;
layout(location = 0) out vec4 result;
void main()
{
  vec3 p = vec3(floor(gl_FragCoord.xy), float(u_layer));
  result = vec4(1.0, 0.0, 0.0, 1.0);
  if (u_mode == 1) {
    if (any(greaterThanEqual(p + vec3(1.0), u_end3))) return;
    p += vec3(0.5);
  }
  vec4 first = sampleField(u_first, p);
  vec4 second = sampleField(u_second, p);
  if (first.w != 1.0 || second.w != 1.0) {
    result = vec4(0.0);
    return;
  }
  float errorMm = length(first.xyz - second.xyz);
  float arithmeticAllowanceMm = errorMm == 0.0 ? 0.0 : 1e-5 * (length(first.xyz) + length(second.xyz));
  result = vec4(1.0, errorMm, arithmeticAllowanceMm, 1.0);
  if (!finite4(result)) result = vec4(0.0);
}
