#version 330 core

#define MIN_IMAGE_TEXCOORD vec3( 0.0 )
#define MAX_IMAGE_TEXCOORD vec3( 1.0 )

#define IMAGE_RENDER_MODE      0
#define CHECKER_RENDER_MODE    1
#define QUADRANTS_RENDER_MODE  2
#define FLASHLIGHT_RENDER_MODE 3

#define NO_IP_MODE   0
#define MAX_IP_MODE  1
#define MEAN_IP_MODE 2
#define MIN_IP_MODE  3
#define XRAY_IP_MODE 4


in VS_OUT // Redeclared vertex shader outputs: now the fragment shader inputs
{
    vec3 v_imgTexCoords[2];
    vec3 v_segTexCoords[2];
} fs_in;

layout (location = 0) out vec4 o_color; // Output RGBA color (pre-multiplied alpha)

uniform sampler3D u_imgTex[2]; // Texture units 0/1: images
uniform usampler3D u_segTex[2]; // Texture units 2/3: segmentations
uniform samplerBuffer u_segLabelCmapTex[2]; // Texutre unit 6/7: label color tables (pre-mult RGBA)

// uniform bool u_useTricubicInterpolation; // Whether to use tricubic interpolation

uniform vec2 u_imgSlopeIntercept[2]; // Slopes and intercepts for image window-leveling
uniform float u_segOpacity[2]; // Segmentation opacities

uniform sampler1D u_metricCmapTex; // Texture unit 4: metric colormap (pre-mult RGBA)
uniform vec2 u_metricCmapSlopeIntercept; // Slope and intercept for the metric colormap
uniform vec2 u_metricSlopeIntercept; // Slope and intercept for the final metric
uniform bool u_metricMasking; // Whether to mask based on segmentation

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

// Texture sampling directions (horizontal and vertical) for calculating the segmentation outline
uniform vec3 u_texSamplingDirsForSegOutline[2];

// Opacity of the interior of the segmentation
uniform float u_segInteriorOpacity;


// Check if a coordinate is inside the texture coordinate bounds.
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
float getSegInteriorAlpha( int texNum, uint seg )
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
        // uint nseg = texture( u_segTex[texNum], fs_in.v_segTexCoords[texNum] + texSamplingPos )[0];

        uint nseg;

        switch (texNum)
        {
        case 0:
        {
            nseg = texture( u_segTex[0], fs_in.v_segTexCoords[texNum] + texSamplingPos )[0];
            break;
        }
        case 1:
        {
            nseg = texture( u_segTex[1], fs_in.v_segTexCoords[texNum] + texSamplingPos )[0];
            break;
        }
        }

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


float getImageValue( sampler3D tex, vec3 texCoord )
{
    return texture( tex, texCoord )[0];
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

vec4 computeLabelColor( int label, int i )
{
//    label -= label * when_ge( label, textureSize(u_segLabelCmapTex[i]) );
//    vec4 color = texelFetch( u_segLabelCmapTex[i], label );

    vec4 color;

    switch (i)
    {
    case 0:
    {
        label -= label * when_ge( label, textureSize(u_segLabelCmapTex[0]) );
        color = texelFetch( u_segLabelCmapTex[0], label );
        break;
    }
    case 1:
    {
        label -= label * when_ge( label, textureSize(u_segLabelCmapTex[1]) );
        color = texelFetch( u_segLabelCmapTex[1], label );
        break;
    }
    }

    return color.a * color;
}

// Get the segmentation color for image i
vec4 getSegColor( int i )
{
    // Look up label value and color:
//    uint label = texture( u_segTex[i], fs_in.v_segTexCoords[i] ).r;

    uint label;

    switch (i)
    {
    case 0:
    {
        label = texture( u_segTex[0], fs_in.v_segTexCoords[i] ).r;
        break;
    }
    case 1:
    {
        label = texture( u_segTex[1], fs_in.v_segTexCoords[i] ).r;
        break;
    }
    }

    vec4 labelColor = computeLabelColor( int(label), i );
    //vec4 labelColor = texelFetch( u_segLabelCmapTex[i], int(label), 0 );

    // Modulate color with the segmentation opacity and mask:
    bool segMask = isInsideTexture( fs_in.v_segTexCoords[i] );
    return labelColor * getSegInteriorAlpha( i, label ) * u_segOpacity[i] * float(segMask);
}


// Compute the metric and mask.
vec2 computeMetricAndMask( in int sampleOffset, out bool hitBoundary )
{
    hitBoundary = false;

    vec3 img_tc[2] = vec3[2]( vec3(0.0), vec3(0.0) );
    vec3 seg_tc[2] = vec3[2]( vec3(0.0), vec3(0.0) );

    vec3 texOffset = float(sampleOffset) * u_texSamplingDirZ;

    img_tc[0] = fs_in.v_imgTexCoords[0] + texOffset;
    img_tc[1] = vec3( img1Tex_T_img0Tex * vec4( img_tc[0], 1.0 ) );

    if ( ! isInsideTexture( img_tc[0] ) || ! isInsideTexture( img_tc[1] ) )
    {
        hitBoundary = true;
        return vec2( 0.0, 0.0 );
    }

    seg_tc[0] = fs_in.v_segTexCoords[0] + texOffset;
    seg_tc[1] = vec3( img1Tex_T_img0Tex * vec4( seg_tc[0], 1.0 ) );

    float imgNorm[2];
    float metricMask = 1.0;

    // Loop over the two images:
    for ( int i = 0; i < 2; ++i )
    {
        // Look up image and label values:
        // float img = texture( u_imgTex[i], img_tc[i] ).r;
        // float img = getImageValue( u_imgTex[i], img_tc[i] );
        //  uint label = texture( u_segTex[i], seg_tc[i] ).r;

        float img;
        uint label;

        switch (i)
        {
        case 0:
        {
            img = getImageValue( u_imgTex[0], img_tc[i] );
            label = texture( u_segTex[0], seg_tc[i] ).r;
            break;
        }
        case 1:
        {
            img = getImageValue( u_imgTex[1], img_tc[i] );
            label = texture( u_segTex[1], seg_tc[i] ).r;
            break;
        }
        }

        // Normalize images to [0.0, 1.0] range:
        imgNorm[i] = clamp( u_imgSlopeIntercept[i][0] * img + u_imgSlopeIntercept[i][1], 0.0, 1.0 );

        // When u_metricMasking is true, the mask is based on the segmentation label.
        // It is the AND of the mask for both images.
        metricMask *= float( ( u_metricMasking && ( label > 0u ) || ! u_metricMasking ) );
    }

    float metric = abs( imgNorm[0] - imgNorm[1] ) * metricMask;
    metric *= mix( 1.0, metric, float(u_useSquare) );

    return vec2( metric, metricMask );
}


void main()
{
    // Flag the the boundary was hit:
    bool hitBoundary = false;

    // Compute the metric and metric mask without zero offset.
    // Computation is on the current slice.
    vec2 MM = computeMetricAndMask( 0, hitBoundary );

    float metric = MM[0];
    float metricMask = MM[1];
    int numSamples = 1;

    // Integrate projection along forwards (+Z) direction:
    for ( int i = 1; i <= u_halfNumMipSamples; ++i )
    {
        MM = computeMetricAndMask( i, hitBoundary );
        if ( hitBoundary ) break;

        // Accumulate the metric:
        metric = float( 1 == u_mipMode ) * max( metric, MM[0] ) +
                 float( 2 == u_mipMode ) * ( metric + MM[0] ) +
                 float( 3 == u_mipMode ) * min( metric, MM[0] );

        // OR the existing metric mask with the one returned:
        metricMask = max( metricMask, MM[1] );

        ++numSamples;
    }

    // Integrate projection along backwards (-Z) direction:
    for ( int i = 1; i <= u_halfNumMipSamples; ++i )
    {
        MM = computeMetricAndMask( -i, hitBoundary );
        if ( hitBoundary ) break;

        metric = float( 1 == u_mipMode ) * max( metric, MM[0] ) +
                 float( 2 == u_mipMode ) * ( metric + MM[0] ) +
                 float( 3 == u_mipMode ) * min( metric, MM[0] );

        metricMask = max( metricMask, MM[1] );
        ++numSamples;
    }


    // If using Mean Intensity Projection mode, then normalize the total metric
    // by the number of samples in order co compute the mean:
    metric /= mix( 1.0, float( numSamples ), float( 2 == u_mipMode ) );

    // Apply slope and intercept to metric:
    metric = clamp( u_metricSlopeIntercept[0] * metric + u_metricSlopeIntercept[1], 0.0, 1.0 );

    // Index into colormap:
    float cmapValue = u_metricCmapSlopeIntercept[0] * metric + u_metricCmapSlopeIntercept[1];

    // Apply colormap and masking (by pre-multiplying RGBA with alpha mask):
    vec4 metricLayer = texture( u_metricCmapTex, cmapValue ) * metricMask;

    // Get segmentation colors at the slice with zero offset.
    // Ignore the segmentation color if metric masking is enabled.
    vec4 segColor[2] = vec4[2]( getSegColor(0) * float( ! u_metricMasking ),
                                getSegColor(1) * float( ! u_metricMasking ) );

    // Blend layers:
    o_color = vec4(0.0, 0.0, 0.0, 0.0);
    o_color = segColor[0] + ( 1.0 - segColor[0].a ) * metricLayer;
    o_color = segColor[1] + ( 1.0 - segColor[1].a ) * o_color;
}
