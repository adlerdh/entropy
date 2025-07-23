#version 330 core

layout (location = 0) in vec2 clipPos;

// Transformation uniforms:
uniform mat4 u_view_T_clip; // Clip to View space
uniform mat4 u_world_T_clip; // Clip to World space
uniform float u_clipDepth; // view plane depth in Clip space
uniform mat4 u_tex_T_world; // World to segmentation texture space
uniform mat4 u_voxel_T_world; // World to segmentation voxel space

// View mode uniforms:
uniform float u_aspectRatio; // view aspect ratio (for checkerboard and flashlight modes)
uniform float u_numCheckers; // number of checker squares along the longest view dimension

// Vertex shader outputs/varyings:
out VS_OUT
{
  vec3 v_texCoord; // Seg texture coords of the vertex
  vec3 v_voxCoord; // Seg voxel coords
  vec2 v_checkerCoord; // Checkerboard square coords
  vec2 v_clipPos; // Clip position
} vs_out;

void main()
{
  vs_out.v_clipPos = clipPos;

  vec2 C = u_numCheckers * 0.5 * (clipPos + vec2(1.0, 1.0));

  vs_out.v_checkerCoord = mix(vec2(C.x, C.y / u_aspectRatio),
    vec2(C.x * u_aspectRatio, C.y), float(u_aspectRatio <= 1.0));

  vec4 clipPos3d = vec4(clipPos, u_clipDepth, 1.0);
  gl_Position = u_view_T_clip * clipPos3d;

  vec4 worldPos = u_world_T_clip * clipPos3d;
  vec4 segTexPos = u_tex_T_world * worldPos;
  vec4 segVoxPos = u_voxel_T_world * worldPos;

  vs_out.v_texCoord = vec3(segTexPos / segTexPos.w);
  vs_out.v_voxCoord = vec3(segVoxPos / segVoxPos.w);
}
