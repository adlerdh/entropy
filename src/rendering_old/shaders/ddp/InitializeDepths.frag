R"(

#version 330 core

// Output the -/+ fragment depth in red/green color channels.
// Min/max blending during dual depth peeling will peel away the
// front-most and back-most layers on each render.

layout (location = 0) out vec4 DepthsNegPos;

uniform sampler2D opaqueDepthTex;

void main()
{
    ivec2 pixelCoord = ivec2( gl_FragCoord.xy );
    float opaqueDepth = texelFetch( opaqueDepthTex, pixelCoord, 0 ).r;

    // The incoming fragment has depth 'gl_FragCoord.z'

    if ( opaqueDepth != -1.0 && gl_FragCoord.z > opaqueDepth )
    {
        // The incoming fragment is occluded by an opaque fragment at depth 'opaqueDepth':
        // Ignore the incoming fragment occluded by opaque geometry.
        DepthsNegPos.xy = vec2( -1.0, opaqueDepth );
    }
    else
    {
        // The incoming fragment is in front of the last opaque fragment.
        DepthsNegPos.xy = vec2( -gl_FragCoord.z, gl_FragCoord.z );
    }
}

)"
