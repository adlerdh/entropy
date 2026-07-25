R"(

#version 330 core

layout (location = 0) in vec3 modelPos;

uniform mat4 world_O_model;
uniform mat4 camera_O_world;
uniform mat4 clip_O_camera;

// Defined in World space
uniform vec4 worldClipPlanes[3];

void main()
{
    vec4 worldPos = world_O_model * vec4( modelPos, 1.0 );
    worldPos /= worldPos.w;

    gl_Position = clip_O_camera * camera_O_world * worldPos;

    float dist0 = dot( worldClipPlanes[0], worldPos );
    float dist1 = dot( worldClipPlanes[1], worldPos );
    float dist2 = dot( worldClipPlanes[2], worldPos );

    gl_ClipDistance[0] = min( dist0, 0.0 ) * min( dist1, 0.0 ) * min( dist2, 0.0 );
}

)"
