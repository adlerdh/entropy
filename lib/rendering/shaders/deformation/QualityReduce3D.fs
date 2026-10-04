#version 330 core
uniform sampler3D u_first;
uniform vec3 u_sourceSize;
uniform vec3 u_requestBegin;
uniform vec3 u_requestEnd;
uniform int u_mode; // 0: initial extrema, 1: initial counts, 2: reduce extrema, 3: reduce counts
uniform int u_layer;
layout(location = 0) out vec4 result;
const float huge = 3.402823e38;

bool finite4(vec4 value)
{
  return !any(isnan(value)) && !any(isinf(value));
}
void main()
{
  ivec3 base = ivec3(ivec2(floor(gl_FragCoord.xy)), u_layer) * 4;
  result = (u_mode == 0 || u_mode == 2) ? vec4(huge, -huge, 0.0, 0.0) : vec4(0.0);
  for (int z = 0; z < 4; ++z) {
    for (int y = 0; y < 4; ++y) {
      for (int x = 0; x < 4; ++x) {
        ivec3 at = base + ivec3(x, y, z);
        if (any(greaterThanEqual(at, ivec3(u_sourceSize)))) continue;
        if (u_mode < 2 && (any(lessThan(at, ivec3(u_requestBegin))) || any(greaterThanEqual(at, ivec3(u_requestEnd)))))
          continue;
        vec4 value = texelFetch(u_first, at, 0);
        if (u_mode == 0) {
          if (value.w == 1.0 && finite4(value) && value.y >= 0.0 && value.z >= 0.0) {
            result =
              vec4(min(result.x, value.x), max(result.y, value.x), max(result.z, value.y), max(result.w, value.z));
          }
        }
        else if (u_mode == 1) {
          result.x += 1.0;
          bool valid = value.w == 1.0 && finite4(value) && value.y >= 0.0 && value.z >= 0.0;
          result.y += valid ? 1.0 : 0.0;
          result.z += valid ? 0.0 : 1.0;
          result.w += finite4(value) ? 0.0 : 1.0;
        }
        else if (u_mode == 2) {
          if (value.x <= value.y) {
            result =
              vec4(min(result.x, value.x), max(result.y, value.y), max(result.z, value.z), max(result.w, value.w));
          }
        }
        else {
          result += value;
        }
      }
    }
  }
}
