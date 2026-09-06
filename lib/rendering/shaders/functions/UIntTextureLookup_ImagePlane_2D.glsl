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
