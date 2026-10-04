#version 330 core
#include "entropy/FIELD_SAMPLING.glsl"
uniform sampler3D u_first;
uniform int u_layer;
uniform float u_scale;
uniform bool u_identity;
layout(location = 0) out vec4 result;
void main()
{
  vec3 index = vec3(floor(gl_FragCoord.xy), float(u_layer));
  result = vec4(0.0);
  if (!covered(index)) return;
  if (u_identity) {
    result = vec4(0.0, 0.0, 0.0, 1.0);
    return;
  }
  vec4 value = sampleField(u_first, index);
  if (value.w != 1.0) return;
  value.xyz *= u_scale;
  if (validVolumeVector(value)) result = value;
}
