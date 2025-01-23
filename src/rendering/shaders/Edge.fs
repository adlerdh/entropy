#version 330 core

#define MIN_IMAGE_TEXCOORD vec3( 0.0 )
#define MAX_IMAGE_TEXCOORD vec3( 1.0 )

in VS_OUT // Redeclared vertex shader outputs: now the fragment shader inputs
{
    vec3 v_imgTexCoords;
    vec3 v_segTexCoords;
    vec3 v_segVoxCoords;
    vec2 v_checkerCoord;
    vec2 v_clipPos;
} fs_in;

layout (location = 0) out vec4 o_color; // Output RGBA color (pre-multiplied alpha)

uniform sampler3D u_imgTex; // Texture unit 0: image
uniform usampler3D u_segTex; // Texture unit 1: segmentation
uniform sampler1D u_imgCmapTex; // Texture unit 2: image color map (pre-mult RGBA)
uniform samplerBuffer u_segLabelCmapTex; // Texutre unit 3: label color map (pre-mult RGBA)

// uniform bool u_useTricubicInterpolation; // Whether to use tricubic interpolation

uniform vec2 u_imgSlopeIntercept; // Slopes and intercepts for image normalization and window-leveling
uniform vec2 u_imgSlopeInterceptLargest; // Slopes and intercepts for image normalization
uniform vec2 u_imgCmapSlopeIntercept; // Slopes and intercepts for the image color maps
uniform int u_imgCmapQuantLevels; // Number of quantization levels
uniform vec3 u_imgCmapHsvModFactors; // HSV modification factors for image color

uniform vec2 u_imgMinMax; // Min and max image values
uniform vec2 u_imgThresholds; // Image lower and upper thresholds, mapped to OpenGL texture intensity
uniform float u_imgOpacity; // Image opacities
uniform float u_segOpacity; // Segmentation opacities

uniform bool u_masking; // Whether to mask image based on segmentation

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

// Texture sampling directions (horizontal and vertical) for calculating the segmentation outline
uniform vec3 u_texSamplingDirsForSegOutline[2];

// Opacity of the interior of the segmentation
uniform float u_segInteriorOpacity;


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
    mat3( A, B, A, 0.0, 0.0, 0.0, -A, -B, -A ),
    mat3( A, 0.0, -A, B, 0.0, -B, A, 0.0, -A )
);

const float SobelFactor = 1.0 / ( 2.0*A + B );

// Frei-Chen edge detection convolution filters:
//const mat3 Filter_FC[9] = mat3[](
//    1.0 / ( 2.0 * sqrt(2.0) ) * mat3( 1.0, sqrt(2.0), 1.0, 0.0, 0.0, 0.0, -1.0, -sqrt(2.0), -1.0 ),
//    1.0 / ( 2.0 * sqrt(2.0) ) * mat3( 1.0, 0.0, -1.0, sqrt(2.0), 0.0, -sqrt(2.0), 1.0, 0.0, -1.0 ),
//    1.0 / ( 2.0 * sqrt(2.0) ) * mat3( 0.0, -1.0, sqrt(2.0), 1.0, 0.0, -1.0, -sqrt(2.0), 1.0, 0.0 ),
//    1.0 / ( 2.0 * sqrt(2.0) ) * mat3( sqrt(2.0), -1.0, 0.0, -1.0, 0.0, 1.0, 0.0, 1.0, -sqrt(2.0) ),
//    1.0 / 2.0 * mat3( 0.0, 1.0, 0.0, -1.0, 0.0, -1.0, 0.0, 1.0, 0.0 ),
//    1.0 / 2.0 * mat3( -1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, -1.0 ),
//    1.0 / 6.0 * mat3( 1.0, -2.0, 1.0, -2.0, 4.0, -2.0, 1.0, -2.0, 1.0 ),
//    1.0 / 6.0 * mat3( -2.0, 1.0, -2.0, 1.0, 4.0, 1.0, -2.0, 1.0, -2.0 ),
//    1.0 / 3.0 * mat3( 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 )
//);


float smoothThreshold( float value, vec2 thresholds )
{
    return smoothstep( thresholds[0] - 0.01, thresholds[0], value ) -
           smoothstep( thresholds[1], thresholds[1] + 0.01, value );
}

float hardThreshold( float value, vec2 thresholds )
{
    return float( thresholds[0] <= value && value <= thresholds[1] );
}

bool isInsideTexture( vec3 a )
{
    return ( all( greaterThanEqual( a, MIN_IMAGE_TEXCOORD ) ) &&
             all( lessThanEqual( a, MAX_IMAGE_TEXCOORD ) ) );
}



//! Tricubic interpolated texture lookup, using unnormalized coordinates.
//! Fast implementation, using 8 trilinear lookups.
//! @param[in] tex  3D texture
//! @param[in] coord  normalized 3D texture coordinate
//! @see https://github.com/DannyRuijters/CubicInterpolationCUDA/blob/master/examples/glCubicRayCast/tricubic.shader
float interpolateTricubicFast( sampler3D tex, vec3 coord )
{
	// Shift the coordinate from [0,1] to [-0.5, nrOfVoxels-0.5]
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

	// Fetch the eight linear interpolations
	// weighting and fetching is interleaved for performance and stability reasons
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


// Compute alpha of fragments based on whether or not they are inside the
// segmentation boundary. Fragments on the boundary are assigned alpha of 1,
// whereas fragments inside are assigned alpha of 'u_segInteriorOpacity'.
float getSegInteriorAlpha( uint seg )
{
    // Look up texture values in 8 neighbors surrounding the center fragment.
    // These may be either neighboring image voxels or neighboring view pixels.
    // The center fragment has index i = 4 (row = 0, col = 0).
    for ( int i = 0; i <= 8; ++i )
    {
        float row = float( mod( i, 3 ) - 1 ); // runs -1 to 1
        float col = float( floor( float(i / 3) ) - 1 ); // runs -1 to 1

        vec3 texSamplingPos = row * u_texSamplingDirsForSegOutline[0] +
                              col * u_texSamplingDirsForSegOutline[1];

        // Segmentation value of neighbor at (row, col) offset
        uint nseg = texture( u_segTex, fs_in.v_segTexCoords + texSamplingPos )[0];

        // Fragment (with segmentation 'seg') is on the boundary (and hence gets
        // full alpha) if its value is not equal to one of its neighbors.
        if ( seg != nseg )
        {
            return 1.0;
        }
    }

    return u_segInteriorOpacity;
}

float getImageValue( vec3 texCoord )
{
    return clamp( texture( u_imgTex, texCoord )[0], u_imgMinMax[0], u_imgMinMax[1] );
    // interpolateTricubicFast( u_imgTex, texCoord )
}

int when_lt( int x, int y )
{
    return max( sign(y - x), 0 );
}

int when_ge( int x, int y )
{
    return ( 1 - when_lt(x, y) );
}

vec4 computeLabelColor( int label )
{
    label -= label * when_ge( label, textureSize(u_segLabelCmapTex) );
    vec4 color = texelFetch( u_segLabelCmapTex, label );
    return color.a * color;
}

void main()
{
    bvec2 Q = bvec2( fs_in.v_clipPos.x <= u_clipCrosshairs.x, fs_in.v_clipPos.y > u_clipCrosshairs.y );

    float flashlightDist = sqrt( pow( u_aspectRatio * ( fs_in.v_clipPos.x - u_clipCrosshairs.x ), 2.0 ) +
                                 pow( fs_in.v_clipPos.y - u_clipCrosshairs.y, 2.0 ) );

    bool doRender = ( 0 == u_renderMode );

    doRender = doRender || ( ( 1 == u_renderMode ) &&
        ( u_showFix == bool( mod( floor( fs_in.v_checkerCoord.x ) + floor( fs_in.v_checkerCoord.y ), 2.0 ) > 0.5 ) ) );

    doRender = doRender || ( ( 2 == u_renderMode ) &&
        ( u_showFix == ( ( ! u_quadrants.x || Q.x ) == ( ! u_quadrants.y || Q.y ) ) ) );

    doRender = doRender || ( ( 3 == u_renderMode ) &&
        ( ( u_showFix == ( flashlightDist > u_flashlightRadius ) ) || ( u_flashlightOverlays && u_showFix ) ) );

    if ( ! doRender ) discard;

    // Foreground masks, based on whether texture coordinates are in range [0.0, 1.0]^3:
    bool imgMask = isInsideTexture( fs_in.v_imgTexCoords );
    bool segMask = isInsideTexture( fs_in.v_segTexCoords );

    // Image values in a 3x3 neighborhood:
    mat3 V;

    for ( int j = 0; j <= 2; ++j )
    {
        for ( int i = 0; i <= 2; ++i )
        {
            vec3 texSamplingPos = float(i - 1) * u_texSamplingDirsForEdges[0] +
                float(j - 1) * u_texSamplingDirsForEdges[1];

            //float v = texture( u_imgTex, fs_in.v_imgTexCoords + texSamplingPos ).r;
            float v = getImageValue( fs_in.v_imgTexCoords + texSamplingPos );

            // Apply maximum window/level to normalize value in [0.0, 1.0]:
            V[i][j] = u_imgSlopeInterceptLargest[0] * v + u_imgSlopeInterceptLargest[1];
        }
    }

    // Convolutions for all masks:
    float C_Sobel[2];
    for ( int i = 0; i <= 1; ++i )
    {
        C_Sobel[i] = dot( Filter_Sobel[i][0], V[0] ) + dot( Filter_Sobel[i][1], V[1] ) + dot( Filter_Sobel[i][2], V[2] );
        C_Sobel[i] *= C_Sobel[i];
    }

    // Gradient magnitude:
    float gradMag_Sobel = SobelFactor * sqrt( C_Sobel[0] + C_Sobel[1] ) / max( u_edgeMagnitude, 0.01 );

//    float C_FC[9];
//    for ( int i = 0; i <= 8; ++i )
//    {
//        C_FC[i] = dot( Filter_FC[i][0], V[0] ) + dot( Filter_FC[i][1], V[1] ) + dot( Filter_FC[i][2], V[2] );
//        C_FC[i] *= C_FC[i];
//    }

//    float M_FC = ( C_FC[0] + C_FC[1] ) + ( C_FC[2] + C_FC[3] );
//    float S_FC = ( C_FC[4] + C_FC[5] ) + ( C_FC[6] + C_FC[7] ) + ( C_FC[8] + M_FC );
//    float gradMag_FC = sqrt( M_FC / S_FC ) / max( u_edgeMagnitude, 0.01 );

    // Choose Sobel or Frei-Chen:
//    float gradMag = mix( gradMag_Sobel, gradMag_FC, float(useFreiChen) );
    float gradMag = gradMag_Sobel;

    // If u_thresholdEdges is true, then threshold gradMag against u_edgeMagnitude:
    gradMag = mix( gradMag, float( gradMag > u_edgeMagnitude ), float(u_thresholdEdges) );

    // Get the image and segmentation label values:
    // float img = texture( u_imgTex, fs_in.v_imgTexCoords ).r;

    float img = getImageValue( fs_in.v_imgTexCoords );

    uint seg = texture( u_segTex, fs_in.v_segTexCoords ).r;

    // Apply window/level and normalize value in [0.0, 1.0]:
    float imgNorm = clamp( u_imgSlopeIntercept[0] * img + u_imgSlopeIntercept[1], 0.0, 1.0 );

    // Mask that accounts for the image boundaries and (if masking is true) the segmentation mask:
    float mask = float( imgMask && ( u_masking && ( seg > 0u ) || ! u_masking ) );

    // Alpha accounts for opacity, masking, and thresholding:
    // Alpha will be applied to the image and edge layers.
    float alpha = u_imgOpacity * mask * hardThreshold( img, u_imgThresholds );

    // Apply color map to the image intensity:
    // Disable the image color if u_overlayEdges is false.

    // Quantize the color map.
    float cmapCoord = mix( floor( float(u_imgCmapQuantLevels) * imgNorm) / float(u_imgCmapQuantLevels - 1), imgNorm, float( 0 == u_imgCmapQuantLevels ) );
    cmapCoord = u_imgCmapSlopeIntercept[0] * cmapCoord + u_imgCmapSlopeIntercept[1];


//    // Look up image color (RGBA):
    vec4 imgColor = texture( u_imgCmapTex, cmapCoord );

//    // Convert RGBA to HSV and apply HSV modification factors:
    vec3 imgColorHsv = rgb2hsv( imgColor.rgb );

    imgColorHsv.r += u_imgCmapHsvModFactors.r;
    imgColorHsv.g *= u_imgCmapHsvModFactors.g;

//    // Convert back to RGB
    imgColor.rgb = hsv2rgb(imgColorHsv);

    vec4 imageLayer = alpha * float(u_overlayEdges) * imgColor.a * vec4(imgColor.rgb, 1.0);
//    vec4 imageLayer = alpha * float(u_overlayEdges) * texture( u_imgCmapTex, cmapCoord );

    // Apply color map to gradient magnitude:
    vec4 gradColormap = texture( u_imgCmapTex, u_imgCmapSlopeIntercept[0] * gradMag + u_imgCmapSlopeIntercept[1] );

    // For the edge layer, use either the solid edge color or the colormapped gradient magnitude:
    vec4 edgeLayer = alpha * mix( gradMag * u_edgeColor, gradColormap, float(u_colormapEdges) );

    // Look up label colors:
    vec4 segColor = computeLabelColor( int(seg) ) * getSegInteriorAlpha( seg ) * u_segOpacity * float(segMask);


    // Blend colors:
    o_color = vec4( 0.0, 0.0, 0.0, fs_in.v_segVoxCoords.x * 0.000000001 );
    o_color = imageLayer + ( 1.0 - imageLayer.a ) * o_color;
    o_color = edgeLayer + ( 1.0 - edgeLayer.a ) * o_color;
    o_color = segColor + ( 1.0 - segColor.a ) * o_color;
}
