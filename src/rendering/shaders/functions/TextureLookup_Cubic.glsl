/**
 * @brief Tricubic interpolated texture look-up. Fast implementation using 8 trilinear lookups.
 * @param[in] tex 3D texture sampler
 * @param[in] coord Normalized 3D texture coordinate
 * @return Interpolated texture value
 * @see https://github.com/DannyRuijters/CubicInterpolationCUDA/blob/master/examples/glCubicRayCast/tricubic.shader
 */
float textureLookup(sampler3D tex, vec3 texCoords)
{
  // Shift the coordinate from [0,1] to [-0.5, nrOfVoxels - 0.5]
  vec3 nrOfVoxels = vec3(textureSize(tex, 0));
  vec3 coord_grid = texCoords * nrOfVoxels - 0.5;
  vec3 index = floor(coord_grid);
  vec3 fraction = coord_grid - index;
  vec3 one_frac = 1.0 - fraction;

  vec3 w0 = 1.0 / 6.0 * one_frac*one_frac*one_frac;
  vec3 w1 = 2.0 / 3.0 - 0.5 * fraction*fraction * (2.0 - fraction);
  vec3 w2 = 2.0 / 3.0 - 0.5 * one_frac*one_frac * (2.0 - one_frac);
  vec3 w3 = 1.0 / 6.0 * fraction * fraction * fraction;

  vec3 g0 = w0 + w1;
  vec3 g1 = w2 + w3;
  vec3 mult = 1.0 / nrOfVoxels;

  // h0 = w1/g0 - 1, move from [-0.5, nrOfVoxels-0.5] to [0,1]
  vec3 h0 = mult * ((w1 / g0) - 0.5 + index);

  // h1 = w3/g1 + 1, move from [-0.5, nrOfVoxels-0.5] to [0,1]
  vec3 h1 = mult * ((w3 / g1) + 1.5 + index);

  // Fetch the eight linear interpolations.
  // Weighting and feching is interleaved for performance and stability reasons.
  float tex000 = texture(tex, h0)[0];
  float tex100 = texture(tex, vec3(h1.x, h0.y, h0.z))[0];

  tex000 = mix(tex100, tex000, g0.x); // weigh along the x-direction
  float tex010 = texture(tex, vec3(h0.x, h1.y, h0.z))[0];
  float tex110 = texture(tex, vec3(h1.x, h1.y, h0.z))[0];

  tex010 = mix(tex110, tex010, g0.x); // weigh along the x-direction
  tex000 = mix(tex010, tex000, g0.y); // weigh along the y-direction

  float tex001 = texture(tex, vec3(h0.x, h0.y, h1.z))[0];
  float tex101 = texture(tex, vec3(h1.x, h0.y, h1.z))[0];

  tex001 = mix(tex101, tex001, g0.x); // weigh along the x-direction

  float tex011 = texture(tex, vec3(h0.x, h1.y, h1.z))[0];
  float tex111 = texture(tex, h1)[0];

  tex011 = mix(tex111, tex011, g0.x); // weigh along the x-direction
  tex001 = mix(tex011, tex001, g0.y); // weigh along the y-direction

  return mix(tex001, tex000, g0.z); // weigh along the z-direction
}
