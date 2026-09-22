#version 330 core

layout(location = 0) in vec2 a_position;
#ifdef FIXED_IMAGE_2D
uniform sampler2D u_fixedImage;
uniform ivec2 u_fixedAxes;
#else
uniform sampler3D u_fixedImage;
#endif
uniform int u_fixedDepth;
uniform vec2 u_normalization;
uniform int u_textureComponent;
out float v_fixedValue;

void main()
{
  vec3 tc = vec3(a_position, (float(gl_InstanceID) + 0.5) / float(u_fixedDepth));
#ifdef FIXED_IMAGE_2D
  float value = texture(u_fixedImage, vec2(tc[u_fixedAxes.x], tc[u_fixedAxes.y]))[u_textureComponent];
#else
  float value = texture(u_fixedImage, tc)[u_textureComponent];
#endif
  v_fixedValue = dot(u_normalization, vec2(value, 1.0));
  gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}
