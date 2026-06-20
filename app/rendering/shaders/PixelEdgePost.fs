#version 330 core

in vec2 v_uv;
out vec4 o_color;

uniform sampler2D u_sceneTex;
uniform sampler1D u_cmapTex;
uniform vec2 u_sceneSizePx;
uniform vec2 u_viewOriginPx;
uniform vec2 u_viewSizePx;
uniform bool u_thresholdEdges;
uniform bool u_thinEdges;
uniform float u_edgeScale;
uniform float u_edgeThreshold;
uniform vec2 u_cmapSlopeIntercept;
uniform bool u_colormapEdges;
uniform vec4 u_edgeColor;
uniform bool u_overlayEdges;

const float k_sourceAlphaEpsilon = 1.0 / 255.0;

bool isInsideView(ivec2 scenePx, ivec2 viewMin, ivec2 viewMax)
{
  return !any(lessThan(scenePx, viewMin)) && !any(greaterThan(scenePx, viewMax));
}

vec4 sourceAt(ivec2 scenePx, ivec2 viewMin, ivec2 viewMax)
{
  return isInsideView(scenePx, viewMin, viewMax) ? texelFetch(u_sceneTex, scenePx, 0) : vec4(0.0);
}

float luminance(vec4 colorPM)
{
  return dot(colorPM.rgb, vec3(0.2126, 0.7152, 0.0722));
}

float neighborLuminanceAt(ivec2 scenePx, float centerLuminance, ivec2 viewMin, ivec2 viewMax)
{
  // Treat samples outside the rendered image domain as equal to the center
  // sample so the post-process does not outline the image rectangle.
  vec4 colorPM = sourceAt(scenePx, viewMin, viewMax);
  return colorPM.a > k_sourceAlphaEpsilon ? luminance(colorPM) : centerLuminance;
}

vec2 gradientAt(ivec2 scenePx, vec4 centerPM, ivec2 viewMin, ivec2 viewMax)
{
  if (centerPM.a <= k_sourceAlphaEpsilon) {
    return vec2(0.0);
  }

  float centerLuminance = luminance(centerPM);
  float tl = neighborLuminanceAt(scenePx + ivec2(-1, 1), centerLuminance, viewMin, viewMax);
  float tc = neighborLuminanceAt(scenePx + ivec2(0, 1), centerLuminance, viewMin, viewMax);
  float tr = neighborLuminanceAt(scenePx + ivec2(1, 1), centerLuminance, viewMin, viewMax);
  float ml = neighborLuminanceAt(scenePx + ivec2(-1, 0), centerLuminance, viewMin, viewMax);
  float mr = neighborLuminanceAt(scenePx + ivec2(1, 0), centerLuminance, viewMin, viewMax);
  float bl = neighborLuminanceAt(scenePx + ivec2(-1, -1), centerLuminance, viewMin, viewMax);
  float bc = neighborLuminanceAt(scenePx + ivec2(0, -1), centerLuminance, viewMin, viewMax);
  float br = neighborLuminanceAt(scenePx + ivec2(1, -1), centerLuminance, viewMin, viewMax);

  return vec2(-tl - 2.0 * ml - bl + tr + 2.0 * mr + br, -bl - 2.0 * bc - br + tl + 2.0 * tc + tr);
}

float gradientMagnitudeAt(ivec2 scenePx, ivec2 viewMin, ivec2 viewMax)
{
  vec4 centerPM = sourceAt(scenePx, viewMin, viewMax);
  return length(gradientAt(scenePx, centerPM, viewMin, viewMax));
}

ivec2 nonMaximumSuppressionStep(vec2 grad)
{
  vec2 absGrad = abs(grad);
  if (absGrad.x >= 2.4142 * absGrad.y) {
    return ivec2(1, 0);
  }
  if (absGrad.y >= 2.4142 * absGrad.x) {
    return ivec2(0, 1);
  }
  return ivec2((grad.x * grad.y) >= 0.0 ? 1 : -1, 1);
}

void main()
{
  ivec2 scenePx = ivec2(clamp(v_uv * u_sceneSizePx, vec2(0.0), u_sceneSizePx - vec2(1.0)));
  ivec2 viewMin = ivec2(u_viewOriginPx);
  ivec2 viewMax = ivec2(u_viewOriginPx + u_viewSizePx) - ivec2(1);

  if (!isInsideView(scenePx, viewMin, viewMax)) {
    discard;
  }

  vec4 sourcePM = texelFetch(u_sceneTex, scenePx, 0);
  if (sourcePM.a <= k_sourceAlphaEpsilon) {
    discard;
  }

  vec2 grad = gradientAt(scenePx, sourcePM, viewMin, viewMax);
  float rawEdge = length(grad);

  if (u_thinEdges && rawEdge > 0.0) {
    ivec2 stepPx = nonMaximumSuppressionStep(grad);
    float edgeForward = gradientMagnitudeAt(scenePx + stepPx, viewMin, viewMax);
    float edgeBackward = gradientMagnitudeAt(scenePx - stepPx, viewMin, viewMax);
    rawEdge *= float(rawEdge >= edgeForward && rawEdge >= edgeBackward);
  }

  float edge = rawEdge * u_edgeScale;
  edge = u_thresholdEdges ? float(edge >= u_edgeThreshold) : clamp(edge, 0.0, 1.0);

  vec4 edgeColormap = texture(u_cmapTex, u_cmapSlopeIntercept[0] * edge + u_cmapSlopeIntercept[1]);
  vec4 edgeColormapPM = edge * edgeColormap.a * vec4(edgeColormap.rgb, 1.0);
  vec4 edgePM = mix(edge * u_edgeColor, edgeColormapPM, float(u_colormapEdges));

  o_color = u_overlayEdges ? edgePM + sourcePM * (1.0 - edgePM.a) : edgePM;
}
