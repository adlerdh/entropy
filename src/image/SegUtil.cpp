#include "image/SegUtil.h"
#include "image/Image.h"

#include "common/MathFuncs.h"

#include "logic/annotation/Annotation.h"
#include "logic/camera/MathUtility.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/string_cast.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <queue>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace
{

// Does the voxel intersect a plane?
// The plane is given in Voxel coordinates.
bool voxelInterectsPlane(const glm::vec4& voxelViewPlane, const glm::vec3& voxelPos)
{
  static const glm::vec3 cornerOffset{0.5f, 0.5f, 0.5f};
  return math::testAABBoxPlaneIntersection(voxelPos, voxelPos + cornerOffset, voxelViewPlane);
}

// Check if the voxel is inside the segmentation
bool isVoxelInSeg(const glm::ivec3 segDims, const glm::ivec3& voxelPos)
{
  return (
    voxelPos.x >= 0 && voxelPos.x < segDims.x && voxelPos.y >= 0 && voxelPos.y < segDims.y
    && voxelPos.z >= 0 && voxelPos.z < segDims.z
  );
}

std::tuple<std::unordered_set<glm::ivec3>, glm::ivec3, glm::ivec3> paintBrush2d(
  const glm::vec4& voxelViewPlane,
  const glm::ivec3& segDims,
  const glm::ivec3& roundedPixelPos,
  const std::array<float, 3>& mmToVoxelSpacings,
  int brushSizeInVoxels,
  bool brushIsRound
)
{
  // Queue of voxels to test for intersection with the view plane
  std::queue<glm::ivec3> voxelsToTest;

  // Set of voxels that have been sent off for testing
  std::unordered_set<glm::ivec3> voxelsProcessed;

  // Set of voxels that intersect with the view plane and that should be painted
  std::unordered_set<glm::ivec3> voxelsToPaint;

  // Set of voxels that do not intersect with view plane or that should not be painted
  std::unordered_set<glm::ivec3> voxelsToIgnore;

  // Insert the first voxel as a voxel to test, if it's inside the segmentation.
  // This voxel should intersect the view plane, since it was clicked by the mouse,
  // but test it to make sure.

  if (isVoxelInSeg(segDims, roundedPixelPos) && voxelInterectsPlane(voxelViewPlane, roundedPixelPos))
  {
    voxelsToTest.push(roundedPixelPos);
    voxelsProcessed.insert(roundedPixelPos);
  }

  std::array<glm::ivec3, 6> neighbors;

  const float radius = static_cast<float>(brushSizeInVoxels - 1);

  // Loop over all voxels in the test queue
  while (!voxelsToTest.empty())
  {
    const glm::ivec3 q = voxelsToTest.front();
    voxelsToTest.pop();

    // Check if the voxel is inside the brush
    const glm::vec3 d = q - roundedPixelPos;

    if (!brushIsRound)
    {
      // Equation for a rectangle:
      if (std::max(std::max(std::abs(d.x / mmToVoxelSpacings[0]), std::abs(d.y / mmToVoxelSpacings[1])), std::abs(d.z / mmToVoxelSpacings[2])) > radius)
      {
        voxelsToIgnore.insert(q);
        continue;
      }
    }
    else
    {
      if ((d.x * d.x / (mmToVoxelSpacings[0] * mmToVoxelSpacings[0]) + d.y * d.y / (mmToVoxelSpacings[1] * mmToVoxelSpacings[1]) + d.z * d.z / (mmToVoxelSpacings[2] * mmToVoxelSpacings[2])) > radius * radius)
      {
        voxelsToIgnore.insert(q);
        continue;
      }
    }

    // Mark that voxel intersects the view plane and is inside the brush:
    voxelsToPaint.insert(q);

    // Test its six neighbors, too:
    neighbors.fill(q);

    neighbors[0].x -= 1;
    neighbors[1].x += 1;
    neighbors[2].y -= 1;
    neighbors[3].y += 1;
    neighbors[4].z -= 1;
    neighbors[5].z += 1;

    for (const auto& n : neighbors)
    {
      if (0 == voxelsProcessed.count(n) && 0 == voxelsToIgnore.count(n) && 0 == voxelsToPaint.count(n) && voxelInterectsPlane(voxelViewPlane, n) && isVoxelInSeg(segDims, n))
      {
        voxelsToTest.push(n);
        voxelsProcessed.insert(n);
      }
      else
      {
        voxelsToIgnore.insert(n);
      }
    }
  }

  // Create a unique set of voxels to change:
  std::unordered_set<glm::ivec3> voxelsToChange;

  // Min/max corners of the set of voxels to change:
  glm::ivec3 minVoxel{std::numeric_limits<int>::max()};
  glm::ivec3 maxVoxel{std::numeric_limits<int>::lowest()};

  for (const auto& p : voxelsToPaint)
  {
    voxelsToChange.insert(p);
    minVoxel = glm::min(minVoxel, p);
    maxVoxel = glm::max(maxVoxel, p);
  }

  return std::make_tuple(voxelsToChange, minVoxel, maxVoxel);
}

std::tuple<std::unordered_set<glm::ivec3>, glm::ivec3, glm::ivec3> paintBrush3d(
  const glm::ivec3& segDims,
  const glm::ivec3& roundedPixelPos,
  const std::array<float, 3>& mmToVoxelSpacings,
  const std::array<int, 3>& mmToVoxelCoeffs,
  int brushSizeInVoxels,
  bool brushIsRound
)
{
  // Set of unique voxels to change:
  std::unordered_set<glm::ivec3> voxelToChange;

  // Min/max corners of the set of voxels to change:
  glm::ivec3 minVoxel{std::numeric_limits<int>::max()};
  glm::ivec3 maxVoxel{std::numeric_limits<int>::lowest()};

  const int radius_int = brushSizeInVoxels - 1;
  const float radius_f = static_cast<float>(radius_int);

  for (int k = -mmToVoxelCoeffs[2] * radius_int; k <= mmToVoxelCoeffs[2] * radius_int; ++k)
  {
    const int K = roundedPixelPos.z + k;
    if (K < 0 || K >= segDims.z)
      continue;

    for (int j = -mmToVoxelCoeffs[1] * radius_int; j <= mmToVoxelCoeffs[1] * radius_int; ++j)
    {
      const int J = roundedPixelPos.y + j;
      if (J < 0 || J >= segDims.y)
        continue;

      for (int i = -mmToVoxelCoeffs[0] * radius_int; i <= mmToVoxelCoeffs[0] * radius_int; ++i)
      {
        const int I = roundedPixelPos.x + i;
        if (I < 0 || I >= segDims.x)
          continue;

        const glm::ivec3 p{I, J, K};
        bool changed = false;

        const float ii = static_cast<float>(i);
        const float jj = static_cast<float>(j);
        const float kk = static_cast<float>(k);

        if (!brushIsRound)
        {
          // Square brush
          if (std::max(std::max(std::abs(ii / mmToVoxelSpacings[0]), std::abs(jj / mmToVoxelSpacings[1])), std::abs(kk / mmToVoxelSpacings[2])) <= radius_f)
          {
            voxelToChange.insert(p);
            changed = true;
          }
        }
        else
        {
          // Round brush: additional check that voxel is inside the sphere of radius N
          if ((ii * ii / (mmToVoxelSpacings[0] * mmToVoxelSpacings[0]) + jj * jj / (mmToVoxelSpacings[1] * mmToVoxelSpacings[1]) + kk * kk / (mmToVoxelSpacings[2] * mmToVoxelSpacings[2])) <= radius_f * radius_f)
          {
            voxelToChange.insert(p);
            changed = true;
          }
        }

        if (changed)
        {
          minVoxel = glm::min(minVoxel, p);
          maxVoxel = glm::max(maxVoxel, p);
        }
      }
    }
  }

  return std::make_tuple(voxelToChange, minVoxel, maxVoxel);
}

void updateSeg(
  const std::unordered_set<glm::ivec3>& voxelsToChange,
  const glm::ivec3& minVoxel,
  const glm::ivec3& maxVoxel,

  int64_t labelToPaint,
  int64_t labelToReplace,
  bool brushReplacesBgWithFg,

  Image& seg,

  const std::function<void(
    const ComponentType& memoryComponentType,
    const glm::uvec3& offset,
    const glm::uvec3& size,
    const int64_t* data
  )>& updateSegTexture
)
{
  static constexpr std::size_t sk_comp = 0;
  static const glm::ivec3 sk_voxelOne{1, 1, 1};

  // Create a rectangular block of contiguous voxel value data that will be set in the texture:
  std::vector<glm::ivec3> voxelPositions;
  std::vector<int64_t> voxelValues;

  for (int k = minVoxel.z; k <= maxVoxel.z; ++k)
  {
    for (int j = minVoxel.y; j <= maxVoxel.y; ++j)
    {
      for (int i = minVoxel.x; i <= maxVoxel.x; ++i)
      {
        const glm::ivec3 p{i, j, k};
        voxelPositions.emplace_back(p);

        if (voxelsToChange.count(p) > 0)
        {
          // Marked to change, so paint it:
          if (brushReplacesBgWithFg)
          {
            const int64_t currentLabel = seg.value<int64_t>(sk_comp, i, j, k).value_or(0);

            if (labelToReplace == currentLabel)
            {
              voxelValues.emplace_back(labelToPaint);
            }
            else
            {
              voxelValues.emplace_back(currentLabel);
            }
          }
          else
          {
            voxelValues.emplace_back(labelToPaint);
          }
        }
        else
        {
          // Not marked to change, so replace with the current label:
          voxelValues.emplace_back(seg.value<int64_t>(sk_comp, i, j, k).value_or(0));
        }
      }
    }
  }

  if (voxelPositions.empty())
  {
    return;
  }

  const glm::uvec3 dataOffset{minVoxel};
  const glm::uvec3 dataSize{maxVoxel - minVoxel + sk_voxelOne};

  // Safety check:
  const std::size_t N = static_cast<std::size_t>(dataSize.x) * static_cast<std::size_t>(dataSize.y)
                        * static_cast<std::size_t>(dataSize.z);

  if (N != voxelPositions.size() || N != voxelValues.size())
  {
    spdlog::error("Invalid number of voxels when performing segmentation");
    return;
  }

  // Set values in the segmentation image:
  for (std::size_t i = 0; i < voxelPositions.size(); ++i)
  {
    seg.setValue(
      sk_comp, voxelPositions[i].x, voxelPositions[i].y, voxelPositions[i].z, voxelValues[i]
    );
  }

  updateSegTexture(seg.header().memoryComponentType(), dataOffset, dataSize, voxelValues.data());
}

} // namespace

void paintSegmentation(
  Image& seg,

  int64_t labelToPaint,
  int64_t labelToReplace,
  bool brushReplacesBgWithFg,
  bool brushIsRound,
  bool brushIs3d,
  bool brushIsIsotropic,
  int brushSizeInVoxels,

  const glm::ivec3& roundedPixelPos,
  const glm::vec4& voxelViewPlane,

  const std::function<void(
    const ComponentType& memoryComponentType,
    const glm::uvec3& offset,
    const glm::uvec3& size,
    const int64_t* data
  )>& updateSegTexture
)
{
  // Set the brush radius (not including the central voxel): Radius = (brush width - 1) / 2
  // A single voxel brush has radius zero, a width 3 voxel brush has radius 1,
  // a width 5 voxel brush has radius 2, etc.

  // Coefficients that convert mm to voxel spacing:
  std::array<float, 3> mmToVoxelSpacings{1.0f, 1.0f, 1.0f};

  // Integer versions of the mm to voxel coefficients:
  std::array<int, 3> mmToVoxelCoeffs{1, 1, 1};

  if (brushIsIsotropic)
  {
    // Compute factors that account for anisotropic spacing:
    static constexpr bool sk_isotropicAlongMaxSpacingAxis = false;

    const float spacing = (sk_isotropicAlongMaxSpacingAxis) ? glm::compMax(seg.header().spacing())
                                                            : glm::compMin(seg.header().spacing());

    for (uint32_t i = 0; i < 3; ++i)
    {
      mmToVoxelSpacings[i] = spacing / seg.header().spacing()[static_cast<int>(i)];
      mmToVoxelCoeffs[i] = std::max(static_cast<int>(std::ceil(mmToVoxelSpacings[i])), 1);
    }
  }

  // Set of unique voxels to change:
  std::unordered_set<glm::ivec3> voxelsToChange;

  // Min/max corners of the set of voxels to change
  glm::ivec3 maxVoxel{std::numeric_limits<int>::lowest()};
  glm::ivec3 minVoxel{std::numeric_limits<int>::max()};

  if (brushIs3d)
  {
    std::tie(voxelsToChange, minVoxel, maxVoxel) = paintBrush3d(
      seg.header().pixelDimensions(),
      roundedPixelPos,
      mmToVoxelSpacings,
      mmToVoxelCoeffs,
      brushSizeInVoxels,
      brushIsRound
    );
  }
  else
  {
    std::tie(voxelsToChange, minVoxel, maxVoxel) = paintBrush2d(
      voxelViewPlane,
      seg.header().pixelDimensions(),
      roundedPixelPos,
      mmToVoxelSpacings,
      brushSizeInVoxels,
      brushIsRound
    );
  }

  updateSeg(
    voxelsToChange,
    minVoxel,
    maxVoxel,
    labelToPaint,
    labelToReplace,
    brushReplacesBgWithFg,
    seg,
    updateSegTexture
  );
}

/// @todo Implement algorithm for filling smoothed polygons.
void fillSegmentationWithPolygon(
  Image& seg,
  const Annotation* annot,

  int64_t labelToPaint,
  int64_t labelToReplace,
  bool brushReplacesBgWithFg,

  const std::function<void(
    const ComponentType& memoryComponentType,
    const glm::uvec3& offset,
    const glm::uvec3& size,
    const int64_t* data
  )>& updateSegTexture
)
{
  static constexpr std::size_t OUTER_BOUNDARY = 0;

  // Fill based on corners of voxels?
  // If true, then the test for whether a voxel is in the polygon is based only the voxel center.
  // If false, then the test is based on all voxel corners.
  /// @todo Make this a setting
  static constexpr bool sk_fillBasedOnCorners = true;

  if (!annot->isClosed() || annot->isSmoothed())
  {
    spdlog::warn("Cannot fill annotation polygon that is not closed and not smoothed.");
    return;
  }

  const glm::mat4& pixel_T_subject = seg.transformations().pixel_T_subject();
  const glm::mat4& subject_T_pixel = seg.transformations().subject_T_pixel();

  // Convert from space of the annotation plane to segmentation pixel coordinates
  auto convertPointFromAnnotPlaneToRoundedSegPixelCoords =
    [&pixel_T_subject, &annot](const glm::vec2& annotPlanePos)
  {
    const glm::vec4
      subjectPos{annot->unprojectFromAnnotationPlaneToSubjectPoint(annotPlanePos), 1.0f};
    const glm::vec4 pixelPos = pixel_T_subject * subjectPos;
    return glm::ivec3{glm::round(glm::vec3{pixelPos / pixelPos.w})};
  };

  // Convert from segmentation pixel coordinates to the space of the annotation plane
  auto convertPointFromSegPixelCoordsToAnnotPlane =
    [&subject_T_pixel, &annot](const glm::vec3& pixelPos)
  {
    const glm::vec4 subjectPos = subject_T_pixel * glm::vec4{pixelPos, 1.0f};
    return annot->projectSubjectPointToAnnotationPlane(glm::vec3{subjectPos / subjectPos.w});
  };

  // Min and max corners of the polygon AABB in annotation plane space:
  const auto aabb = annot->polygon().getAABBox();
  if (!aabb)
    return;

  const glm::vec2 annotPlaneAabbMinCorner = aabb->first;
  const glm::vec2 annotPlaneAabbMaxCorner = aabb->second;

  const glm::ivec3 pixelAabbMinCorner = convertPointFromAnnotPlaneToRoundedSegPixelCoords(
    annotPlaneAabbMinCorner
  );
  const glm::ivec3 pixelAabbMaxCorner = convertPointFromAnnotPlaneToRoundedSegPixelCoords(
    annotPlaneAabbMaxCorner
  );

  // Polygon vertices in the space of the annotation plane
  const std::vector<glm::vec2>& annotPlaneVertices = annot->getBoundaryVertices(OUTER_BOUNDARY);
  if (annotPlaneVertices.empty())
    return;

  // Subject plane normal vector transformed into Voxel space:
  const glm::vec3 pixelAnnotPlaneNormal = glm::normalize(
    glm::inverseTranspose(glm::mat3(pixel_T_subject)) * glm::vec3{annot->getSubjectPlaneEquation()}
  );

  // First vertex in Subject space, then in Pixel space:
  const glm::vec3 subjectAnnotPlanePoint = annot->unprojectFromAnnotationPlaneToSubjectPoint(
    annotPlaneVertices.front()
  );

  const glm::vec4 pixelAnnotPlanePoint = pixel_T_subject * glm::vec4{subjectAnnotPlanePoint, 1.0f};

  // Annotation plane in Pixel space:
  const glm::vec4 pixelPlaneEquation = math::makePlane(
    pixelAnnotPlaneNormal, glm::vec3{pixelAnnotPlanePoint / pixelAnnotPlanePoint.w}
  );

  // Loop over the AABB in Pixel/Voxel space. Note that this is inefficient and tests
  // too many voxels when the annotation plane is oblique in Voxel space.

  const int minK = std::min(pixelAabbMinCorner.z, pixelAabbMaxCorner.z) - 1;
  const int maxK = std::max(pixelAabbMinCorner.z, pixelAabbMaxCorner.z) + 1;

  const int minJ = std::min(pixelAabbMinCorner.y, pixelAabbMaxCorner.y) - 1;
  const int maxJ = std::max(pixelAabbMinCorner.y, pixelAabbMaxCorner.y) + 1;

  const int minI = std::min(pixelAabbMinCorner.x, pixelAabbMaxCorner.x) - 1;
  const int maxI = std::max(pixelAabbMinCorner.x, pixelAabbMaxCorner.x) + 1;

  // Set of unique voxels to change:
  std::unordered_set<glm::ivec3> voxelsToChange;

  const glm::ivec3 segDims{seg.header().pixelDimensions()};

  for (int k = minK; k <= maxK; ++k)
  {
    for (int j = minJ; j <= maxJ; ++j)
    {
      for (int i = minI; i <= maxI; ++i)
      {
        const glm::ivec3 roundedPixelPos{i, j, k};
        const glm::vec3 pixelPos{roundedPixelPos};

        if (!isVoxelInSeg(segDims, roundedPixelPos))
          continue;

        // Does the voxel intersect the annotation plane?
        // This check is needed when the annotation plane is oblique in Pixel space
        // due to our inefficient algorithm.
        if (!voxelInterectsPlane(pixelPlaneEquation, pixelPos))
          continue;

        const glm::vec2 annotPlanePos = convertPointFromSegPixelCoordsToAnnotPlane(pixelPos);

        if (math::pnpoly(annotPlaneVertices, annotPlanePos) || (sk_fillBasedOnCorners && (math::pnpoly(annotPlaneVertices, convertPointFromSegPixelCoordsToAnnotPlane(pixelPos + glm::vec3{0.5f, 0.5f, 0.5f})) || math::pnpoly(annotPlaneVertices, convertPointFromSegPixelCoordsToAnnotPlane(pixelPos + glm::vec3{0.5f, 0.5f, -0.5f})) || math::pnpoly(annotPlaneVertices, convertPointFromSegPixelCoordsToAnnotPlane(pixelPos + glm::vec3{0.5f, -0.5f, 0.5f})) || math::pnpoly(annotPlaneVertices, convertPointFromSegPixelCoordsToAnnotPlane(pixelPos + glm::vec3{0.5f, -0.5f, -0.5f})) || math::pnpoly(annotPlaneVertices, convertPointFromSegPixelCoordsToAnnotPlane(pixelPos + glm::vec3{-0.5f, 0.5f, 0.5f})) || math::pnpoly(annotPlaneVertices, convertPointFromSegPixelCoordsToAnnotPlane(pixelPos + glm::vec3{-0.5f, 0.5f, -0.5f})) || math::pnpoly(annotPlaneVertices, convertPointFromSegPixelCoordsToAnnotPlane(pixelPos + glm::vec3{-0.5f, -0.5f, 0.5f})) || math::pnpoly(annotPlaneVertices, convertPointFromSegPixelCoordsToAnnotPlane(pixelPos + glm::vec3{-0.5f, -0.5f, -0.5f})))))
        {
          voxelsToChange.insert(roundedPixelPos);
          continue;
        }
      }
    }
  }

  // Min/max corners of the set of voxels to change
  glm::ivec3 maxVoxel{std::numeric_limits<int>::lowest()};
  glm::ivec3 minVoxel{std::numeric_limits<int>::max()};

  for (const auto& p : voxelsToChange)
  {
    minVoxel = glm::min(minVoxel, p);
    maxVoxel = glm::max(maxVoxel, p);
  }

  updateSeg(
    voxelsToChange,
    minVoxel,
    maxVoxel,
    labelToPaint,
    labelToReplace,
    brushReplacesBgWithFg,
    seg,
    updateSegTexture
  );
}
