#version 330 core

// 3D texture coordinates (s,t,p) are in [0.0, 1.0]^3
#define MIN_IMAGE_TEXCOORD vec3(0.0)
#define MAX_IMAGE_TEXCOORD vec3(1.0)

// Rendering modes:
#define IMAGE_RENDER_MODE 0
#define CHECKER_RENDER_MODE 1
#define QUADRANTS_RENDER_MODE 2
#define FLASHLIGHT_RENDER_MODE 3

in VS_OUT
{
  vec3 v_texCoord;
  vec2 v_checkerCoord;
  vec2 v_clipPos;
} fs_in;

layout (location = 0) out vec4 o_color; // Output RGBA color (premultiplied alpha)

// Texture samplers:
uniform sampler3D u_imgTex; // image (scalar, red channel only)
uniform sampler1D u_cmapTex; // image color map (non-premultiplied RGBA)

// Image adjustment uniforms:
uniform vec2 u_imgSlopeIntercept; // slope/intercept for normalization to the largest window
uniform vec2 u_imgMinMax; // min/max image values (texture intenstiy units)
uniform vec2 u_imgThresholds; // lower/upper image thresholds (texture intensity units)
uniform float u_imgOpacity; // image opacity

// Image color map adjustment uniforms:
uniform vec2 u_cmapSlopeIntercept; // map texels to normalized range [0, 1]

// View render mode uniforms:
uniform int u_renderMode; // mode (0: normal, 1: checkerboard, 2: quadrants, 3: flashlight)
uniform vec2 u_clipCrosshairs; // crosshairs position in Clip space

// Should quadrants comparison mode be done along the x, y directions?
// If x is true, then compare along x; if y is true, then compare along y.
// If both are true, then compare along both.
uniform bvec2 u_quadrants;
uniform bool u_showFix; // flag that the either the fixed (true) or moving image is shown
uniform float u_aspectRatio; // view aspect ratio (width / height)
uniform float u_flashlightRadius; // flashlight circle radius
uniform bool u_flashlightMovingOnFixed; // overlay moving on fixed image (true) or opposite (false)

// Edge properties:
uniform bool u_thresholdEdges; // flag to threshold the edges
uniform float u_edgeMagnitude; // magnitude of edges to compute
uniform bool u_colormapEdges; // flag to apply colormap to edges
uniform vec4 u_edgeColor; // edge color (premultiplied RGBA)
uniform vec3 u_texelDirs[2]; // texture sampling direction for edges
uniform bool u_useFreiChen;

{{HELPER_FUNCTIONS}}
{{COLOR_HELPER_FUNCTIONS}}

/// float textureLookup(sampler3D texture, vec3 texCoord);
{{TEXTURE_LOOKUP_FUNCTION}}

/// bool doRender(vec2 clipPos, vec2 checkerCoord);
{{DO_RENDER_FUNCTION}}

// Sobel edge detection convolution filters:
// Scharr version (see https://en.wikipedia.org/wiki/Sobel_operator)
const float A = 1.0;
const float B = 2.0;

const mat3 Filter_Sobel[2] = mat3[](
  mat3(A,   B,  A,
       0,   0,  0,
       -A, -B, -A),
  mat3(A, 0, -A,
       B, 0, -B,
       A, 0, -A)
);

const float SobelFactor = 1.0 / (2.0 * A + B);

// Frei-Chen edge detection convolution filters:
const mat3 Filter_FC[9] = mat3[](
  1.0 / (2.0 * sqrt(2.0)) * mat3(1.0, sqrt(2.0), 1.0, 0.0, 0.0, 0.0, -1.0, -sqrt(2.0), -1.0),
  1.0 / (2.0 * sqrt(2.0)) * mat3(1.0, 0.0, -1.0, sqrt(2.0), 0.0, -sqrt(2.0), 1.0, 0.0, -1.0),
  1.0 / (2.0 * sqrt(2.0)) * mat3(0.0, -1.0, sqrt(2.0), 1.0, 0.0, -1.0, -sqrt(2.0), 1.0, 0.0),
  1.0 / (2.0 * sqrt(2.0)) * mat3(sqrt(2.0), -1.0, 0.0, -1.0, 0.0, 1.0, 0.0, 1.0, -sqrt(2.0)),
  1.0 / 2.0 * mat3(0.0, 1.0, 0.0, -1.0, 0.0, -1.0, 0.0, 1.0, 0.0),
  1.0 / 2.0 * mat3(-1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, -1.0),
  1.0 / 6.0 * mat3(1.0, -2.0, 1.0, -2.0, 4.0, -2.0, 1.0, -2.0, 1.0),
  1.0 / 6.0 * mat3(-2.0, 1.0, -2.0, 1.0, 4.0, 1.0, -2.0, 1.0, -2.0),
  1.0 / 3.0 * mat3(1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0)
);

void main()
{
  if (!doRender(fs_in.v_clipPos, fs_in.v_checkerCoord)) {
    discard;
  }

  mat3 V; // Image values in a 3x3 neighborhood

  for (int j = 0; j <= 2; ++j) {
    for (int i = 0; i <= 2; ++i)
    {
      vec3 texSamplingPos = float(i - 1) * u_texelDirs[0] + float(j - 1) * u_texelDirs[1];
      float v = clamp(textureLookup(u_imgTex, fs_in.v_texCoord + texSamplingPos), u_imgMinMax[0], u_imgMinMax[1]);
      V[i][j] = u_imgSlopeIntercept[0] * v + u_imgSlopeIntercept[1]; // max window/level to normalize values in [0.0, 1.0]
    }
  }

  // Convolutions for all masks:
  float C_Sobel[2];
  for (int i = 0; i <= 1; ++i) {
    C_Sobel[i] = dot(Filter_Sobel[i][0], V[0]) + dot(Filter_Sobel[i][1], V[1]) + dot(Filter_Sobel[i][2], V[2]);
    C_Sobel[i] *= C_Sobel[i];
  }



  float gradMag_Sobel = SobelFactor * sqrt(C_Sobel[0] + C_Sobel[1]) / max(u_edgeMagnitude, 0.01);

  float C_FC[9];
  for (int i = 0; i <= 8; ++i)
  {
    C_FC[i] = dot(Filter_FC[i][0], V[0]) + dot(Filter_FC[i][1], V[1]) + dot(Filter_FC[i][2], V[2]);
    C_FC[i] *= C_FC[i];
  }

  float M_FC = (C_FC[0] + C_FC[1]) + (C_FC[2] + C_FC[3]);
  float S_FC = (C_FC[4] + C_FC[5]) + (C_FC[6] + C_FC[7]) + (C_FC[8] + M_FC);
  float gradMag_FC = sqrt(M_FC / S_FC) / max(u_edgeMagnitude, 0.01);

  // Choose Sobel or Frei-Chen:
  float gradMag = mix(gradMag_Sobel, gradMag_FC, float(u_useFreiChen));

  // If u_thresholdEdges is true, then threshold gradMag against u_edgeMagnitude:
  // float gradMag = gradMag_Sobel;
  gradMag = mix(gradMag, float(gradMag > u_edgeMagnitude), float(u_thresholdEdges));

  float mask = float(isInsideTexture(fs_in.v_texCoord));
  float img = clamp(textureLookup(u_imgTex, fs_in.v_texCoord), u_imgMinMax[0], u_imgMinMax[1]);
  float alpha = u_imgOpacity * mask * hardThreshold(img, u_imgThresholds);

  vec4 gradColormap = texture(u_cmapTex, u_cmapSlopeIntercept[0] * gradMag + u_cmapSlopeIntercept[1]);

  // Output color (premult. RGBA):
  o_color = alpha * mix(gradMag * u_edgeColor, gradColormap, float(u_colormapEdges));
}
