#version 330 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in vec3 edge_worldPosition[];
in vec3 edge_worldNormal[];
in vec4 edge_color[];

out vec3 v_worldPosition;
out vec3 v_worldNormal;
out vec4 v_color;
flat out vec3 v_worldFaceNormal;
noperspective out vec3 v_barycentric;

invariant gl_Position;

void emitCorner(int corner, vec3 barycentric, vec3 faceNormal)
{
  gl_Position = gl_in[corner].gl_Position;
  v_worldPosition = edge_worldPosition[corner];
  v_worldNormal = edge_worldNormal[corner];
  v_color = edge_color[corner];
  v_worldFaceNormal = faceNormal;
  v_barycentric = barycentric;
  EmitVertex();
}

void main()
{
  vec3 unnormalizedNormal =
    cross(edge_worldPosition[1] - edge_worldPosition[0], edge_worldPosition[2] - edge_worldPosition[0]);
  vec3 faceNormal =
    dot(unnormalizedNormal, unnormalizedNormal) > 0.000001 ? normalize(unnormalizedNormal) : vec3(0.0, 0.0, 1.0);

  // Compute the primitive normal before rasterizer near-plane clipping. Screen-space derivatives of a clipped
  // triangle become poorly conditioned when the camera is close to its surface.
  emitCorner(0, vec3(1.0, 0.0, 0.0), faceNormal);
  emitCorner(1, vec3(0.0, 1.0, 0.0), faceNormal);
  emitCorner(2, vec3(0.0, 0.0, 1.0), faceNormal);
  EndPrimitive();
}
