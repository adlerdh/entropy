R"(

#version 330 core

layout (location = 0) in vec3 modelPos;

out VS_OUT
{
    vec3 Color;
} vs_out;

uniform mat4 world_O_model;
uniform mat4 camera_O_world;
uniform mat4 clip_O_camera;

uniform vec3 color;

void main()
{
    gl_Position = clip_O_camera * camera_O_world * world_O_model * vec4( modelPos, 1.0 );
    vs_out.Color = color;
}

)"
