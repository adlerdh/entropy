#version 330 core

#define INVALID_TRANSPARENT 0
#define INVALID_GRAY 1

in VS_OUT
{
  vec3 v_texCoord[2];
  vec3 v_worldPos;
}
fs_in;

layout(location = 0) out vec4 o_color; // output RGBA color (premultiplied alpha RGBA)

uniform $$IMAGE_SAMPLER_TYPE$$ u_imgTex[2]; // images (scalar, red channel only)
uniform sampler1D u_metricCmapTex;          // metric color map (non-premultiplied RGBA)

uniform vec2 u_imgSlopeIntercept[2];     // map texture to normalized intensity [0, 1]
uniform vec2 u_metricCmapSlopeIntercept; // slope and intercept for the metric colormap
uniform vec2 u_metricSlopeIntercept;     // slope and intercept for the final metric

uniform mat4 img1Tex_T_img0Tex;
uniform vec3 u_tex0SamplingDirX;
uniform vec3 u_tex0SamplingDirY;
uniform vec3 u_texSamplingDirZ;
uniform int u_patchRadius;
uniform float u_sampleSpacing;
uniform float u_minValidFraction;
uniform float u_varianceEpsilon;
uniform int u_invalidStyle; // INVALID_TRANSPARENT or INVALID_GRAY

$$HELPER_FUNCTIONS$$

/// float textureLookup(sampler3D texture, vec3 texCoords);
$$TEXTURE_LOOKUP_FUNCTION$$

/// vec3 metricTexCoord(int imageIndex, vec2 patchOffset, int slabOffset);
$$METRIC_SAMPLING_FUNCTIONS$$

bool pairedSample(vec2 patchOffset, out float value0, out float value1)
{
  vec3 tex0 = metricTexCoord(0, patchOffset, 0);
  vec3 tex1 = metricTexCoord(1, patchOffset, 0);

  if (!isInsideTexture(tex0) || !isInsideTexture(tex1)) {
    return false;
  }

  value0 = clamp(u_imgSlopeIntercept[0][0] * textureLookup(u_imgTex[0], tex0, 0) + u_imgSlopeIntercept[0][1], 0.0, 1.0);
  value1 = clamp(u_imgSlopeIntercept[1][0] * textureLookup(u_imgTex[1], tex1, 1) + u_imgSlopeIntercept[1][1], 0.0, 1.0);
  return true;
}

void main()
{
  float center0 = 0.0;
  float center1 = 0.0;
  if (!pairedSample(vec2(0.0), center0, center1)) {
    discard;
  }

  float sum0 = 0.0;
  float sum1 = 0.0;
  float count = 0.0;

  for (int y = -u_patchRadius; y <= u_patchRadius; ++y) {
    for (int x = -u_patchRadius; x <= u_patchRadius; ++x) {
      float value0 = 0.0;
      float value1 = 0.0;
      if (pairedSample(u_sampleSpacing * vec2(float(x), float(y)), value0, value1)) {
        sum0 += value0;
        sum1 += value1;
        count += 1.0;
      }
    }
  }

  float sideLength = float(2 * u_patchRadius + 1);
  float requiredCount = ceil(clamp(u_minValidFraction, 0.0, 1.0) * sideLength * sideLength);
  bool valid = count >= max(requiredCount, 1.0);

  float mean0 = sum0 / max(count, 1.0);
  float mean1 = sum1 / max(count, 1.0);
  float variance0 = 0.0;
  float covariance = 0.0;

  for (int y = -u_patchRadius; y <= u_patchRadius; ++y) {
    for (int x = -u_patchRadius; x <= u_patchRadius; ++x) {
      float value0 = 0.0;
      float value1 = 0.0;
      if (pairedSample(u_sampleSpacing * vec2(float(x), float(y)), value0, value1)) {
        float centered0 = value0 - mean0;
        variance0 += centered0 * centered0;
        covariance += centered0 * (value1 - mean1);
      }
    }
  }

  valid = valid && variance0 > u_varianceEpsilon;
  if (!valid) {
    if (INVALID_TRANSPARENT == u_invalidStyle) {
      discard;
    }
    o_color = vec4(0.35, 0.35, 0.35, 1.0);
    return;
  }

  float gain = covariance / variance0;
  float bias = mean1 - gain * mean0;
  float residualSum = 0.0;

  for (int y = -u_patchRadius; y <= u_patchRadius; ++y) {
    for (int x = -u_patchRadius; x <= u_patchRadius; ++x) {
      float value0 = 0.0;
      float value1 = 0.0;
      if (pairedSample(u_sampleSpacing * vec2(float(x), float(y)), value0, value1)) {
        float residual = value1 - (gain * value0 + bias);
        residualSum += residual * residual;
      }
    }
  }

  float metric = sqrt(residualSum / max(count, 1.0));
  metric = clamp(u_metricSlopeIntercept[0] * metric + u_metricSlopeIntercept[1], 0.0, 1.0);
  float cmapValue = u_metricCmapSlopeIntercept[0] * metric + u_metricCmapSlopeIntercept[1];

  o_color = texture(u_metricCmapTex, cmapValue);
}
