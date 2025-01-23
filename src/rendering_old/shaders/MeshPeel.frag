R"(

#version 330 core

layout (location = 0) out vec4 OutMinMaxDepths;
layout (location = 1) out vec4 OutFrontColor;
layout (location = 2) out vec4 OutBackColor;
layout (location = 3) out uint OutObjectId;

in VS_OUT
{
    vec3 WorldPos;
    vec3 WorldNormal;
    vec2 TexCoords2D;
    vec3 ImageTexCoords3D;
    vec3 LabelTexCoords3D;
    vec4 Color; // pre-multiplied
} fs_in;


struct Material
{
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

struct SimpleLight
{
    vec3 position;
    vec3 direction;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform uint objectId;

uniform Material material;
uniform SimpleLight simpleLight;

uniform vec3 cameraDir;
uniform vec3 cameraPos;
uniform bool cameraIsOrthographic;

uniform float masterOpacityMultiplier = 1.0;

uniform bool autoHidingMode;
uniform bool image3DThresholdMode;
uniform bool xrayMode;
uniform float xrayPower;

#define NUM_LAYERS 5
uniform float layerOpacities[NUM_LAYERS];
uniform uint layerPermutation[NUM_LAYERS];


// texture unit 0:
uniform sampler2D depthBlenderTex; // depth blending output

// texture unit 1:
uniform sampler2D frontBlenderTex; // front blending output

// texture unit 2:
// (premultiplied colors)
uniform sampler2D tex2D;

// texture unit 3:
uniform sampler3D imageTex3D;

// texture unit 4:
uniform usampler3D labelTex3D;

// texture unit 5:
// (premultiplied colors)
uniform samplerBuffer labelColormapTexture;

// texture unit 6:
// (premultiplied colors)
uniform sampler1D imageColorMapTexture;

uniform float slope;
uniform float intercept;

uniform float cmapSlope;
uniform float cmapIntercept;

// Lower and upper thresholds on the normalized 2D image texel values
uniform vec2 image2dThresholds;

// Lower and upper thresholds on the original 3D image texel values, mapped to range [0.0, 1.0]
uniform vec2 thresholds;


#define MAX_DEPTH 1.0
#define epsilon 0.000001


int when_lt( int x, int y )
{
  return max(sign(y - x), 0);
}

int when_ge( int x, int y )
{
  return 1 - when_lt(x, y);
}

int when_gt(int x, int y) {
  return max(sign(x - y), 0);
}


vec4 composeLayers( vec4 layers[NUM_LAYERS] );
vec4 computeImageColor();
vec4 computeLabelColor();
float computeOpacity( vec3 viewDir, float image3DAlpha );
vec4 computeShadedColor();
vec4 CalcSimpleLight( SimpleLight light, vec3 normal, vec3 lightDir, vec3 viewDir, vec4 image3DColor );


float computeOpacity( vec3 viewDir, float image3DAlpha )
{
    float d = abs( dot( fs_in.WorldNormal, viewDir ) );
    float x = pow( 1.0 - d, xrayPower );

    float autoHidingFactor = mix( 1.0, d, float(autoHidingMode) );
    float xrayFactor = mix( 1.0, x, float(xrayMode) );
    float imgThreshFactor = mix( 1.0, image3DAlpha, float(image3DThresholdMode) );

    return autoHidingFactor * xrayFactor * imgThreshFactor * masterOpacityMultiplier;
}


vec4 computeShadedColor()
{
    vec4 image3DColor = computeImageColor();

    vec3 viewDir = mix( normalize( cameraPos - fs_in.WorldPos ), cameraDir, float(cameraIsOrthographic) );
    vec3 lightDir = mix( normalize( simpleLight.position - fs_in.WorldPos ), simpleLight.direction, float(cameraIsOrthographic) );
    vec4 color = CalcSimpleLight( simpleLight, fs_in.WorldNormal, lightDir, viewDir, image3DColor );

    return color * computeOpacity( viewDir, image3DColor.a );
}


// get the front blending output
void renderPeel()
{
    // This is a floor() operation:
    ivec2 pixelCoord = ivec2( gl_FragCoord.xy );

    vec4 frontColor = texelFetch( frontBlenderTex, pixelCoord, 0 );

    // Fragment depths and 1.0 - alphaMult always increase,
    // so we can use pass-through by default with MAX blending
    OutMinMaxDepths.xy = vec2( -MAX_DEPTH );

    // Front colors always increase (dest += source * alpha_mult),
    // so we can use pass-through by default with MAX blending
    OutFrontColor = frontColor;

    // Because OVER blending makes color increase or decrease,
    // we cannot pass-through by default.
    // Each pass, only one fragment can a color greater than 0
    OutBackColor = vec4( 0.0 );


    float fragDepth = gl_FragCoord.z;

    // Get the depth value from the depth blending output:
    vec2 minMaxDepths = texelFetch( depthBlenderTex, pixelCoord, 0 ).xy;
    float nearestDepth = -minMaxDepths.x;
    float farthestDepth = minMaxDepths.y;

    if ( fragDepth < nearestDepth - epsilon ||
         fragDepth > farthestDepth + epsilon )
    {
        // Skip this depth in the peeling algorithm
        return;
    }

    if ( fragDepth > nearestDepth + epsilon &&
         fragDepth < farthestDepth - epsilon )
    {
        // This fragment needs to be peeled again
        OutMinMaxDepths.xy = vec2( -fragDepth, fragDepth );
        return;
    }

    // If we made it here, this fragment is on the peeled layer from last pass
    // Therefore, we need to shade it and make sure it is not peeled any farther.

    vec4 shadedColor = computeShadedColor();


    // If the fragment depth is the nearest depth,
    // we blend the color to the second attachment:
    if ( fragDepth >= nearestDepth - epsilon &&
         fragDepth <= nearestDepth + epsilon )
    {
        OutFrontColor = frontColor + shadedColor * (1.0 - frontColor.a);
    }
    //    else if ( fragDepth >= farthestDepth - epsilon ||
    //              fragDepth <= farthestDepth + epsilon )
    else
    {
        OutBackColor = shadedColor;
    }
}


void main()
{
    renderPeel();
    OutObjectId = objectId;
}


vec4 composeLayers( vec4 layers[NUM_LAYERS] )
{
    vec4 blended = layers[ layerPermutation[0] ];

    for ( int i = 1; i < NUM_LAYERS; ++i )
    {
        vec4 front = layers[ layerPermutation[i] ];
        blended = front + blended * ( 1.0 - front.a );
    }

    return blended;
}


vec4 computeImage2dColor()
{
    vec4 color = texture( tex2D, fs_in.TexCoords2D );
    float mag = dot( color.rgb, vec3( 0.299, 0.587, 0.114 ) );

    float threshAlpha = smoothstep( image2dThresholds[0] - 0.01, image2dThresholds[0], mag ) -
                        smoothstep( image2dThresholds[1], image2dThresholds[1] + 0.01, mag );

    return threshAlpha * color;
}


vec4 computeImageColor()
{
    // Image intensity as stored in 3D texture:
    float gray = texture( imageTex3D, fs_in.ImageTexCoords3D ).r;

    // Image intensity mapped to range [0.0, 1.0]:
    float grayNorm = clamp( slope * gray + intercept, 0.0, 1.0 );

    // Foreground mask, based on whether image texture coordinates are in range [0.0, 1.0]^3:
    bool isForeground = all( greaterThan( fs_in.ImageTexCoords3D, vec3(0.0) ) ) &&
                        all( lessThan( fs_in.ImageTexCoords3D, vec3(1.0) ) );

    // Smoothed mask from lower and upper image thresholds:
    float threshAlpha = smoothstep( thresholds[0] - 0.01, thresholds[0], grayNorm ) -
                        smoothstep( thresholds[1], thresholds[1] + 0.01, grayNorm );

    // Apply color maps to gray intensity values and multiply by threshold and foreground masks:
    return texture( imageColorMapTexture, cmapSlope * grayNorm + cmapIntercept ) * threshAlpha * float(isForeground);
}


vec4 computeLabelColor()
{
    int label = int( texture( labelTex3D, fs_in.LabelTexCoords3D ).r );
    label -= label * when_ge( label, textureSize(labelColormapTexture) );
    return texelFetch( labelColormapTexture, label );
//    int dd = when_gt( int( fwidth( float(label) ) ), 0 );
//    return texelFetch( labelColormapTexture, dd * label );
}


vec4 CalcSimpleLight( SimpleLight light, vec3 normal, vec3 lightDir, vec3 viewDir, vec4 image3DColor )
{
    vec4 layers[NUM_LAYERS] = vec4[NUM_LAYERS](
        layerOpacities[0] * vec4( material.diffuse, 1.0 ),       // Material
        layerOpacities[1] * fs_in.Color,                         // Vertex
        layerOpacities[2] * computeImage2dColor(),               // Texture 2D
        layerOpacities[3] * image3DColor,                        // Image 3D
        layerOpacities[4] * computeLabelColor() );               // Parcellation 3D

    vec4 composedColor = composeLayers( layers );

    vec3 baseColor = vec3( 0.0 );

    if ( composedColor.a > 0.0 )
    {
        baseColor = composedColor.rgb / composedColor.a;
    }

    vec3 halfwayDir = normalize( lightDir + viewDir );
    float diff = abs( dot(normal, lightDir) );
    float spec = pow( abs( dot(normal, halfwayDir) ), material.shininess );

    vec3 ambient = light.ambient * baseColor;
    vec3 diffuse = diff * light.diffuse * baseColor;
    vec3 specular = spec * light.specular * material.specular;

    return vec4( ambient + diffuse + specular, 1.0 ) * composedColor.a;
}

)"
