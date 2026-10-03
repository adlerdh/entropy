#version 330 core
#include "entropy/FIELD_SAMPLING.glsl"
uniform sampler2D u_first;  // outer map
uniform sampler2D u_second; // inner map
layout(location = 0) out vec4 result;
void main()
{
  vec2 index = floor(gl_FragCoord.xy);
  result = vec4(0.0);
  vec4 inner = sampleField(u_second, index);
  if (inner.w != 1.0) return;
  vec4 outer = sampleField(u_first, index + (u_worldToIndex * inner.xyz).xy);
  if (outer.w != 1.0) return;
  vec4 value = vec4(inner.xyz + outer.xyz, 1.0);
  if (validVector(value)) result = value;
}
