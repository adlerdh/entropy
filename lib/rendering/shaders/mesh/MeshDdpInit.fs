#version 330 core

in vec3 v_worldPosition;
in vec3 v_worldNormal;
in vec4 v_color;

uniform int u_clipPlaneCount;
uniform vec4 u_clipPlanes[8];

layout(location = 0) out vec2 outDepthBounds;

void main()
{
  for (int i = 0; i < clamp(u_clipPlaneCount, 0, 8); ++i) {
    if (dot(u_clipPlanes[i].xyz, v_worldPosition) + u_clipPlanes[i].w < 0.0) {
      discard;
    }
  }

  // GL_MAX blending converts these fragment contributions into (-nearestDepth, farthestDepth) per pixel.
  outDepthBounds = vec2(-gl_FragCoord.z, gl_FragCoord.z);
}
