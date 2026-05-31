R"(

#version 330 core

layout (location = 0) out vec4 FragColor;
layout (location = 1) out uint OutObjectId;

in VS_OUT
{
    vec3 Color;
} fs_in;

uniform uint objectId;

uniform float opacity = 1.0;

void main()
{
    FragColor = vec4( fs_in.Color, 1.0 ) * opacity;
    OutObjectId = objectId;
}

)"
