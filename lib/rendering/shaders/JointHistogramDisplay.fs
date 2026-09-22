#version 330 core

in vec2 v_uv;
layout(location = 0) out vec4 o_color;

uniform sampler2D u_counts;
uniform vec2 u_histogramFraction;
uniform sampler1D u_colormap;
uniform float u_referenceCount;
uniform bool u_logarithmicScale;
uniform vec2 u_visibleMinimum;
uniform vec2 u_visibleMaximum;
uniform vec2 u_metricSlopeIntercept;
uniform vec2 u_cmapSlopeIntercept;
uniform int u_cmapQuantizationLevels;

void main()
{
  vec2 histogramUv = mix(u_visibleMinimum, u_visibleMaximum, v_uv);
  histogramUv *= u_histogramFraction;
  float count = texture(u_counts, histogramUv).r;
  float density = u_logarithmicScale ? log(1.0 + count) / log(1.0 + u_referenceCount) : count / u_referenceCount;
  density = clamp(density, 0.0, 1.0);
  density = clamp(dot(u_metricSlopeIntercept, vec2(density, 1.0)), 0.0, 1.0);
  float cmapCoordinate = mix(
    floor(float(u_cmapQuantizationLevels) * density) / max(float(u_cmapQuantizationLevels - 1), 1.0),
    density,
    float(u_cmapQuantizationLevels == 0));
  vec4 color = texture(u_colormap, dot(u_cmapSlopeIntercept, vec2(cmapCoordinate, 1.0)));
  o_color = vec4(color.rgb * color.a, 1.0);
}
