#version 330 core

layout(location = 0) out vec4 o_regionsA; // regions 0–3 (top-left, top-right, mid-left, mid-right)
layout(location = 1) out vec2 o_regionsB; // regions 4–5 (bot-left, bot-right)

uniform sampler2D u_sceneTex;
uniform vec2 u_viewSizePx;     // device pixels
uniform vec2 u_cellSizePx;     // device pixels
uniform ivec2 u_cellSizePxInt; // integer cell size = round(u_cellSizePx)

// Region convention (matches AsciiAtlasBaker.h and AsciiPostSpatial.fs):
// 2 columns × 3 rows layout (glyphs are taller than wide):
//   Region 0: top-left    (x < W/2,   y >= 2H/3 in GL y-up)
//   Region 1: top-right   (x >= W/2,  y >= 2H/3)
//   Region 2: mid-left    (x < W/2,   H/3 <= y < 2H/3)
//   Region 3: mid-right   (x >= W/2,  H/3 <= y < 2H/3)
//   Region 4: bot-left    (x < W/2,   y < H/3)
//   Region 5: bot-right   (x >= W/2,  y < H/3)
//
//   col = (lx < W/2) ? 0 : 1
//   row = (ly >= 2H/3) ? 0 : (ly >= H/3) ? 1 : 2
//   reg = row * 2 + col

void main()
{
  ivec2 cellCoord = ivec2(gl_FragCoord.xy);
  ivec2 cellOrigin = ivec2(vec2(cellCoord) * u_cellSizePx);
  ivec2 cellEnd = ivec2(min(vec2(cellOrigin) + u_cellSizePx, u_viewSizePx));

  float sums[6];
  float counts[6];
  for (int i = 0; i < 6; ++i) {
    sums[i] = 0.0;
    counts[i] = 0.0;
  }

  int halfW = u_cellSizePxInt.x / 2;
  int thirdH = u_cellSizePxInt.y / 3;

  for (int dy = cellOrigin.y; dy < cellEnd.y; ++dy) {
    for (int dx = cellOrigin.x; dx < cellEnd.x; ++dx) {
      vec4 pm = texelFetch(u_sceneTex, ivec2(dx, dy), 0);
      if (pm.a < 0.001) continue;
      vec3 rgb = pm.rgb / max(pm.a, 1e-4);
      float lum = dot(rgb, vec3(0.299, 0.587, 0.114));

      int lx = dx - cellOrigin.x;
      int ly = dy - cellOrigin.y;

      // 2-column index
      int col = (lx < halfW) ? 0 : 1;
      // GL y-up: higher y = top; 3-row index
      int row = (ly >= 2 * thirdH) ? 0 : (ly >= thirdH) ? 1 : 2;
      int reg = row * 2 + col;

      sums[reg] += lum;
      counts[reg] += 1.0;
    }
  }

  // Output mean per region; 0.0 where no pixels (empty region)
  o_regionsA = vec4(
    sums[0] / max(counts[0], 1.0),
    sums[1] / max(counts[1], 1.0),
    sums[2] / max(counts[2], 1.0),
    sums[3] / max(counts[3], 1.0));
  o_regionsB = vec2(sums[4] / max(counts[4], 1.0), sums[5] / max(counts[5], 1.0));
}
