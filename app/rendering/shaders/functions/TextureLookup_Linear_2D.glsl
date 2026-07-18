uniform ivec2 u_tex2DAxes[4];

vec2 texCoord2D(vec3 texCoord, int imageIndex)
{
  ivec2 axes = u_tex2DAxes[imageIndex];
  return vec2(texCoord[axes.x], texCoord[axes.y]);
}

float textureLookup(sampler2D tex, vec3 texCoord)
{
  return texture(tex, texCoord2D(texCoord, 0))[0];
}

float textureLookup(sampler2D tex, vec3 texCoord, int imageIndex)
{
  return texture(tex, texCoord2D(texCoord, imageIndex))[0];
}
