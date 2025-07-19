#version 330 core

// 3D texture coordinates (s,t,p) are in [0.0, 1.0]^3
#define MIN_IMAGE_TEXCOORD vec3(0.0)
#define MAX_IMAGE_TEXCOORD vec3(1.0)

// Rendering modes:
#define IMAGE_RENDER_MODE 0
#define CHECKER_RENDER_MODE 1
#define QUADRANTS_RENDER_MODE 2
#define FLASHLIGHT_RENDER_MODE 3

in VS_OUT // Redeclared vertex shader outputs: now the fragment shader inputs
{
  vec3 v_imgTexCoords;
  vec2 v_checkerCoord;
  vec2 v_clipPos;
} fs_in;

layout (location = 0) out vec4 o_color; // Output RGBA color (pre-multiplied alpha)

uniform sampler3D u_imgTex; // Texture unit 0: image
uniform sampler1D u_imgCmapTex; // Texture unit 1: image color map (pre-mult RGBA)

uniform vec2 u_imgSlopeIntercept; // Slopes and intercepts for image normalization and window-leveling
uniform vec2 u_imgSlopeInterceptLargest; // Slopes and intercepts for image normalization
uniform vec2 u_imgCmapSlopeIntercept; // Slopes and intercepts for the image color maps
uniform int u_imgCmapQuantLevels; // Number of quantization levels
uniform vec3 u_imgCmapHsvModFactors; // HSV modification factors for image color
uniform bool u_useHsv; // Flag that HSV modification is used

uniform vec2 u_imgMinMax; // Min and max image values
uniform vec2 u_imgThresholds; // Image lower and upper thresholds, mapped to OpenGL texture intensity
uniform float u_imgOpacity; // Image opacities

uniform vec2 u_clipCrosshairs; // Crosshairs in Clip space

// Should comparison be done in x,y directions?
// If x is true, then compare along x; if y is true, then compare along y;
// if both are true, then compare along both.
uniform bvec2 u_quadrants;

// Should the fixed image be rendered (true) or the moving image (false):
uniform bool u_showFix;

// Render mode: 0 - normal, 1 - checkerboard, 2 - u_quadrants, 3 - flashlight
uniform int u_renderMode;

uniform float u_aspectRatio;

uniform float u_flashlightRadius;

// When true, the flashlight overlays the moving image on top of fixed image.
// When false, the flashlight replaces the fixed image with the moving image.
uniform bool u_flashlightOverlays;

// Edge properties:
uniform bool u_thresholdEdges; // Threshold the edges
uniform float u_edgeMagnitude; // Magnitude of edges to compute
uniform bool u_overlayEdges; // Overlay edges on image
uniform bool u_colormapEdges; // Apply colormap to edges
uniform vec4 u_edgeColor; // RGBA, pre-multiplied by alpha
//uniform bool useFreiChen;

uniform vec3 u_texSamplingDirsForEdges[2];

/// Copied from https://www.laurivan.com/rgb-to-hsv-to-rgb-for-shaders/
vec3 rgb2hsv(vec3 c)
{
  vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
  vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
  vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

  float d = q.x - min(q.w, q.y);
  float e = 1.0e-10;
  return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c)
{
  vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
  return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// Sobel edge detection convolution filters:
// Scharr version (see https://en.wikipedia.org/wiki/Sobel_operator)
const float A = 1.0;
const float B = 2.0;

const mat3 Filter_Sobel[2] = mat3[](
  mat3(A, B, A, 0.0, 0.0, 0.0, -A, -B, -A),
  mat3(A, 0.0, -A, B, 0.0, -B, A, 0.0, -A)
);

const float SobelFactor = 1.0 / (2.0*A + B);

// Frei-Chen edge detection convolution filters:
//const mat3 Filter_FC[9] = mat3[](
//  1.0 / (2.0 * sqrt(2.0)) * mat3(1.0, sqrt(2.0), 1.0, 0.0, 0.0, 0.0, -1.0, -sqrt(2.0), -1.0),
//  1.0 / (2.0 * sqrt(2.0)) * mat3(1.0, 0.0, -1.0, sqrt(2.0), 0.0, -sqrt(2.0), 1.0, 0.0, -1.0),
//  1.0 / (2.0 * sqrt(2.0)) * mat3(0.0, -1.0, sqrt(2.0), 1.0, 0.0, -1.0, -sqrt(2.0), 1.0, 0.0),
//  1.0 / (2.0 * sqrt(2.0)) * mat3(sqrt(2.0), -1.0, 0.0, -1.0, 0.0, 1.0, 0.0, 1.0, -sqrt(2.0)),
//  1.0 / 2.0 * mat3(0.0, 1.0, 0.0, -1.0, 0.0, -1.0, 0.0, 1.0, 0.0),
//  1.0 / 2.0 * mat3(-1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, -1.0),
//  1.0 / 6.0 * mat3(1.0, -2.0, 1.0, -2.0, 4.0, -2.0, 1.0, -2.0, 1.0),
//  1.0 / 6.0 * mat3(-2.0, 1.0, -2.0, 1.0, 4.0, 1.0, -2.0, 1.0, -2.0),
//  1.0 / 3.0 * mat3(1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0)
//);

float smoothThreshold(float value, vec2 thresholds)
{
  return smoothstep(thresholds[0] - 0.01, thresholds[0], value) -
         smoothstep(thresholds[1], thresholds[1] + 0.01, value);
}

float hardThreshold(float value, vec2 thresholds)
{
  return float(thresholds[0] <= value && value <= thresholds[1]);
}

bool isInsideTexture(vec3 a)
{
  return (all(greaterThanEqual(a, MIN_IMAGE_TEXCOORD)) &&
          all(lessThanEqual(a, MAX_IMAGE_TEXCOORD)));
}

// float textureLookup(sampler3D texture, vec3 texCoords);
{{TEXTURE_LOOKUP_FUNCTION}}

bool doRender()
{
  // Indicator of the quadrant of the crosshairs that the fragment is in:
  bvec2 Q = bvec2(fs_in.v_clipPos.x <= u_clipCrosshairs.x, fs_in.v_clipPos.y > u_clipCrosshairs.y);

  // Distance of the fragment from the crosshairs, accounting for aspect ratio:
  float flashlightDist = sqrt(pow(u_aspectRatio * (fs_in.v_clipPos.x - u_clipCrosshairs.x), 2.0) +
                              pow(fs_in.v_clipPos.y - u_clipCrosshairs.y, 2.0));

  bool render = (IMAGE_RENDER_MODE == u_renderMode); // Flag indicating whether the fragment will be rendere

  // If in Checkerboard/Quadrants/Flashlight mode, then render the fragment?
  render = render || ((CHECKER_RENDER_MODE == u_renderMode) &&
    (u_showFix == bool(mod(floor(fs_in.v_checkerCoord.x) + floor(fs_in.v_checkerCoord.y), 2.0) > 0.5)));

  render = render || ((QUADRANTS_RENDER_MODE == u_renderMode) &&
    (u_showFix == ((! u_quadrants.x || Q.x) == (! u_quadrants.y || Q.y))));

  render = render || ((FLASHLIGHT_RENDER_MODE == u_renderMode) &&
    ((u_showFix == (flashlightDist > u_flashlightRadius)) || (u_flashlightOverlays && u_showFix)));

  return render;
}

void main()
{
  if (!doRender()) discard;

  mat3 V; // Image values in a 3x3 neighborhood

  for (int j = 0; j <= 2; ++j) {
    for (int i = 0; i <= 2; ++i)
    {
      vec3 texSamplingPos = float(i - 1) * u_texSamplingDirsForEdges[0] +
                            float(j - 1) * u_texSamplingDirsForEdges[1];

      //float v = texture(u_imgTex, fs_in.v_imgTexCoords + texSamplingPos).r;
      float v = clamp(textureLookup(u_imgTex, fs_in.v_imgTexCoords + texSamplingPos), u_imgMinMax[0], u_imgMinMax[1]);

      // Apply maximum window/level to normalize value in [0.0, 1.0]:
      V[i][j] = u_imgSlopeInterceptLargest[0] * v + u_imgSlopeInterceptLargest[1];
    }
  }

  // Convolutions for all masks:
  float C_Sobel[2];
  for (int i = 0; i <= 1; ++i) {
    C_Sobel[i] = dot(Filter_Sobel[i][0], V[0]) + dot(Filter_Sobel[i][1], V[1]) + dot(Filter_Sobel[i][2], V[2]);
    C_Sobel[i] *= C_Sobel[i];
  }

  float gradMag_Sobel = SobelFactor * sqrt(C_Sobel[0] + C_Sobel[1]) / max(u_edgeMagnitude, 0.01);

//  float C_FC[9];
//  for (int i = 0; i <= 8; ++i)
//  {
//    C_FC[i] = dot(Filter_FC[i][0], V[0]) + dot(Filter_FC[i][1], V[1]) + dot(Filter_FC[i][2], V[2]);
//    C_FC[i] *= C_FC[i];
//  }

//  float M_FC = (C_FC[0] + C_FC[1]) + (C_FC[2] + C_FC[3]);
//  float S_FC = (C_FC[4] + C_FC[5]) + (C_FC[6] + C_FC[7]) + (C_FC[8] + M_FC);
//  float gradMag_FC = sqrt(M_FC / S_FC) / max(u_edgeMagnitude, 0.01);

  // Choose Sobel or Frei-Chen:
//  float gradMag = mix(gradMag_Sobel, gradMag_FC, float(useFreiChen));
  float gradMag = gradMag_Sobel;

  // If u_thresholdEdges is true, then threshold gradMag against u_edgeMagnitude:
  gradMag = mix(gradMag, float(gradMag > u_edgeMagnitude), float(u_thresholdEdges));

  // Get the image value:
  // float img = texture(u_imgTex, fs_in.v_imgTexCoords).r;

  float img = clamp(textureLookup(u_imgTex, fs_in.v_imgTexCoords), u_imgMinMax[0], u_imgMinMax[1]);

  // Apply window/level and normalize value in [0.0, 1.0]:
  float imgNorm = clamp(u_imgSlopeIntercept[0] * img + u_imgSlopeIntercept[1], 0.0, 1.0);

  // Apply color map to the image intensity:
  // Compute coords into the image color map, accounting for quantization levels:
  float cmapCoord = mix(floor(float(u_imgCmapQuantLevels) * imgNorm) / float(u_imgCmapQuantLevels - 1), imgNorm, float(0 == u_imgCmapQuantLevels));
  cmapCoord = u_imgCmapSlopeIntercept[0] * cmapCoord + u_imgCmapSlopeIntercept[1]; // normalize coords

  vec4 imgColorOrig = texture(u_imgCmapTex, cmapCoord); // image color (non-pre-mult. RGBA)

  // Apply HSV modification factors:
  vec3 imgColorHsv = rgb2hsv(imgColorOrig.rgb);
  imgColorHsv.x += u_imgCmapHsvModFactors.x;
  imgColorHsv.yz *= u_imgCmapHsvModFactors.yz;

  // Alpha accounts for opacity, masking, and thresholding:
  // Alpha will be applied to the image and edge layers.
  // Foreground mask based on whether texture coordinates are in range [0.0, 1.0]^3:
  float mask = float(isInsideTexture(fs_in.v_imgTexCoords));
  float alpha = u_imgOpacity * mask * hardThreshold(img, u_imgThresholds);

  // Disable the image color if u_overlayEdges is false
  vec4 imgLayer = alpha * float(u_overlayEdges) * imgColorOrig.a * vec4(mix(imgColorOrig.rgb, hsv2rgb(imgColorHsv), float(u_useHsv)), 1.0);

  // Apply color map to gradient magnitude:
  vec4 gradColormap = texture(u_imgCmapTex, u_imgCmapSlopeIntercept[0] * gradMag + u_imgCmapSlopeIntercept[1]);

  // For the edge layer, use either the solid edge color or the colormapped gradient magnitude:
  vec4 edgeLayer = alpha * mix(gradMag * u_edgeColor, gradColormap, float(u_colormapEdges));

  // Blend colors:
  o_color = vec4(0.0, 0.0, 0.0, 0.0);
  o_color = imgLayer + (1.0 - imgLayer.a) * o_color;
  o_color = edgeLayer + (1.0 - edgeLayer.a) * o_color;
}
