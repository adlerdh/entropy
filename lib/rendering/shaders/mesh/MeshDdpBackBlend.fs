#version 330 core

uniform sampler2D u_backTempTex;

layout(location = 0) out vec4 outBackColor;

void main()
{
  outBackColor = texelFetch(u_backTempTex, ivec2(gl_FragCoord.xy), 0);
  if (outBackColor.a == 0.0) {
    discard;
  }
}
