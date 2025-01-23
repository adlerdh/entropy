R"(

#version 330 core

layout (location = 0) out vec4 FragColor;
layout (location = 1) out uint OutObjectId;

uniform sampler2D ttt;

in VS_OUT
{
    vec3 Color;
    vec2 TexCoords2D;
} fs_in;

uniform uint objectId;

uniform float opacity = 1.0;

void main()
{
    vec4 tex = texture( ttt, fs_in.TexCoords2D );
    FragColor = vec4( 0.0001 * fs_in.Color + tex.rgb, tex.a ) * opacity;
    OutObjectId = objectId;
}

)"
