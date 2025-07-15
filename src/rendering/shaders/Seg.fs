#version 330 core

// 3D texture coordinates (s,t,p) are in [0.0, 1.0]^3
#define MIN_IMAGE_TEXCOORD vec3(0.0)
#define MAX_IMAGE_TEXCOORD vec3(1.0)

// Rendering modes:
#define IMAGE_RENDER_MODE      0
#define CHECKER_RENDER_MODE    1
#define QUADRANTS_RENDER_MODE  2
#define FLASHLIGHT_RENDER_MODE 3

// Redeclared vertex shader outputs, which are now the fragment shader inputs
in VS_OUT
{
  vec3 v_segTexCoords;
  vec3 v_segVoxCoords;
  vec2 v_checkerCoord;
  vec2 v_clipPos;
} fs_in;

layout (location = 0) out vec4 o_color; // Output RGBA color (pre-multiplied alpha)

uniform usampler3D u_segTex; // Texture unit 1: segmentation (scalar)
uniform samplerBuffer u_segLabelCmapTex; // Texutre unit 3: label color map (non-pre-multiplied RGBA)

uniform float u_segOpacity; // Segmentation opacity
uniform vec2 u_clipCrosshairs; // Crosshairs in Clip space
uniform float u_aspectRatio; // View aspect ratio (width / height)

// Should quadrants comparison mode be done along the x, y directions?
// If x is true, then compare along x; if y is true, then compare along y.
// If both are true, then compare along both.
uniform bvec2 u_quadrants;

// Should the fixed image be rendered (true) or the moving image (false)?
uniform bool u_showFix;

// Render mode (0: normal, 1: checkerboard, 2: u_quadrants, 3: flashlight)
uniform int u_renderMode;

// When true, the flashlight overlays the moving image on top of fixed image.
// When false, the flashlight replaces the fixed image with the moving image.
uniform bool u_flashlightOverlays;
uniform float u_flashlightRadius;

// Opacity of the interior of the segmentation
uniform float u_segInteriorOpacity;

// Texture sampling directions (horizontal and vertical) for calculating the seg outline
uniform vec3 u_texSamplingDirsForSegOutline[2];

/// Linear lookup:
//uniform vec3 u_texSamplingDirsForSmoothSeg[2];

//// Interpolation cut-off for segmentation (in [0, 1])
//uniform float u_segInterpCutoff;

// OPTIONS:
// 1) Image projection: none, enabled (per image setting)
// 2) Segmentation interpolation: nn, linear, cubic (global setting)
// 3) Outlining: solid, outline (global setting)

const uvec3 neigh[8] = uvec3[8](
  uvec3(0, 0, 0), uvec3(0, 0, 1), uvec3(0, 1, 0), uvec3(0, 1, 1),
  uvec3(1, 0, 0), uvec3(1, 0, 1), uvec3(1, 1, 0), uvec3(1, 1, 1));

int when_lt(int x, int y)
{
  return max(sign(y - x), 0);
}

int when_ge(int x, int y)
{
  return (1 - when_lt(x, y));
}

// Remapping the unit interval into the unit interval by expanding the
// sides and compressing the center, and keeping 1/2 mapped to 1/2,
// that can be done with the gain() function. This was a common function
// in RSL tutorials (the Renderman Shading Language). k=1 is the identity curve,
// k<1 produces the classic gain() shape, and k>1 produces "s" shaped curces.
// The curves are symmetric (and inverse) for k=a and k=1/a.

float cubicPulse(float center, float width, float x)
{
  x = abs(x - center);
  if (x > width) return 0.0;
  x /= width;
  return 1.0 - x * x * (3.0 - 2.0 * x);
}

// Check if inside texture coordinates
bool isInsideTexture(vec3 a)
{
  return (all(greaterThanEqual(a, MIN_IMAGE_TEXCOORD)) &&
          all(lessThanEqual(a, MAX_IMAGE_TEXCOORD)));
}

//! Tricubic interpolated texture lookup
//! Fast implementation, using 8 trilinear lookups.
//! @param[in] tex 3D texture sampler
//! @param[in] coord Normalized 3D texture coordinate
//! @see https://github.com/DannyRuijters/CubicInterpolationCUDA/blob/master/examples/glCubicRayCast/tricubic.shader
float interpolateTricubicFast(sampler3D tex, vec3 coord)
{
  // Shift the coordinate from [0,1] to [-0.5, nrOfVoxels - 0.5]
  vec3 nrOfVoxels = vec3(textureSize(tex, 0));
  vec3 coord_grid = coord * nrOfVoxels - 0.5;
  vec3 index = floor(coord_grid);
  vec3 fraction = coord_grid - index;
  vec3 one_frac = 1.0 - fraction;

  vec3 w0 = 1.0/6.0 * one_frac*one_frac*one_frac;
  vec3 w1 = 2.0/3.0 - 0.5 * fraction*fraction*(2.0-fraction);
  vec3 w2 = 2.0/3.0 - 0.5 * one_frac*one_frac*(2.0-one_frac);
  vec3 w3 = 1.0/6.0 * fraction*fraction*fraction;

  vec3 g0 = w0 + w1;
  vec3 g1 = w2 + w3;
  vec3 mult = 1.0 / nrOfVoxels;

  // h0 = w1/g0 - 1, move from [-0.5, nrOfVoxels-0.5] to [0,1]
  vec3 h0 = mult * ((w1 / g0) - 0.5 + index);

  // h1 = w3/g1 + 1, move from [-0.5, nrOfVoxels-0.5] to [0,1]
  vec3 h1 = mult * ((w3 / g1) + 1.5 + index);

  // Fetch the eight linear interpolations.
  // Weighting and feching is interleaved for performance and stability reasons.
  float tex000 = texture(tex, h0)[0];
  float tex100 = texture(tex, vec3(h1.x, h0.y, h0.z))[0];

  tex000 = mix(tex100, tex000, g0.x); // weigh along the x-direction
  float tex010 = texture(tex, vec3(h0.x, h1.y, h0.z))[0];
  float tex110 = texture(tex, vec3(h1.x, h1.y, h0.z))[0];

  tex010 = mix(tex110, tex010, g0.x); // weigh along the x-direction
  tex000 = mix(tex010, tex000, g0.y); // weigh along the y-direction

  float tex001 = texture(tex, vec3(h0.x, h0.y, h1.z))[0];
  float tex101 = texture(tex, vec3(h1.x, h0.y, h1.z))[0];

  tex001 = mix(tex101, tex001, g0.x); // weigh along the x-direction

  float tex011 = texture(tex, vec3(h0.x, h1.y, h1.z))[0];
  float tex111 = texture(tex, h1)[0];

  tex011 = mix(tex111, tex011, g0.x); // weigh along the x-direction
  tex001 = mix(tex011, tex001, g0.y); // weigh along the y-direction

  return mix(tex001, tex000, g0.z); // weigh along the z-direction
}


/// Look up the image value (after mapping to GL texture units)

/// Nearest-neighbor or linear interpolation:
float getImageValue(sampler3D tex, vec3 texCoords, float minVal, float maxVal)
{
   return clamp(texture(tex, texCoords)[0], minVal, maxVal);
}

/// Cubic interpolation:
//float getImageValue(sampler3D tex, vec3 texCoords, float minVal, float maxVal)
//{
//  return clamp(interpolateTricubicFast(tex, texCoords), minVal, maxVal);
//}

vec4 computeLabelColor(int label)
{
  // Labels greater than the size of the segmentation labelc color texture are mapped to 0
  label -= label * when_ge(label, textureSize(u_segLabelCmapTex));

  vec4 color = texelFetch(u_segLabelCmapTex, label);
  return color.a * color; // pre-multiply by alpha
}

/// Look up segmentation texture label value

/// Default nearest-neighbor lookup:
uint getSegValue(vec3 texOffset, out float opacity)
{
  opacity = 1.0;
  return texture(u_segTex, fs_in.v_segTexCoords + texOffset)[0];
}


//uint getSegValue(vec3 texOffset, out float opacity)
//{
//  uint seg = 0u;
//  opacity = 0.0;

//  vec3 c = floor(fs_in.v_segVoxCoords);
//  vec3 d = pow(vec3(textureSize(u_segTex, 0)), vec3(-1));
//  vec3 t = vec3(c.x * d.x, c.y * d.y, c.z * d.z) + 0.5 * d;

//  uint s[8];
//  for (int i = 0; i < 8; ++i)
//  {
//    s[i] = texture(u_segTex, t + neigh[i] * d + texOffset)[0];
//  }

//  vec3 b = fs_in.v_segVoxCoords + texOffset * vec3(textureSize(u_segTex, 0)) - c;

//  vec3 g[2] = vec3[2](vec3(1) - b, b);

//  // float segEdgeWidth = 0.02;


//  uint neighSegs[9];

//  // Look up texture values in the fragment and its 8 neighbors.
//  // The center fragment (row = 0, col = 0) has index i = 4.
//  for (int i = 0; i <= 8; ++i)
//  {
//    int j = int(mod(i + 4, 9)); // j = [4,5,6,7,8,0,1,2,3]

//    float row = float(mod(j, 3) - 1); // [-1,0,1]
//    float col = float(floor(float(j / 3)) - 1); // [-1,0,1]

//    vec3 texPos = row * u_texSamplingDirsForSmoothSeg[0] +
//            col * u_texSamplingDirsForSmoothSeg[1];

//    // Segmentation value of neighbor at (row, col) offset
//    neighSegs[i] = texture(u_segTex, fs_in.v_segTexCoords + texPos)[0];
//  }


//  float maxInterp = 0.0;

//  for (int i = 0; i <= 8; ++i)
//  {
//    uint label = neighSegs[i];

//    float interp = 0.0;
//    for (int j = 0; j <= 7; ++j)
//    {
//      interp += float(s[j] == label) *
//        g[neigh[j].x].x * g[neigh[j].y].y * g[neigh[j].z].z;
//    }

//    // This feathers the edges:
//    // opacity = smoothstep(
//    //   clamp(u_segInterpCutoff - segEdgeWidth/2.0, 0.0, 1.0),
//    //   clamp(u_segInterpCutoff + segEdgeWidth/2.0, 0.0, 1.0), interp);

//    opacity = 1.0;
//    // opacity = cubicPulse(u_segInterpCutoff, segEdgeWidth, interp);

//    if (interp > maxInterp &&
//       interp >= u_segInterpCutoff &&
//       computeLabelColor(int(label)).a > 0.0)
//    {
//      seg = label;
//      maxInterp = interp;

//      if (u_segInterpCutoff >= 0.5)
//      {
//        break;
//      }
//    }
//  }

//  return seg;
//}



/// Compute alpha of fragments based on whether or not they are inside the
/// segmentation boundary. Fragments on the boundary are assigned alpha of 1,
/// whereas fragments inside are assigned alpha of 'u_segInteriorOpacity'.

/// Nearest neighbor sampling:
//float getSegInteriorAlpha(uint seg)
//{
//  return 1.0;
//}


/// Linear sampling:

float getSegInteriorAlpha(uint seg)
{
  // Look up texture values in 8 neighbors surrounding the center fragment.
  // These may be either neighboring image voxels or neighboring view pixels.
  // The center fragment (row = 0, col = 0) has index i = 4.
  for (int i = 0; i <= 8; ++i)
  {
    float row = float(mod(i, 3) - 1); // [-1,0,1]
    float col = float(floor(float(i / 3)) - 1); // [-1,0,1]

    vec3 texPosOffset = row * u_texSamplingDirsForSegOutline[0] +
      col * u_texSamplingDirsForSegOutline[1];

    // Segmentation value of neighbor at (row, col) offset:
    float ignore;

    if (seg != getSegValue(texPosOffset, ignore))
    {
      // Fragment (with segmentation 'seg') is on the segmentation boundary,
      // since its value is not equal to one of its neighbors. Therefore, it gets full alpha.
      return 1.0;
    }
  }

  return u_segInteriorOpacity;
}

bool doRender()
{
  // Indicator of the quadrant of the crosshairs that the fragment is in:
  bvec2 Q = bvec2(fs_in.v_clipPos.x <= u_clipCrosshairs.x,
           fs_in.v_clipPos.y > u_clipCrosshairs.y);

  // Distance of the fragment from the crosshairs, accounting for aspect ratio:
  float flashlightDist = sqrt(
    pow(u_aspectRatio * (fs_in.v_clipPos.x - u_clipCrosshairs.x), 2.0) +
    pow(fs_in.v_clipPos.y - u_clipCrosshairs.y, 2.0));

  // Flag indicating whether the fragment will be rendered:
  bool render = (IMAGE_RENDER_MODE == u_renderMode);

  // If in Checkerboard mode, then render the fragment?
  render = render || ((CHECKER_RENDER_MODE == u_renderMode) &&
    (u_showFix == bool(mod(floor(fs_in.v_checkerCoord.x) +
                floor(fs_in.v_checkerCoord.y), 2.0) > 0.5)));

  // If in Quadrants mode, then render the fragment?
  render = render || ((QUADRANTS_RENDER_MODE == u_renderMode) &&
    (u_showFix == ((! u_quadrants.x || Q.x) == (! u_quadrants.y || Q.y))));

  // If in Flashlight mode, then render the fragment?
  render = render || ((FLASHLIGHT_RENDER_MODE == u_renderMode) &&
    ((u_showFix == (flashlightDist > u_flashlightRadius)) ||
      (u_flashlightOverlays && u_showFix)));

  return render;
}

void main()
{
  if (!doRender()) discard;

  float segInterpOpacity = 1.0;
  uint seg = getSegValue(vec3(0, 0, 0), segInterpOpacity);

  // Segmentation mask based on texture coordinates:
  bool segMask = isInsideTexture(fs_in.v_segTexCoords);

  // Compute segmentation alpha based on opacity and mask:
  float segAlpha = u_segOpacity * segInterpOpacity * getSegInteriorAlpha(seg) * float(segMask);

  // Look up segmentation color:
  vec4 segLayerColor = computeLabelColor(int(seg)) * segAlpha;

  o_color = vec4(0.0, 0.0, 0.0, 0.0);
  o_color = segLayerColor + (1.0 - segLayerColor.a) * o_color;
}
