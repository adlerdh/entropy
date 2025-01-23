#version 330 core

#define NISO 16
#define NUM_BISECTIONS 5
#define MAX_HITS 100
#define MAX_JUMPS 1000

// 3D texture coordinates (s,t,p) are in [0.0, 1.0]^3
#define MIN_IMAGE_TEXCOORD vec3( 0.0 )
#define MAX_IMAGE_TEXCOORD vec3( 1.0 )

out vec4 FragColor;

uniform sampler3D u_imgTex; // Texture unit 0: image
uniform usampler3D u_segTex; // Texture unit 1: segmentation
uniform usampler3D u_jumpTex; // Texture unit 5: distance texture

// uniform bool u_useTricubicInterpolation; // Whether to use tricubic interpolation

uniform mat4 u_imgTexture_T_world;
uniform mat4 world_T_imgTexture;

uniform mat4 clip_T_world;

uniform vec3 worldEyePos;
uniform mat3 texGrads;

uniform float samplingFactor;

uniform float u_isoValues[NISO];
uniform float u_isoOpacities[NISO];
uniform float isoEdges[NISO];

uniform vec3 lightAmbient[NISO];
uniform vec3 lightDiffuse[NISO];
uniform vec3 lightSpecular[NISO];
uniform float lightShininess[NISO];

uniform vec4 bgColor; // pre-multiplied by alpha

uniform bool renderFrontFaces;
uniform bool renderBackFaces;
uniform bool noHitTransparent;

// Mutually exclusive flags controlling whether the segmentation masks the isosurfaces in or out:
uniform bool segMasksIn;
uniform bool segMasksOut;


// Redeclared vertex shader outputs: now the fragment shader inputs
in VS_OUT
{
    vec3 v_worldRayDir; // Ray direction in World space (NOT normalized)
} fs_in;


float rand( vec2 v ) { return fract( sin( dot( v.xy, vec2(12.9898, 78.233) ) ) * 43758.5453 ); }

float compMax( vec3 v ) { return max( max(v.x, v.y), v.z ); }
float compMin( vec3 v ) { return min( min(v.x, v.y), v.z ); }

float when_lt( float x, float y ) { return max( sign(y - x), 0.0 ); }
float when_ge( float x, float y ) { return 1.0 - when_lt(x, y); }



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


float getImageValue( sampler3D tex, vec3 texCoord )
{
    return texture( tex, texCoord )[0];
    // interpolateTricubicFast( tex, texCoord )
}


vec2 slabs( vec3 texRayPos, vec3 texRayDir )
{
    vec3 t0 = (MIN_IMAGE_TEXCOORD - texRayPos) / texRayDir;
    vec3 t1 = (MAX_IMAGE_TEXCOORD - texRayPos) / texRayDir;
    vec3 tMin = min(t0, t1);
    vec3 tMax = max(t0, t1);
    return vec2( compMax(tMin), compMin(tMax) );
}

vec3 gradient( vec3 texPos )
{
    return normalize( vec3(
        getImageValue( u_imgTex, texPos + texGrads[0] ) - getImageValue( u_imgTex, texPos - texGrads[0] ),
        getImageValue( u_imgTex, texPos + texGrads[1] ) - getImageValue( u_imgTex, texPos - texGrads[1] ),
        getImageValue( u_imgTex, texPos + texGrads[2] ) - getImageValue( u_imgTex, texPos - texGrads[2] ) ) );
}

vec3 bisect( vec3 pos, vec3 dir, float t0, float t1, float sgn, float iso )
{
    float t = 0.5 * (t0 + t1);
    vec3 c = pos + t * dir;

    for ( int i = 0; i < NUM_BISECTIONS; ++i )
    {
        float test = sgn * ( getImageValue(u_imgTex, c) - iso );
        t1 += (t - t1) * when_ge(test, 0.0); // if (test >= 0.0) { t1 = t; }
        t0 += (t - t0) * when_lt(test, 0.0); // else { t0 = t; }
        t = 0.5 * (t0 + t1);
        c = pos + t * dir;
    }

    return c;
}

// NOTE: add point light?
vec4 shade( vec3 lightDir, vec3 viewDir, vec3 normal, int i )
{
    vec3 h = normalize(lightDir + viewDir);
    float a = mix( 1.0, 4.0, isoEdges[i] > 0.0 );
    float d = abs( dot(normal, lightDir) );
    float s = pow( abs( dot(normal, h) ), lightShininess[i] );
    float e = pow( 1.0 - abs( dot(normal, viewDir) ), isoEdges[i] );

    return u_isoOpacities[i] * e * vec4(a * lightAmbient[i] + d * lightDiffuse[i] + s * lightSpecular[i], 1.0);
}


void main()
{
    gl_FragDepth = gl_DepthRange.near;

    // Final fragment color that gets composited by ray traversal through image volume:
    vec4 color = vec4(0.0);

    // The ray direction must be re-normalized after interpolation from Vertex to Fragment stage:
    vec3 texRayDir = mat3(u_imgTexture_T_world) * normalize(fs_in.v_worldRayDir);

    // Convert physical (mm) to texel units along the ray direction
    float texel_T_mm = length( texRayDir );
    texRayDir /= texel_T_mm; // normalize the direction

    vec3 dirSq = texRayDir * texRayDir;

    vec3 dims = vec3( textureSize(u_imgTex, 0) );
    vec3 spacing = vec3(1.0 / dims.x, 1.0 / dims.y, 1.0 / dims.z);

    // Step size computed as a samplingFactor fraction of the voxel spacing along the ray:
    float texStep = samplingFactor * min( min(
        spacing.x * sqrt(1 + (dirSq.y + dirSq.z)) / max(dirSq.x, 1.0e-6),
        spacing.y * sqrt(1 + (dirSq.z + dirSq.x)) / max(dirSq.y, 1.0e-6) ),
        spacing.z * sqrt(1 + (dirSq.x + dirSq.y)) / max(dirSq.z, 1.0e-6) );

    // Randomly purturb the ray starting positions along the ray direction:
    vec4 texEyePos = u_imgTexture_T_world * vec4(worldEyePos, 1.0);
    vec3 texStartPos = vec3(texEyePos) + 0.5 * texStep * rand( gl_FragCoord.xy ) * texRayDir;

    vec2 interx = slabs( texStartPos, texRayDir );

    if ( interx[1] <= interx[0] || interx[1] <= 0.0 )
    {
        // The ray did not intersect the bounding box:
        FragColor = float( ! noHitTransparent ) * bgColor;
        return;
    }

    float tMin = max(0.0, interx[0]);
    float tMax = interx[1];

    vec3 texPosMin = texStartPos + tMin * texRayDir;
    vec3 texPosMax = texStartPos + tMax * texRayDir;

    int hitCount = 0;
    int stepCount = 0;

//    bool FIRST = true;

    // Current World position along the ray
    vec3 texPos = texStartPos + tMin * texRayDir;
    vec3 texFirstHitPos = texPos; // Initialize first hit position to front of volume
    int firstHit = 1;

    // Save old value and position
    float oldValue = getImageValue(u_imgTex, texPos);
    vec3 oldTexPos = texPos;
    float oldT = tMin;

    for ( float t = tMin; t <= tMax; t += texStep )
    {
        float jump = 0.0;
        int numJumps = 0;

        do
        {
            t += jump;
            oldTexPos = texPos;

            texPos = texStartPos + t * texRayDir;

            // Use a 2% safety factor:
            jump = 0.98 * texel_T_mm * float( texture(u_jumpTex, texPos).r ); // = min( j1, j2 );
            ++numJumps;
        } while ( jump > texStep && t <= tMax && numJumps <= MAX_JUMPS );

//        if ( FIRST )
//        {
//            float n = float(numJumps) / float(25);
//            FragColor = vec4(n, n, n, 1.0);
//            FIRST = false;
//        }

        float value = getImageValue(u_imgTex, texPos);

        for ( int i = 0; i < NISO; ++i )
        {
            if ( u_isoOpacities[i] > 0.0 &&
                 renderFrontFaces && value >= u_isoValues[i] && oldValue < u_isoValues[i] ||
                 renderBackFaces  && value < u_isoValues[i] && oldValue >= u_isoValues[i] )
            {
                ++hitCount;
                vec3 texHitPos = bisect( texStartPos, texRayDir, oldT, t, value - u_isoValues[i], u_isoValues[i] );
                vec3 texLightDir = normalize(texStartPos - texHitPos);
                vec3 texNormal = gradient(texHitPos);

                float segMask = float(
                    ( ! segMasksIn && ! segMasksOut ) ||
                    ( segMasksIn && texture(u_segTex, texPos).r > 0u ) ||
                    ( segMasksOut && texture(u_segTex, texPos).r == 0u ) );

                // Blend new color UNDER the old color:
                color += segMask * (1.0 - color.a) * shade(texLightDir, -texRayDir, texNormal, i);

                // Record the first hit:
                texFirstHitPos = mix( texFirstHitPos, texHitPos, float(firstHit) );
                firstHit = firstHit ^ 1; // if (firstHit) { firstHit = 0; }

                // An isosurface intersection occurred, so there is no need to loop over the remaining
                // isosurfaces for intersections at this position.
                break;
            }
        }

        if ( color.a >= 0.95 || hitCount >= MAX_HITS )
        {
            break; // Early ray termination due to sufficiently high opacity
        }

        oldValue = value;
        oldT = t;
        ++stepCount;
    }

//    float normDistance = abs(oldT - tMin);
//    float fog = exp( -normDistance*normDistance );
    FragColor = color + (1.0 - color.a) * bgColor;

    vec4 clip1stHitPos = clip_T_world * world_T_imgTexture * vec4(texFirstHitPos, 1.0);
    float ndc1stHitDepth = clip1stHitPos.z / clip1stHitPos.w;
    gl_FragDepth = 0.5 * ( (gl_DepthRange.far - gl_DepthRange.near) * ndc1stHitDepth + gl_DepthRange.near + gl_DepthRange.far );
}
