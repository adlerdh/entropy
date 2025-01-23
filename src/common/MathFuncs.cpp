#include "common/MathFuncs.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <random>
#include <string>

#undef min
#undef max

namespace math
{

std::vector<glm::vec3> generateRandomHsvSamples(
  size_t numSamples,
  const std::pair<float, float>& hueMinMax,
  const std::pair<float, float>& satMinMax,
  const std::pair<float, float>& valMinMax,
  const std::optional<uint32_t>& seed
)
{
  std::mt19937_64 generator;
  generator.seed(seed ? *seed : std::mt19937_64::default_seed);

  std::uniform_real_distribution<float> dist(0.0f, 1.0f);

  const float A = (satMinMax.second * satMinMax.second - satMinMax.first * satMinMax.first);

  const float B = satMinMax.first * satMinMax.first;

  const float C = ( valMinMax.second * valMinMax.second * valMinMax.second -
                      valMinMax.first * valMinMax.first * valMinMax.first );

  const float D = valMinMax.first * valMinMax.first * valMinMax.first;

  std::vector<glm::vec3> samples;
  samples.reserve(numSamples);

  for (size_t i = 0; i < numSamples; ++i)
  {
    float hue = (hueMinMax.second - hueMinMax.first) * dist(generator) + hueMinMax.first;
    float sat = std::sqrt(dist(generator) * A + B);
    float val = std::pow(dist(generator) * C + D, 1.0f / 3.0f);

    samples.push_back(glm::vec3{hue, sat, val});
  }

  return samples;
}

glm::dvec3 computeSubjectImageDimensions(
  const glm::u64vec3& pixelDimensions, const glm::dvec3& pixelSpacing
)
{
  return glm::dvec3{
    static_cast<double>(pixelDimensions.x) * pixelSpacing.x,
    static_cast<double>(pixelDimensions.y) * pixelSpacing.y,
    static_cast<double>(pixelDimensions.z) * pixelSpacing.z
  };
}

glm::dmat4 computeImagePixelToSubjectTransformation(
  const glm::dmat3& directions, const glm::dvec3& pixelSpacing, const glm::dvec3& origin
)
{
  return glm::dmat4{
    glm::dvec4{pixelSpacing.x * directions[0], 0.0}, // column 0
    glm::dvec4{pixelSpacing.y * directions[1], 0.0}, // column 1
    glm::dvec4{pixelSpacing.z * directions[2], 0.0}, // column 2
    glm::dvec4{origin, 1.0}
  }; // column 3
}

glm::dmat4 computeImagePixelToTextureTransformation(const glm::u64vec3& pixelDimensions)
{
  const glm::dvec3 invDim(
    1.0 / static_cast<double>(pixelDimensions.x),
    1.0 / static_cast<double>(pixelDimensions.y),
    1.0 / static_cast<double>(pixelDimensions.z)
  );

  return glm::translate(0.5 * invDim) * glm::scale(invDim);
}

glm::vec3 computeInvPixelDimensions(const glm::u64vec3& pixelDimensions)
{
  const glm::dvec3 invDim(
    1.0 / static_cast<double>(pixelDimensions.x),
    1.0 / static_cast<double>(pixelDimensions.y),
    1.0 / static_cast<double>(pixelDimensions.z)
  );

  return invDim;
}

std::array<glm::vec3, 8> computeImagePixelAABBoxCorners(const glm::u64vec3& pixelDims)
{
  constexpr size_t N = 8; // Image has 8 corners

  // To get the pixel edges/corners, offset integer coordinates by half of a pixel,
  // because integer pixel coordinates are at the CENTER of the pixel
  static const glm::vec3 sk_halfPixel{0.5f, 0.5f, 0.5f};

  const glm::vec3 D = glm::vec3{pixelDims} - sk_halfPixel;

  const std::array<glm::vec3, N> pixelCorners = {
    glm::vec3{-0.5f, -0.5f, -0.5f},
    glm::vec3{D.x, -0.5f, -0.5f},
    glm::vec3{-0.5f, D.y, -0.5f},
    glm::vec3{D.x, D.y, -0.5f},
    glm::vec3{-0.5f, -0.5f, D.z},
    glm::vec3{D.x, -0.5f, D.z},
    glm::vec3{-0.5f, D.y, D.z},
    glm::vec3{D.x, D.y, D.z}
  };

  return pixelCorners;
}

std::array<glm::vec3, 8> computeImageSubjectBoundingBoxCorners(
  const glm::u64vec3& pixelDims,
  const glm::mat3& directions,
  const glm::vec3& spacing,
  const glm::vec3& origin
)
{
  constexpr size_t N = 8; // Image has 8 corners

  const glm::mat4 subject_T_pixel
    = computeImagePixelToSubjectTransformation(directions, spacing, origin);

  const std::array<glm::vec3, N> pixelCorners = computeImagePixelAABBoxCorners(pixelDims);

  std::array<glm::vec3, N> subjectCorners;

  std::transform(
    std::begin(pixelCorners),
    std::end(pixelCorners),
    std::begin(subjectCorners),
    [&subject_T_pixel](const glm::dvec3& v)
    {
      const glm::vec4 subv = subject_T_pixel * glm::vec4{v, 1.0f};
      return glm::vec3{subv / subv.w};
    }
  );

  return subjectCorners;
}

std::pair<glm::vec3, glm::vec3> computeMinMaxCornersOfAABBox(
  const std::array<glm::vec3, 8>& subjectCorners
)
{
  glm::vec3 minSubjectCorner{std::numeric_limits<float>::max()};
  glm::vec3 maxSubjectCorner{std::numeric_limits<float>::lowest()};

  for (uint32_t c = 0; c < 8; ++c)
  {
    for (int i = 0; i < 3; ++i)
    {
      if (subjectCorners[c][i] < minSubjectCorner[i])
      {
        minSubjectCorner[i] = subjectCorners[c][i];
      }

      if (subjectCorners[c][i] > maxSubjectCorner[i])
      {
        maxSubjectCorner[i] = subjectCorners[c][i];
      }
    }
  }

  return std::make_pair(minSubjectCorner, maxSubjectCorner);
}

std::array<glm::vec3, 8> computeAllAABBoxCornersFromMinMaxCorners(
  const std::pair<glm::vec3, glm::vec3>& boxMinMaxCorners
)
{
  const glm::vec3 size = boxMinMaxCorners.second - boxMinMaxCorners.first;

  std::array<glm::vec3, 8> corners = {
    {glm::vec3{0, 0, 0},
     glm::vec3{size.x, 0, 0},
     glm::vec3{0, size.y, 0},
     glm::vec3{0, 0, size.z},
     glm::vec3{size.x, size.y, 0},
     glm::vec3{size.x, 0, size.z},
     glm::vec3{0, size.y, size.z},
     glm::vec3{size.x, size.y, size.z}}
  };

  std::for_each(
    std::begin(corners),
    std::end(corners),
    [&boxMinMaxCorners](glm::vec3& corner) { corner = corner + boxMinMaxCorners.first; }
  );

  return corners;
}

std::pair<std::string, bool> computeSpiralCodeFromDirectionMatrix(const glm::dmat3& directions)
{
  // Fourth character is null-terminating character
  char spiralCode[4] = "???";

  // LPS directions are positive
  static const char codes[3][2] = {{'R', 'L'}, {'A', 'P'}, {'I', 'S'}};

  bool isOblique = false;

  for (uint32_t i = 0; i < 3; ++i)
  {
    // Direction cosine for voxel direction i
    const glm::dvec3 dir = directions[int(i)];

    uint32_t closestDir = 0;
    double maxDot = -999.0;
    uint32_t sign = 0;

    for (uint32_t j = 0; j < 3; ++j)
    {
      glm::dvec3 a{0.0, 0.0, 0.0};
      a[int(j)] = 1.0;

      const double newDot = glm::dot(glm::abs(dir), a);

      if (newDot > maxDot)
      {
        maxDot = newDot;
        closestDir = j;

        if (glm::dot(dir, a) < 0.0)
        {
          sign = 0;
        }
        else
        {
          sign = 1;
        }
      }
    }

    spiralCode[i] = codes[closestDir][sign];

    if (glm::abs(maxDot) < 1.0)
    {
      isOblique = true;
    }
  }

  return std::make_pair(std::string{spiralCode}, isOblique);
}

glm::dmat3 computeClosestOrthogonalDirectionMatrix(const glm::dmat3& directions)
{
  glm::dmat3 closestMatrix(0);

  for (uint32_t i = 0; i < 3; ++i)
  {
    // Direction cosine for voxel direction i
    const glm::dvec3 dir = directions[int(i)];

    uint32_t closestDir = 0;

    closestMatrix[i] = glm::dvec3{0.0};
    closestMatrix[i][closestDir] = 1.0;

    double maxDot = -1.0e9;

    for (uint32_t j = 0; j < 3; ++j)
    {
      glm::dvec3 a{0.0, 0.0, 0.0};
      a[int(j)] = 1.0;

      const double newDot = glm::dot(glm::abs(dir), a);

      if (newDot > maxDot)
      {
        maxDot = newDot;
        closestDir = j;

        closestMatrix[i] = glm::dvec3{0.0};

        if (glm::dot(dir, a) < 0.0)
        {
          closestMatrix[i][closestDir] = -1.0;
        }
        else
        {
          closestMatrix[i][closestDir] = 1.0;
        }
      }
    }
  }

  return closestMatrix;
}

void rotateFrameAboutWorldPos(
  CoordinateFrame& frame, const glm::quat& rotation, const glm::vec3& worldCenter
)
{
  const glm::quat oldRotation = frame.world_T_frame_rotation();
  const glm::vec3 oldOrigin = frame.worldOrigin();

  frame.setFrameToWorldRotation(rotation * oldRotation);
  frame.setWorldOrigin(rotation * (oldOrigin - worldCenter) + worldCenter);
}

float computeRayAABBoxIntersection(
  const glm::vec3& start, const glm::vec3& dir, const glm::vec3& minCorner, const glm::vec3& maxCorner
)
{
  const float t = glm::distance(minCorner, maxCorner);

  const glm::vec3 a = (minCorner - start) / dir;
  const glm::vec3 b = (maxCorner - start) / dir;
  const glm::vec3 u = glm::min(a, b);

  return std::max(std::max(-t, u.x), std::max(u.y, u.z));
}

std::pair<float, float> hits(glm::vec3 e1, glm::vec3 d, glm::vec3 uMinCorner, glm::vec3 uMaxCorner)
{
  float t = glm::distance(uMinCorner, uMaxCorner);

  glm::vec3 a = (uMinCorner - e1) / d;
  glm::vec3 b = (uMaxCorner - e1) / d;
  glm::vec3 u = glm::min(a, b);
  glm::vec3 v = glm::max(a, b);

  //    std::cout << "a = " << glm::to_string(a) << std::endl;
  //    std::cout << "b = " << glm::to_string(b) << std::endl;
  //    std::cout << "u = " << glm::to_string(u) << std::endl;
  //    std::cout << "v = " << glm::to_string(v) << std::endl;

  float i = std::max(std::max(-t, u.x), std::max(u.y, u.z));
  float j = std::min(std::min(t, v.x), std::min(v.y, v.z));

  //    std::cout << "i = " << i << std::endl;
  //    std::cout << "j = " << j << std::endl;

  return std::make_pair(i, j);
}

std::tuple<bool, float, float> slabs(
  glm::vec3 rayPos, glm::vec3 rayDir, glm::vec3 boxMin, glm::vec3 boxMax
)
{
  glm::vec3 t0 = (boxMin - rayPos) / rayDir;
  glm::vec3 t1 = (boxMax - rayPos) / rayDir;

  glm::vec3 tmin = min(t0, t1);
  glm::vec3 tmax = max(t0, t1);

  float a = glm::compMax(tmin);
  float b = glm::compMin(tmax);

  return std::make_tuple(a <= b, a, b);
}

std::optional<float> computeRayLineSegmentIntersection(
  const glm::vec2& rayOrigin, const glm::vec2& rayDir, const glm::vec2& lineA, const glm::vec2& lineB
)
{
  const glm::vec2 v1 = rayOrigin - lineA;
  const glm::vec2 v2 = lineB - lineA;
  const glm::vec2 v3{-rayDir.y, rayDir.x};

  const float d = glm::dot(v2, v3);

  if (std::abs(d) < glm::epsilon<float>())
  {
    return std::nullopt;
  }

  const float t1 = (v2.x * v1.y - v2.y * v1.x) / d;
  const float t2 = glm::dot(v1, v3) / glm::dot(v2, v3);

  if (0.0f <= t1 && 0.0f <= t2 && t2 <= 1.0f)
  {
    return t1;
  }

  return std::nullopt;
}

std::vector<glm::vec2> computeRayAABoxIntersections(
  const glm::vec2& rayStart,
  const glm::vec2& rayDir,
  const glm::vec2& boxMin,
  const glm::vec2& boxSize,
  bool doBothRayDirections
)
{
  std::vector<glm::vec2> allHits;

  std::array<std::pair<glm::vec2, glm::vec2>, 4> lineSegs = {
    std::make_pair(glm::vec2{boxMin.x, boxMin.y}, glm::vec2{boxMin.x, boxMin.y + boxSize.y}), // left
    std::make_pair(
      glm::vec2{boxMin.x + boxSize.x, boxMin.y},
      glm::vec2{boxMin.x + boxSize.x, boxMin.y + boxSize.y}
    ), // right
    std::make_pair(
      glm::vec2{boxMin.x, boxMin.y + boxSize.y},
      glm::vec2{boxMin.x + boxSize.x, boxMin.y + boxSize.y}
    ),                                                                                       // top
    std::make_pair(glm::vec2{boxMin.x, boxMin.y}, glm::vec2{boxMin.x + boxSize.x, boxMin.y}) // bottom
  };

  const glm::vec2 dirPos = glm::normalize(rayDir);

  for (const auto& lineSeg : lineSegs)
  {
    if ( const auto hit = math::computeRayLineSegmentIntersection( rayStart, dirPos, lineSeg.first, lineSeg.second ) )
    {
      allHits.push_back(rayStart + (*hit) * dirPos);
    }
  }

  if (doBothRayDirections)
  {
    const glm::vec2 dirNeg = -dirPos;

    for (const auto& lineSeg : lineSegs)
    {
      if ( const auto hit = math::computeRayLineSegmentIntersection( rayStart, dirNeg, lineSeg.first, lineSeg.second ) )
      {
        allHits.push_back(rayStart + (*hit) * dirNeg);
      }
    }
  }

  return allHits;
}

int pnpoly(const std::vector<glm::vec2>& poly, const glm::vec2& p)
{
  size_t i, j;
  bool c = false;

  for (i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
  {
    if ( ( ( poly[i].y > p.y ) != ( poly[j].y > p.y ) ) &&
             ( p.x < ( poly[j].x - poly[i].x ) * ( p.y - poly[i].y ) / ( poly[j].y - poly[i].y ) + poly[i].x ) )
    {
      c = !c;
    }
  }

  return c;
}

float interpolate(float x, const std::map<float, float>& table)
{
  if (table.empty())
  {
    return 0.0f;
  }

  auto it = table.lower_bound(x);

  if (std::end(table) == it)
  {
    return table.rbegin()->second;
  }
  else
  {
    if (std::begin(table) == it)
    {
      return it->second;
    }
    else
    {
      const float x2 = it->first;
      const float y2 = it->second;
      --it;

      const float x1 = it->first;
      const float y1 = it->second;
      const float p = (x - x1) / (x2 - x1);

      return (1.0f - p) * y1 + p * y2;
    }
  }
}

} // namespace math
