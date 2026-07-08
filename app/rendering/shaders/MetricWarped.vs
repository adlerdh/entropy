#version 330 core

layout(location = 0) in vec2 clipPos;

// Transformation uniforms:
uniform mat4 u_view_T_clip;  // Clip to View space
uniform mat4 u_world_T_clip; // Clip to World space
uniform float u_clipDepth;   // view plane depth in Clip space

// Vertex shader outputs/varyings:
out VS_OUT
{
  vec3 v_texCoord[2]; // Image texture coords of the vertex
  vec3 v_worldPos;    // World-space position of the vertex
}
vs_out;

void main()
{
  vec4 clipPos3d = vec4(clipPos, u_clipDepth, 1.0);
  gl_Position = u_view_T_clip * clipPos3d;

  vec4 worldPos = u_world_T_clip * clipPos3d;
  vs_out.v_worldPos = vec3(worldPos / worldPos.w);

  for (int i = 0; i < 2; ++i) {
    vs_out.v_texCoord[i] = vec3(0.0);
  }
}
