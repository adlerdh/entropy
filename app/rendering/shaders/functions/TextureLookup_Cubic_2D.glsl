uniform ivec2 u_tex2DAxes[4];

vec2 texCoord2D(vec3 texCoord, int imageIndex)
{
  ivec2 axes = u_tex2DAxes[imageIndex];
  return vec2(texCoord[axes.x], texCoord[axes.y]);
}

float bicubicLookup2D(sampler2D tex, vec2 texCoord)
{
  vec2 nrOfTexels = vec2(textureSize(tex, 0));
  vec2 coordGrid = texCoord * nrOfTexels - 0.5;
  ivec2 index = ivec2(floor(coordGrid));
  vec2 fraction = coordGrid - index;
  vec2 oneFraction = 1.0 - fraction;

  vec2 w0 = oneFraction * oneFraction * oneFraction / 6.0;
  vec2 w1 = 2.0 / 3.0 - 0.5 * fraction * fraction * (2.0 - fraction);
  vec2 w2 = 2.0 / 3.0 - 0.5 * oneFraction * oneFraction * (2.0 - oneFraction);
  vec2 w3 = fraction * fraction * fraction / 6.0;
  vec4 wx = vec4(w0.x, w1.x, w2.x, w3.x);
  vec4 wy = vec4(w0.y, w1.y, w2.y, w3.y);

  ivec2 texelMax = ivec2(nrOfTexels) - ivec2(1);
  float value = 0.0;
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      ivec2 sampleIndex = clamp(index + ivec2(x - 1, y - 1), ivec2(0), texelMax);
      value += wx[x] * wy[y] * texelFetch(tex, sampleIndex, 0).r;
    }
  }
  return value;
}

float textureLookup(sampler2D tex, vec3 texCoord)
{
  if (!isInsideTexture(texCoord)) {
    return 0.0;
  }

  vec2 tc2D = texCoord2D(texCoord, 0);
  return bicubicLookup2D(tex, tc2D);
}

float textureLookup(sampler2D tex, vec3 texCoord, int imageIndex)
{
  if (!isInsideTexture(texCoord)) {
    return 0.0;
  }

  vec2 tc2D = texCoord2D(texCoord, imageIndex);
  return bicubicLookup2D(tex, tc2D);
}
