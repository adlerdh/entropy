// 3D texture coordinates (s,t,p) are in [0.0, 1.0]^3
#define MIN_VOX_COORD ivec3(0)

/**
 * @brief Trilinear interpolation using floating points as alternative to GPU-based interpolation,
 * which is based on 24.8 fixed-point arithmetic with 8 bits for the fractional part, and hence 256
 * intermediate values between adjacent pixels.
 *
 * The GPU uses limited-precision sampler weight / coordinate math in the fixed-function texture unit.
 * This is not limited precision of the stored texel values themselves.
 *
 *
 * For hardware trilinear filtering, the GPU does:
 * 1. Convert normalized texture coordinates to texel space (multiply by size, apply wrapping/clamp, subtract 0.5).
 * 2. Split into integer base index + fractional part.
 * 3. Use the fractional part as the interpolation weights for the 8 neighbors.
 * On many GPUs, steps 2 and 3 use a fixed-point representation for the fractional coordinate
 * (or equivalently for the lerp weights). A common precision is 8 fractional bits (described as "24.8" or
 * "8-bit sub-texel precision"), meaning there are only 256 distinct weight values between adjacent texels
 * along an axis. That produces stair step artifacts: the weights only change in 1/256 increments,
 * so the output changes in visible quantized steps when the texture values are smooth/high precision.
 *
 * This version does two key things differently:
 * 1. It uses texelFetch to load the 8 texels exactly (no filtering).
 * 2. It computes the weights w in floating point and performs the lerps (mix) in floating point.
 * So the interpolation parameter is not quantized to 8-bit (or whatever the sampler uses).
 * Instead, it has ~23 bits of mantissa if the shader is using 32-bit floats.
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
  ivec3 voxMax = ivec3(res) - ivec3(1);

  vec3 vox = (fract(texCoords) - 0.5 / res) * res;
  ivec3 i = max(ivec3(floor(vox)), MIN_VOX_COORD);
  vec3 w = fract(vox);

  float t000 = texelFetch(tex, min(i + ivec3(0, 0, 0), voxMax), 0)[0];
  float t100 = texelFetch(tex, min(i + ivec3(1, 0, 0), voxMax), 0)[0];
  float t010 = texelFetch(tex, min(i + ivec3(0, 1, 0), voxMax), 0)[0];
  float t110 = texelFetch(tex, min(i + ivec3(1, 1, 0), voxMax), 0)[0];
  float t001 = texelFetch(tex, min(i + ivec3(0, 0, 1), voxMax), 0)[0];
  float t101 = texelFetch(tex, min(i + ivec3(1, 0, 1), voxMax), 0)[0];
  float t011 = texelFetch(tex, min(i + ivec3(0, 1, 1), voxMax), 0)[0];
  float t111 = texelFetch(tex, min(i + ivec3(1, 1, 1), voxMax), 0)[0];

  float tx00 = mix(t000, t100, w.x);
  float tx10 = mix(t010, t110, w.x);
  float tx01 = mix(t001, t101, w.x);
  float tx11 = mix(t011, t111, w.x);

  float txy0 = mix(tx00, tx10, w.y);
  float txy1 = mix(tx01, tx11, w.y);

  return mix(txy0, txy1, w.z);
}
