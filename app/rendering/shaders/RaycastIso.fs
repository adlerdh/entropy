#version 330 core

#define NISO 8
#define NUM_BISECTIONS 5
#define MAX_HITS 100
#define MAX_JUMPS 1000

// 3D texture coordinates (s,t,p) are in [0.0, 1.0]^3
#define MIN_IMAGE_TEXCOORD vec3(0.0)
#define MAX_IMAGE_TEXCOORD vec3(1.0)
#define RAY_BOX_PARALLEL_EPS 1.0e-7
#define RAY_BOX_BIG 1.0e20

out vec4 FragColor;

uniform sampler3D u_imgTex;   // image
uniform usampler3D u_jumpTex; // distance texture
// uniform usampler3D u_segTex; // segmentation

uniform mat4 u_tex_T_world;
uniform mat4 u_clip_T_imgTex;

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
uniform bool u_bgEdgeBrighteningEnabled;

uniform bool u_showCrosshairs3D;
uniform vec3 u_crosshairsWorldPos;
uniform vec4 u_crosshairsColor; // non-premultiplied by alpha
uniform float u_crosshairsRadiusMm;

uniform bool u_renderFrontFaces;
uniform bool u_renderBackFaces;
uniform bool u_noHitTransparent;

// Mutually exclusive flags controlling whether the segmentation masks the isosurfaces in or out:
// uniform bool segMasksIn;
// uniform bool segMasksOut;

// Redeclared vertex shader outputs: now the fragment shader inputs
in VS_OUT
{
  vec3 v_worldRayStart; // Ray start in World space
  vec3 v_worldRayDir;   // Ray direction in World space (NOT normalized)
}
fs_in;

float rand(vec2 v)
{
  return fract(sin(dot(v.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

float compMax(vec3 v)
{
  return max(max(v.x, v.y), v.z);
}
float compMin(vec3 v)
{
  return min(min(v.x, v.y), v.z);
}

float when_lt(float x, float y)
{
  return max(sign(y - x), 0.0);
}
float when_ge(float x, float y)
{
  return 1.0 - when_lt(x, y);
}

float getImageValue(sampler3D tex, vec3 texCoord)
{
  return texture(tex, texCoord)[0];
}

const float EDGE_BRIGHTENING_DISTANCE_VOX = 1.0;
const float EDGE_BACKGROUND_BRIGHTENING = 0.65;
const float EDGE_OVERLAY_BRIGHTENING = 0.35;

float faceEdgeDistanceVox(vec3 texPos)
{
  vec3 pos = clamp(texPos, MIN_IMAGE_TEXCOORD, MAX_IMAGE_TEXCOORD);
  vec3 faceDistanceTex = min(pos - MIN_IMAGE_TEXCOORD, MAX_IMAGE_TEXCOORD - pos);
  vec3 faceDistanceVox = faceDistanceTex / max(u_imgInvDims, vec3(1.0e-6));

  // Identify the image-box face containing this point, then measure distance to that
  // rectangular face's nearest edge. Applying this to both ray entry and exit points handles
  // visible edges on both front and back faces.
  if (faceDistanceTex.x <= faceDistanceTex.y && faceDistanceTex.x <= faceDistanceTex.z) {
    return min(faceDistanceVox.y, faceDistanceVox.z);
  }
  if (faceDistanceTex.y <= faceDistanceTex.z) {
    return min(faceDistanceVox.x, faceDistanceVox.z);
  }
  return min(faceDistanceVox.x, faceDistanceVox.y);
}

float imageBoxEdgeAmount(vec3 texPos)
{
  if (!u_bgEdgeBrighteningEnabled) {
    return 0.0;
  }

  float nearestFaceEdgeDistanceVox = faceEdgeDistanceVox(texPos);
  float aaVox = fwidth(nearestFaceEdgeDistanceVox);
  return 1.0 - smoothstep(
                 max(EDGE_BRIGHTENING_DISTANCE_VOX - aaVox, 0.0),
                 EDGE_BRIGHTENING_DISTANCE_VOX + aaVox,
                 nearestFaceEdgeDistanceVox);
}

vec4 brightenRaycastBackground(vec4 bgColor, float edgeAmount)
{
  // Preserve premultiplied alpha while brightening RGB toward premultiplied white. This makes
  // even a black raycast background visibly brighten at image-box face edges.
  return vec4(mix(bgColor.rgb, vec3(bgColor.a), EDGE_BACKGROUND_BRIGHTENING * edgeAmount), bgColor.a);
}

vec4 brightenRaycastResult(vec4 color, float edgeAmount)
{
  return vec4(mix(color.rgb, vec3(max(color.a, u_bgColor.a)), EDGE_OVERLAY_BRIGHTENING * edgeAmount), color.a);
}

vec2 slabs(vec3 texRayPos, vec3 texRayDir)
{
  // Intersect the ray with the image-domain AABB in normalized texture coordinates.
  // The explicit parallel-ray branch avoids NaNs from 0/0 when a ray starts exactly
  // on a box face and travels parallel to that face.
  vec3 tMin = vec3(-RAY_BOX_BIG);
  vec3 tMax = vec3(RAY_BOX_BIG);

  for (int axis = 0; axis < 3; ++axis) {
    if (abs(texRayDir[axis]) < RAY_BOX_PARALLEL_EPS) {
      if (texRayPos[axis] < MIN_IMAGE_TEXCOORD[axis] || texRayPos[axis] > MAX_IMAGE_TEXCOORD[axis]) {
        return vec2(1.0, 0.0);
      }
    }
    else {
      float t0 = (MIN_IMAGE_TEXCOORD[axis] - texRayPos[axis]) / texRayDir[axis];
      float t1 = (MAX_IMAGE_TEXCOORD[axis] - texRayPos[axis]) / texRayDir[axis];
      tMin[axis] = min(t0, t1);
      tMax[axis] = max(t0, t1);
    }
  }

  return vec2(compMax(tMin), compMin(tMax));
}

float raySphereFirstHit(vec3 rayPos, vec3 rayDir, vec3 sphereCenter, float sphereRadius)
{
  if (sphereRadius <= 0.0) {
    return -1.0;
  }

  vec3 oc = rayPos - sphereCenter;
  float b = dot(oc, rayDir);
  float c = dot(oc, oc) - sphereRadius * sphereRadius;
  float discriminant = b * b - c;

  if (discriminant < 0.0) {
    return -1.0;
  }

  float root = sqrt(discriminant);
  float t = -b - root;
  if (t < 0.0) {
    t = -b + root;
  }
  return t >= 0.0 ? t : -1.0;
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
  vec3 worldRayDir = normalize(fs_in.v_worldRayDir);
  vec3 texRayDir = mat3(u_tex_T_world) * worldRayDir;

  // Convert physical (mm) to texel units along the ray direction
  float texel_T_mm = length(texRayDir);
  texRayDir /= texel_T_mm; // normalize the direction

  vec3 dirSq = texRayDir * texRayDir;

  // Step size computed as a u_samplingFactor fraction of the voxel spacing along the ray:
  float texStep = u_samplingFactor * min(
                                       min(
                                         u_imgInvDims.x * sqrt(1.0 + (dirSq.y + dirSq.z)) / max(dirSq.x, 1.0e-6),
                                         u_imgInvDims.y * sqrt(1.0 + (dirSq.z + dirSq.x)) / max(dirSq.y, 1.0e-6)),
                                       u_imgInvDims.z * sqrt(1.0 + (dirSq.x + dirSq.y)) / max(dirSq.z, 1.0e-6));

  // Randomly perturb the ray starting positions along the ray direction. The vertex shader provides
  // the per-fragment ray start; using the camera eye here would break orthographic projection because
  // orthographic rays are parallel but originate at different positions across the view plane.
  vec4 texRayStartPos = u_tex_T_world * vec4(fs_in.v_worldRayStart, 1.0);
  vec3 texUnjitteredStartPos = vec3(texRayStartPos);
  vec3 texStartPos = texUnjitteredStartPos + 0.5 * texStep * rand(gl_FragCoord.xy) * texRayDir;

  vec2 interx = slabs(texStartPos, texRayDir);
  vec2 unjitteredInterx = slabs(texUnjitteredStartPos, texRayDir);

  if (interx[1] <= interx[0] || interx[1] <= 0.0) {
    // The ray did not intersect the bounding box
    FragColor = float(!u_noHitTransparent) * u_bgColor;
    return;
  }

  float tMin = max(0.0, interx[0]);
  float tMax = interx[1];

  float edgeTMin = max(0.0, unjitteredInterx[0]);
  float edgeTMax = unjitteredInterx[1];
  if (unjitteredInterx[1] <= unjitteredInterx[0] || unjitteredInterx[1] <= 0.0) {
    edgeTMin = tMin;
    edgeTMax = tMax;
  }
  vec3 texPosMin = texUnjitteredStartPos + edgeTMin * texRayDir;
  vec3 texPosMax = texUnjitteredStartPos + edgeTMax * texRayDir;
  float frontEdgeAmount = imageBoxEdgeAmount(texPosMin);
  float backEdgeAmount = imageBoxEdgeAmount(texPosMax);
  vec4 bgColor = brightenRaycastBackground(u_bgColor, backEdgeAmount);

  int hitCount = 0;
  int stepCount = 0;

  // Current position along the ray
  vec3 texPos = texStartPos + tMin * texRayDir;
  vec3 texFirstHitPos = texPos; // Initialize first hit position to front of volume
  float firstHitT = RAY_BOX_BIG;
  int firstHit = 1;

  // Save old value and position
  float oldValue = getImageValue(u_imgTex, texPos);
  vec3 oldTexPos = texPos;
  float oldT = tMin;

  for (float t = tMin; t <= tMax; t += texStep) {
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

    for (int i = 0; i < u_numIsos; ++i) {
      bool frontHit = u_renderFrontFaces && value >= u_isoValues[i] && oldValue < u_isoValues[i];
      bool backHit = u_renderBackFaces && value < u_isoValues[i] && oldValue >= u_isoValues[i];

      if (u_isoOpacities[i] > 0.0 && (frontHit || backHit)) {
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
        firstHitT = mix(firstHitT, dot(texHitPos - texStartPos, texRayDir), float(firstHit));
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

  if (u_showCrosshairs3D) {
    float crosshairsWorldT =
      raySphereFirstHit(fs_in.v_worldRayStart, worldRayDir, u_crosshairsWorldPos, u_crosshairsRadiusMm);
    vec3 crosshairsWorldHitPos = fs_in.v_worldRayStart + crosshairsWorldT * worldRayDir;
    vec3 crosshairsTexHitPos = vec3(u_tex_T_world * vec4(crosshairsWorldHitPos, 1.0));
    float crosshairsTexT = dot(crosshairsTexHitPos - texStartPos, texRayDir);

    if (crosshairsWorldT >= 0.0 && crosshairsTexT >= tMin && crosshairsTexT <= tMax && crosshairsTexT <= firstHitT) {
      vec3 sphereNormal = normalize(crosshairsWorldHitPos - u_crosshairsWorldPos);
      float diffuse = clamp(dot(sphereNormal, -worldRayDir), 0.0, 1.0);
      float specular = pow(diffuse, 24.0);
      vec3 glyphRgb = min(u_crosshairsColor.rgb * (0.35 + 0.65 * diffuse) + 0.18 * specular, vec3(1.0));
      vec4 glyphColor = vec4(glyphRgb * u_crosshairsColor.a, u_crosshairsColor.a);
      color = glyphColor + (1.0 - glyphColor.a) * color;
      texFirstHitPos = crosshairsTexHitPos;
    }
  }

  FragColor = brightenRaycastResult(color + (1.0 - color.a) * bgColor, frontEdgeAmount);

  vec4 clipFirstHitPos = u_clip_T_imgTex * vec4(texFirstHitPos, 1.0);
  float ndcFirstHitDepth = clipFirstHitPos.z / clipFirstHitPos.w;

  gl_FragDepth =
    0.5 * ((gl_DepthRange.far - gl_DepthRange.near) * ndcFirstHitDepth + gl_DepthRange.near + gl_DepthRange.far);
}
