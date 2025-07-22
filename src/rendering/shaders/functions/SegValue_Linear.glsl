const uvec3 neigh[8] = uvec3[8](
  uvec3(0, 0, 0), uvec3(0, 0, 1), uvec3(0, 1, 0), uvec3(0, 1, 1),
  uvec3(1, 0, 0), uvec3(1, 0, 1), uvec3(1, 1, 0), uvec3(1, 1, 1));

/// This does a LINEAR loookup over texture values in the segmentation.
uint getSegValue(vec3 texOffset, out float opacity)
{
  opacity = 1.0;

  vec3 c = floor(fs_in.v_segVoxCoords);
  vec3 d = pow(vec3(textureSize(u_segTex, 0)), vec3(-1));

  // texture coordinates corresponding to the CENTER of the voxel
  vec3 t = vec3(c.x * d.x, c.y * d.y, c.z * d.z) + 0.5 * d;

  uint neighCenterLabels[8];
  for (int i = 0; i < 8; ++i) {
    neighCenterLabels[i] = uintTextureLookup(u_segTex, t + texOffset + neigh[i] * d);
  }

  vec3 fracPart = fs_in.v_segVoxCoords + texOffset * vec3(textureSize(u_segTex, 0)) - c; // fractional part
  vec3 w[2] = vec3[2](vec3(1.0) - fracPart, fracPart); // interpolation weights

  // float segEdgeWidth = 0.02;
  float maxInterp = 0.0;

  // Look up texture values in the fragment and its 8 neighbors.
  // The center fragment (row = 0, col = 0) has index i = 4.
  for (int i = 0; i <= 8; ++i)
  {
    int j = int(mod(i + 4, 9)); // j = [4, 5, 6, 7, 8, 0, 1, 2, 3]
    float row = float(mod(j, 3) - 1); // [-1, 0, 1]
    float col = float(floor(float(j / 3)) - 1); // [-1, 0, 1]

    vec3 texPos = row * u_texSamplingDirsForSmoothSeg[0] + col * u_texSamplingDirsForSmoothSeg[1];

    // Segmentation value of neighbor at (row, col) offset
    uint label = uintTextureLookup(u_segTex, fs_in.v_segTexCoords + texPos);

    float interp = 0.0;
    for (int j = 0; j <= 7; ++j) {
      interp += float(neighCenterLabels[j] == label) * w[neigh[j].x].x * w[neigh[j].y].y * w[neigh[j].z].z;
    }

    // This feathers the edges:
    // opacity = smoothstep(
    //   clamp(u_segInterpCutoff - segEdgeWidth/2.0, 0.0, 1.0),
    //   clamp(u_segInterpCutoff + segEdgeWidth/2.0, 0.0, 1.0), interp);
    // opacity = cubicPulse(u_segInterpCutoff, segEdgeWidth, interp);

    if (interp > maxInterp && interp >= u_segInterpCutoff && computeLabelColor(int(label)).a > 0.0) {
      maxInterp = interp;
      return label;
    }
  }

  return 0u;
}
