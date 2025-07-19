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
  vec3 v_imgTexCoords[2];
} fs_in;

layout (location = 0) out vec4 o_color; // Output RGBA color (pre-multiplied alpha)

uniform sampler3D u_imgTex[2]; // Texture units 0/1: images
uniform sampler1D u_metricCmapTex; // Texture unit 2: metric colormap (pre-mult RGBA)

uniform vec2 u_imgSlopeIntercept[2]; // Slopes and intercepts for image window-leveling

uniform vec2 u_metricCmapSlopeIntercept; // Slope and intercept for the metric colormap
uniform vec2 u_metricSlopeIntercept; // Slope and intercept for the final metric

// Whether to use squared difference (true) or absolute difference (false)
uniform bool u_useSquare;

// MIP mode (0: none, 1: max, 2: mean, 3: min, 4: xray)
uniform int u_mipMode;

// Half the number of samples for MIP (for image 0). Is set to 0 when u_mipMode == 0.
uniform int u_halfNumMipSamples;

// Z view camera direction, represented in image 0 texture sampling space.
uniform vec3 u_texSamplingDirZ;

// Transformation from image 0 to image 1 Texture space.
uniform mat4 img1Tex_T_img0Tex;

// Check if a coordinate is inside the texture coordinate bounds.
bool isInsideTexture(vec3 a)
{
  return (all(greaterThanEqual(a, MIN_IMAGE_TEXCOORD)) &&
          all(lessThanEqual(a, MAX_IMAGE_TEXCOORD)));
}

// float textureLookup(sampler3D texture, vec3 texCoords);
{{TEXTURE_LOOKUP_FUNCTION}}

// Compute the metric
float computeMetricAndMask(in int sampleOffset, out bool hitBoundary)
{
  hitBoundary = false;

  vec3 img_tc[2] = vec3[2](vec3(0.0), vec3(0.0));
  vec3 texOffset = float(sampleOffset) * u_texSamplingDirZ;

  img_tc[0] = fs_in.v_imgTexCoords[0] + texOffset;
  img_tc[1] = vec3(img1Tex_T_img0Tex * vec4(img_tc[0], 1.0));

  if (!isInsideTexture(img_tc[0]) || !isInsideTexture(img_tc[1])) {
    hitBoundary = true;
    return 0.0;
  }

  float imgNorm[2];

  // Loop over the two images:
  for (int i = 0; i < 2; ++i)
  {
    float img;
    switch (i)
    {
    case 0: {
      img = textureLookup(u_imgTex[0], img_tc[i]);
      break;
    }
    case 1: {
      img = textureLookup(u_imgTex[1], img_tc[i]);
      break;
    }
    }

    // Normalize images to [0.0, 1.0] range:
    imgNorm[i] = clamp(u_imgSlopeIntercept[i][0] * img + u_imgSlopeIntercept[i][1], 0.0, 1.0);
  }

  float metric = abs(imgNorm[0] - imgNorm[1]);
  metric *= mix(1.0, metric, float(u_useSquare));
  return metric;
}

void main()
{
  // Flag the the boundary was hit:
  bool hitBoundary = false;

  // Compute the metric and metric mask without zero offset.
  // Computation is on the current slice.
  float metric = computeMetricAndMask(0, hitBoundary);
  int numSamples = 1;

  // Integrate projection along forwards (+Z) direction:
  for (int i = 1; i <= u_halfNumMipSamples; ++i)
  {
    float m = computeMetricAndMask(i, hitBoundary);
    if (hitBoundary) break;

    // Accumulate the metric:
    metric = float(MAX_IP_MODE == u_mipMode) * max(metric, m) +
             float(MEAN_IP_MODE == u_mipMode) * (metric + m) +
             float(MIN_IP_MODE == u_mipMode) * min(metric, m);

    ++numSamples;
  }

  // Integrate projection along backwards (-Z) direction:
  for (int i = 1; i <= u_halfNumMipSamples; ++i)
  {
    float m = computeMetricAndMask(-i, hitBoundary);
    if (hitBoundary) break;

    metric = float(MAX_IP_MODE == u_mipMode) * max(metric, m) +
             float(MEAN_IP_MODE == u_mipMode) * (metric + m) +
             float(MIN_IP_MODE == u_mipMode) * min(metric, m);

    ++numSamples;
  }

  // If using Mean Intensity Projection mode, then normalize the total metric
  // by the number of samples in order co compute the mean:
  metric /= mix(1.0, float(numSamples), float(MEAN_IP_MODE == u_mipMode));

  // Apply slope and intercept to metric:
  metric = clamp(u_metricSlopeIntercept[0] * metric + u_metricSlopeIntercept[1], 0.0, 1.0);

  // Index into colormap:
  float cmapValue = u_metricCmapSlopeIntercept[0] * metric + u_metricCmapSlopeIntercept[1];

  // Apply colormap and masking (by pre-multiplying RGBA with alpha mask):
  vec4 metricLayer = texture(u_metricCmapTex, cmapValue);

  o_color = vec4(0.0, 0.0, 0.0, 0.0);
  o_color = metricLayer + (1.0 - metricLayer.a) * o_color;
}
