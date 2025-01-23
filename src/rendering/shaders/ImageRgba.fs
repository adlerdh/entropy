#version 330 core

// 3D texture coordinates (s,t,p) are in [0.0, 1.0]^3
#define MIN_IMAGE_TEXCOORD vec3( 0.0 )
#define MAX_IMAGE_TEXCOORD vec3( 1.0 )

// Rendering modes:
#define IMAGE_RENDER_MODE      0
#define CHECKER_RENDER_MODE    1
#define QUADRANTS_RENDER_MODE  2
#define FLASHLIGHT_RENDER_MODE 3

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

uniform sampler3D u_imgTex[4]; // Texture units 0, 5, 6, 7: Red, green, blue, alpha images
uniform usampler3D u_segTex; // Texture unit 1: segmentation
uniform samplerBuffer u_segLabelCmapTex; // Texutre unit 3: label color map (pre-mult RGBA)

// uniform bool u_useTricubicInterpolation; // Whether to use tricubic interpolation

// Slope and intercept for mapping texture intensity to normalized intensity, accounting for window-leveling
uniform vec2 u_imgSlopeIntercept[4];

// Should alpha be forced to 1? Set to true for 3-component images, where no alpha is provided.
uniform bool u_alphaIsOne;

uniform float u_imgOpacity[4]; // Image opacities
uniform float u_segOpacity; // Segmentation opacities

// Texture sampling directions (horizontal and vertical) for calculating the segmentation outline
uniform vec3 u_texSamplingDirsForSegOutline[2];

// Opacity of the interior of the segmentation
uniform float u_segInteriorOpacity;

uniform vec2 u_imgMinMax[4]; // Min and max image values
uniform vec2 u_imgThresholds[4]; // Image lower and upper thresholds, mapped to OpenGL texture intensity

uniform bool u_masking; // Whether to mask image based on segmentation

uniform vec2 u_clipCrosshairs; // Crosshairs in Clip space

// Should comparison be done in x,y directions?
// If x is true, then compare along x; if y is true, then compare along y;
// if both are true, then compare along both.
uniform bvec2 u_quadrants;

// Should the fixed image be rendered (true) or the moving image (false):
uniform bool u_showFix;

// Render mode (0: normal, 1: checkerboard, 2: u_quadrants, 3: flashlight)
uniform int u_renderMode;

uniform float u_aspectRatio;

uniform float u_flashlightRadius;

// When true, the flashlight overlays the moving image on top of fixed image.
// When false, the flashlight replaces the fixed image with the moving image.
uniform bool u_flashlightOverlays;


float hardThreshold( float value, vec2 thresholds )
{
    return float( thresholds[0] <= value && value <= thresholds[1] );
}

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

float getImageValue( sampler3D tex, vec3 texCoord, vec2 minMax )
{
    return clamp( texture( tex, texCoord )[0], minMax[0], minMax[1] );
    // interpolateTricubicFast( tex, texCoord )
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


// Compute alpha of fragments based on whether or not they are inside the
// segmentation boundary. Fragments on the boundary are assigned alpha of 1,
// whereas fragments inside are assigned alpha of 'u_segInteriorOpacity'.
float getSegInteriorAlpha( uint seg )
{
    float segInteriorAlpha = u_segInteriorOpacity;

    // Look up texture values in 8 neighbors surrounding the center fragment.
    // These may be either neighboring image voxels or neighboring view pixels.
    // The center fragment has index i = 4 (row = 0, col = 0).
    for ( int i = 0; i < 9; ++i )
    {
        float row = float( mod( i, 3 ) - 1 ); // runs -1 to 1
        float col = float( floor( float(i / 3) ) - 1 ); // runs -1 to 1

        vec3 texSamplingPos = row * u_texSamplingDirsForSegOutline[0] +
            col * u_texSamplingDirsForSegOutline[1];

        // Segmentation value of neighbor at (row, col) offset
        uint nseg = texture( u_segTex, fs_in.v_segTexCoords + texSamplingPos )[0];

        // Fragment (with segmentation 'seg') is on the boundary (and hence gets
        // full alpha) if its value is not equal to one of its neighbors.
        if ( nseg != seg )
        {
            segInteriorAlpha = 1.0;
            break;
        }
    }

    return segInteriorAlpha;
}


void main()
{
    if ( ! doRender() ) discard;

    // Image and segmentation masks based on texture coordinates:
    bool imgMask = isInsideTexture( fs_in.v_imgTexCoords );
    bool segMask = isInsideTexture( fs_in.v_segTexCoords );

    // Look up the image values (after mapping to GL texture units):
    // vec4 img = vec4( texture( u_imgTex[0], fs_in.v_imgTexCoords )[0],
    //                  texture( u_imgTex[1], fs_in.v_imgTexCoords )[0],
    //                  texture( u_imgTex[2], fs_in.v_imgTexCoords )[0],
    //                  texture( u_imgTex[3], fs_in.v_imgTexCoords )[0] );

    vec4 img = vec4(
        getImageValue( u_imgTex[0], fs_in.v_imgTexCoords, u_imgMinMax[0] ),
        getImageValue( u_imgTex[1], fs_in.v_imgTexCoords, u_imgMinMax[1] ),
        getImageValue( u_imgTex[2], fs_in.v_imgTexCoords, u_imgMinMax[2] ),
        getImageValue( u_imgTex[3], fs_in.v_imgTexCoords, u_imgMinMax[3] ) );

    // Look up segmentation texture label value:
    uint seg = texture( u_segTex, fs_in.v_segTexCoords )[0];

    // Compute the image mask:
    float mask = float( imgMask && ( u_masking && ( seg > 0u ) || ! u_masking ) );

    // Apply window/level to normalize image values in [0.0, 1.0] range:
    vec4 imgNorm = vec4(0.0, 0.0, 0.0, 0.0);

    // Compute image alpha based on opacity, mask, and thresholds:
    float imgAlpha[4];

    float forcedOpaque = float( true == u_alphaIsOne );

    float thresh[4];

    for ( int i = 0; i <= 3; ++i )
    {
        thresh[i] = hardThreshold( img[i], u_imgThresholds[i] );
    }

    thresh[3] = forcedOpaque + (1.0 - forcedOpaque) * thresh[3];

    for ( int i = 0; i <= 3; ++i )
    {
        imgAlpha[i] = u_imgOpacity[i] * mask * thresh[i];
        imgNorm[i] = clamp( u_imgSlopeIntercept[i][0] * img[i] + u_imgSlopeIntercept[i][1], 0.0, 1.0 );
    }

    imgNorm.a = forcedOpaque + (1.0 - forcedOpaque) * imgNorm.a;

    // Apply alpha:
    for ( int i = 0; i <= 3; ++i )
    {
        imgNorm[i] *= imgAlpha[i];
    }

    vec4 imgLayer = imgNorm.a * vec4( imgNorm.rgb, 1.0 );

    // Compute segmentation alpha based on opacity and mask:
    float segAlpha = getSegInteriorAlpha( seg ) * u_segOpacity * float(segMask);

    // Look up segmentation color and apply:
    vec4 segLayer = computeLabelColor( int(seg) ) * segAlpha;

    // Blend layers:
    o_color = vec4(0.0, 0.0, 0.0, fs_in.v_segVoxCoords.x * 0.000000001);
    o_color = imgLayer + (1.0 - imgLayer.a) * o_color;
    o_color = segLayer + (1.0 - segLayer.a) * o_color;
}
