#version 330 core

#define MIN_IMAGE_TEXCOORD vec3(0.0)
#define MAX_IMAGE_TEXCOORD vec3(1.0)

#define N 2

in VS_OUT // Redeclared vertex shader outputs: now the fragment shader inputs
{
  vec3 v_texCoord[N];
  vec3 v_texCoord[N];
} fs_in;

layout (location = 0) out vec4 o_color; // Output RGBA color (premultiplied alpha)

uniform sampler3D u_imgTex[N]; // Texture units 0/1: images
uniform usampler3D u_segTex[N]; // Texture units 2/3: segmentations
uniform samplerBuffer u_segLabelCmapTex[N]; // Texutre unit 6/7: label color tables (pre-mult RGBA)

uniform float u_segOpacity[N]; // Segmentation opacities

// Texture sampling directions (horizontal and vertical) for calculating the segmentation outline
uniform vec3 u_texSamplingDirsForSegOutline[2];

// Opacity of the interior of the segmentation
uniform float u_segFillOpacity;

uniform sampler1D u_metricCmapTex; // Texture unit 4: metric colormap (pre-mult RGBA)
uniform vec2 u_metricCmapSlopeIntercept; // Slope and intercept for the metric colormap
uniform vec2 u_metricSlopeIntercept; // Slope and intercept for the final metric
uniform bool u_metricMasking; // Whether to mask based on segmentation

uniform mat4 u_texture1_T_texture0;
uniform vec3 u_texSampleSize[N];
uniform vec3 u_tex0SamplingDirX;
uniform vec3 u_tex0SamplingDirY;

// Compute alpha of fragments based on whether or not they are inside the
// segmentation boundary. Fragments on the boundary are assigned alpha of 1,
// whereas fragments inside are assigned alpha of 'u_segFillOpacity'.
float getSegInteriorAlpha(int texNum, uint seg)
{
  float segInteriorAlpha = u_segFillOpacity;

  // Look up texture values in 8 neighbors surrounding the center fragment.
  // These may be either neighboring image voxels or neighboring view pixels.
  // The center fragment has index i = 4 (row = 0, col = 0).
  for (int i = 0; i < 9; ++i)
  {
    float row = float(mod(i, 3) - 1); // runs -1 to 1
    float col = float(floor(float(i / 3)) - 1); // runs -1 to 1

    vec3 texSamplingPos = row * u_texSamplingDirsForSegOutline[0] +
      col * u_texSamplingDirsForSegOutline[1];

    // Segmentation value of neighbor at (row, col) offset
//    uint nseg = texture(u_segTex[texNum], fs_in.v_texCoord[texNum] + texSamplingPos)[0];
    uint nseg;

    switch (texNum)
    {
    case 0:
    {
      nseg = texture(u_segTex[0], fs_in.v_texCoord[texNum] + texSamplingPos)[0];
      break;
    }
    case 1:
    {
      nseg = texture(u_segTex[1], fs_in.v_texCoord[texNum] + texSamplingPos)[0];
      break;
    }
    }

    // Fragment (with segmentation 'seg') is on the boundary (and hence gets
    // full alpha) if its value is not equal to one of its neighbors.
    if (nseg != seg)
    {
      segInteriorAlpha = 1.0;
      break;
    }
  }

  return segInteriorAlpha;
}

float getImageValue(sampler3D tex, vec3 texCoord)
{
  return texture(tex, texCoord)[0];
}

int when_lt(int x, int y)
{
  return max(sign(y - x), 0);
}

int when_ge(int x, int y)
{
  return (1 - when_lt(x, y));
}

vec4 getLabelColor(int label, int i)
{
//  label -= label * when_ge(label, textureSize(u_segLabelCmapTex[i]));
//  vec4 color = texelFetch(u_segLabelCmapTex[i], label);

  vec4 color;

  switch (i)
  {
  case 0:
  {
    label -= label * when_ge(label, textureSize(u_segLabelCmapTex[0]));
    color = texelFetch(u_segLabelCmapTex[0], label);
    break;
  }
  case 1:
  {
    label -= label * when_ge(label, textureSize(u_segLabelCmapTex[1]));
    color = texelFetch(u_segLabelCmapTex[1], label);
    break;
  }
  }

  return color.a * color;
}

void main()
{
  float imgNorm[N];
  float mask[N];
  vec4 segColor[N];

  for (int i = 0; i < N; ++i)
  {
    // Foreground masks, based on whether texture coordinates are in range [0.0, 1.0]^3:
    bool imgMask = ! (any( lessThan(fs_in.v_texCoord[i], MIN_IMAGE_TEXCOORD + u_texSampleSize[i])) ||
               any(greaterThan(fs_in.v_texCoord[i], MAX_IMAGE_TEXCOORD - u_texSampleSize[i])));

    bool segMask = ! (any( lessThan(fs_in.v_texCoord[i], MIN_IMAGE_TEXCOORD + u_texSampleSize[i])) ||
               any(greaterThan(fs_in.v_texCoord[i], MAX_IMAGE_TEXCOORD - u_texSampleSize[i])));

    // uint label = texture(u_segTex[i], fs_in.v_texCoord[i]).r; // Label value
    uint label;

    switch (i)
    {
    case 0:
    {
      label = texture(u_segTex[0], fs_in.v_texCoord[i]).r; // Label value
      break;
    }
    case 1:
    {
      label = texture(u_segTex[1], fs_in.v_texCoord[i]).r; // Label value
      break;
    }
    }

    // Apply foreground mask and masking based on segmentation label:
    mask[i] = float(imgMask && (u_metricMasking && (label > 0u) || ! u_metricMasking));

    // Look up label colors:
    segColor[i] = getLabelColor(int(label), i) * getSegInteriorAlpha(i, label) * u_segOpacity[i] * float(segMask);
  }

  float val0[9];
  float val1[9];

  int count = 0;
  for (int j = -1; j <= 1; ++j)
  {
    for (int i = -1; i <= 1; ++i)
    {
      vec3 tex0Pos = fs_in.v_texCoord[0] + float(i) * u_tex0SamplingDirX + float(j) * u_tex0SamplingDirY;
      vec3 tex1Pos = vec3(u_texture1_T_texture0 * vec4(tex0Pos, 1.0));

      // val0[count] = texture(u_imgTex[0], tex0Pos).r;
      // val1[count] = texture(u_imgTex[1], tex1Pos).r;

      val0[count] = getImageValue(u_imgTex[0], tex0Pos);
      val1[count] = getImageValue(u_imgTex[1], tex1Pos);

      ++count;
    }
  }

  float mean0 = 0.0;
  float mean1 = 0.0;

  for (int i = 0; i < 9; ++i)
  {
    mean0 += val0[i] / 9.0;
    mean1 += val1[i] / 9.0;
  }

  float numer = 0.0;
  float denom0 = 0.0;
  float denom1 = 0.0;

  for (int i = 0; i < 9; ++i)
  {
    float a = (val0[i] - mean0);
    float b = (val1[i] - mean1);
    numer += a * b;
    denom0 += a * a;
    denom1 += b * b;
  }

  // Overall mask is the AND of both image masks:
  float metricMask = mask[0] * mask[1];

  // Compute metric and apply window/level to it:
  //float metric = (val0 * val1) * metricMask;
  float metric = numer / sqrt(denom0 * denom1);
  metric = 0.5 * (metric + 1.0);

  //metric = clamp(u_metricSlopeIntercept[0] * metric + u_metricSlopeIntercept[1], 0.0, 1.0);
  metric = u_metricSlopeIntercept[0] * metric + u_metricSlopeIntercept[1];

  // Index into colormap:
  float cmapValue = u_metricCmapSlopeIntercept[0] * metric + u_metricCmapSlopeIntercept[1];

  // Apply colormap and masking (by pre-multiplying RGBA with alpha mask):
  vec4 metricColor = texture(u_metricCmapTex, cmapValue) * metricMask;

  // Ignore the segmentation layers if metric masking is enabled:
  float overlaySegs = float(! u_metricMasking);
  segColor[0] = overlaySegs * segColor[0];
  segColor[1] = overlaySegs * segColor[1];

  // Blend colors:
  o_color = vec4(0.0, 0.0, 0.0, 0.0);
  o_color = segColor[0] + (1.0 - segColor[0].a) * metricColor;
  o_color = segColor[1] + (1.0 - segColor[1].a) * o_color;
}
