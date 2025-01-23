R"(

#version 330 core

// Vertex position in local Model space
layout (location = 0) in vec3 modelPosition;

// Vetex normal vector in local Model space (w coord. is ignored)
layout (location = 1) in vec4 modelNormal;

// Vertex RGBA color (with pre-multiplied alpha)
layout (location = 2) in vec4 color;

// Vertex shader outputs
out VS_OUT
{
    vec3 WorldPos; // Vertex position in World space
    vec3 WorldNormal; // Vertex normal vector in World space
    vec4 Color; // Vertex RGBA color (with pre-multiplied alpha)
} vs_out;

// In order to write to the gl_ClipDistance array, we must first redeclare this array
// with an explicit size: the number of array elements that we intend to use.
out float gl_ClipDistance[1];

// Transformation from Model to World space
uniform mat4 world_O_model;

// Transformation from World to Camera space (i.e. camera model-view matrix)
uniform mat4 camera_O_world;

// Transformation from Camera to Clip space (i.e. camera projection matrix)
uniform mat4 clip_O_camera;

// Inverse-transpose of the transformation from Model to World space
uniform mat4 world_O_model_inv_trans;

// Array of three clip planes defined in World space: (a, b, c, d), where
// (a, b, c) is the normalized plane normal vector in World space and
// d is the distance from the origin.
uniform vec4 worldClipPlanes[3];


void main()
{
    vec4 worldPos = world_O_model * vec4( modelPosition, 1.0 );
    gl_Position = clip_O_camera * camera_O_world * worldPos;

    worldPos /= worldPos.w;
    vs_out.WorldPos = worldPos.xyz;

    vec4 n = vec4( modelNormal.xyz, -dot( modelNormal.xyz, worldPos.xyz ) );
    vs_out.WorldNormal = normalize( vec3( world_O_model_inv_trans * n ) );
    vs_out.Color = color;

    float dist0 = dot( worldClipPlanes[0], worldPos );
    float dist1 = dot( worldClipPlanes[1], worldPos );
    float dist2 = dot( worldClipPlanes[2], worldPos );

    gl_ClipDistance[0] = min( dist0, 0.0 ) * min( dist1, 0.0 ) * min( dist2, 0.0 );
}

)"
