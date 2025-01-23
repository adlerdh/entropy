#version 330 core

#define N 2

layout (location = 0) in vec2 clipPos;

// View transformation data:
uniform mat4 u_view_T_clip;
uniform mat4 u_world_T_clip;
uniform float u_clipDepth;

// Image transformation data:
uniform mat4 u_imgTexture_T_world[N];
uniform mat4 u_segTexture_T_world[N];

// Vertex shader outputs (varyings):
out VS_OUT
{
    vec3 v_imgTexCoords[N]; // Image texture coords
    vec3 v_segTexCoords[N]; // Seg texture coords
} vs_out;


void main()
{
    vec4 clipPos3d = vec4( clipPos, u_clipDepth, 1.0 );
    gl_Position = u_view_T_clip * clipPos3d;

    vec4 worldPos = u_world_T_clip * clipPos3d;

    for ( int i = 0; i < N; ++i )
    {
        vec4 imgTexPos = u_imgTexture_T_world[i] * worldPos;
        vec4 segTexPos = u_segTexture_T_world[i] * worldPos;

        vs_out.v_imgTexCoords[i] = vec3( imgTexPos / imgTexPos.w );
        vs_out.v_segTexCoords[i] = vec3( segTexPos / segTexPos.w );
    }
}
