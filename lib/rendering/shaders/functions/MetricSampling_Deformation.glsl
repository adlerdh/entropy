uniform sampler3D u_defTex0[3];
uniform sampler3D u_defTex1[3];
uniform mat4 u_defTex_T_world[2];
uniform mat4 u_sampleTex_T_world[2];
uniform float u_defSlope_native_T_texture[2];
uniform float u_deformationStrength[2];
uniform bool u_defInterleaved[2];
uniform bool u_warpEnabled[2];
uniform vec3 u_worldSamplingDirX;
uniform vec3 u_worldSamplingDirY;
uniform vec3 u_worldSamplingDirZ;

vec3 metricDisplacementWorld(int imageIndex, vec3 worldPos)
{
  if (!u_warpEnabled[imageIndex]) {
    return vec3(0.0);
  }

  vec3 defTc = vec3(u_defTex_T_world[imageIndex] * vec4(worldPos, 1.0));
  if (!isInsideTexture(defTc)) {
    return vec3(0.0);
  }

  vec3 displacement = vec3(0.0);
  if (0 == imageIndex) {
    displacement =
      u_defInterleaved[0]
        ? texture(u_defTex0[0], defTc).rgb
        : vec3(texture(u_defTex0[0], defTc).r, texture(u_defTex0[1], defTc).r, texture(u_defTex0[2], defTc).r);
  }
  else {
    displacement =
      u_defInterleaved[1]
        ? texture(u_defTex1[0], defTc).rgb
        : vec3(texture(u_defTex1[0], defTc).r, texture(u_defTex1[1], defTc).r, texture(u_defTex1[2], defTc).r);
  }

  return u_deformationStrength[imageIndex] * u_defSlope_native_T_texture[imageIndex] * displacement;
}

vec3 metricTexCoord(int imageIndex, vec2 patchOffset, int slabOffset)
{
  vec3 worldPos = fs_in.v_worldPos + patchOffset.x * u_worldSamplingDirX + patchOffset.y * u_worldSamplingDirY +
                  float(slabOffset) * u_worldSamplingDirZ;
  vec3 sampleWorld = worldPos + metricDisplacementWorld(imageIndex, worldPos);
  return vec3(u_sampleTex_T_world[imageIndex] * vec4(sampleWorld, 1.0));
}
