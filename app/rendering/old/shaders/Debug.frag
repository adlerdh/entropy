R"(

#version 330 core

layout (location = 0) out vec4 FragColor;

uniform sampler2D debugTexture;

void main()
{
    ivec2 pixelCoord = ivec2( gl_FragCoord.xy );
    vec4 T = texelFetch( debugTexture, pixelCoord, 0 );
    FragColor = vec4( T.rgb, 1.0f );
}

)"
