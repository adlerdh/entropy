#version 330 core

uniform sampler2D u_compositeColorTex;
uniform sampler2D u_compositeDepthTex;
uniform sampler2D u_previousDepthBoundsTex;
uniform sampler2D u_previousFrontColorTex;
uniform uint u_ddpDepthOrder;

layout(location = 0) out vec2 outDepthBounds;
layout(location = 1) out vec4 outFrontColor;
layout(location = 2) out vec4 outBackColor;

#include "entropy/DDP_DEPTH_FUNCTIONS.glsl"

void main()
{
  ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
  vec4 premultipliedColor = texelFetch(u_compositeColorTex, pixelCoord, 0);
  if (premultipliedColor.a <= 0.0) {
    discard;
  }

  vec2 previousDepthBounds = texelFetch(u_previousDepthBoundsTex, pixelCoord, 0).xy;
  vec4 previousFrontColor = texelFetch(u_previousFrontColorTex, pixelCoord, 0);
  float depth = texelFetch(u_compositeDepthTex, pixelCoord, 0).r;
  float fragmentDepth = ddpOrderedImagePlaneDepth(depth, u_ddpDepthOrder);

  outDepthBounds = vec2(-kDdpMaxDepth);
  outFrontColor = previousFrontColor;
  outBackColor = vec4(0.0);

  if (ddpDepthIsOutside(fragmentDepth, previousDepthBounds)) {
    return;
  }
  if (ddpDepthIsInterior(fragmentDepth, previousDepthBounds)) {
    outDepthBounds = vec2(-fragmentDepth, fragmentDepth);
    return;
  }
  if (ddpDepthIsNearest(fragmentDepth, previousDepthBounds)) {
    outFrontColor = previousFrontColor + premultipliedColor * (1.0 - previousFrontColor.a);
  }
  else {
    outBackColor = premultipliedColor;
  }
}
