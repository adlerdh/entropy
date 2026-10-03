#version 330 core
#include "entropy/FIELD_SAMPLING.glsl"
uniform sampler2D u_first;
uniform float u_scale;
uniform bool u_identity;
layout(location = 0) out vec4 result;
void main()
{
  vec2 index = floor(gl_FragCoord.xy);
  result = vec4(0.0);
  if (!covered(index)) return;
  if (u_identity) {
    result = vec4(0.0, 0.0, 0.0, 1.0);
    return;
  }
  vec4 value = sampleField(u_first, index);
  if (value.w != 1.0) return;
  value.xyz *= u_scale;
  if (validVector(value)) result = value;
}
