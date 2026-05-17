#version 330 core

layout(location = 0) out vec4 o_regionsA;  // regions 0–3 (top-left, top-center, top-right, bottom-left)
layout(location = 1) out vec2 o_regionsB;  // regions 4–5 (bottom-center, bottom-right)

uniform sampler2D u_sceneTex;
uniform vec2  u_viewSizePx;    // device pixels
uniform vec2  u_cellSizePx;    // device pixels
uniform ivec2 u_cellSizePxInt; // integer cell size = round(u_cellSizePx)

// Region convention (matches AsciiAtlasBaker.h and AsciiPostSpatial.fs):
// 3 columns × 2 rows layout:
//   Region 0: top-left      (x < W/3,        y >= H/2 in GL y-up)
//   Region 1: top-center    (W/3 <= x < 2W/3, y >= H/2)
//   Region 2: top-right     (x >= 2W/3,      y >= H/2)
//   Region 3: bottom-left   (x < W/3,        y < H/2)
//   Region 4: bottom-center (W/3 <= x < 2W/3, y < H/2)
//   Region 5: bottom-right  (x >= 2W/3,      y < H/2)
//
//   col = (lx < W/3) ? 0 : (lx < 2W/3) ? 1 : 2
//   row = (ly >= H/2) ? 0 : 1
//   reg = row * 3 + col

void main()
{
    ivec2 cellCoord  = ivec2(gl_FragCoord.xy);
    ivec2 cellOrigin = ivec2(vec2(cellCoord) * u_cellSizePx);
    ivec2 cellEnd    = ivec2(min(vec2(cellOrigin) + u_cellSizePx, u_viewSizePx));

    float sums[6];
    float counts[6];
    for (int i = 0; i < 6; ++i) { sums[i] = 0.0; counts[i] = 0.0; }

    int thirdW = u_cellSizePxInt.x / 3;
    int halfH  = u_cellSizePxInt.y / 2;

    for (int dy = cellOrigin.y; dy < cellEnd.y; ++dy) {
        for (int dx = cellOrigin.x; dx < cellEnd.x; ++dx) {
            vec4 pm = texelFetch(u_sceneTex, ivec2(dx, dy), 0);
            if (pm.a < 0.001) continue;
            vec3 rgb = pm.rgb / max(pm.a, 1e-4);
            float lum = dot(rgb, vec3(0.299, 0.587, 0.114));

            int lx = dx - cellOrigin.x;
            int ly = dy - cellOrigin.y;

            // 3-column index — integer arithmetic matching baker's sampleW/3
            int col = (lx < thirdW) ? 0 : (lx < 2 * thirdW) ? 1 : 2;
            // GL y-up: higher y = top
            bool isTop = (ly >= halfH);
            int reg = (isTop ? 0 : 1) * 3 + col;

            sums[reg]   += lum;
            counts[reg] += 1.0;
        }
    }

    // Output mean per region; 0.0 where no pixels (empty region)
    o_regionsA = vec4(
        sums[0] / max(counts[0], 1.0),
        sums[1] / max(counts[1], 1.0),
        sums[2] / max(counts[2], 1.0),
        sums[3] / max(counts[3], 1.0)
    );
    o_regionsB = vec2(
        sums[4] / max(counts[4], 1.0),
        sums[5] / max(counts[5], 1.0)
    );
}
