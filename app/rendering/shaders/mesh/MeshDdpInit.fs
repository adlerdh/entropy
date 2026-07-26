#version 330 core

in vec3 v_worldPosition;
in vec3 v_worldNormal;
in vec4 v_color;

uniform vec4 u_baseColor;
uniform bool u_hasVertexColors;
uniform vec3 u_cameraWorldPosition;
uniform int u_clipPlaneCount;
uniform vec4 u_clipPlanes[8];

layout(location = 0) out vec2 outDepthBounds;

void main()
{
  for (int i = 0; i < u_clipPlaneCount; ++i) {
    if (dot(u_clipPlanes[i].xyz, v_worldPosition) + u_clipPlanes[i].w < 0.0) {
      discard;
    }
  }

  // GL_MAX blending converts these fragment contributions into (-nearestDepth, farthestDepth) per pixel.
  outDepthBounds = vec2(-gl_FragCoord.z, gl_FragCoord.z);
}
