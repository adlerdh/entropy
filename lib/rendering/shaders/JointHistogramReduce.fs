#version 330 core

uniform sampler2D u_source;
uniform ivec2 u_sourceOffset;
layout(location = 0) out float o_count;

void main()
{
  ivec2 origin = u_sourceOffset + 4 * ivec2(gl_FragCoord.xy);
  float count = 0.0;
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      count += texelFetch(u_source, origin + ivec2(x, y), 0).r;
    }
  }
  o_count = count;
}
