#version 330 core

layout (location = 0) in vec2 clipPos;

// View transformation data:
uniform mat4 u_view_T_clip;
uniform mat4 u_world_T_clip;
uniform float u_clipDepth;

// View aspect ratio (for checkerboarding and flashlight):
uniform float u_aspectRatio;

// Number of checkerboard squares along the longest view dimension:
uniform float u_numSquares;

// Image transformation data:
uniform mat4 u_imgTexture_T_world;
uniform mat4 u_segTexture_T_world;
uniform mat4 u_segVoxel_T_world;

// Vertex shader outputs (varyings):
out VS_OUT
{
    vec3 v_imgTexCoords; // Image texture coords
    vec3 v_segTexCoords; // Seg texture coords
    vec3 v_segVoxCoords; // Seg voxel coords
    vec2 v_checkerCoord; // 2D clip position
    vec2 v_clipPos; // 2D clip position
} vs_out;


void main()
{
    vs_out.v_clipPos = clipPos;

    vec2 C = u_numSquares * 0.5 * ( clipPos + vec2( 1.0, 1.0 ) );

    vs_out.v_checkerCoord = mix( vec2( C.x, C.y / u_aspectRatio ),
        vec2( C.x * u_aspectRatio, C.y ), float( u_aspectRatio <= 1.0 ) );

    vec4 clipPos3d = vec4( clipPos, u_clipDepth, 1.0 );
    gl_Position = u_view_T_clip * clipPos3d;

    vec4 worldPos = u_world_T_clip * clipPos3d;

    vec4 imgTexPos = u_imgTexture_T_world * worldPos;
    vec4 segTexPos = u_segTexture_T_world * worldPos;
    vec4 segVoxPos = u_segVoxel_T_world * worldPos;

    vs_out.v_imgTexCoords = vec3( imgTexPos / imgTexPos.w );
    vs_out.v_segTexCoords = vec3( segTexPos / segTexPos.w );
    vs_out.v_segVoxCoords = vec3( segVoxPos / segVoxPos.w );
}
