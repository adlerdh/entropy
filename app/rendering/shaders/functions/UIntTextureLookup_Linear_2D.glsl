uniform ivec2 u_tex2DAxes[4];

vec2 texCoord2D(vec3 texCoord, int imageIndex)
{
  ivec2 axes = u_tex2DAxes[imageIndex];
  return vec2(texCoord[axes.x], texCoord[axes.y]);
}

ivec3 segTextureSize()
{
  ivec2 texSize = textureSize(u_segTex, 0);
  ivec3 size = ivec3(1);
  size[u_tex2DAxes[0].x] = texSize.x;
  size[u_tex2DAxes[0].y] = texSize.y;
  return size;
}

uint uintTextureLookup(usampler2D tex, vec3 texCoord)
{
  return texture(tex, texCoord2D(texCoord, 0))[0];
}
