R"(

#version 330 core

// Output RGBA color (pre-multiplied alpha)
layout (location = 0) out vec4 OutColor;

// Output object ID
layout (location = 1) out uint OutObjectId;

// Redeclared vertex shader outputs: now the fragment shader inputs
in VS_OUT
{
    vec3 WorldPos; // Vertex position in World space
    vec3 WorldNormal; // Vertex normal vector in World space
    vec2 TexCoords2D; // Vertex 2D texture coordinates
    vec3 ImageTexCoords3D; // 3D Image texture coordinates
    vec3 LabelTexCoords3D; // 3D Parcellation texture coordinates
    vec4 Color; // Vertex RGBA color (with pre-multiplied alpha)
} fs_in;


// Material properties
struct Material
{
    vec3 diffuse;
    vec3 specular;
    float shininess;
};


// Parameters of Blinn-Phong lighting model
struct SimpleLight
{
    vec3 position;
    vec3 direction;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};


uniform uint objectId; // Unique object ID

uniform Material material; // Mesh material

uniform SimpleLight simpleLight; // Lighting parameters

// Backwards camera direction: used only for orthographic views
uniform vec3 cameraDir;

// Camera position in World space
uniform vec3 cameraPos;

// Flag indicating whether the camera projection is orthographic
uniform bool cameraIsOrthographic;

// Master opacity that modulates all colors
uniform float masterOpacityMultiplier = 1.0;

// Flag for modulating opacity with dot product between view vector and plane normal
uniform bool autoHidingMode;

// Flag for enabling 3D image thresholding
uniform bool image3DThresholdMode;

// Flag for using x-ray mode
uniform bool xrayMode;

// Power of x-ray mode
uniform float xrayPower;

// The five color layers are
// 0) Material color
// 1) Vertex color
// 2) 2D Image texture
// 3) 3D Image texture
// 4) 3D Parcellation texture
#define NUM_LAYERS 5
//#define NUM_IMAGES 4

#define MIN_IMAGE_TEXCOORD vec3(0.0)
#define MAX_IMAGE_TEXCOORD vec3(1.0)

// Opacities of the color layers
uniform float layerOpacities[NUM_LAYERS];

// Opacities of the 3D image layers
//uniform float imageOpacities[NUM_IMAGES];

// Permutation of the color layers
uniform uint layerPermutation[NUM_LAYERS];

// Permutation of the 3D image layers
//uniform uint imagePermutation[NUM_IMAGES];

// Texture unit 2: The 2D image texture with pre-multiplied RGBA colors
uniform sampler2D tex2D;

// Texture unit 3: The 3D image texture with scalar values
uniform sampler3D imageTex3D;
//uniform sampler3D imageTex3D[NUM_IMAGES];

// Texture unit 4: The 3D image parcellation texture
uniform usampler3D labelTex3D;

// Texture unit 5: The label color lookup table texture with pre-multiplied RGBA colors
uniform samplerBuffer labelColormapTexture;

// Texture unit 6: The image colormap texture with RGBA pre-multiplied colors
uniform sampler1D imageColorMapTexture;
//uniform sampler1D imageColorMapTexture[NUM_IMAGES];

// Slope and intercept for remapping the 3D image
uniform float slope;
uniform float intercept;
//uniform vec2 imageSlopeIntercept[NUM_IMAGES];

// Slope and intercept for remapping the image colormap
uniform float cmapSlope;
uniform float cmapIntercept;
//uniform vec2 colormapSlopeIntercept;

// Lower and upper thresholds on the normalized 2D image texel values
uniform vec2 image2dThresholds;

// Lower and upper thresholds on the original 3D image texel values, mapped to range [0.0, 1.0]
uniform vec2 thresholds;
//uniform vec2 imageThresholds[NUM_IMAGES];


int when_lt( int x, int y )
{
    return max( sign(y - x), 0 );
}

int when_ge( int x, int y )
{
    return ( 1 - when_lt(x, y) );
}

int when_gt(int x, int y)
{
    return max( sign(x - y), 0 );
}

float smoothedThreshold( float value, float lowThreshold, float highThreshold )
{
    return smoothstep( lowThreshold - 0.01, lowThreshold, value ) -
           smoothstep( highThreshold, highThreshold + 0.01, value );
}


vec4 composeLayers( vec4 layers[NUM_LAYERS] );
vec4 computeImageColor();
vec4 computeLabelColor();
float computeOpacity( vec3 viewDir, float image3DAlpha );
vec4 computeShadedColor();
vec4 CalcSimpleLight( SimpleLight light, vec3 normal, vec3 lightDir, vec3 viewDir, vec4 image3DColor );

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


float computeOpacity( vec3 viewDir, float image3DAlpha )
{
    float d = abs( dot( fs_in.WorldNormal, viewDir ) );
    float x = pow( 1.0 - d, xrayPower );

    float autoHidingFactor = mix( 1.0, d, float(autoHidingMode) );
    float xrayFactor = mix( 1.0, x, float(xrayMode) );
    float imgThreshFactor = mix( 1.0, image3DAlpha, float(image3DThresholdMode) );

    return autoHidingFactor * xrayFactor * imgThreshFactor * masterOpacityMultiplier;
}



vec4 computeImage2dColor()
{
    vec4 color = texture( tex2D, fs_in.TexCoords2D );
    float mag = dot( color.rgb, vec3( 0.299, 0.587, 0.114 ) );

    float threshAlpha = smoothedThreshold( mag, image2dThresholds[0], image2dThresholds[1] );
    return threshAlpha * color;
}


vec4 computeImageColor()
{
    // This function will have to loop over all NUM_IMAGES images:

    // Image intensity as stored in 3D texture:
    float gray = texture( imageTex3D, fs_in.ImageTexCoords3D ).r;

    // Image intensity mapped to range [0.0, 1.0]:
    float grayNorm = clamp( slope * gray + intercept, 0.0, 1.0 );

    // Foreground mask, based on whether image texture coordinates are in range [0.0, 1.0]^3:
    bool isForeground = all( greaterThan( fs_in.ImageTexCoords3D, MIN_IMAGE_TEXCOORD ) ) &&
                        all( lessThan( fs_in.ImageTexCoords3D, MAX_IMAGE_TEXCOORD ) );

    // Smoothed mask from lower and upper image thresholds:
    float threshAlpha = smoothedThreshold( grayNorm, thresholds[0], thresholds[1] );

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


vec4 computeShadedColor()
{
    vec4 image3DColor = computeImageColor();

    vec3 viewDir = mix( normalize( cameraPos - fs_in.WorldPos ), cameraDir, float(cameraIsOrthographic) );
    vec3 lightDir = mix( normalize( simpleLight.position - fs_in.WorldPos ), simpleLight.direction, float(cameraIsOrthographic) );
    vec4 color = CalcSimpleLight( simpleLight, fs_in.WorldNormal, lightDir, viewDir, image3DColor );

    return color * computeOpacity( viewDir, image3DColor.a );
}


void render()
{
    OutColor = computeShadedColor();
    OutObjectId = objectId;
}

void main()
{
    render();
}




//struct DirLight
//{
//    vec3 direction;

////    Material colors;
//    vec3 ambient;
//    vec3 diffuse;
//    vec3 specular;
//};

//struct PointLight
//{
//    vec3 position;

//    float constant;
//    float linear;
//    float quadratic;

//    vec3 ambient;
//    vec3 diffuse;
//    vec3 specular;
//};

//struct SpotLight
//{
//    vec3 position;
//    vec3 direction;
//    float cutOff;
//    float outerCutOff;

//    float constant;
//    float linear;
//    float quadratic;

//    vec3 ambient;
//    vec3 diffuse;
//    vec3 specular;
//};


//#define MAX_NR_POINT_LIGHTS 4


//uniform int nrPointLights;
//uniform DirLight dirLight;
//uniform SpotLight spotLight;
//uniform PointLight pointLights[MAX_NR_POINT_LIGHTS];

//vec3 CalcLight( SimpleLight light, vec3 normal, vec3 lightDir, vec3 viewDir );
//vec3 CalcDirLight( DirLight light, vec3 normal, vec3 viewDir );
//vec3 CalcPointLight( PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir );
//vec3 CalcSpotLight( SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir );


//vec3 CalcLight( SimpleLight light, vec3 normal, vec3 lightDir, vec3 viewDir )
//{
//    vec4 tex = texture( tex2D, fs_in.TexCoords2D );

//    float diff = max( dot(normal, lightDir), 0.0 );

//    vec3 halfwayDir = normalize( lightDir + viewDir );
//    float spec = pow( max( dot(normal, halfwayDir), 0.0 ), material.shininess );

//    vec3 ambient = light.ambient * mix( vec3(material.ambient), tex.rgb, textureContrib );
//    vec3 diffuse = float(useLighting) * light.diffuse * diff * mix( vec3(material.diffuse), tex.rgb, textureContrib );
//    vec3 specular = float(useLighting) * light.specular * spec * vec3(material.specular);

//    return (ambient + diffuse + specular);
//}


//// Calculates the color when using a directional light.
//vec3 CalcDirLight( DirLight light, vec3 normal, vec3 viewDir )
//{
//    vec4 tex = texture( tex2D, fs_in.TexCoords2D );

//    vec3 lightDir = normalize( -light.direction );

//    float diff = max( dot(normal, lightDir), 0.0 );

//    vec3 reflectDir = reflect( -lightDir, normal );
//    float spec = pow( max( dot(viewDir, reflectDir), 0.0 ), material.shininess );

//    vec3 ambient = light.ambient * mix( vec3(material.ambient), tex.rgb, textureContrib );
//    vec3 diffuse = float(useLighting) * light.diffuse * diff * mix( vec3(material.diffuse), tex.rgb, textureContrib );
//    vec3 specular = float(useLighting) * light.specular * spec * vec3(material.specular);

//    return (ambient + diffuse + specular);
//}


//// Calculates the color when using a point light.
//vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
//{
//    vec3 lightDir = normalize(light.position - fragPos);

//    // Diffuse shading
//    float diff = max(dot(normal, lightDir), 0.0);

//    // Specular shading
//    vec3 reflectDir = reflect(-lightDir, normal);
//    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

//    // Attenuation
//    float distance = length(light.position - fragPos);
//    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

//    // Combine results
//    vec3 ambient = light.ambient * vec3(material.ambient);
//    vec3 diffuse = light.diffuse * diff * vec3(material.diffuse);
//    vec3 specular = light.specular * spec * vec3(material.specular);

//    ambient *= attenuation;
//    diffuse *= attenuation;
//    specular *= attenuation;

//    return (ambient + diffuse + specular);
//}


//// Calculates the color when using a spot light.
//vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
//{
//    vec3 lightDir = normalize(light.position - fragPos);

//    // Diffuse shading
//    float diff = max(dot(normal, lightDir), 0.0);

//    // Specular shading
//    vec3 reflectDir = reflect(-lightDir, normal);
//    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

//    // Attenuation
//    float distance = length(light.position - fragPos);
//    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

//    // Spotlight intensity
//    float theta = dot(lightDir, normalize(-light.direction));
//    float epsilon = light.cutOff - light.outerCutOff;
//    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

//    // Combine results
//    vec3 ambient = light.ambient * vec3(material.ambient);
//    vec3 diffuse = light.diffuse * diff * vec3(material.diffuse);
//    vec3 specular = light.specular * spec * vec3(material.specular);

//    ambient *= attenuation * intensity;
//    diffuse *= attenuation * intensity;
//    specular *= attenuation * intensity;

//    return (ambient + diffuse + specular);
//}

//    color += CalcDirLight(dirLight, fs_in.WorldNormal, cameraPos );

//    for(int i = 0; i < NR_POINT_LIGHTS; i++)
//        color += CalcPointLight(pointLights[i], fs_in.WorldNormal, FragPos, cameraPos );

//    color += CalcSpotLight( spotLight, fs_in.WorldNormal, FragPos, cameraPos );




//int when_eq(int x, int y) {
//  return 1 - abs(sign(x - y));
//}

//int when_neq(int x, int y) {
//  return abs(sign(x - y));
//}

//int when_gt(int x, int y) {
//  return max(sign(x - y), 0);
//}

//int when_lt(int x, int y) {
//  return max(sign(y - x), 0);
//}

//int when_le(int x, int y) {
//  return 1 - when_gt(x, y);
//}

//int when_ge(int x, int y) {
//  return 1 - when_lt(x, y);
//}

//int and(int a, int b) {
//  return a * b;
//}

//int or(int a, int b) {
//  return min(a + b, 1);
//}

//int xor(int a, int b) {
//  return (a + b) % 2;
//}

//int not(int a) {
//  return 1 - a;
//}


///// @see https://stackoverflow.com/questions/5802351/what-happens-when-you-divide-by-0-in-a-shader
//float invert_value_ifless( float value )
//{
//    float sign_value = sign( value );
//    float sign_value_squared = sign_value * sign_value;
//    return sign_value_squared / ( value + sign_value_squared - 1.0 );
//}


)"
