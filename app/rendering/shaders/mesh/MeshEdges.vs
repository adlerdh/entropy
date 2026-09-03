#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_color;

uniform mat4 u_clip_T_world;
uniform mat4 u_world_T_mesh;
uniform mat3 u_world_T_meshNormal;
uniform bool u_hasVertexNormals;

out vec3 edge_worldPosition;
out vec3 edge_worldNormal;
out vec4 edge_color;

void main()
{
  vec4 worldPosition = u_world_T_mesh * vec4(a_position, 1.0);
  edge_worldPosition = worldPosition.xyz;
  edge_worldNormal = u_hasVertexNormals ? normalize(u_world_T_meshNormal * a_normal) : vec3(0.0);
  edge_color = a_color;
  gl_Position = u_clip_T_world * worldPosition;
}
