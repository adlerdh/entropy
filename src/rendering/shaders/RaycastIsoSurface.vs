#version 330 core

// View corner vertex position in Clip space
layout (location = 0) in vec2 clipVertexPos;

uniform mat4 u_view_T_clip;
uniform mat4 u_world_T_clip;
uniform float u_clipDepth;

// Vertex shader outputs:
out VS_OUT
{
    vec3 v_worldRayDir; // Ray direction in World space (NOT normalized)
} vs_out;


void main()
{
    // Rays are defined by two points:
    // 1) One point on the plane positioned at NDC depth u_clipDepth
    vec4 clipPos1 = vec4(clipVertexPos, u_clipDepth, 1.0);
    gl_Position = u_view_T_clip * clipPos1;

    // 2) Another point on the far clipping plane positioned at NDC depth 1.0
    vec4 clipPos2 = vec4(clipVertexPos, 1.0, 1.0);

    vec4 worldPos1 = u_world_T_clip * clipPos1;
    worldPos1 /= worldPos1.w;

    vec4 worldPos2 = u_world_T_clip * clipPos2;
    worldPos2 /= worldPos2.w;

    vs_out.v_worldRayDir = vec3(worldPos2 - worldPos1);
}
