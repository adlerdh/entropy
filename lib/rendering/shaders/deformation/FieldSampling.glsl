// Integer indices denote sample centers. Values are world-space mm; only lookup
// coordinates are converted to voxel units. Unknown coverage stays unknown.
uniform ivec2 u_begin;
uniform ivec2 u_end;
uniform mat3 u_worldToIndex;
uniform mat3 u_directions;
uniform vec3 u_spacing;

bool finite4(vec4 v)
{
  return !any(isnan(v)) && !any(isinf(v));
}
bool covered(vec2 p)
{
  return !any(isnan(p)) && !any(isinf(p)) && all(greaterThanEqual(p, vec2(u_begin))) &&
         all(lessThanEqual(p, vec2(u_end - 1)));
}
bool validVector(vec4 v)
{
  return finite4(v) && v.w == 1.0 && abs(dot(u_directions[2], v.xyz)) <= 1e-6;
}
vec4 sampleField(sampler2D field, vec2 p)
{
  if (!covered(p)) return vec4(0.0);
  ivec2 lo = ivec2(floor(p));
  ivec2 hi = min(lo + 1, u_end - 1);
  vec2 f = fract(p);
  vec3 result = vec3(0.0);
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 2; ++x) {
      float weight = (x == 0 ? 1.0 - f.x : f.x) * (y == 0 ? 1.0 - f.y : f.y);
      // A zero-weight neighbor is not part of the interpolation support.
      if (weight == 0.0) continue;
      vec4 value = texelFetch(field, ivec2(x == 0 ? lo.x : hi.x, y == 0 ? lo.y : hi.y), 0);
      if (!validVector(value)) return vec4(0.0);
      result += weight * value.xyz;
    }
  }
  vec4 value = vec4(result, 1.0);
  return validVector(value) ? value : vec4(0.0);
}
