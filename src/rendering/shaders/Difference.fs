#version 330 core

#define MIN_IMAGE_TEXCOORD vec3(0.0)
#define MAX_IMAGE_TEXCOORD vec3(1.0)

#define NO_IP_MODE 0
#define MAX_IP_MODE 1
#define MEAN_IP_MODE 2
#define MIN_IP_MODE 3
#define XRAY_IP_MODE 4

in VS_OUT
{
  vec3 v_texCoord[2];
} fs_in;

layout (location = 0) out vec4 o_color; // output RGBA color (premultiplied alpha RGBA)

// Texture samplers:
uniform sampler3D u_imgTex[2]; // images (scalar, red channel only)
uniform sampler1D u_metricCmapTex; // metric color map (non-premultiplied RGBA)

uniform mat4 img1Tex_T_img0Tex; // transform from image 0 to image 1 Texture space.

// Image adjustment uniforms:
uniform vec2 u_imgSlopeIntercept[2]; // map texture to normalized intensity [0, 1], plus window/leveling
uniform vec2 u_metricCmapSlopeIntercept; // Slope and intercept for the metric colormap
uniform vec2 u_metricSlopeIntercept; // Slope and intercept for the final metric
uniform bool u_useSquare; // use squared difference (true) or absolute difference (false)

// Intensiy Projection mode uniforms:
uniform int u_mipMode; // MIP mode (0: none, 1: max, 2: mean, 3: min, 4: X-ray)
uniform int u_halfNumMipSamples; // half number of MIP samples (0 when no projection used)
uniform vec3 u_texSamplingDirZ; // Z view camera direction (in texture sampling space)

/// float textureLookup(sampler3D texture, vec3 texCoords);
{{TEXTURE_LOOKUP_FUNCTION}}

/**
 * @brief Check if coordinates are inside the image texture
 */
bool isInsideTexture(vec3 texCoord)
{
  return (all(greaterThanEqual(texCoord, MIN_IMAGE_TEXCOORD)) &&
          all(lessThanEqual(texCoord, MAX_IMAGE_TEXCOORD)));
}

/**
 * @brief Compute the  metric
 */
float computeMetricAndMask(in int sampleOffset, out bool hitBoundary)
{
  hitBoundary = false;

  vec3 texOffset = float(sampleOffset) * u_texSamplingDirZ;
  vec3 tc[2] = vec3[2](vec3(0.0), vec3(0.0));
  tc[0] = fs_in.v_texCoord[0] + texOffset;
  tc[1] = vec3(img1Tex_T_img0Tex * vec4(tc[0], 1.0));

  if (!isInsideTexture(tc[0]) || !isInsideTexture(tc[1])) {
    hitBoundary = true;
    return 0.0;
  }

  float imgNorm[2]; // image normalized to [0, 1]
  for (int i = 0; i < 2; ++i)
  {
    float img;
    switch (i) {
    case 0: {
      img = textureLookup(u_imgTex[0], tc[i]);
      break;
    }
    case 1: {
      img = textureLookup(u_imgTex[1], tc[i]);
      break;
    }
    }
    imgNorm[i] = clamp(u_imgSlopeIntercept[i][0] * img + u_imgSlopeIntercept[i][1], 0.0, 1.0);
  }

  float metric = abs(imgNorm[0] - imgNorm[1]);
  return metric * mix(1.0, metric, float(u_useSquare));
}

void main()
{
  // Compute the metric on the view plane (0 offset):
  bool hitBoundary = false;
  float metric = computeMetricAndMask(0, hitBoundary);
  int numSamples = 1;

  // Accumulate intensity projection in forwards (+Z) and backwards (-Z) directions:
  for (int dir = -1; dir <= 1; dir += 2) // dir in {-1, 1}
  {
    for (int i = 1; i <= u_halfNumMipSamples; ++i)
    {
      float m = computeMetricAndMask(dir * i, hitBoundary);
      if (hitBoundary) { break; }

      metric = float(NO_IP_MODE == u_mipMode) * metric +
               float(MAX_IP_MODE == u_mipMode) * max(metric, m) +
               float(MEAN_IP_MODE == u_mipMode) * (metric + m) +
               float(MIN_IP_MODE == u_mipMode) * min(metric, m);

      ++numSamples;
    }
  }

  // If using Mean IP mode, then normalize by the number of samples:
  metric /= mix(1.0, float(numSamples), float(MEAN_IP_MODE == u_mipMode));

  // Normalize and apply colormap:
  metric = clamp(u_metricSlopeIntercept[0] * metric + u_metricSlopeIntercept[1], 0.0, 1.0);
  float cmapValue = u_metricCmapSlopeIntercept[0] * metric + u_metricCmapSlopeIntercept[1];

  // Output color (premult. RGBA)
  o_color = texture(u_metricCmapTex, cmapValue);
}
