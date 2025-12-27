#version 330 core

#define NISO 8
#define NUM_BISECTIONS 5
#define MAX_HITS 100
#define MAX_JUMPS 1000

// 3D texture coordinates (s,t,p) are in [0.0, 1.0]^3
#define MIN_IMAGE_TEXCOORD vec3(0.0)
#define MAX_IMAGE_TEXCOORD vec3(1.0)

out vec4 FragColor;

uniform sampler3D u_imgTex; // image
uniform usampler3D u_jumpTex; // distance texture
//uniform usampler3D u_segTex; // segmentation

uniform mat4 u_tex_T_world;
uniform mat4 u_world_T_imgTex;
uniform mat4 u_clip_T_world;
uniform mat4 u_clip_T_imgTex;

uniform vec3 u_worldEyePos;
uniform mat3 u_texGrads;

uniform vec3 u_imgInvDims;
uniform float u_samplingFactor;

uniform int u_numIsos;
uniform float u_isoValues[NISO];
uniform float u_isoOpacities[NISO];
uniform float u_isoEdges[NISO];

uniform vec3 u_ambient[NISO];
uniform vec3 u_diffuse[NISO];
uniform vec3 u_specular[NISO];
uniform float u_shininess[NISO];

uniform vec4 u_bgColor; // premultiplied by alpha

uniform bool u_renderFrontFaces;
uniform bool u_renderBackFaces;
uniform bool u_noHitTransparent;

// Mutually exclusive flags controlling whether the segmentation masks the isosurfaces in or out:
//uniform bool segMasksIn;
//uniform bool segMasksOut;

// Redeclared vertex shader outputs: now the fragment shader inputs
in VS_OUT
{
  vec3 v_worldRayDir; // Ray direction in World space (NOT normalized)
} fs_in;

float rand(vec2 v) { return fract(sin(dot(v.xy, vec2(12.9898, 78.233))) * 43758.5453); }

float compMax(vec3 v) { return max(max(v.x, v.y), v.z); }
float compMin(vec3 v) { return min(min(v.x, v.y), v.z); }

float when_lt(float x, float y) { return max(sign(y - x), 0.0); }
float when_ge(float x, float y) { return 1.0 - when_lt(x, y); }

float getImageValue(sampler3D tex, vec3 texCoord)
{
  return texture(tex, texCoord)[0];
}

vec2 slabs(vec3 texRayPos, vec3 texRayDir)
{
  vec3 t0 = (MIN_IMAGE_TEXCOORD - texRayPos) / texRayDir;
  vec3 t1 = (MAX_IMAGE_TEXCOORD - texRayPos) / texRayDir;
  vec3 tMin = min(t0, t1);
  vec3 tMax = max(t0, t1);
  return vec2(compMax(tMin), compMin(tMax));
}

vec3 gradient(vec3 texPos)
{
  return normalize(vec3(
    getImageValue(u_imgTex, texPos + u_texGrads[0]) - getImageValue(u_imgTex, texPos - u_texGrads[0]),
    getImageValue(u_imgTex, texPos + u_texGrads[1]) - getImageValue(u_imgTex, texPos - u_texGrads[1]),
    getImageValue(u_imgTex, texPos + u_texGrads[2]) - getImageValue(u_imgTex, texPos - u_texGrads[2])));
}

vec3 bisect(vec3 pos, vec3 dir, float t0, float t1, float sgn, float iso)
{
  float t = 0.5 * (t0 + t1);
  vec3 c = pos + t * dir;

  for (int i = 0; i < NUM_BISECTIONS; ++i) {
    float test = sgn * (getImageValue(u_imgTex, c) - iso);
    t1 += (t - t1) * when_ge(test, 0.0); // if (test >= 0.0) { t1 = t; }
    t0 += (t - t0) * when_lt(test, 0.0); // else { t0 = t; }
    t = 0.5 * (t0 + t1);
    c = pos + t * dir;
  }

  return c;
}

// TODO add point light?
vec4 shade(vec3 lightDir, vec3 viewDir, vec3 normal, int i)
{
  vec3 h = normalize(lightDir + viewDir);
  float a = mix(1.0, 4.0, u_isoEdges[i] > 0.0);
  float d = abs(dot(normal, lightDir));
  float s = pow(abs(dot(normal, h)), u_shininess[i]);
  float e = pow(1.0 - abs(dot(normal, viewDir)), u_isoEdges[i]);

  return u_isoOpacities[i] * e * vec4(a * u_ambient[i] + d * u_diffuse[i] + s * u_specular[i], 1.0);
}

void main()
{
  gl_FragDepth = gl_DepthRange.near;

  // Final fragment color that gets composited by ray traversal through image volume:
  vec4 color = vec4(0.0);

  // The ray direction must be re-normalized after interpolation from Vertex to Fragment stage:
  vec3 texRayDir = mat3(u_tex_T_world) * normalize(fs_in.v_worldRayDir);

  // Convert physical (mm) to texel units along the ray direction
  float texel_T_mm = length(texRayDir);
  texRayDir /= texel_T_mm; // normalize the direction

  vec3 dirSq = texRayDir * texRayDir;

  // Step size computed as a u_samplingFactor fraction of the voxel spacing along the ray:
  float texStep = u_samplingFactor * min(min(
    u_imgInvDims.x * sqrt(1.0 + (dirSq.y + dirSq.z)) / max(dirSq.x, 1.0e-6),
    u_imgInvDims.y * sqrt(1.0 + (dirSq.z + dirSq.x)) / max(dirSq.y, 1.0e-6)),
    u_imgInvDims.z * sqrt(1.0 + (dirSq.x + dirSq.y)) / max(dirSq.z, 1.0e-6));

  // Randomly purturb the ray starting positions along the ray direction:
  vec4 texEyePos = u_tex_T_world * vec4(u_worldEyePos, 1.0);
  vec3 texStartPos = vec3(texEyePos) + 0.5 * texStep * rand(gl_FragCoord.xy) * texRayDir;

  vec2 interx = slabs(texStartPos, texRayDir);

  if (interx[1] <= interx[0] || interx[1] <= 0.0) {
    // The ray did not intersect the bounding box
    FragColor = float(!u_noHitTransparent) * u_bgColor;
    return;
  }

  float tMin = max(0.0, interx[0]);
  float tMax = interx[1];

  vec3 texPosMin = texStartPos + tMin * texRayDir;
  vec3 texPosMax = texStartPos + tMax * texRayDir;

  int hitCount = 0;
  int stepCount = 0;

  // Current position along the ray
  vec3 texPos = texStartPos + tMin * texRayDir;
  vec3 texFirstHitPos = texPos; // Initialize first hit position to front of volume
  int firstHit = 1;

  // Save old value and position
  float oldValue = getImageValue(u_imgTex, texPos);
  vec3 oldTexPos = texPos;
  float oldT = tMin;

  for (float t = tMin; t <= tMax; t += texStep)
  {
    float jump = 0.0;
    int numJumps = 0;

    do {
      t += jump;
      oldTexPos = texPos;
      texPos = texStartPos + t * texRayDir;
      jump = 0.98 * texel_T_mm * float(texture(u_jumpTex, texPos).r); // 2% safety factor
      ++numJumps;
    } while (jump > texStep && t <= tMax && numJumps <= MAX_JUMPS);

//    FragColor = vec4(vec3(float(numJumps)/10.0), 1.0);
//    return;

    float value = getImageValue(u_imgTex, texPos);

    for (int i = 0; i < u_numIsos; ++i)
    {
      bool frontHit = u_renderFrontFaces && value >= u_isoValues[i] && oldValue <  u_isoValues[i];
      bool backHit = u_renderBackFaces && value < u_isoValues[i] && oldValue >= u_isoValues[i];

      if (u_isoOpacities[i] > 0.0 && (frontHit || backHit))
      {
        ++hitCount;
        vec3 texHitPos = bisect(texStartPos, texRayDir, oldT, t, value - u_isoValues[i], u_isoValues[i]);
        vec3 texLightDir = normalize(texStartPos - texHitPos);
        vec3 texNormal = gradient(texHitPos);

//        float segMask = float(
//          (!segMasksIn && !segMasksOut) ||
//          (segMasksIn && texture(u_segTex, texPos).r > 0u) ||
//          (segMasksOut && texture(u_segTex, texPos).r == 0u));
//        color += segMask * (1.0 - color.a) * shade(texLightDir, -texRayDir, texNormal, i);

        color += (1.0 - color.a) * shade(texLightDir, -texRayDir, texNormal, i); // blend under

        // Record the first hit:
        texFirstHitPos = mix(texFirstHitPos, texHitPos, float(firstHit));
        firstHit = firstHit ^ 1;

        // An isosurface intersection occurred, so there is no need to loop over the remaining
        // isosurfaces for intersections at this position.
        break;
      }
    }

    if (color.a >= 0.95 || hitCount >= MAX_HITS) {
      break; // Early ray termination
    }

    oldValue = value;
    oldT = t;
    ++stepCount;
  }

//  float normDistance = abs(oldT - tMin);
//  float fog = exp(-normDistance*normDistance);

  FragColor = color + (1.0 - color.a) * u_bgColor;

  vec4 clipFirstHitPos = u_clip_T_imgTex * vec4(texFirstHitPos, 1.0);
  float ndcFirstHitDepth = clipFirstHitPos.z / clipFirstHitPos.w;

  gl_FragDepth = 0.5 * ((gl_DepthRange.far - gl_DepthRange.near) * ndcFirstHitDepth +
                 gl_DepthRange.near + gl_DepthRange.far);
}
