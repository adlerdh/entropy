float raycastJumpDistance(vec3 texCoord)
{
  return float(texture(u_jumpTex, texCoord).r);
}
