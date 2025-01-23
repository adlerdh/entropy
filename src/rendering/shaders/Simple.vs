#version 330 core

layout (location = 0) in vec2 clipPos;

uniform mat4 u_view_T_clip;
uniform float u_clipDepth;
uniform vec2 u_clipMin;
uniform vec2 u_clipMax;

void main()
{
    gl_Position = u_view_T_clip * vec4( clipPos, u_clipDepth, 1.0 );
    gl_Position.xy = clamp( gl_Position.xy, u_clipMin, u_clipMax );
}
