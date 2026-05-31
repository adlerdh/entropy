R"(

#version 330 core

layout (location = 0) out vec4 OutMinMaxDepths;
layout (location = 1) out vec4 OutFrontColor;
layout (location = 2) out vec4 OutBackColor;

in VS_OUT
{
    vec3 Color;
} fs_in;

//uniform uint objectId;

uniform float opacity = 1.0;

uniform sampler2D depthBlenderTex; // depth blending output
uniform sampler2D frontBlenderTex; // front blending output

#define MAX_DEPTH 1.0


vec4 computeShadedColor()
{
    return vec4( fs_in.Color, 1.0 ) * opacity;
}


void main()
{
    float epsilon = 0.000001;

    float fragDepth = gl_FragCoord.z;

    // Works like floor
    ivec2 pixelCoord = ivec2( gl_FragCoord.xy );

    //get the depth value from the depth blending output
    vec2 minMaxDepths = texelFetch( depthBlenderTex, pixelCoord, 0 ).xy;

    float nearestDepth = -minMaxDepths.x;
    float farthestDepth = minMaxDepths.y;

    // get the front blending output
    vec4 frontColor = texelFetch( frontBlenderTex, pixelCoord, 0 );

    // Depths and 1.0 - alphaMult always increase
    // so we can use pass-through by default with MAX blending
    OutMinMaxDepths.xy = vec2(-MAX_DEPTH);

    // Front colors always increase (DST += SRC*ALPHA_MULT)
    // so we can use pass-through by default with MAX blending
    OutFrontColor = frontColor;

    // Because over blending makes color increase or decrease,
    // we cannot pass-through by default.
    // Each pass, only one fragment can a color greater than 0
    OutBackColor = vec4(0.0);


    if (fragDepth < nearestDepth - epsilon || fragDepth > farthestDepth + epsilon)
    {
        // Skip this depth in the peeling algorithm
        return;
    }

    if (fragDepth > nearestDepth + epsilon && fragDepth < farthestDepth - epsilon)
    {
        // This fragment needs to be peeled again
        OutMinMaxDepths.xy = vec2(-fragDepth, fragDepth);
        return;
    }

    // If we made it here, this fragment is on the peeled layer from last pass
    // therefore, we need to shade it, and make sure it is not peeled any farther

    vec4 shadedColor = computeShadedColor();

    // if the fragment depth is the nearest depth, we blend the colour to the second attachment
    if ( fragDepth >= nearestDepth - epsilon &&
         fragDepth <= nearestDepth + epsilon )
    {
        OutFrontColor = frontColor + shadedColor * ( 1.0 - frontColor.a );
    }
    //    else if ( fragDepth >= farthestDepth - epsilon ||
    //              fragDepth <= farthestDepth + epsilon )
    else
    {
        OutBackColor = shadedColor;
    }
}

)"
