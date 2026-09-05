#version 330 core

uniform sampler2D u_depthBoundsTex;

$$DDP_DEPTH_FUNCTIONS$$

void main()
{
  vec2 depthBounds = texelFetch(u_depthBoundsTex, ivec2(gl_FragCoord.xy), 0).xy;
  if (!ddpDepthBoundsAreValid(depthBounds)) {
    discard;
  }
}
