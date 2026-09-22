#version 330 core

layout(location = 0) in vec2 a_position;
uniform samplerBuffer u_fixedValues;
uniform int u_planeSize;
uniform int u_fixedDepth;
uniform int u_sampleOffset;
uniform int u_sampleStride;
#ifdef MOVING_IMAGE_2D
uniform sampler2D u_movingImage;
uniform ivec2 u_movingAxes;
#else
uniform sampler3D u_movingImage;
#endif
uniform mat4 u_moving_T_fixedTexture;
uniform vec2 u_normalization;
uniform int u_textureComponent;
uniform int u_bins;
uniform int u_backgroundRows;

void main()
{
  gl_PointSize = 1.0;
  float slice = (float(gl_InstanceID) + 0.5) / float(u_fixedDepth);
  vec3 tc = vec3(u_moving_T_fixedTexture * vec4(a_position, slice, 1.0));
  if (any(lessThan(tc, vec3(0.0))) || any(greaterThan(tc, vec3(1.0)))) {
    gl_Position = vec4(2.0, 2.0, 0.0, 1.0);
    return;
  }
#ifdef MOVING_IMAGE_2D
  float moving = texture(u_movingImage, vec2(tc[u_movingAxes.x], tc[u_movingAxes.y]))[u_textureComponent];
#else
  float moving = texture(u_movingImage, tc)[u_textureComponent];
#endif
  int index = gl_InstanceID * u_planeSize + u_sampleOffset + gl_VertexID * u_sampleStride;
  vec2 values = vec2(texelFetch(u_fixedValues, index).r, dot(u_normalization, vec2(moving, 1.0)));
  if (any(isnan(values)) || any(isinf(values))) {
    gl_Position = vec4(2.0, 2.0, 0.0, 1.0);
    return;
  }
  ivec2 bin = clamp(ivec2(floor(values * float(u_bins))), ivec2(0), ivec2(u_bins - 1));
  if (u_backgroundRows > 0 && all(equal(bin, ivec2(0)))) {
    bin = ivec2(index % 16, u_bins + (index / 16) % 16);
  }
  gl_Position =
    vec4(2.0 * (vec2(bin) + 0.5) / vec2(max(u_bins, u_backgroundRows), u_bins + u_backgroundRows) - 1.0, 0.0, 1.0);
}
