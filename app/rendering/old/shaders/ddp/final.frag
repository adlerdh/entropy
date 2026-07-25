R"(

#version 330 core

// Output the final blended color
layout (location = 0) out vec4 FragColor;

in VS_OUT
{
    vec2 TexCoords;
} fs_in;

uniform sampler2D frontBlenderTex; // front blending output
uniform sampler2D backBlenderTex; // back blending output

void main()
{
    // This is a 'floor' operation:
    ivec2 pixelCoord = ivec2( gl_FragCoord.xy );

    vec4 frontColor = texelFetch( frontBlenderTex, pixelCoord, 0 );
    vec4 backColor = texelFetch( backBlenderTex, pixelCoord, 0 );

    FragColor = frontColor + ( 1.0 - frontColor.a ) * backColor;
}

)"
