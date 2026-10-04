#version 330 core
#include "entropy/FIELD_SAMPLING.glsl"
uniform sampler3D u_first;
uniform int u_layer;
uniform int u_protectionCount;
uniform vec4 u_protection[32]; // index-space center xyz, physical core radius w
layout(location = 0) out vec4 result;
void main()
{
  ivec3 cell = ivec3(ivec2(floor(gl_FragCoord.xy)), u_layer);
  result = vec4(0.0, 0.0, 0.0, 1.0);
  if (any(greaterThanEqual(cell + ivec3(1), ivec3(u_end3)))) return;
  bool intersectsCore = false;
  for (int i = 0; i < u_protectionCount; ++i) {
    vec3 center = u_protection[i].xyz;
    vec3 delta = max(max(vec3(cell) - center, center - vec3(cell + ivec3(1))), vec3(0.0));
    float distanceMm = length(delta * u_spacing);
    float radiusMm = u_protection[i].w + 0.01 * max(max(u_spacing.x, u_spacing.y), u_spacing.z);
    intersectsCore = intersectsCore || distanceMm <= radiusMm;
  }
  if (!intersectsCore) return;
  float bound = 0.0;
  for (int z = 0; z < 2; ++z) {
    for (int y = 0; y < 2; ++y) {
      for (int x = 0; x < 2; ++x) {
        vec4 value = texelFetch(u_first, cell + ivec3(x, y, z), 0);
        if (!validVolumeVector(value)) {
          result = vec4(0.0);
          return;
        }
        if (any(notEqual(value.xyz, vec3(0.0)))) bound = max(bound, 1e-30);
        bound = max(bound, length(value.xyz) * 1.0001);
      }
    }
  }
  result = vec4(1.0, bound, 0.0, 1.0);
  if (!finite4(result)) result = vec4(0.0);
}
