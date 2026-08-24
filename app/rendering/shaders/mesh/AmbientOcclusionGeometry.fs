#version 330 core

in vec3 v_worldPosition;
in vec3 v_worldNormal;

uniform int u_clipPlaneCount;
uniform vec4 u_clipPlanes[8];

layout(location = 0) out vec4 outNormal;

void main()
{
  for (int i = 0; i < u_clipPlaneCount; ++i) {
    if (dot(u_clipPlanes[i].xyz, v_worldPosition) + u_clipPlanes[i].w < 0.0) {
      discard;
    }
  }

  vec3 geometricNormal = cross(dFdx(v_worldPosition), dFdy(v_worldPosition));
  vec3 normal =
    dot(v_worldNormal, v_worldNormal) > 0.000001
      ? normalize(v_worldNormal)
      : (dot(geometricNormal, geometricNormal) > 0.000001 ? normalize(geometricNormal) : vec3(0.0, 0.0, 1.0));
  outNormal = vec4(normal * 0.5 + 0.5, 1.0);
}
