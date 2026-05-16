/**
 * @brief Compute min/mean/max intensity projection.
 * @param baseTc Texture coordinate of the base sample (view-plane intersection).
 * @param img Image value at baseTc, prior to projection.
 * @return Projected intensity; original value when MIP is not used
 * (i.e. either u_mipMode == NO_IP_MODE or u_halfNumMipSamples == 0)
 */
float computeProjection(vec3 baseTc, float img)
{
  // Number of samples used for computing the final image value:
  int numSamples = 1;

  // Accumulate intensity projection in forwards (+Z) and backwards (-Z) directions:
  for (int dir = -1; dir <= 1; dir += 2) // dir in {-1, 1}
  {
    for (int i = 1; i <= u_halfNumMipSamples; ++i)
    {
      vec3 c = baseTc + dir * i * u_texSamplingDirZ;
      if (!isInsideTexture(c)) { break; }

      float a = clamp(textureLookup(u_imgTex, c), u_imgMinMax[0], u_imgMinMax[1]);

      img = float(NO_IP_MODE == u_mipMode) * img +
            float(MAX_IP_MODE == u_mipMode) * max(img, a) +
            float(MEAN_IP_MODE == u_mipMode) * (img + a) +
            float(MIN_IP_MODE == u_mipMode) * min(img, a);

      ++numSamples;
    }
  }

  // If using Mean IP mode, then normalize by the number of samples
  return img / mix(1.0, float(numSamples), float(MEAN_IP_MODE == u_mipMode));
}
