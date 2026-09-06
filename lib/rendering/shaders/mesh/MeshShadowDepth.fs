#version 330 core

in vec3 v_worldPosition;

uniform int u_clipPlaneCount;
uniform vec4 u_clipPlanes[8];

void main()
{
  for (int i = 0; i < clamp(u_clipPlaneCount, 0, 8); ++i) {
    if (dot(u_clipPlanes[i].xyz, v_worldPosition) + u_clipPlanes[i].w < 0.0) {
      discard;
    }
  }
}
