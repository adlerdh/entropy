#version 330 core

in vec3 v_worldPosition;
in vec3 v_worldNormal;
in vec4 v_color;

uniform vec4 u_baseColor;
uniform float u_metallic;
uniform float u_roughness;
uniform float u_ambientOcclusion;
uniform int u_shadingModel;
uniform bool u_hasVertexColors;
uniform vec3 u_cameraWorldPosition;
uniform vec3 u_lightDirectionWorld;
uniform bool u_shadowMapEnabled;
uniform sampler2D u_shadowMapTex;
uniform mat4 u_lightClip_T_world;
uniform float u_shadowStrength;
uniform float u_shadowDepthBias;
uniform bool u_screenAmbientOcclusionEnabled;
uniform sampler2D u_screenAmbientOcclusionTex;
uniform int u_clipPlaneCount;
uniform vec4 u_clipPlanes[8];

layout(location = 0) out vec4 fragColor;

const int kShadingModelSimpleLit = 0;
const int kShadingModelPhysicallyBased = 1;
const float kPi = 3.14159265359;

vec3 simpleLitColor(vec3 albedo, vec3 normal, vec3 lightDirection, vec3 viewDirection)
{
  vec3 halfVector = normalize(lightDirection + viewDirection);
  float diffuse = max(dot(normal, lightDirection), 0.0);
  float specular = pow(max(dot(normal, halfVector), 0.0), 32.0);
  return albedo * (0.18 + 0.82 * diffuse) + vec3(0.18 * specular);
}

float distributionGgx(vec3 normal, vec3 halfVector, float roughness)
{
  float alpha = roughness * roughness;
  float alpha2 = alpha * alpha;
  float nDotH = max(dot(normal, halfVector), 0.0);
  float nDotH2 = nDotH * nDotH;
  float denominator = (nDotH2 * (alpha2 - 1.0) + 1.0);
  return alpha2 / max(kPi * denominator * denominator, 0.000001);
}

float geometrySchlickGgx(float nDotV, float roughness)
{
  float r = roughness + 1.0;
  float k = (r * r) / 8.0;
  return nDotV / max(nDotV * (1.0 - k) + k, 0.000001);
}

vec3 fresnelSchlick(float cosTheta, vec3 f0)
{
  return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 physicallyBasedColor(vec3 albedo, vec3 normal, vec3 lightDirection, vec3 viewDirection)
{
  vec3 halfVector = normalize(lightDirection + viewDirection);
  float nDotL = max(dot(normal, lightDirection), 0.0);
  float nDotV = max(dot(normal, viewDirection), 0.0);
  float hDotV = max(dot(halfVector, viewDirection), 0.0);

  vec3 f0 = mix(vec3(0.04), albedo, u_metallic);
  float distribution = distributionGgx(normal, halfVector, u_roughness);
  float geometry = geometrySchlickGgx(nDotV, u_roughness) * geometrySchlickGgx(nDotL, u_roughness);
  vec3 fresnel = fresnelSchlick(hDotV, f0);

  vec3 specular = (distribution * geometry * fresnel) / max(4.0 * nDotV * nDotL, 0.000001);
  vec3 diffuse = (vec3(1.0) - fresnel) * (1.0 - u_metallic) * albedo / kPi;
  vec3 ambient = 0.14 * albedo * u_ambientOcclusion;
  return ambient + (diffuse + specular) * nDotL;
}

float shadowVisibility(vec3 worldPosition)
{
  if (!u_shadowMapEnabled) {
    return 1.0;
  }

  vec4 lightClip = u_lightClip_T_world * vec4(worldPosition, 1.0);
  vec3 lightNdc = lightClip.xyz / lightClip.w;
  vec3 shadowCoord = lightNdc * 0.5 + 0.5;
  if (
    shadowCoord.x < 0.0 || shadowCoord.x > 1.0 || shadowCoord.y < 0.0 || shadowCoord.y > 1.0 || shadowCoord.z < 0.0 ||
    shadowCoord.z > 1.0)
  {
    return 1.0;
  }

  float closestDepth = texture(u_shadowMapTex, shadowCoord.xy).r;
  float currentDepth = shadowCoord.z;
  return currentDepth - u_shadowDepthBias > closestDepth ? 1.0 - u_shadowStrength : 1.0;
}

float screenAmbientOcclusion()
{
  if (!u_screenAmbientOcclusionEnabled) {
    return 1.0;
  }

  ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
  return texelFetch(u_screenAmbientOcclusionTex, pixelCoord, 0).r;
}

void main()
{
  for (int i = 0; i < u_clipPlaneCount; ++i) {
    if (dot(u_clipPlanes[i].xyz, v_worldPosition) + u_clipPlanes[i].w < 0.0) {
      discard;
    }
  }

  vec3 normal = normalize(v_worldNormal);
  vec3 lightDirection = normalize(u_lightDirectionWorld);
  vec3 viewDirection = normalize(u_cameraWorldPosition - v_worldPosition);
  vec4 color = u_hasVertexColors ? v_color * u_baseColor : u_baseColor;
  vec3 litColor = u_shadingModel == kShadingModelPhysicallyBased
                    ? physicallyBasedColor(color.rgb, normal, lightDirection, viewDirection)
                    : simpleLitColor(color.rgb, normal, lightDirection, viewDirection);
  litColor *= shadowVisibility(v_worldPosition);
  litColor *= screenAmbientOcclusion();

  fragColor = vec4(litColor, color.a);
}
