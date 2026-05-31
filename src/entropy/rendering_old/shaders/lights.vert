R"(

#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec4 normal;
//layout (location = 2) in vec2 texCoord;
//layout (location = 3) in vec3 color;

out VS_OUT
{
//	vec2 TexCoord;
	vec3 Normal;
	vec3 WorldPos;
//    vec3 Color;
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0);

//    vs_out.TexCoord = vec2(texCoord.x, texCoord.y);
    vs_out.WorldPos = vec3(model * vec4(position, 1.0));
    vs_out.Normal = vec3( transpose(inverse(model)) * normal );

//    vs_out.Color = color;
}

)"
