#version 330 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in vec3 edge_worldPosition[];
in vec3 edge_worldNormal[];
in vec4 edge_color[];

out vec3 v_worldPosition;
out vec3 v_worldNormal;
out vec4 v_color;
noperspective out vec3 v_barycentric;

void emitCorner(int corner, vec3 barycentric)
{
  gl_Position = gl_in[corner].gl_Position;
  v_worldPosition = edge_worldPosition[corner];
  v_worldNormal = edge_worldNormal[corner];
  v_color = edge_color[corner];
  v_barycentric = barycentric;
  EmitVertex();
}

void main()
{
  emitCorner(0, vec3(1.0, 0.0, 0.0));
  emitCorner(1, vec3(0.0, 1.0, 0.0));
  emitCorner(2, vec3(0.0, 0.0, 1.0));
  EndPrimitive();
}
