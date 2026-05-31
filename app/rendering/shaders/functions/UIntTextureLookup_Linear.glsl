/**
 * @brief Default trilinear texture lookup.
 * @param[in] tex 3D texture sampler
 * @param[in] coord Normalized 3D texture coordinate
 * @return Interpolated texture value
 */
uint uintTextureLookup(usampler3D tex, vec3 texCoords)
{
  return texture(tex, texCoords)[0];

  // vec3 res = vec3(textureSize(tex, 0));
  // vec3 vox = (fract(texCoords) - 0.5 / res) * res;
  // return texelFetch(tex, ivec3(floor(vox)), 0).r;
}
