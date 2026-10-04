#version 330 core
#include "entropy/FIELD_SAMPLING.glsl"
uniform sampler2D u_first;
uniform int u_protectionCount;
uniform vec4 u_protection[32]; // index-space center xyz, physical core radius w
layout(location = 0) out vec4 result;
void main()
{
  ivec2 cell = ivec2(floor(gl_FragCoord.xy));
  result = vec4(0.0, 0.0, 0.0, 1.0);
  if (any(greaterThanEqual(cell + ivec2(1), u_end))) return;
  bool intersectsCore = false;
  for (int i = 0; i < u_protectionCount; ++i) {
    vec2 center = u_protection[i].xy;
    vec2 delta = max(max(vec2(cell) - center, center - vec2(cell + ivec2(1))), vec2(0.0));
    float distanceMm = length(delta * u_spacing.xy);
    float radiusMm = u_protection[i].w + 0.01 * max(u_spacing.x, u_spacing.y);
    intersectsCore = intersectsCore || distanceMm <= radiusMm;
  }
  if (!intersectsCore) return;
  float bound = 0.0;
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 2; ++x) {
      vec4 value = texelFetch(u_first, cell + ivec2(x, y), 0);
      if (!validVector(value)) {
        result = vec4(0.0);
        return;
      }
      if (any(notEqual(value.xyz, vec3(0.0)))) bound = max(bound, 1e-30);
      bound = max(bound, length(value.xyz) * 1.0001);
    }
  }
  result = vec4(1.0, bound, 0.0, 1.0);
  if (!finite4(result)) result = vec4(0.0);
}
