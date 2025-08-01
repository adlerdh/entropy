R"(

#version 330 core

// Output RGBA color (premultiplied alpha)
layout (location = 0) out vec4 OutColor;

// Output object ID
layout (location = 1) out uint OutObjectId;


// Redeclared vertex shader outputs: now the fragment shader inputs
in VS_OUT
{
    vec3 WorldPos; // Vertex position in World space
    vec3 WorldNormal; // Vertex normal vector in World space
    vec4 Color; // Vertex RGBA color (with premultiplied alpha)
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
uniform float masterOpacityMultiplier;

// Flag for using x-ray mode
uniform bool xrayMode;

// Power of x-ray mode
uniform float xrayPower;

// The two color layers are
// 0) Material color
// 1) Vertex color
#define NUM_LAYERS 2

// Opacities of the two color layers
uniform float layerOpacities[NUM_LAYERS];


vec4 composeLayers( vec4 layers[NUM_LAYERS] );
float computeOpacity( vec3 viewDir );
vec4 computeSimpleLight( SimpleLight light, vec3 normal, vec3 lightDir, vec3 viewDir );
vec4 computeShadedColor();


vec4 composeLayers( vec4 layers[NUM_LAYERS] )
{
    vec4 blended = layers[0];

    for ( int i = 1; i < NUM_LAYERS; ++i )
    {
        vec4 front = layers[i];
        blended = front + blended * ( 1.0 - front.a );
    }

    return blended;
}


float computeOpacity( vec3 viewDir )
{
    float d = abs( dot( fs_in.WorldNormal, viewDir ) );
    float x = pow( 1.0 - d, xrayPower );

    float xrayFactor = mix( 1.0, x, float(xrayMode) );
    return xrayFactor * masterOpacityMultiplier;
}


vec4 computeSimpleLight( SimpleLight light, vec3 normal, vec3 lightDir, vec3 viewDir )
{
    vec4 layers[NUM_LAYERS] = vec4[NUM_LAYERS](
        layerOpacities[0] * vec4( material.diffuse, 1.0 ),
        layerOpacities[1] * fs_in.Color );

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
    vec3 viewDir = mix( normalize( cameraPos - fs_in.WorldPos ), cameraDir, float(cameraIsOrthographic) );
    vec3 lightDir = mix( normalize( simpleLight.position - fs_in.WorldPos ), simpleLight.direction, float(cameraIsOrthographic) );
    vec4 color = computeSimpleLight( simpleLight, fs_in.WorldNormal, lightDir, viewDir );

    return color * computeOpacity( viewDir );
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

)"
