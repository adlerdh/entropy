R"glsl(

#version 330 core

layout ( points ) in;

// Maximum number of vertices that will be written by a single invocation of the
// geometry shader is 15 (i.e. 5 triangles)
layout ( triangle_strip, max_vertices = 15 ) out;


in VS_OUT
{
    vec3 position;
} gs_in[];

out vec3 outPosition;
out vec3 outNormal;


// Volumetric scalar field
uniform sampler3D tex3D;

// Triangles table
//uniform usamplerBuffer triTableTex;
uniform sampler3D triTableTex;

// Isosurface level
uniform float isolevel;

// Offsets to cube corners
uniform vec3 vertDecals[8];

uniform mat3 gradDeltas;

uniform mat4 world_O_tex;


// Get vertex i position within current voxel
vec3 cubePos( int i )
{
    return gs_in[0].position + vertDecals[i];
}

// Get vertex i value within current voxel
float cubeVal( int i )
{
    return texture( tex3D, cubePos(i) ).r;
}

vec3 normalVec( vec3 texCoord )
{
    vec3 p0 = vec3( texture( tex3D, texCoord + gradDeltas[0] ).r,
                    texture( tex3D, texCoord + gradDeltas[1] ).r,
                    texture( tex3D, texCoord + gradDeltas[2] ).r );

    vec3 p1 = vec3( texture( tex3D, texCoord - gradDeltas[0] ).r,
                    texture( tex3D, texCoord - gradDeltas[1] ).r,
                    texture( tex3D, texCoord - gradDeltas[2] ).r );

    return normalize( p1 - p0 );
}

// Get triangle table value
int triTableValue( int cubeIndex, int j ) // ( row, col )
{
    //return texelFetch( triTableTex, 16*cubei + j ).r;
    return int( texelFetch( triTableTex, ivec3( j, cubeIndex, 0 ), 0 ).r );
}

// Compute interpolated vertex along a voxel edge
vec3 vertexInterp( float isolevel, vec3 v0, float l0, vec3 v1, float l1 )
{
    float t = clamp( (isolevel - l0) / (l1 - l0), 0.0, 1.0 );
    return mix( v0, v1, t );
}

// expects n's components to be clamped to -1 1
//uint packSnorm_2_10_10_10( vec3 n )
//{
////    vec<4, signed char, defaultp> result(round(clamp(v, -1.0f, 1.0f) * 127.0f));
//    vec4 p = clamp( n, -1.0, 1.0 ) * vec4( 511.0, 511.0, 511.0, 1.0 );

//    uint q = 0;
//    q = q | (w << 30);
//    q = q | (z << 20);
//    q = q | (y << 10);
//    q = q | (x << 0);
//}


void main()
{
    float cubeValues[8];
    vec3 cubePositions[8];

    // Determine the index into the edge table which
    // tells us which vertices are inside of the surface
    int cubeIndex = 0;

    for ( int i = 0; i < 8; i++ )
    {
        cubePositions[i] = cubePos( i );
        cubeValues[i] = cubeVal( i );
        cubeIndex += int( cubeValues[i] < isolevel ) * ( 1 << i );
    }

    // Cube is entirely in/out of the surface
    if ( 0 == cubeIndex || 255 == cubeIndex )
    {
        return;
    }

    // Find the vertices where the surface intersects the cube
    vec3 vertlist[12];

    vertlist[0]  = vertexInterp( isolevel, cubePositions[0], cubeValues[0], cubePositions[1], cubeValues[1] );
    vertlist[1]  = vertexInterp( isolevel, cubePositions[1], cubeValues[1], cubePositions[2], cubeValues[2] );
    vertlist[2]  = vertexInterp( isolevel, cubePositions[2], cubeValues[2], cubePositions[3], cubeValues[3] );
    vertlist[3]  = vertexInterp( isolevel, cubePositions[3], cubeValues[3], cubePositions[0], cubeValues[0] );
    vertlist[4]  = vertexInterp( isolevel, cubePositions[4], cubeValues[4], cubePositions[5], cubeValues[5] );
    vertlist[5]  = vertexInterp( isolevel, cubePositions[5], cubeValues[5], cubePositions[6], cubeValues[6] );
    vertlist[6]  = vertexInterp( isolevel, cubePositions[6], cubeValues[6], cubePositions[7], cubeValues[7] );
    vertlist[7]  = vertexInterp( isolevel, cubePositions[7], cubeValues[7], cubePositions[4], cubeValues[4] );
    vertlist[8]  = vertexInterp( isolevel, cubePositions[0], cubeValues[0], cubePositions[4], cubeValues[4] );
    vertlist[9]  = vertexInterp( isolevel, cubePositions[1], cubeValues[1], cubePositions[5], cubeValues[5] );
    vertlist[10] = vertexInterp( isolevel, cubePositions[2], cubeValues[2], cubePositions[6], cubeValues[6] );
    vertlist[11] = vertexInterp( isolevel, cubePositions[3], cubeValues[3], cubePositions[7], cubeValues[7] );


    int i = 0;

    while ( i <= 12 )
    {
        int v0 = triTableValue( cubeIndex, i );

        if ( 255 == v0 )
        {
            break;
        }

        // Generate vertices of triangle:

        int v1 = triTableValue( cubeIndex, i + 1 );
        int v2 = triTableValue( cubeIndex, i + 2 );

        outPosition = vec3( world_O_tex * vec4( vertlist[v0], 1.0 ) );
        outNormal = normalVec( vertlist[v0] ); // vec3( v0, v1, v2 );//
        EmitVertex();

        outPosition = vec3( world_O_tex * vec4( vertlist[v1], 1.0 ) );
        outNormal = normalVec( vertlist[v1] ); // vec3( cubeIndex, cubeIndex, cubeIndex );
        EmitVertex();

        outPosition = vec3( world_O_tex * vec4( vertlist[v2], 1.0 ) );
        outNormal = normalVec( vertlist[v2] );
        EmitVertex();

        EndPrimitive();

        i = i + 3;
    }
}

)glsl"
