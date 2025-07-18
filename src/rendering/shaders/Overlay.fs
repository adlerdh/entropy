#version 330 core

#define MIN_IMAGE_TEXCOORD vec3(0.0)
#define MAX_IMAGE_TEXCOORD vec3(1.0)

#define N 2

in VS_OUT // Redeclared vertex shader outputs: now the fragment shader inputs
{
  vec3 v_imgTexCoords[N];
  vec3 v_segTexCoords[N];
} fs_in;

layout (location = 0) out vec4 o_color; // Output RGBA color (pre-multiplied alpha)

uniform sampler3D u_imgTex[N]; // Texture units 0/1: images
uniform usampler3D u_segTex[N]; // Texture units 2/3: segmentations
uniform samplerBuffer u_segLabelCmapTex[N]; // Texutre unit 4/5: label color maps (pre-mult RGBA)

uniform vec2 u_imgSlopeIntercept[N]; // Slopes and intercepts for image window-leveling

uniform vec2 u_imgMinMax[N]; // Min and max image values
uniform vec2 u_imgThresholds[N]; // Image lower and upper thresholds, mapped to OpenGL texture intensity
uniform float u_imgOpacity[N]; // Image opacities
uniform float u_segOpacity[N]; // Segmentation opacities

// Texture sampling directions (horizontal and vertical) for calculating the segmentation outline
uniform vec3 u_texSamplingDirsForSegOutline[2];

// Opacity of the interior of the segmentation
uniform float u_segInteriorOpacity;

uniform bool magentaCyan;

float smoothThreshold(float value, vec2 thresholds)
{
  return smoothstep(thresholds[0] - 0.01, thresholds[0], value) -
       smoothstep(thresholds[1], thresholds[1] + 0.01, value);
}

float hardThreshold(float value, vec2 thresholds)
{
  return float(thresholds[0] <= value && value <= thresholds[1]);
}

int when_lt(int x, int y)
{
  return max(sign(y - x), 0);
}

int when_ge(int x, int y)
{
  return (1 - when_lt(x, y));
}

vec4 computeLabelColor(int label, int i)
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

// Compute alpha of fragments based on whether or not they are inside the
// segmentation boundary. Fragments on the boundary are assigned alpha of 1,
// whereas fragments inside are assigned alpha of 'u_segInteriorOpacity'.
float getSegInteriorAlpha(int texNum, uint seg)
{
  float segInteriorAlpha = u_segInteriorOpacity;

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
    // uint nseg = texture(u_segTex[texNum], fs_in.v_segTexCoords[texNum] + texSamplingPos)[0];
    uint nseg;

    switch (texNum)
    {
    case 0:
    {
      nseg = texture(u_segTex[0], fs_in.v_segTexCoords[texNum] + texSamplingPos)[0];
      break;
    }
    case 1:
    {
      nseg = texture(u_segTex[1], fs_in.v_segTexCoords[texNum] + texSamplingPos)[0];
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

float getImageValue(sampler3D tex, vec3 texCoord, vec2 minMax)
{
  return clamp(texture(tex, texCoord)[0], minMax[0], minMax[1]);
}

void main()
{
  vec4 overlapColor = vec4(0.0, 0.0, 0.0, 0.0);
  vec4 segColor[2];

  for (int i = 0; i < N; ++i)
  {
    // Foreground masks, based on whether texture coordinates are in range [0.0, 1.0]^3:
    bool imgMask = ! (any( lessThan(fs_in.v_imgTexCoords[i], MIN_IMAGE_TEXCOORD)) ||
               any(greaterThan(fs_in.v_imgTexCoords[i], MAX_IMAGE_TEXCOORD)));

    bool segMask = ! (any( lessThan(fs_in.v_segTexCoords[i], MIN_IMAGE_TEXCOORD)) ||
               any(greaterThan(fs_in.v_segTexCoords[i], MAX_IMAGE_TEXCOORD)));

    // float val = texture(u_imgTex[i], fs_in.v_imgTexCoords[i]).r; // Image value
//    float val = getImageValue(u_imgTex[i], fs_in.v_imgTexCoords[i], u_imgMinMax[i]);
//    uint label = texture(u_segTex[i], fs_in.v_segTexCoords[i]).r; // Label value

    float val;
    uint label;

    switch (i)
    {
    case 0:
    {
      val = getImageValue(u_imgTex[0], fs_in.v_imgTexCoords[i], u_imgMinMax[i]);
      label = texture(u_segTex[0], fs_in.v_segTexCoords[i]).r;
      break;
    }
    case 1:
    {
      val = getImageValue(u_imgTex[1], fs_in.v_imgTexCoords[i], u_imgMinMax[i]);
      label = texture(u_segTex[1], fs_in.v_segTexCoords[i]).r;
      break;
    }
    }

    float norm = clamp(u_imgSlopeIntercept[i][0] * val + u_imgSlopeIntercept[i][1], 0.0, 1.0); // Apply W/L

    // Apply opacity, foreground mask, and thresholds for images:
    float alpha = u_imgOpacity[i] * float(imgMask) * hardThreshold(val, u_imgThresholds[i]);

    // Look up label colors:
    segColor[i] = computeLabelColor(int(label), i) * getSegInteriorAlpha(i, label) * u_segOpacity[i] * float(segMask);

    overlapColor[i] = norm * alpha;
  }

  // Apply magenta/cyan option:
  overlapColor.b = float(magentaCyan) * max(overlapColor.r, overlapColor.g);

  // Turn on overlap color if either the reference or moving image is present:
  // (note that this operation effectively thresholds out 0.0 as background)
  overlapColor.a = float((overlapColor.r > 0.0) || (overlapColor.g > 0.0));
  overlapColor.rgb = overlapColor.a * overlapColor.rgb;

  o_color = segColor[0] + (1.0 - segColor[0].a) * overlapColor;
  o_color = segColor[1] + (1.0 - segColor[1].a) * o_color;
}
