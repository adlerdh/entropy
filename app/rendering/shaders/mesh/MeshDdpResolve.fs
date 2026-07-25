#version 330 core

uniform sampler2D u_frontColorTex;
uniform sampler2D u_backColorTex;

layout(location = 0) out vec4 outColor;

void main()
{
  ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
  vec4 frontColor = texelFetch(u_frontColorTex, pixelCoord, 0);
  vec4 backColor = texelFetch(u_backColorTex, pixelCoord, 0);
  outColor = frontColor + (1.0 - frontColor.a) * backColor;
}
