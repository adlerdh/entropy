#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 3) in vec3 a_texCoord;

uniform mat4 u_clip_T_world;
uniform mat4 u_world_T_mesh;
uniform float u_aspectRatio;
uniform int u_numCheckers;

out VS_OUT
{
  vec3 v_texCoord;
  vec3 v_worldPos;
  vec2 v_checkerCoord;
  vec2 v_clipPos;
}
vs_out;

void main()
{
  vec4 worldPosition = u_world_T_mesh * vec4(a_position, 1.0);
  vec4 clipPosition = u_clip_T_world * worldPosition;
  vec2 clipPos = clipPosition.xy / clipPosition.w;
  vec2 checkerBase = float(u_numCheckers) * 0.5 * (clipPos + vec2(1.0, 1.0));

  vs_out.v_texCoord = a_texCoord;
  vs_out.v_worldPos = worldPosition.xyz / worldPosition.w;
  vs_out.v_clipPos = clipPos;
  vs_out.v_checkerCoord = mix(
    vec2(checkerBase.x, checkerBase.y / u_aspectRatio),
    vec2(checkerBase.x * u_aspectRatio, checkerBase.y),
    float(u_aspectRatio <= 1.0));
  gl_Position = clipPosition;
}
