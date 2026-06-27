uniform sampler3D u_defTex[3];
uniform mat4 u_defTex_T_world;
uniform mat4 u_sampleTex_T_world;
uniform float u_defSlope_native_T_texture;
uniform float u_deformationStrength;
uniform bool u_defInterleaved;

// Deformation fields are interpreted as sampling fields:
// moving_sample_world = displayed_world + strength * displacement_world(displayed_world).
vec3 sampleDisplacementWorld(vec3 worldPos)
{
  vec3 defTc = vec3(u_defTex_T_world * vec4(worldPos, 1.0));
  if (!isInsideTexture(defTc)) {
    return vec3(0.0);
  }

  vec3 displacement =
    u_defInterleaved
      ? texture(u_defTex[0], defTc).rgb
      : vec3(texture(u_defTex[0], defTc).r, texture(u_defTex[1], defTc).r, texture(u_defTex[2], defTc).r);
  return u_deformationStrength * u_defSlope_native_T_texture * displacement;
}

vec3 sampleTexCoord(vec3 texCoord, vec3 worldPos)
{
  vec3 sampleWorld = worldPos + sampleDisplacementWorld(worldPos);
  return vec3(u_sampleTex_T_world * vec4(sampleWorld, 1.0));
}
