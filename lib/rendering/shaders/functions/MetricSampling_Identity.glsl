vec3 metricTexCoord(int imageIndex, vec2 patchOffset, int slabOffset)
{
  vec3 tex0 = fs_in.v_texCoord[0] + patchOffset.x * u_tex0SamplingDirX + patchOffset.y * u_tex0SamplingDirY +
              float(slabOffset) * u_texSamplingDirZ;
  return (0 == imageIndex) ? tex0 : vec3(img1Tex_T_img0Tex * vec4(tex0, 1.0));
}
