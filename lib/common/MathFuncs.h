#pragma once

#include "common/CoordinateFrame.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>

#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace math
{

/**
 * @brief Generate random HSV color samples inside the requested ranges.
 * @param numSamples Number of colors to generate.
 * @param hueMinMax Inclusive hue range.
 * @param satMinMax Inclusive saturation range.
 * @param valMinMax Inclusive value range.
 * @param seed Optional deterministic random seed.
 * @return Vector of HSV colors.
 */
std::vector<glm::vec3> generateRandomHsvSamples(
  size_t numSamples,
  const std::pair<float, float>& hueMinMax,
  const std::pair<float, float>& satMinMax,
  const std::pair<float, float>& valMinMax,
  const std::optional<uint32_t>& seed);

/**
 * @brief Compute physical image dimensions in subject space.
 * @param pixelDimensions Number of pixels along each image axis.
 * @param pixelSpacing Pixel spacing along each image axis, in subject-space units.
 * @return Physical image dimensions in subject-space units.
 */
glm::dvec3 computeSubjectImageDimensions(const glm::u64vec3& pixelDimensions, const glm::dvec3& pixelSpacing);

/**
 * @brief Compute transformation from image Pixel space to Subject space.
 * @param directions Direction cosine matrix whose columns are image axis directions in subject space.
 * @param pixelSpacing Pixel spacing along each image axis, in subject-space units.
 * @param origin Image origin in subject space.
 * @return Matrix transforming image pixel coordinates to subject coordinates.
 */
glm::dmat4 computeImagePixelToSubjectTransformation(
  const glm::dmat3& directions,
  const glm::dvec3& pixelSpacing,
  const glm::dvec3& origin);

/**
 * @brief Compute the transform from image pixel coordinates to normalized texture coordinates.
 * @param pixelDimensions Number of pixels along each image axis.
 * @return Matrix transforming pixel coordinates to texture coordinates.
 *
 * Pixel coordinates (i, j, k) in [0, N - 1] map to texture coordinates (s, t, p) in
 * [1 / (2N), 1 - 1 / (2N)], so integer pixel coordinates sample texel centers.
 */
glm::dmat4 computeImagePixelToTextureTransformation(const glm::u64vec3& pixelDimensions);

/**
 * @brief Compute reciprocal pixel dimensions as floats.
 * @param pixelDimensions Number of pixels along each image axis.
 * @return Reciprocal dimensions, with zero returned for zero-sized axes.
 */
glm::vec3 computeInvPixelDimensions(const glm::u64vec3& pixelDimensions);

/**
 * @brief Compute the corner coordinates of an image pixel-space axis-aligned bounding box.
 * @param pixelDims Number of pixels along each image axis.
 * @return Eight pixel-space corners spanning the image voxel-edge bounds.
 */
std::array<glm::vec3, 8> computeImagePixelAABBoxCorners(const glm::u64vec3& pixelDims);

/**
 * @brief Compute the bounding box of the image in physical Subject space.
 * @param pixelDims Number of pixels along each image axis.
 * @param directions Direction cosine matrix whose columns are image axis directions in subject space.
 * @param pixelSpacing Pixel spacing along each image axis, in subject-space units.
 * @param origin Image origin in subject space.
 * @return Eight image bounding-box corners in subject space.
 */
std::array<glm::vec3, 8> computeImageSubjectBoundingBoxCorners(
  const glm::u64vec3& pixelDims,
  const glm::mat3& directions,
  const glm::vec3& pixelSpacing,
  const glm::vec3& origin);

/**
 * @brief Compute minimum and maximum corners from a set of bounding-box corners.
 * @param subjectCorners Bounding-box corners.
 * @return Pair of minimum and maximum corners.
 */
std::pair<glm::vec3, glm::vec3> computeMinMaxCornersOfAABBox(const std::array<glm::vec3, 8>& subjectCorners);

/**
 * @brief Test whether an axis-aligned bounding box intersects a plane.
 * @tparam T Scalar type.
 * @tparam Q GLM qualifier.
 * @param boxCenter Center of the axis-aligned box.
 * @param boxMaxCorner Maximum corner of the axis-aligned box.
 * @param plane Plane equation coefficients (normal, distance).
 * @return True when the plane intersects the box.
 */
template<typename T, glm::qualifier Q>
bool testAABBoxPlaneIntersection(
  const glm::vec<3, T, Q>& boxCenter,
  const glm::vec<3, T, Q>& boxMaxCorner,
  const glm::vec<4, T, Q>& plane)
{
  const glm::vec<3, T, Q> extent = boxMaxCorner - boxCenter;
  const T radius = glm::dot(extent, glm::abs(glm::vec<3, T, Q>{plane}));
  const T dist = glm::dot(plane, glm::vec<4, T, Q>{boxCenter, 1});
  return (std::abs(dist) <= radius);
}

/**
 * @brief Compute the corners of an axis-aligned bounding box from its min/max corners.
 * @param boxMinMaxCorners Min/max corners of the AABB.
 * @return All eight AABB corners.
 */
std::array<glm::vec3, 8> computeAllAABBoxCornersFromMinMaxCorners(
  const std::pair<glm::vec3, glm::vec3>& boxMinMaxCorners);

/**
 * @brief Compute the anatomical direction code of an image from its direction matrix.
 * @param directions Direction matrix whose columns are image axis direction vectors.
 * @return Pair of the three-letter direction code and a flag that is true when directions are oblique.
 */
std::pair<std::string, bool> computeSpiralCodeFromDirectionMatrix(const glm::dmat3& directions);

/**
 * @brief Compute the closest orthogonal anatomical direction matrix.
 * @param directions Direction matrix whose columns are image axis direction vectors.
 * @return Orthogonalized direction matrix.
 */
glm::dmat3 computeClosestOrthogonalDirectionMatrix(const glm::dmat3& directions);

/**
 * @brief Apply a rotation to a coordinate frame about a world-space center.
 * @param frame Frame to rotate.
 * @param rotation Rotation expressed as a quaternion.
 * @param worldCenter Center of rotation in world space.
 */
void rotateFrameAboutWorldPos(CoordinateFrame& frame, const glm::quat& rotation, const glm::vec3& worldCenter);

/**
 * @brief Compute the entering intersection between a ray and an axis-aligned bounding box.
 * @param start Ray origin.
 * @param dir Ray direction.
 * @param minCorner Minimum box corner.
 * @param maxCorner Maximum box corner.
 * @return Ray parameter at the entering intersection, or an implementation-defined miss value.
 */
float computeRayAABBoxIntersection(
  const glm::vec3& start,
  const glm::vec3& dir,
  const glm::vec3& minCorner,
  const glm::vec3& maxCorner);

/**
 * @brief Compute near and far ray parameters for a ray-box intersection.
 * @param e1 Ray origin.
 * @param d Ray direction.
 * @param uMinCorner Minimum box corner.
 * @param uMaxCorner Maximum box corner.
 * @return Near and far ray parameters.
 */
std::pair<float, float> hits(glm::vec3 e1, glm::vec3 d, glm::vec3 uMinCorner, glm::vec3 uMaxCorner);

/**
 * @brief Compute ray-box intersection using the slab method.
 * @param rayPos Ray origin.
 * @param rayDir Ray direction.
 * @param boxMin Minimum box corner.
 * @param boxMax Maximum box corner.
 * @return Tuple of hit flag, entering ray parameter, and exiting ray parameter.
 */
std::tuple<bool, float, float> slabs(glm::vec3 rayPos, glm::vec3 rayDir, glm::vec3 boxMin, glm::vec3 boxMax);

/**
 * @brief Compute the forward intersection between a 2D ray and a line segment.
 * @param rayOrigin Ray origin.
 * @param rayDir Ray direction.
 * @param lineA First segment endpoint.
 * @param lineB Second segment endpoint.
 * @return Ray parameter at the intersection, or std::nullopt when there is no forward hit.
 */
std::optional<float> computeRayLineSegmentIntersection(
  const glm::vec2& rayOrigin,
  const glm::vec2& rayDir,
  const glm::vec2& lineA,
  const glm::vec2& lineB);

/**
 * @brief Compute intersections between a 2D ray and an axis-aligned box.
 * @param rayStart Ray origin.
 * @param rayDir Ray direction.
 * @param boxMin Minimum box corner.
 * @param boxSize Box size.
 * @param doBothRayDirections Whether to also test the opposite ray direction.
 * @return Intersection points.
 */
std::vector<glm::vec2> computeRayAABoxIntersections(
  const glm::vec2& rayStart,
  const glm::vec2& rayDir,
  const glm::vec2& boxMin,
  const glm::vec2& boxSize,
  bool doBothRayDirections = false);

/**
 * @brief Test whether a 2D point is inside a polygon.
 * @param poly Polygon vertices.
 * @param p Test point.
 * @return Nonzero when the point is inside the polygon.
 */
int pnpoly(const std::vector<glm::vec2>& poly, const glm::vec2& p);

/**
 * @brief Interpolate a scalar lookup table.
 * @param x Key at which to interpolate.
 * @param table Sorted key/value pairs.
 * @return Interpolated value at x.
 */
float interpolate(float x, const std::map<float, float>& table);

} // namespace math
