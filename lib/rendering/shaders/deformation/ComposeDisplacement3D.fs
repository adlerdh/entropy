#version 330 core
#include "entropy/FIELD_SAMPLING.glsl"
uniform sampler3D u_first;  // outer map
uniform sampler3D u_second; // inner map
uniform int u_layer;
layout(location = 0) out vec4 result;
void main()
{
  vec3 index = vec3(floor(gl_FragCoord.xy), float(u_layer));
  result = vec4(0.0);
  vec4 inner = sampleField(u_second, index);
  if (inner.w != 1.0) return;
  vec4 outer = sampleField(u_first, index + u_worldToIndex * inner.xyz);
  if (outer.w != 1.0) return;
  vec4 value = vec4(inner.xyz + outer.xyz, 1.0);
  if (validVolumeVector(value)) result = value;
}
