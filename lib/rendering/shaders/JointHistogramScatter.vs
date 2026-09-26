#version 330 core

#ifdef FIXED_IMAGE_2D
uniform sampler2D u_fixedImage;
#else
uniform sampler3D u_fixedImage;
#endif
#ifdef MOVING_IMAGE_2D
uniform sampler2D u_movingImage;
#else
uniform sampler3D u_movingImage;
#endif
uniform sampler3D u_defTex0[3];
uniform sampler3D u_defTex1[3];
uniform ivec2 u_fixedSizeXY;
uniform int u_fixedDepth;
uniform int u_batchOffset;
uniform int u_batchStride;
uniform ivec2 u_tex2DAxes[2];
uniform mat4 u_world_T_fixedTexture;
uniform mat4 u_tex_T_world[2];
uniform mat4 u_defTex_T_world[2];
uniform vec2 u_normalized_T_texture[2];
uniform ivec2 u_textureComponents;
uniform float u_defSlope_native_T_texture[2];
uniform float u_deformationStrength[2];
uniform bool u_defInterleaved[2];
uniform bool u_warpEnabled[2];
uniform int u_bins;
uniform int u_backgroundRows;

bool inside(vec3 tc)
{
  return all(greaterThanEqual(tc, vec3(0.0))) && all(lessThanEqual(tc, vec3(1.0)));
}

float imageValue(int slot, vec3 tc)
{
  if (slot == 0) {
#ifdef FIXED_IMAGE_2D
    ivec2 axes = u_tex2DAxes[0];
    return texture(u_fixedImage, vec2(tc[axes.x], tc[axes.y]))[u_textureComponents.x];
#else
    return texture(u_fixedImage, tc)[u_textureComponents.x];
#endif
  }
#ifdef MOVING_IMAGE_2D
  ivec2 axes = u_tex2DAxes[1];
  return texture(u_movingImage, vec2(tc[axes.x], tc[axes.y]))[u_textureComponents.y];
#else
  return texture(u_movingImage, tc)[u_textureComponents.y];
#endif
}

vec3 displacement(int slot, vec3 world)
{
  if (!u_warpEnabled[slot]) {
    return vec3(0.0);
  }
  vec3 tc = vec3(u_defTex_T_world[slot] * vec4(world, 1.0));
  if (!inside(tc)) {
    return vec3(0.0);
  }
  vec3 value;
  if (slot == 0) {
    value = u_defInterleaved[0]
              ? texture(u_defTex0[0], tc).rgb
              : vec3(texture(u_defTex0[0], tc).r, texture(u_defTex0[1], tc).r, texture(u_defTex0[2], tc).r);
  }
  else {
    value = u_defInterleaved[1]
              ? texture(u_defTex1[0], tc).rgb
              : vec3(texture(u_defTex1[0], tc).r, texture(u_defTex1[1], tc).r, texture(u_defTex1[2], tc).r);
  }
  return u_deformationStrength[slot] * u_defSlope_native_T_texture[slot] * value;
}

void main()
{
  gl_PointSize = 1.0;
  int linearIndex = u_batchOffset + gl_VertexID * u_batchStride;
  int xy = u_fixedSizeXY.x * u_fixedSizeXY.y;
  ivec3 voxel =
    ivec3(linearIndex % u_fixedSizeXY.x, (linearIndex / u_fixedSizeXY.x) % u_fixedSizeXY.y, linearIndex / xy);
  vec3 fixedTc = (vec3(voxel) + 0.5) / vec3(u_fixedSizeXY, u_fixedDepth);
  vec3 world = vec3(u_world_T_fixedTexture * vec4(fixedTc, 1.0));
  vec3 tc0 = vec3(u_tex_T_world[0] * vec4(world + displacement(0, world), 1.0));
  vec3 tc1 = vec3(u_tex_T_world[1] * vec4(world + displacement(1, world), 1.0));
  if (!inside(tc0) || !inside(tc1)) {
    gl_Position = vec4(2.0, 2.0, 0.0, 1.0);
    return;
  }

  vec2 values = vec2(imageValue(0, tc0), imageValue(1, tc1));
  values =
    vec2(dot(u_normalized_T_texture[0], vec2(values.x, 1.0)), dot(u_normalized_T_texture[1], vec2(values.y, 1.0)));
  if (any(isnan(values)) || any(isinf(values))) {
    gl_Position = vec4(2.0, 2.0, 0.0, 1.0);
    return;
  }
  ivec2 bin = clamp(ivec2(floor(values * float(u_bins))), ivec2(0), ivec2(u_bins - 1));
  // Distribute background writes across a scratch tile instead of contending for one pixel.
  if (u_backgroundRows > 0 && all(equal(bin, ivec2(0)))) {
    bin = ivec2(linearIndex % 16, u_bins + (linearIndex / 16) % 16);
  }
  gl_Position =
    vec4(2.0 * (vec2(bin) + 0.5) / vec2(max(u_bins, u_backgroundRows), u_bins + u_backgroundRows) - 1.0, 0.0, 1.0);
}
