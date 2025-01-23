#version 330 core

// 3D texture coordinates (s,t,p) are in [0.0, 1.0]^3
#define MIN_IMAGE_TEXCOORD vec3( 0.0 )
#define MAX_IMAGE_TEXCOORD vec3( 1.0 )

// Rendering modes:
#define IMAGE_RENDER_MODE      0
#define CHECKER_RENDER_MODE    1
#define QUADRANTS_RENDER_MODE  2
#define FLASHLIGHT_RENDER_MODE 3

// Intensity Projection modes:
#define NO_IP_MODE   0
#define MAX_IP_MODE  1
#define MEAN_IP_MODE 2
#define MIN_IP_MODE  3

// Maximum number of isosurfaces:
#define NISO 16

// Redeclared vertex shader outputs, which are now the fragment shader inputs
in VS_OUT
{
    vec3 v_imgTexCoords;
    vec3 v_segTexCoords;
    vec3 v_segVoxCoords;
    vec2 v_checkerCoord;
    vec2 v_clipPos;
} fs_in;

layout (location = 0) out vec4 o_color; // Output RGBA color (pre-multiplied alpha)

uniform sampler3D u_imgTex; // Texture unit 0: image (scalar)
uniform usampler3D u_segTex; // Texture unit 1: segmentation (scalar)
uniform sampler1D u_imgCmapTex; // Texture unit 2: image color map (non-pre-multiplied RGBA)
uniform samplerBuffer u_segLabelCmapTex; // Texutre unit 3: label color map (non-pre-multiplied RGBA)

// Slope and intercept for mapping texture intensity to normalized intensity,
// while also accounting for window/leveling
uniform vec2 u_imgSlopeIntercept;

uniform vec2 u_imgCmapSlopeIntercept; // Slopes and intercepts for the image color maps
uniform int u_imgCmapQuantLevels; // Number of image color map quantization levels
uniform vec3 u_imgCmapHsvModFactors; // HSV modification factors for image color

uniform vec2 u_imgMinMax; // Min and max image values (in texture intenstiy units)
uniform vec2 u_imgThresholds; // Image lower and upper thresholds (in texture intensity units)

uniform float u_imgOpacity; // Image opacity
uniform float u_segOpacity; // Segmentation opacity

uniform bool u_masking; // Whether to mask image based on segmentation being non-zero

uniform vec2 u_clipCrosshairs; // Crosshairs in Clip space

// Should quadrants comparison mode be done along the x, y directions?
// If x is true, then compare along x; if y is true, then compare along y.
// If both are true, then compare along both.
uniform bvec2 u_quadrants;

// Should the fixed image be rendered (true) or the moving image (false).
uniform bool u_showFix;

// Render mode (0: normal, 1: checkerboard, 2: u_quadrants, 3: flashlight)
uniform int u_renderMode;

uniform float u_aspectRatio; // View aspect ratio (width / height)

uniform float u_flashlightRadius;

// When true, the flashlight overlays the moving image on top of fixed image.
// When false, the flashlight replaces the fixed image with the moving image.
uniform bool u_flashlightOverlays;

// Intensity Projection (MIP) mode (0: none, 1: Max, 2: Mean, 3: Min, 4: X-ray)
uniform int u_mipMode;

uniform float u_isoValues[NISO]; // Isosurface values
uniform float u_isoOpacities[NISO]; // Isosurface opacities
uniform vec3 u_isoColors[NISO]; // Isosurface colors
uniform float u_isoWidth; // Width of isosurface


// OPTIONS:
// 1) Image interpolation: linear, cubic (per image setting)
// 2) Image projection: none, enabled (per image setting)
// 3) Segmentation interpolation: nn, linear (global setting)
// 4) Outlining: solid, outline (global setting)

const uvec3 neigh[8] = uvec3[8](
    uvec3(0, 0, 0), uvec3(0, 0, 1), uvec3(0, 1, 0), uvec3(0, 1, 1),
    uvec3(1, 0, 0), uvec3(1, 0, 1), uvec3(1, 1, 0), uvec3(1, 1, 1) );


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

int when_lt( int x, int y )
{
    return max( sign(y - x), 0 );
}

int when_ge( int x, int y )
{
    return ( 1 - when_lt(x, y) );
}

float hardThreshold( float value, vec2 thresholds )
{
    return float( thresholds[0] <= value && value <= thresholds[1] );
}

// Remapping the unit interval into the unit interval by expanding the
// sides and compressing the center, and keeping 1/2 mapped to 1/2,
// that can be done with the gain() function. This was a common function
// in RSL tutorials (the Renderman Shading Language). k=1 is the identity curve,
// k<1 produces the classic gain() shape, and k>1 produces "s" shaped curces.
// The curves are symmetric (and inverse) for k=a and k=1/a.

//float gain(float x, float k)
//{
//    float a = 0.5*pow(2.0*((x<0.5)?x:1.0-x), k);
//    return (x<0.5)?a:1.0-a;
//}

float cubicPulse( float center, float width, float x )
{
    x = abs( x - center );
    if ( x > width ) return 0.0;
    x /= width;
    return 1.0 - x * x * ( 3.0 - 2.0 * x );
}

// Check if inside texture coordinates
bool isInsideTexture( vec3 a )
{
    return ( all( greaterThanEqual( a, MIN_IMAGE_TEXCOORD ) ) &&
             all( lessThanEqual( a, MAX_IMAGE_TEXCOORD ) ) );
}


//! Tricubic interpolated texture lookup
//! Fast implementation, using 8 trilinear lookups.
//! @param[in] tex 3D texture sampler
//! @param[in] coord Normalized 3D texture coordinate
//! @see https://github.com/DannyRuijters/CubicInterpolationCUDA/blob/master/examples/glCubicRayCast/tricubic.shader
float interpolateTricubicFast( sampler3D tex, vec3 coord )
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
float getImageValue( sampler3D tex, vec3 texCoords, float minVal, float maxVal )
{
   return clamp( texture( tex, texCoords )[0], minVal, maxVal );
}

/// Cubic interpolation:
//float getImageValue( sampler3D tex, vec3 texCoords, float minVal, float maxVal )
//{
//    return clamp( interpolateTricubicFast(tex, texCoords), minVal, maxVal );
//}


/// No projection:
// float computeProjection( float img )
// {
//     return img;
// }

/// MIP projection:

// Half the number of samples for MIP. Set to 0 when no projection is used.
uniform int u_halfNumMipSamples;

// Z view camera direction, represented in texture sampling space
uniform vec3 u_texSamplingDirZ;

float computeProjection( float img )
{
    // Number of samples used for computing the final image value:
    int numSamples = 1;

    // Accumulate intensity projection in forwards (+Z) and backwards (-Z) directions:
    for ( int dir = -1; dir <= 1; dir += 2 )
    {
        for ( int i = 1; i <= u_halfNumMipSamples; ++i )
        {
            vec3 c = fs_in.v_imgTexCoords + dir * i * u_texSamplingDirZ;

            if ( ! isInsideTexture( c ) ) break;

            float a = getImageValue( u_imgTex, c, u_imgMinMax[0], u_imgMinMax[1] );

            img = float( MAX_IP_MODE == u_mipMode ) * max( img, a ) +
                  float( MEAN_IP_MODE == u_mipMode ) * ( img + a ) +
                  float( MIN_IP_MODE == u_mipMode ) * min( img, a );

            ++numSamples;
        }
    }

    // If using Mean Intensity Projection mode, then normalize by the number of samples:
    return img / mix( 1.0, float( numSamples ), float( MEAN_IP_MODE == u_mipMode ) );
}

vec4 computeLabelColor( int label )
{
    // Labels greater than the size of the segmentation labelc color texture are mapped to 0
    label -= label * when_ge( label, textureSize(u_segLabelCmapTex) );

    vec4 color = texelFetch( u_segLabelCmapTex, label );
    return color.a * color; // pre-multiply by alpha
}


/// Look up segmentation texture label value

/// Default nearest-neighbor lookup:
uint getSegValue( vec3 texOffset, out float opacity )
{
    opacity = 1.0;
    return texture( u_segTex, fs_in.v_segTexCoords + texOffset )[0];
}


/// Linear lookup:
//uniform vec3 u_texSamplingDirsForSmoothSeg[2];

//// Interpolation cut-off for segmentation (in [0, 1])
//uniform float u_segInterpCutoff;

//uint getSegValue( vec3 texOffset, out float opacity )
//{
//    uint seg = 0u;
//    opacity = 0.0;

//    vec3 c = floor( fs_in.v_segVoxCoords );
//    vec3 d = pow( vec3( textureSize(u_segTex, 0) ), vec3(-1) );
//    vec3 t = vec3(c.x * d.x, c.y * d.y, c.z * d.z) + 0.5 * d;

//    uint s[8];
//    for ( int i = 0; i < 8; ++i )
//    {
//        s[i] = texture( u_segTex, t + neigh[i] * d + texOffset )[0];
//    }

//    vec3 b = fs_in.v_segVoxCoords + texOffset * vec3( textureSize(u_segTex, 0) ) - c;

//    vec3 g[2] = vec3[2]( vec3(1) - b, b );

//    // float segEdgeWidth = 0.02;


//    uint neighSegs[9];

//    // Look up texture values in the fragment and its 8 neighbors.
//    // The center fragment (row = 0, col = 0) has index i = 4.
//    for ( int i = 0; i <= 8; ++i )
//    {
//        int j = int( mod( i + 4, 9 ) ); // j = [4,5,6,7,8,0,1,2,3]

//        float row = float( mod( j, 3 ) - 1 ); // [-1,0,1]
//        float col = float( floor( float(j / 3) ) - 1 ); // [-1,0,1]

//        vec3 texPos = row * u_texSamplingDirsForSmoothSeg[0] +
//                      col * u_texSamplingDirsForSmoothSeg[1];

//        // Segmentation value of neighbor at (row, col) offset
//        neighSegs[i] = texture( u_segTex, fs_in.v_segTexCoords + texPos )[0];
//    }


//    float maxInterp = 0.0;

//    for ( int i = 0; i <= 8; ++i )
//    {
//        uint label = neighSegs[i];

//        float interp = 0.0;
//        for ( int j = 0; j <= 7; ++j )
//        {
//            interp += float(s[j] == label) *
//                g[neigh[j].x].x * g[neigh[j].y].y * g[neigh[j].z].z;
//        }

//        // This feathers the edges:
//        // opacity = smoothstep(
//        //     clamp( u_segInterpCutoff - segEdgeWidth/2.0, 0.0, 1.0 ),
//        //     clamp( u_segInterpCutoff + segEdgeWidth/2.0, 0.0, 1.0 ), interp );

//        opacity = 1.0;
//        // opacity = cubicPulse( u_segInterpCutoff, segEdgeWidth, interp );

//        if ( interp > maxInterp &&
//             interp >= u_segInterpCutoff &&
//             computeLabelColor( int(label) ).a > 0.0 )
//        {
//            seg = label;
//            maxInterp = interp;

//            if ( u_segInterpCutoff >= 0.5 )
//            {
//                break;
//            }
//        }
//    }

//    return seg;
//}



/// Compute alpha of fragments based on whether or not they are inside the
/// segmentation boundary. Fragments on the boundary are assigned alpha of 1,
/// whereas fragments inside are assigned alpha of 'u_segInteriorOpacity'.

/// Nearest neighbor sampling:
//float getSegInteriorAlpha( uint seg )
//{
//    return 1.0;
//}


/// Linear sampling:

// Texture sampling directions (horizontal and vertical) for calculating the segmentation outline
uniform vec3 u_texSamplingDirsForSegOutline[2];

// Opacity of the interior of the segmentation
uniform float u_segInteriorOpacity;

float getSegInteriorAlpha( uint seg )
{
    // Look up texture values in 8 neighbors surrounding the center fragment.
    // These may be either neighboring image voxels or neighboring view pixels.
    // The center fragment (row = 0, col = 0) has index i = 4.
    for ( int i = 0; i <= 8; ++i )
    {
        float row = float( mod( i, 3 ) - 1 ); // [-1,0,1]
        float col = float( floor( float(i / 3) ) - 1 ); // [-1,0,1]

        vec3 texPosOffset = row * u_texSamplingDirsForSegOutline[0] +
            col * u_texSamplingDirsForSegOutline[1];

        // Segmentation value of neighbor at (row, col) offset:
        float ignore;
        if ( seg != getSegValue( texPosOffset, ignore ) )
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
    bvec2 Q = bvec2( fs_in.v_clipPos.x <= u_clipCrosshairs.x,
                     fs_in.v_clipPos.y > u_clipCrosshairs.y );

    // Distance of the fragment from the crosshairs, accounting for aspect ratio:
    float flashlightDist = sqrt(
        pow( u_aspectRatio * ( fs_in.v_clipPos.x - u_clipCrosshairs.x ), 2.0 ) +
        pow( fs_in.v_clipPos.y - u_clipCrosshairs.y, 2.0 ) );

    // Flag indicating whether the fragment will be rendered:
    bool render = ( IMAGE_RENDER_MODE == u_renderMode );

    // If in Checkerboard mode, then render the fragment?
    render = render || ( ( CHECKER_RENDER_MODE == u_renderMode ) &&
        ( u_showFix == bool( mod( floor( fs_in.v_checkerCoord.x ) +
                                floor( fs_in.v_checkerCoord.y ), 2.0 ) > 0.5 ) ) );

    // If in Quadrants mode, then render the fragment?
    render = render || ( ( QUADRANTS_RENDER_MODE == u_renderMode ) &&
        ( u_showFix == ( ( ! u_quadrants.x || Q.x ) == ( ! u_quadrants.y || Q.y ) ) ) );

    // If in Flashlight mode, then render the fragment?
    render = render || ( ( FLASHLIGHT_RENDER_MODE == u_renderMode ) &&
        ( ( u_showFix == ( flashlightDist > u_flashlightRadius ) ) ||
            ( u_flashlightOverlays && u_showFix ) ) );

    return render;
}


void main()
{
    if ( ! doRender() ) discard;

    float img = getImageValue( u_imgTex, fs_in.v_imgTexCoords, u_imgMinMax[0], u_imgMinMax[1] );
    img = computeProjection( img );

    float segInterpOpacity = 1.0;
    uint seg = getSegValue( vec3(0, 0, 0), segInterpOpacity );

    // Apply window/level and normalize image values to [0.0, 1.0] range:
    float imgNorm = clamp( u_imgSlopeIntercept[0] * img + u_imgSlopeIntercept[1], 0.0, 1.0 );

    // Image and segmentation masks based on texture coordinates:
    bool imgMask = isInsideTexture( fs_in.v_imgTexCoords );
    bool segMask = isInsideTexture( fs_in.v_segTexCoords );

    // Compute the image mask:
    float mask = float( imgMask && ( u_masking && ( seg > 0u ) || ! u_masking ) );

    // Compute image alpha based on opacity, mask, and thresholds:
    float imgAlpha = u_imgOpacity * mask * hardThreshold( img, u_imgThresholds );

    // Compute segmentation alpha based on opacity and mask:
    float segAlpha = u_segOpacity * segInterpOpacity * getSegInteriorAlpha( seg ) * float(segMask);

    // Compute coordinate into the image color map, accounting for quantization levels:
    float cmapCoord = mix( floor( float(u_imgCmapQuantLevels) * imgNorm) / float(u_imgCmapQuantLevels - 1), imgNorm,
        float( 0 == u_imgCmapQuantLevels ) );

    // Normalize color map coordinates:
    cmapCoord = u_imgCmapSlopeIntercept[0] * cmapCoord + u_imgCmapSlopeIntercept[1];


    // Look up image color (RGBA):
    vec4 imgColor = texture( u_imgCmapTex, cmapCoord );

    // Convert RGBA to HSV and apply HSV modification factors:
    vec3 imgColorHsv = rgb2hsv( imgColor.rgb );

    imgColorHsv.x += u_imgCmapHsvModFactors.x;
    imgColorHsv.yz *= u_imgCmapHsvModFactors.yz;

    // Convert back to RGB
    imgColor.rgb = hsv2rgb( imgColorHsv );

    vec4 imgLayer = imgAlpha * imgColor.a * vec4(imgColor.rgb, 1.0);
//    vec4 imgLayer = texture( u_imgCmapTex, cmapCoord ) * imgAlpha;


    // Look up segmentation color:
    vec4 segLayer = computeLabelColor( int(seg) ) * segAlpha;

    // Isosurface layer:
    vec4 isoLayer = vec4(0.0, 0.0, 0.0, 0.0);

    for ( int i = 0; i < NISO; ++i )
    {
        vec4 color = float(imgMask) * u_isoOpacities[i] *
            cubicPulse( u_isoValues[i], u_isoWidth, img ) * vec4( u_isoColors[i], 1.0 );

        isoLayer = color + (1.0 - color.a) * isoLayer;
    }

    // Blend all layers in order (1) image, (2) isosurfaces, (3) segmentation:
    o_color = vec4(0.0, 0.0, 0.0, fs_in.v_segVoxCoords.x * 0.000000001);
    o_color = imgLayer + (1.0 - imgLayer.a) * o_color;
    o_color = isoLayer + (1.0 - isoLayer.a) * o_color;
    o_color = segLayer + (1.0 - segLayer.a) * o_color;

    //o_color.rgb = pow( o_color.rgb, vec3(1.8) );
}
