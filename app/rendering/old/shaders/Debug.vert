R"(

#version 330 core

layout (location = 0) in vec3 clipCoords;

void main()
{
    gl_Position = vec4( clipCoords, 1.0 );
}

)"
