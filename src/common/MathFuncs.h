#ifndef MATH_FUNCS_H
#define MATH_FUNCS_H

#include "common/CoordinateFrame.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vnl/vnl_matrix_fixed.h>

#include <array>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace math
{

/**
 * @brief generateRandomHsvSamples
 * @param numSamples Number of colors to generate
 * @param hueMinMax Min and max hue
 * @param satMinMax Min and max saturation
 * @param valMinMax Min and max value/intensity
 * @param seed
 * @return Vector of colors in HSV format
 */
std::vector<glm::vec3> generateRandomHsvSamples(
  size_t numSamples,
  const std::pair<float, float>& hueMinMax,
  const std::pair<float, float>& satMinMax,
  const std::pair<float, float>& valMinMax,
  const std::optional<uint32_t>& seed
);

/**
 * @brief Compute dimensions of image in Subject space
 *
 * @param[in] pixelDimensions Number of pixels per image dimensions
 * @param[in] pixelSpacing Pixel spacing in Subject space
 *
 * @return Vector of image dimensions in Subject space
 */
glm::dvec3 computeSubjectImageDimensions(
  const glm::u64vec3& pixelDimensions, const glm::dvec3& pixelSpacing
);

/**
 * @brief Compute transformation from image Pixel space to Subject space.
 *
 * @param[in] directions Directions of image Pixel axes in Subject space
 * @param[in] pixelSpacing Pixel spacing in Subject space
 * @param[in] origin Image origin in Subject space
 *
 * @return 4x4 matrix transforming image Pixel to Subject space
 */
glm::dmat4 computeImagePixelToSubjectTransformation(
  const glm::dmat3& directions, const glm::dvec3& pixelSpacing, const glm::dvec3& origin
);

/**
 * @brief Compute transformation from image Pixel space, with coordinates (i, j, k) representing
 * pixel indices in [0, N-1] range,  to image Texture coordinates (s, t, p) in [1/(2N), 1 - 1/(2N)] range
 *
 * @param[in] pixelDimensions Number of pixels per image dimensions
 *
 * @return 4x4 matrix transforming image Pixel to Texture space
 */
glm::dmat4 computeImagePixelToTextureTransformation(const glm::u64vec3& pixelDimensions);

/**
 * @brief computeInvPixelDimensions
 * @param pixelDimensions
 * @return
 */
glm::vec3 computeInvPixelDimensions(const glm::u64vec3& pixelDimensions);

std::array<glm::vec3, 8> computeImagePixelAABBoxCorners(const glm::u64vec3& pixelDimensions);

/**
 * @brief Compute the bounding box of the image in physical Subject space.
 *
 * @param[in] pixelDimensions Number of pixels per image dimension
 * @param[in] directions Direction cosine matrix of image Pixel axes in Subject space
 * @param[in] pixelSpacing Pixel spacing in Subject space
 * @param[in] origin Image origin in Subject space
 *
 * @return Array of corners of the image bounding box in Subject space
 */
std::array<glm::vec3, 8> computeImageSubjectBoundingBoxCorners(
  const glm::u64vec3& pixelDimensions,
  const glm::mat3& directions,
  const glm::vec3& pixelSpacing,
  const glm::vec3& origin
);

std::pair<glm::vec3, glm::vec3> computeMinMaxCornersOfAABBox(
  const std::array<glm::vec3, 8>& subjectCorners
);

/**
 * @brief Compute the corners of an axis-aligned bounding box with given min/max corners
 *
 * @param[in] boxMinMaxCorners Min/max corners of the AABB
 *
 * @return All eight AABB corners
 */
std::array<glm::vec3, 8> computeAllAABBoxCornersFromMinMaxCorners(
  const std::pair<glm::vec3, glm::vec3>& boxMinMaxCorners
);

/**
 * @brief Compute the anatomical direction "SPIRAL" code of an image from its direction matrix
 *
 * @param[in] directions 3x3 direction matrix: columns are the direction vectors
 *
 * @return Pair of the three-letter direction code and boolean flag that is true when the directions are oblique
 * to the coordinate axes
 *
 * @todo SPIRAL CODE IS WRONG FOR hippo warp image
 */
std::pair<std::string, bool> computeSpiralCodeFromDirectionMatrix(const glm::dmat3& directions);

/**
 * @brief Compute the closest orthogonal anatomical direction matrix of an image
 *
 * @param[in] directions 3x3 direction matrix: columns are the direction vectors
 *
 * @return Pair of the three-letter direction code and boolean flag that is true when the directions are oblique
 * to the coordinate axes
 */
glm::dmat3 computeClosestOrthogonalDirectionMatrix(const glm::dmat3& directions);

/**
 * @brief Apply rotation to a coordinate frame about a given world center position
 * @param frame Frame to rotate
 * @param rotation Rotation, expressed as a quaternion
 * @param worldCenter Center of rotation in World space
 */
void rotateFrameAboutWorldPos(
  CoordinateFrame& frame, const glm::quat& rotation, const glm::vec3& worldCenter
);

/**
 * @brief Finds the entering intersection between a ray e1+d and the volume's bounding box.
 */
float computeRayAABBoxIntersection(
  const glm::vec3& start, const glm::vec3& dir, const glm::vec3& minCorner, const glm::vec3& maxCorner
);

std::pair<float, float> hits(glm::vec3 e1, glm::vec3 d, glm::vec3 uMinCorner, glm::vec3 uMaxCorner);

std::tuple<bool, float, float> slabs(
  glm::vec3 rayPos, glm::vec3 rayDir, glm::vec3 boxMin, glm::vec3 boxMax
);

std::optional<float> computeRayLineSegmentIntersection(
  const glm::vec2& rayOrigin, const glm::vec2& rayDir, const glm::vec2& lineA, const glm::vec2& lineB
);

std::vector<glm::vec2> computeRayAABoxIntersections(
  const glm::vec2& rayOrigin,
  const glm::vec2& rayDir,
  const glm::vec2& boxMin,
  const glm::vec2& boxSize,
  bool doBothRayDirections = false
);

/**
 * @brief Point inclusion in polygon test
 * @param poly Polygon vertices
 * @param p Test point
 * @return True iff the point is inside the polygon
 *
 * @author W. Randolph Franklin
 * @cite https://wrf.ecse.rpi.edu//Research/Short_Notes/pnpoly.html
 * @copyright 1970-2003, Wm. Randolph Franklin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * Redistributions of source code must retain the above copyright notice, this list of conditions and
 * the following disclaimers. Redistributions in binary form must reproduce the above copyright notice
 * in the documentation and/or other materials provided with the distribution.
 *
 * The name of W. Randolph Franklin may not be used to endorse or promote products derived from this
 * Software without specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
int pnpoly(const std::vector<glm::vec2>& poly, const glm::vec2& p);

/**
 * @brief Interpolate table at at key value
 * @param x Value at which to interpolate table
 * @param table Table of key/value pairs
 * @return Interpolated table value at x
 */
float interpolate(float x, const std::map<float, float>& table);

namespace convert
{

/**
 * @brief Convert a 3x3 GLM matrix to a 3x3 VNL matrix
 */
template<class T>
vnl_matrix_fixed<T, 3, 3> toVnlMatrixFixed(const glm::tmat3x3<T, glm::highp>& glmMatrix)
{
  const vnl_matrix_fixed<T, 3, 3> vnlMatrixTransposed(glm::value_ptr(glmMatrix));
  return vnlMatrixTransposed.transpose();
}

} // namespace convert

} // namespace math

#endif // MATH_FUNCS_H
