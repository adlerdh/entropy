uniform ivec2 u_tex2DAxes[4];

vec2 texCoord2D(vec3 texCoord, int imageIndex)
{
  ivec2 axes = u_tex2DAxes[imageIndex];
  return vec2(texCoord[axes.x], texCoord[axes.y]);
}

float floatingPointLinearLookup2D(sampler2D tex, vec2 texCoord)
{
  vec2 res = vec2(textureSize(tex, 0));
  ivec2 texelMax = ivec2(res) - ivec2(1);

  vec2 texel = texCoord * res - 0.5;
  vec2 clampedTexel = clamp(texel, vec2(0.0), res - vec2(1.0));
  ivec2 i = clamp(ivec2(floor(clampedTexel)), ivec2(0), texelMax);
  vec2 w = fract(clampedTexel);

  float t00 = texelFetch(tex, min(i + ivec2(0, 0), texelMax), 0).r;
  float t10 = texelFetch(tex, min(i + ivec2(1, 0), texelMax), 0).r;
  float t01 = texelFetch(tex, min(i + ivec2(0, 1), texelMax), 0).r;
  float t11 = texelFetch(tex, min(i + ivec2(1, 1), texelMax), 0).r;

  return mix(mix(t00, t10, w.x), mix(t01, t11, w.x), w.y);
}

float textureLookup(sampler2D tex, vec3 texCoord)
{
  return floatingPointLinearLookup2D(tex, texCoord2D(texCoord, 0));
}

float textureLookup(sampler2D tex, vec3 texCoord, int imageIndex)
{
  return floatingPointLinearLookup2D(tex, texCoord2D(texCoord, imageIndex));
}
