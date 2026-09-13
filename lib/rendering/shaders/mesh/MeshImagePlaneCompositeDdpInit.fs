#version 330 core

uniform sampler2D u_compositeColorTex;
uniform sampler2D u_compositeDepthTex;
uniform uint u_ddpDepthOrder;

layout(location = 0) out vec2 outDepthBounds;

#include "entropy/DDP_DEPTH_FUNCTIONS.glsl"

void main()
{
  ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
  if (texelFetch(u_compositeColorTex, pixelCoord, 0).a <= 0.0) {
    discard;
  }

  float depth = texelFetch(u_compositeDepthTex, pixelCoord, 0).r;
  float orderedDepth = ddpOrderedImagePlaneDepth(depth, u_ddpDepthOrder);
  outDepthBounds = vec2(-orderedDepth, orderedDepth);
}
