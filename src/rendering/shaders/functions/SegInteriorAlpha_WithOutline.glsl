/// This function is used to turn ON outlining of the segmentation.

/// Compute alpha of fragments based on whether or not they are inside the
/// segmentation boundary. Fragments on the boundary are assigned alpha of 1,
/// whereas fragments inside are assigned alpha of 'u_segInteriorOpacity'.
float getSegInteriorAlpha(uint seg)
{
  // Look up texture values in 8 neighbors surrounding the center fragment.
  // These may be either neighboring image voxels or neighboring view pixels.
  // The center fragment (row = 0, col = 0) has index i = 4.

  // Loop over rows in [-1, 0, 1] and cols in [-1, 0, 1]
  for (int i = 0; i <= 8; ++i)
  {
    float row = float(mod(i, 3) - 1);
    float col = float(floor(float(i / 3)) - 1);

    vec3 texPosOffset = row * u_texSamplingDirsForSegOutline[0] +
                        col * u_texSamplingDirsForSegOutline[1];

    // Segmentation value of neighbor at (row, col) offset:
    float ignore;

    if (seg != getSegValue(texPosOffset, ignore))
    {
      // Fragment (with segmentation 'seg') is on the segmentation boundary,
      // since its value is not equal to one of its neighbors. Therefore, it gets full alpha.
      return 1.0;
    }
  }

  return u_segInteriorOpacity;
}
