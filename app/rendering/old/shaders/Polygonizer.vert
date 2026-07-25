R"(

#version 330 core

layout ( location = 0 ) in vec3 inPosition;

out VS_OUT
{
    vec3 position;
} vs_out;

uniform mat4 tex_O_image;

void main()
{
    vs_out.position = vec3( tex_O_image * vec4( inPosition, 1.0 ) );
}

)"
