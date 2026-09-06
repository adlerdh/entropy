float sampleImageValue(vec3 texCoord)
{
  vec3 worldPos = vec3(u_world_T_tex * vec4(texCoord, 1.0));
  vec3 sampleTc = sampleTexCoord(texCoord, worldPos);
  return isInsideTexture(sampleTc) ? getImageValue(u_imgTex, sampleTc) : 0.0;
}
