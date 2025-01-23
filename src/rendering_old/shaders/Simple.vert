R"(

#version 330 core

layout (location = 0) in vec3 modelPos;
layout (location = 1) in vec2 texCoords2D;

out VS_OUT
{
    vec3 Color;
    vec2 TexCoords2D;
} vs_out;

uniform mat4 world_O_model;
uniform mat4 camera_O_world;
uniform mat4 clip_O_camera;

uniform vec3 color;

void main()
{
    gl_Position = clip_O_camera * camera_O_world * world_O_model * vec4( modelPos, 1.0 );

    vs_out.Color = color;
    vs_out.TexCoords2D = texCoords2D;
}

)"
