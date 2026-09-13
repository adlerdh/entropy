#version 330 core

in vec3 v_worldPosition;

uniform int u_clipPlaneCount;
uniform vec4 u_clipPlanes[8];
uniform bool u_cutawayEnabled;
uniform vec4 u_cutawayPlanes[3];

void main()
{
  for (int i = 0; i < clamp(u_clipPlaneCount, 0, 8); ++i) {
    if (dot(u_clipPlanes[i].xyz, v_worldPosition) + u_clipPlanes[i].w < 0.0) {
      discard;
    }
  }
  if (
    u_cutawayEnabled && dot(u_cutawayPlanes[0].xyz, v_worldPosition) + u_cutawayPlanes[0].w >= 0.0 &&
    dot(u_cutawayPlanes[1].xyz, v_worldPosition) + u_cutawayPlanes[1].w >= 0.0 &&
    dot(u_cutawayPlanes[2].xyz, v_worldPosition) + u_cutawayPlanes[2].w >= 0.0)
  {
    discard;
  }
}
