#include "logic/segmentation/AnnotationSegmentation.h"

#include "Image.h"
#include "SegUtil.h"

#include "MathFuncs.h"

#include "logic/annotation/Annotation.h"
#include "logic/camera/MathUtility.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>

#include <spdlog/spdlog.h>

#include <limits>
#include <vector>

namespace
{

bool voxelInterectsPlane(const glm::vec4& voxelViewPlane, const glm::vec3& voxelPos)
{
  static const glm::vec3 cornerOffset{0.5f, 0.5f, 0.5f};
  return math::testAABBoxPlaneIntersection(voxelPos, voxelPos + cornerOffset, voxelViewPlane);
}

bool isVoxelInSeg(const glm::ivec3 segDims, const glm::ivec3& voxelPos)
{
  return (
    voxelPos.x >= 0 && voxelPos.x < segDims.x && voxelPos.y >= 0 && voxelPos.y < segDims.y && voxelPos.z >= 0 &&
    voxelPos.z < segDims.z);
}

} // namespace

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
    const int64_t* data)>& updateSegTexture)
{
  static constexpr std::size_t OUTER_BOUNDARY = 0;

  // Fill based on corners of voxels?
  // If true, then the test for whether a voxel is in the polygon is based only the voxel center.
  // If false, then the test is based on all voxel corners.
  /// @todo Make this a setting
  static constexpr bool sk_fillBasedOnCorners = true;

  if (!annot->isClosed() || annot->isSmoothed()) {
    spdlog::warn("Cannot fill annotation polygon that is not closed and not smoothed.");
    return;
  }

  const glm::mat4& pixel_T_subject = seg.transformations().pixel_T_subject();
  const glm::mat4& subject_T_pixel = seg.transformations().subject_T_pixel();

  // Convert from space of the annotation plane to segmentation pixel coordinates
  auto convertPointFromAnnotPlaneToRoundedSegPixelCoords = [&pixel_T_subject, &annot](const glm::vec2& annotPlanePos) {
    const glm::vec4 subjectPos{annot->unprojectFromAnnotationPlaneToSubjectPoint(annotPlanePos), 1.0f};
    const glm::vec4 pixelPos = pixel_T_subject * subjectPos;
    return glm::ivec3{glm::round(glm::vec3{pixelPos / pixelPos.w})};
  };

  // Convert from segmentation pixel coordinates to the space of the annotation plane
  auto convertPointFromSegPixelCoordsToAnnotPlane = [&subject_T_pixel, &annot](const glm::vec3& pixelPos) {
    const glm::vec4 subjectPos = subject_T_pixel * glm::vec4{pixelPos, 1.0f};
    return annot->projectSubjectPointToAnnotationPlane(glm::vec3{subjectPos / subjectPos.w});
  };

  // Min and max corners of the polygon AABB in annotation plane space:
  const auto aabb = annot->polygon().getAABBox();
  if (!aabb) return;

  const glm::vec2 annotPlaneAabbMinCorner = aabb->first;
  const glm::vec2 annotPlaneAabbMaxCorner = aabb->second;

  const glm::ivec3 pixelAabbMinCorner = convertPointFromAnnotPlaneToRoundedSegPixelCoords(annotPlaneAabbMinCorner);
  const glm::ivec3 pixelAabbMaxCorner = convertPointFromAnnotPlaneToRoundedSegPixelCoords(annotPlaneAabbMaxCorner);

  // Polygon vertices in the space of the annotation plane
  const std::vector<glm::vec2>& annotPlaneVertices = annot->getBoundaryVertices(OUTER_BOUNDARY);
  if (annotPlaneVertices.empty()) return;

  // Subject plane normal vector transformed into Voxel space:
  const glm::vec3 pixelAnnotPlaneNormal =
    glm::normalize(glm::inverseTranspose(glm::mat3(pixel_T_subject)) * glm::vec3{annot->getSubjectPlaneEquation()});

  // First vertex in Subject space, then in Pixel space:
  const glm::vec3 subjectAnnotPlanePoint =
    annot->unprojectFromAnnotationPlaneToSubjectPoint(annotPlaneVertices.front());

  const glm::vec4 pixelAnnotPlanePoint = pixel_T_subject * glm::vec4{subjectAnnotPlanePoint, 1.0f};

  // Annotation plane in Pixel space:
  const glm::vec4 pixelPlaneEquation =
    math::makePlane(pixelAnnotPlaneNormal, glm::vec3{pixelAnnotPlanePoint / pixelAnnotPlanePoint.w});

  // Loop over the AABB in Pixel/Voxel space. Note that this is inefficient and tests
  // too many voxels when the annotation plane is oblique in Voxel space.

  const int minK = std::min(pixelAabbMinCorner.z, pixelAabbMaxCorner.z) - 1;
  const int maxK = std::max(pixelAabbMinCorner.z, pixelAabbMaxCorner.z) + 1;

  const int minJ = std::min(pixelAabbMinCorner.y, pixelAabbMaxCorner.y) - 1;
  const int maxJ = std::max(pixelAabbMinCorner.y, pixelAabbMaxCorner.y) + 1;

  const int minI = std::min(pixelAabbMinCorner.x, pixelAabbMaxCorner.x) - 1;
  const int maxI = std::max(pixelAabbMinCorner.x, pixelAabbMaxCorner.x) + 1;

  SegmentationVoxelSet voxelsToChange;

  const glm::ivec3 segDims{seg.header().pixelDimensions()};

  for (int k = minK; k <= maxK; ++k) {
    for (int j = minJ; j <= maxJ; ++j) {
      for (int i = minI; i <= maxI; ++i) {
        const glm::ivec3 roundedPixelPos{i, j, k};
        const glm::vec3 pixelPos{roundedPixelPos};

        if (!isVoxelInSeg(segDims, roundedPixelPos)) continue;

        // Does the voxel intersect the annotation plane?
        // This check is needed when the annotation plane is oblique in Pixel space
        // due to our inefficient algorithm.
        if (!voxelInterectsPlane(pixelPlaneEquation, pixelPos)) continue;

        const glm::vec2 annotPlanePos = convertPointFromSegPixelCoordsToAnnotPlane(pixelPos);

        if (
          math::pnpoly(annotPlaneVertices, annotPlanePos) ||
          (sk_fillBasedOnCorners &&
           (math::pnpoly(
              annotPlaneVertices,
              convertPointFromSegPixelCoordsToAnnotPlane(pixelPos + glm::vec3{0.5f, 0.5f, 0.5f})) ||
            math::pnpoly(
              annotPlaneVertices,
              convertPointFromSegPixelCoordsToAnnotPlane(pixelPos + glm::vec3{0.5f, 0.5f, -0.5f})) ||
            math::pnpoly(
              annotPlaneVertices,
              convertPointFromSegPixelCoordsToAnnotPlane(pixelPos + glm::vec3{0.5f, -0.5f, 0.5f})) ||
            math::pnpoly(
              annotPlaneVertices,
              convertPointFromSegPixelCoordsToAnnotPlane(pixelPos + glm::vec3{0.5f, -0.5f, -0.5f})) ||
            math::pnpoly(
              annotPlaneVertices,
              convertPointFromSegPixelCoordsToAnnotPlane(pixelPos + glm::vec3{-0.5f, 0.5f, 0.5f})) ||
            math::pnpoly(
              annotPlaneVertices,
              convertPointFromSegPixelCoordsToAnnotPlane(pixelPos + glm::vec3{-0.5f, 0.5f, -0.5f})) ||
            math::pnpoly(
              annotPlaneVertices,
              convertPointFromSegPixelCoordsToAnnotPlane(pixelPos + glm::vec3{-0.5f, -0.5f, 0.5f})) ||
            math::pnpoly(
              annotPlaneVertices,
              convertPointFromSegPixelCoordsToAnnotPlane(pixelPos + glm::vec3{-0.5f, -0.5f, -0.5f})))))
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

  for (const auto& p : voxelsToChange) {
    minVoxel = glm::min(minVoxel, p);
    maxVoxel = glm::max(maxVoxel, p);
  }

  updateSegmentationVoxels(
    voxelsToChange,
    minVoxel,
    maxVoxel,
    labelToPaint,
    labelToReplace,
    brushReplacesBgWithFg,
    seg,
    updateSegTexture);
}
