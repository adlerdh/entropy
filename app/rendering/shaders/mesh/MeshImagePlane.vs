#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 3) in vec3 a_texCoord;

uniform mat4 u_clip_T_world;
uniform mat4 u_world_T_mesh;
uniform mat3 u_world_T_meshNormal;
uniform bool u_hasVertexNormals;
uniform float u_aspectRatio;
uniform int u_numCheckers;

out VS_OUT
{
  vec3 v_texCoord;
  vec3 v_worldPos;
  vec3 v_worldNormal;
  vec2 v_checkerCoord;
  vec2 v_clipPos;
}
vs_out;

invariant gl_Position;

void main()
{
  vec4 worldPosition = u_world_T_mesh * vec4(a_position, 1.0);
  vec4 clipPosition = u_clip_T_world * worldPosition;
  float safeClipW = abs(clipPosition.w) > 1.0e-12 ? clipPosition.w : 1.0;
  float safeWorldW = abs(worldPosition.w) > 1.0e-12 ? worldPosition.w : 1.0;
  vec2 clipPos = clipPosition.xy / safeClipW;
  vec2 checkerBase = float(u_numCheckers) * 0.5 * (clipPos + vec2(1.0, 1.0));
  vec3 transformedNormal = u_world_T_meshNormal * a_normal;
  float normalLength2 = dot(transformedNormal, transformedNormal);
  float safeAspectRatio = max(abs(u_aspectRatio), 1.0e-6);

  vs_out.v_texCoord = a_texCoord;
  vs_out.v_worldPos = worldPosition.xyz / safeWorldW;
  vs_out.v_worldNormal = u_hasVertexNormals && normalLength2 > 1.0e-12 ? transformedNormal * inversesqrt(normalLength2)
                                                                       : vec3(0.0, 0.0, 1.0);
  vs_out.v_clipPos = clipPos;
  vs_out.v_checkerCoord = mix(
    vec2(checkerBase.x, checkerBase.y / safeAspectRatio),
    vec2(checkerBase.x * safeAspectRatio, checkerBase.y),
    float(safeAspectRatio <= 1.0));
  gl_Position = clipPosition;
}
