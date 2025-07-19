/**
 * @brief Default trilinear texture lookup.
 * @param[in] tex 3D texture sampler
 * @param[in] coord Normalized 3D texture coordinate
 * @return Interpolated texture value
 */
uint uintTextureLookup(usampler3D tex, vec3 texCoords)
{
  return texture(tex, texCoords)[0];
}
