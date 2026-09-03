#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_color;

uniform mat4 u_clip_T_world;
uniform mat4 u_world_T_mesh;
uniform mat3 u_world_T_meshNormal;
uniform bool u_hasVertexNormals;

out vec3 v_worldPosition;
out vec3 v_worldNormal;
out vec4 v_color;
noperspective out vec3 v_barycentric;

void main()
{
  vec4 worldPosition = u_world_T_mesh * vec4(a_position, 1.0);
  v_worldPosition = worldPosition.xyz;
  // A zero vector tells the fragment shader to reconstruct a flat geometric normal from screen-space derivatives.
  v_worldNormal = u_hasVertexNormals ? normalize(u_world_T_meshNormal * a_normal) : vec3(0.0);
  v_color = a_color;
  // The ordinary program does not draw topology edges. Its linked fragment shader still consumes this varying.
  v_barycentric = vec3(1.0);
  gl_Position = u_clip_T_world * worldPosition;
}
