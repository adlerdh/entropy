#version 330 core

out vec4 o_cellMean;

uniform sampler2D u_sceneTex;
uniform vec2 u_viewSizePx; // device pixels
uniform vec2 u_cellSizePx; // device pixels

void main()
{
  ivec2 cellCoord = ivec2(gl_FragCoord.xy);
  ivec2 cellOrigin = ivec2(vec2(cellCoord) * u_cellSizePx);
  ivec2 cellEnd = ivec2(min(vec2(cellOrigin) + u_cellSizePx, u_viewSizePx));
  ivec2 cellSize = cellEnd - cellOrigin;

  vec4 sum = vec4(0.0);
  for (int y = cellOrigin.y; y < cellEnd.y; ++y)
    for (int x = cellOrigin.x; x < cellEnd.x; ++x)
      sum += texelFetch(u_sceneTex, ivec2(x, y), 0);

  int count = cellSize.x * cellSize.y;
  o_cellMean = (count > 0) ? sum / float(count) : vec4(0.0);
}
