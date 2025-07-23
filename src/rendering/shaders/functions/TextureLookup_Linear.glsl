/**
 * @brief Default (GPU fixed-point) trilinear texture lookup.
 * @param[in] tex 3D floating point texture sampler
 * @param[in] texCoord Normalized 3D texture coordinate
 * @return Interpolated texture value
 */
float textureLookup(sampler3D tex, vec3 texCoord)
{
  return texture(tex, texCoord)[0];
}
