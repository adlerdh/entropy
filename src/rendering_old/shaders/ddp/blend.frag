R"(

#version 330 core

// This shader blends the texel in tempTexture atop the draw buffer
// (the back-blender texture). Blending is done with "over" compositing,
// assuming colors that are pre-multiplied by alpha:
// resultRGBA = frontRGBA + ( 1 - frontRGBA.a ) * backRGBA

// Output intermediate blending result:
layout (location = 0) out vec4 BlendedFragColor;

// Front color texture that will be blended over the back-blender texture:
uniform sampler2D tempTexture;

void main()
{
    ivec2 pixelCoord = ivec2( gl_FragCoord.xy );
    BlendedFragColor = texelFetch( tempTexture, pixelCoord, 0 );

    // This check is used for occlusion queries:
    if ( 0.0 == BlendedFragColor.a )
    {
        // If the front color is totally transparent (i.e. there is no
        // further color to blend), then the final result for this fragment
        // has been achieved. We signal this as a "discarded" or "occluded" fragment:
        discard;
    }
}

)"
