/**
 * @brief Trilinear interpolation using floating points as alternative to GPU-based interpolation,
 * which is based on 24.8 fixed-point arithmetic with 8 bits for the fractional part, and hence 256
 * intermediate values between adjacent pixels.
 *
 * @see https://iquilezles.org/articles/interpolation/
 *
 * @param[in] tex 3D texture sampler
 * @param[in] coord Normalized 3D texture coordinate
 * @return Interpolated texture value
 */
float textureLookup(sampler3D tex, vec3 texCoords)
{
  vec3 res = vec3(textureSize(tex, 0));
  vec3 vox = (fract(texCoords) - 0.5 / res) * res;
  ivec3 i = ivec3(floor(vox));
  vec3 w = fract(vox);

  float t000 = texelFetch(tex, (i + ivec3(0, 0, 0)), 0)[0];
  float t100 = texelFetch(tex, (i + ivec3(1, 0, 0)), 0)[0];
  float t010 = texelFetch(tex, (i + ivec3(0, 1, 0)), 0)[0];
  float t110 = texelFetch(tex, (i + ivec3(1, 1, 0)), 0)[0];
  float t001 = texelFetch(tex, (i + ivec3(0, 0, 1)), 0)[0];
  float t101 = texelFetch(tex, (i + ivec3(1, 0, 1)), 0)[0];
  float t011 = texelFetch(tex, (i + ivec3(0, 1, 1)), 0)[0];
  float t111 = texelFetch(tex, (i + ivec3(1, 1, 1)), 0)[0];

  float tx00 = mix(t000, t100, w.x);
  float tx10 = mix(t010, t110, w.x);
  float tx01 = mix(t001, t101, w.x);
  float tx11 = mix(t011, t111, w.x);

  float txy0 = mix(tx00, tx10, w.y);
  float txy1 = mix(tx01, tx11, w.y);

  return mix(txy0, txy1, w.z);
}
