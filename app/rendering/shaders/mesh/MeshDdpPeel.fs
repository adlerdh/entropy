#version 330 core

in vec3 v_worldPosition;
in vec3 v_worldNormal;
in vec4 v_color;
noperspective in vec3 v_barycentric;

uniform vec4 u_baseColor;
uniform float u_metallic;
uniform float u_roughness;
uniform float u_ambientOcclusion;
uniform int u_shadingModel;
uniform bool u_flatShadingEnabled;
uniform bool u_triangleEdgesEnabled;
uniform vec3 u_triangleEdgeColor;
uniform float u_lightingAmbient;
uniform float u_lightingDiffuse;
uniform float u_lightingSpecular;
uniform float u_lightingSpecularPower;
uniform bool u_rimLightingEnabled;
uniform float u_rimOpacityStrength;
uniform float u_rimEmissionStrength;
uniform float u_rimPower;
uniform bool u_hasVertexColors;
uniform vec3 u_cameraWorldPosition;
uniform bool u_shadowMapEnabled;
uniform sampler2D u_shadowMapTex;
uniform mat4 u_lightClip_T_world;
uniform float u_shadowStrength;
uniform float u_shadowDepthBias;
uniform vec3 u_lightDirectionWorld;
uniform bool u_screenAmbientOcclusionEnabled;
uniform sampler2D u_screenAmbientOcclusionTex;
uniform ivec2 u_viewportOrigin;
uniform int u_clipPlaneCount;
uniform vec4 u_clipPlanes[8];
uniform sampler2D u_previousDepthBoundsTex;
uniform sampler2D u_previousFrontColorTex;

layout(location = 0) out vec2 outDepthBounds;
layout(location = 1) out vec4 outFrontColor;
layout(location = 2) out vec4 outBackColor;

const float kMaxDepth = 1.0;
const float kDepthEpsilon = 0.000001;
const int kShadingModelUnlit = 0;
const int kShadingModelSimpleLit = 1;
const int kShadingModelPhysicallyBased = 2;
const float kPi = 3.14159265359;
const vec3 kPbrFillLightDirection = vec3(-0.43643578, 0.21821789, 0.87287156);
const float kPbrFillLightStrength = 0.18;
const float kPbrAmbientStrength = 0.30;
const float kPbrDiffuseStrength = 0.50;
const float kPbrSpecularStrength = 0.20;

vec3 simpleLitColor(vec3 albedo, vec3 normal, vec3 lightDirection, vec3 viewDirection, float ao, float shadow)
{
  vec3 halfDirection = lightDirection + viewDirection;
  vec3 halfVector = dot(halfDirection, halfDirection) > 0.000001 ? normalize(halfDirection) : normal;
  float diffuse = abs(dot(normal, lightDirection));
  float specular = pow(abs(dot(normal, halfVector)), max(u_lightingSpecularPower, 0.001));
  vec3 ambient = albedo * u_lightingAmbient * ao;
  vec3 direct = albedo * u_lightingDiffuse * diffuse + vec3(u_lightingSpecular * specular);
  return ambient + direct * shadow;
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

vec3 physicallyBasedDirectColor(vec3 albedo, vec3 normal, vec3 lightDirection, vec3 viewDirection, float lightStrength)
{
  vec3 halfDirection = lightDirection + viewDirection;
  vec3 halfVector = dot(halfDirection, halfDirection) > 0.000001 ? normalize(halfDirection) : normal;
  float nDotL = max(dot(normal, lightDirection), 0.0);
  float nDotV = max(dot(normal, viewDirection), 0.0);
  float hDotV = max(dot(halfVector, viewDirection), 0.0);

  vec3 f0 = mix(vec3(0.04), albedo, u_metallic);
  float distribution = distributionGgx(normal, halfVector, u_roughness);
  float geometry = geometrySchlickGgx(nDotV, u_roughness) * geometrySchlickGgx(nDotL, u_roughness);
  vec3 fresnel = fresnelSchlick(hDotV, f0);

  vec3 specular = (distribution * geometry * fresnel) / max(4.0 * nDotV * nDotL, 0.000001);
  vec3 diffuse = (vec3(1.0) - fresnel) * (1.0 - u_metallic) * albedo / kPi;
  // Fixed neutral light strengths keep PBR independent of the disabled Blinn-Phong controls.
  // Multiplying by pi treats the strengths as irradiance and keeps brightness comparable.
  vec3 diffuseLighting = diffuse * (kPi * kPbrDiffuseStrength);
  vec3 specularLighting = specular * (kPi * kPbrSpecularStrength);
  return (diffuseLighting + specularLighting) * nDotL * lightStrength;
}

vec3 physicallyBasedColor(vec3 albedo, vec3 normal, vec3 lightDirection, vec3 viewDirection, float ao, float shadow)
{
  vec3 ambient = albedo * kPbrAmbientStrength * u_ambientOcclusion * ao;
  vec3 keyLighting = physicallyBasedDirectColor(albedo, normal, lightDirection, viewDirection, shadow);
  vec3 fillLighting =
    physicallyBasedDirectColor(albedo, normal, kPbrFillLightDirection, viewDirection, kPbrFillLightStrength);
  return ambient + keyLighting + fillLighting;
}

vec3 surfaceNormal()
{
  if (!u_flatShadingEnabled && dot(v_worldNormal, v_worldNormal) > 0.000001) {
    return normalize(v_worldNormal);
  }

  vec3 geometricNormal = cross(dFdx(v_worldPosition), dFdy(v_worldPosition));
  return dot(geometricNormal, geometricNormal) > 0.000001 ? normalize(geometricNormal) : vec3(0.0, 0.0, 1.0);
}

float shadowVisibility(vec3 worldPosition, vec3 normal, vec3 lightDirection)
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

  float currentDepth = shadowCoord.z;
  float normalOffset = 1.0 - clamp(abs(dot(normal, lightDirection)), 0.0, 1.0);
  float receiverBias = u_shadowDepthBias * mix(1.0, 3.0, normalOffset);
  vec2 texelSize = 1.0 / vec2(textureSize(u_shadowMapTex, 0));
  float occludedSamples = 0.0;
  for (int y = -1; y <= 1; ++y) {
    for (int x = -1; x <= 1; ++x) {
      float closestDepth = texture(u_shadowMapTex, shadowCoord.xy + vec2(x, y) * texelSize).r;
      occludedSamples += currentDepth - receiverBias > closestDepth ? 1.0 : 0.0;
    }
  }
  return 1.0 - u_shadowStrength * occludedSamples / 9.0;
}

float screenAmbientOcclusion()
{
  if (!u_screenAmbientOcclusionEnabled) {
    return 1.0;
  }

  ivec2 pixelCoord = ivec2(gl_FragCoord.xy) - u_viewportOrigin;
  pixelCoord = clamp(pixelCoord, ivec2(0), textureSize(u_screenAmbientOcclusionTex, 0) - ivec2(1));
  return texelFetch(u_screenAmbientOcclusionTex, pixelCoord, 0).r;
}

vec4 applyRimLighting(vec4 color, vec3 normal, vec3 viewDirection)
{
  if (!u_rimLightingEnabled) {
    return color;
  }

  float rim = pow(clamp(1.0 - abs(dot(normal, viewDirection)), 0.0, 1.0), max(u_rimPower, 0.001));
  float alphaScale = mix(1.0, rim, clamp(u_rimOpacityStrength, 0.0, 1.0));
  vec3 rimColor = max(u_baseColor.rgb, vec3(0.0));
  color.rgb += max(u_rimEmissionStrength, 0.0) * rim * rimColor;
  color.a *= alphaScale;
  return color;
}

vec4 applyTriangleEdges(vec4 color)
{
  if (!u_triangleEdgesEnabled) {
    return color;
  }

  vec3 antialiasedInterior = smoothstep(vec3(0.0), fwidth(v_barycentric) * 1.25, v_barycentric);
  float interior = min(min(antialiasedInterior.x, antialiasedInterior.y), antialiasedInterior.z);
  color.rgb = mix(u_triangleEdgeColor, color.rgb, interior);
  return color;
}

vec4 shadedMeshColor()
{
  vec3 normal = surfaceNormal();
  vec3 viewDirection = normalize(u_cameraWorldPosition - v_worldPosition);
  vec3 shadingNormal = faceforward(normal, -viewDirection, normal);
  vec3 lightDirection = u_shadowMapEnabled ? normalize(u_lightDirectionWorld) : viewDirection;
  vec4 color = u_hasVertexColors ? v_color * u_baseColor : u_baseColor;
  float shadow = shadowVisibility(v_worldPosition, shadingNormal, lightDirection);
  float ao = screenAmbientOcclusion();
  vec3 litColor = u_shadingModel == kShadingModelUnlit
                    ? color.rgb
                    : (u_shadingModel == kShadingModelPhysicallyBased
                         ? physicallyBasedColor(color.rgb, shadingNormal, lightDirection, viewDirection, ao, shadow)
                         : simpleLitColor(color.rgb, shadingNormal, lightDirection, viewDirection, ao, shadow));
  return applyTriangleEdges(applyRimLighting(vec4(litColor, color.a), normal, viewDirection));
}

void main()
{
  for (int i = 0; i < u_clipPlaneCount; ++i) {
    if (dot(u_clipPlanes[i].xyz, v_worldPosition) + u_clipPlanes[i].w < 0.0) {
      discard;
    }
  }

  ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
  vec2 previousDepthBounds = texelFetch(u_previousDepthBoundsTex, pixelCoord, 0).xy;
  vec4 previousFrontColor = texelFetch(u_previousFrontColorTex, pixelCoord, 0);
  float fragmentDepth = gl_FragCoord.z;
  float nearestDepth = -previousDepthBounds.x;
  float farthestDepth = previousDepthBounds.y;

  outDepthBounds = vec2(-kMaxDepth);
  outFrontColor = previousFrontColor;
  outBackColor = vec4(0.0);

  // Fragments outside the previous bounds were already peeled in an earlier pass.
  if (fragmentDepth < nearestDepth - kDepthEpsilon || fragmentDepth > farthestDepth + kDepthEpsilon) {
    return;
  }

  // Interior fragments are candidates for the next pass. GL_MAX blending keeps the nearest and farthest candidates.
  if (fragmentDepth > nearestDepth + kDepthEpsilon && fragmentDepth < farthestDepth - kDepthEpsilon) {
    outDepthBounds = vec2(-fragmentDepth, fragmentDepth);
    return;
  }

  // Boundary fragments are the frontmost and backmost layers for this pass.
  vec4 color = shadedMeshColor();
  vec4 premultipliedColor = vec4(color.rgb * color.a, color.a);
  if (fragmentDepth >= nearestDepth - kDepthEpsilon && fragmentDepth <= nearestDepth + kDepthEpsilon) {
    outFrontColor = previousFrontColor + premultipliedColor * (1.0 - previousFrontColor.a);
  }
  else {
    outBackColor = premultipliedColor;
  }
}
